# SpecTalkZX — Changelog

## [Unreleased] — Uncommitted WIP

**Local build verificado con `make` el 2026-04-22.** No hay commit todavía.
Snapshots progresivos en `Development/dev-1.3.7.N/`.

### TAP size progression
| Estado | TAP | Delta vs v1.3.7.1 |
|---|---|---|
| v1.3.7.1 (committed) | 35,719 | — |
| + NiCK + cfg_apply + audit fixes | 35,861 | +142 |
| + shrink round (dedup+data+copt+arch+OVL2) | 35,534 | −185 |
| + ikkle footer PART/BPE fix (optimized) | 35,817 | +98 |
| + `/quit` disconnect confirmation guard | 35,844 | +125 |
| + overlay exit RX reset hardening | 35,844 | +125 |
| + `ABOUT` keepalive fix | 35,861 | +142 |

### Functional fixes

#### NiCK service fix
- **Bug**: auto-identify y detección de servicio fallaban cuando el servicio se llama "NiCK" en vez de "NickServ" — heurística fallback solo buscaba "Serv" en sender
- **Fix**: 5 puntos parcheados en `src/irc_handlers.c`, `src/spectalk.c`, `asm/spectalk_asm.asm` — reutiliza `S_NICK_CMD` ("NICK") con `st_stricmp` para match case-insensitive
- **Coste**: +78B (0 DATA)

#### cfg_apply key[4] guard
- **Bug**: `cfg_apply()` (`src/spectalk.c:2495`, `:2511`) accedía a `key[4]` sin verificar longitud. Con claves cortas malformadas ("ni=...", "au=...") leía pasado el NUL terminator
- **Fix**: guard `(key[2] && key[3])` en ramas "ni" y "au" antes del acceso a `key[4]`. En "ni", local `k4` evita recomputar
- **Origen**: auditoría ChatGPT 2026-04-12
- **Coste**: +64B

#### Ikkle footer PART/BPE fix
- **Bug**: con `!notif 1`, salir de un canal podía mostrar `You hav????CHANNEL` en el footer. La causa real era `notify2(S_YOU_LEFT, ...)`: si `S_YOU_LEFT` entraba comprimido con BPE, `notify2()` concatenaba bytes raw en `temp_input`.
- **Fix**:
  - `src/user_cmds.c`, `src/irc_handlers.c` y `src/spectalk.c` cancelan la notificación activa antes de mostrar `You have left ...`
  - `tools/bpe_build.py` saca `S_YOU_LEFT` de `SAFE_CONSTANTS`, de modo que `notify2()` vuelve a concatenar ASCII plano
  - la versión intermedia que añadía expansión BPE genérica en `notify()` se retiró por coste; el fix final queda estrecho al caso real
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 35,817B
- **Coste neto actual**: +283B vs `35,534B` pre-fix; recuperación de `-377B` frente a la versión genérica de 36,194B

#### `/quit` disconnect confirmation guard
- **Bug/UI gap**: `/quit` desconectaba inmediatamente, sin guard de seguridad. El usuario esperaba confirmación `Disconnect (y/n)?` en rojo antes de cerrar la sesión.
- **Fix**:
  - `src/user_cmds.c` añade `confirm_disconnect()` como helper compartido con timeout de ~5s
  - `/quit` ahora exige confirmación antes de enviar `QUIT`
  - `/connect` reutiliza el mismo helper cuando ya hay una conexión activa, unificando prompt y comportamiento
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 35,844B
- **Coste**: +27B vs la build anterior de 35,817B

#### Overlay exit RX reset hardening
- **Bug/risk**: `overlay_exit_full()` limpiaba UI/notification state, pero no descartaba el estado del parser/ring aunque `ring_buffer` se reutiliza para cargar overlays. Tras una salida de overlay o fallo de carga, podían quedar bytes/estado no-IRC visibles para el parser.
- **Fix**:
  - `asm/spectalk_asm.asm` limpia `rx_pos`
  - limpia `rx_overflow`
  - vacía el ring con `rb_tail = rb_head`
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 35,844B
- **Coste**: sin cambio visible en TAP frente a la build anterior de 35,844B

#### `ABOUT` keepalive fix
- **Bug**: con `OVERLAY_ABOUT` abierto, el main loop pausaba el keepalive saliente y `overlay_keepalive()` ignoraba `PONG`. Si un `PING :keepalive` quedaba pendiente al entrar en `ABOUT`, el `PONG` podía perderse y el cliente acababa forzando disconnect al salir pese a que la conexión seguía viva.
- **Fix**:
  - `src/spectalk.c` mantiene activo el bloque principal de keepalive también durante `ABOUT`
  - `overlay_keepalive()` consume `PONG` y limpia `keepalive_ping_sent` / `keepalive_timeout`
  - si el timeout salta durante `ABOUT`, primero hace `overlay_exit_full()` y luego ya imprime el error/desconecta
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 35,861B
- **Coste medido**: +17B vs la build anterior de 35,844B
- **Nota de shrink**: fusionar el parser `PING/PONG` dentro de `overlay_keepalive()` recuperó **64B** frente a la primera implementación del fix (`35,925B -> 35,861B`)

