# SpecTalkZX — Changelog

## [Unreleased] — Uncommitted WIP

**Local build verificado con `make` el 2026-04-21.** No hay commit todavía.
Snapshots progresivos en `Development/dev-1.3.7.N/`.

### TAP size progression
| Estado | TAP | Delta vs v1.3.7.1 |
|---|---|---|
| v1.3.7.1 (committed) | 35,719 | — |
| + NiCK + cfg_apply + audit fixes | 35,861 | +142 |
| + shrink round (dedup+data+copt+arch+OVL2) | 35,534 | −185 |
| + ikkle footer PART/BPE fix | 36,194 | +475 |

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
- **Bug**: con `!notif 1`, salir de un canal podía mostrar `You hav????CHANNEL` en el footer. El prefijo `S_YOU_LEFT` podía llegar comprimido con BPE y `notify()` copiaba bytes raw a `notif_buf` sin expandir tokens
- **Fix**:
  - `src/spectalk.c` expande tokens BPE dentro de `notify()` antes de poblar `notif_buf` y compara contra la versión ya expandida para deduplicar bien
  - `src/user_cmds.c`, `src/irc_handlers.c` y `src/spectalk.c` cancelan la notificación activa antes de mostrar `You have left ...`
  - `tools/bpe_build.py` saca `S_YOU_LEFT` de `SAFE_CONSTANTS` como defensa extra
- **Verificación**: `make` OK el 2026-04-21, `build/SpecTalkZX.tap` = 36,194B

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
- OVL2: 2034/2048 (+13B headroom recuperados)
- OVL3: 1591/2048
- OVL4: 1961/2048
- SPECTALK.OVL: 8192B fijo

### Pending opportunities (rendimientos decrecientes)

- `jp` → `jr` en `main_print_wrapped_ram` (~1–3B)
- `notify3()` helper para secuencias `nb_init/nb/nb/NB_END` en `src/irc_handlers.c` (~15–30B)
- Revisar doble default de `irc_port` en `src/spectalk.c` (muy pequeño)

### Known open bugs

- **Overlay keepalive**: server puede desconectar si overlay abierto varios minutos (pendiente desde v1.3.7)

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