### Hardening & defensas (audit-z80 Codex 2026-04-12)

#### Doc `overlay_slot` unificada a 512B
- `asm/overlay_loader.asm:4` — comentario 576B → 512B (mentira histórica)
- `asm/spectalk_asm.asm:224` — alias comment añade "512B" explícito
- `overlay/overlay_api.h:58` — nuevo `#define OVERLAY_SLOT_SIZE 512` (antes sólo en comentario)

#### Capacity guard en `save_config_ovl`
- `overlay/spectalk_ovl4.c` — magic `500` → `OVERLAY_SLOT_SIZE - 12`
- Bounds check final antes de `esx_fwrite`: si `p > overlay_slot + OVERLAY_SLOT_SIZE` → `ui_err("Config too large")` y aborta
- Convierte un límite implícito en defensa explícita para futuras ampliaciones de claves

#### Stack budget note técnica
- `asm/spectalk_asm.asm:217` — bloque de comentarios documentando layout:
  - ring_buffer $F500..$FCFF (2048B)
  - free $FD00..$FF57 = 600B headroom
  - CRT_STACK_SIZE 512 → **88B margen**
- Identifica condiciones que obligan a reubicar `friend_nicks`/`away_message`/`names_target_channel`

### Size optimization round (shrink-z80 Codex, 2026-04-13)

Total: **−327B** vs baseline post-audit (35,861 → 35,534)

| Técnica | Ahorro | Ficheros |
|---|---|---|
| dedup | 64B | `src/user_cmds.c` (abort_disc redundancy + timeout path merge) |
| data | 111B | `src/user_cmds.c` (K_* keys muertas en core) |
| copt | 150B | `src/spectalk_copt.rul` + `asm/spectalk_asm.asm` (reglas `ld hl,_sym` + `ld (hl),0/1`) |
| arch | 2B | `src/spectalk.c` + `src/irc_handlers.c` (`uart_send_target_line()` helper) |
| overlay headroom | +13B OVL2 | `overlay/spectalk_ovl2.c` (no reduce .OVL final pero baja riesgo del slot) |

**Mejor ROI**: copt > data > dedup. libpull/micro ya en zona marginal.

### BSS & overlays

- BSS guard: **729B libres** antes de ring_buffer ($F36E → $F500)
- OVL1: 1385/2048
- OVL2: 1968/2048
- OVL3: 1591/2048
- OVL4: 1796/2048
- SPECTALK.OVL: 8192B fijo

### Overlay string dedup round (shrink-z80 Codex, 2026-04-21)

- `overlay/overlay_api.h` exporta claves `K_*` del core residente para overlays
- `tools/gen_overlay_defs.py` añade esos símbolos al ABI generado
- `overlay/spectalk_ovl4.c` reutiliza claves residentes en `save_config_ovl()`; ahorro verificado: **1954B → 1796B** (`-158B`)
- `overlay/spectalk_ovl2.c` reutiliza las mismas claves residentes para labels de config idénticas; build verificada en **1968B**
- El TAP queda en **35,861B** tras el fix de keepalive en `ABOUT`; `SPECTALK.OVL` sigue empaquetado a tamaño fijo

### Pending opportunities (rendimientos decrecientes)

- `jp` → `jr` en `main_print_wrapped_ram` (~1–3B)
- `notify3()` helper para secuencias `nb_init/nb/nb/NB_END` en `src/irc_handlers.c` (~15–30B)
- Revisar doble default de `irc_port` en `src/spectalk.c` (muy pequeño)

### Known open bugs

---

## [v1.3.7.1] — 2026-04-08 (committed `98321e1`)

TAP: 35,719B (−353B vs v1.3.7). Shrink-only release, cero cambios funcionales.

- `overlay_exit_full` ASM helper — 4 overlay exit sequences colapsadas (~46B)
- `switch_or_notify` C helper — 4 blocks deduplicados (~220B)
- `S_SWITCHED`, `S_ALREADY`, `S_YOU_LEFT` shared string constants
- `S_CRLF` dead code eliminado
- CRT `zero_fill_256` subroutine — 2 fills factorizados (~5B)
- `irc_send_cmd1/cmd2` shared tail via `jr isc_do_call` (~3B)

HW test verificado 2026-04-08.

---

## [v1.3.7] — Artemis II

Baseline pre-shrink. TAP 36,072B.

Features (según `release/changes.txt`):
- Nick coloring (per-nick hash)
- Smart notifications
- Word navigation (SS+arrows)
- Config save detection
- Overlay system (C compiled)
- Prompt @/> (query/channel)
- Ikkle-4 mini font
- Better esxDOS file management
- Code optimizations (−2KB)
- What's New screen
