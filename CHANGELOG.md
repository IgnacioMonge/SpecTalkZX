# SpecTalkZX — Changelog

## [v1.3.8] — Unreleased

**Último build local verificado con `make` el 2026-04-26.** Todo lo posterior a `v1.3.7` forma parte de la futura `v1.3.8`; `v1.3.7` sigue siendo la última versión estable.
Snapshots progresivos en `Development/dev-1.3.7.N/`.

### TAP size progression
| Estado | TAP | Delta vs v1.3.7 |
|---|---|---|
| v1.3.7 stable | 36,072 | — |
| internal shrink checkpoint, formerly `v1.3.7.1` | 35,719 | −353 |
| + NiCK + cfg_apply + audit fixes | 35,861 | −211 |
| + shrink round (dedup+data+copt+arch+OVL2) | 35,534 | −538 |
| + ikkle footer PART/BPE fix (optimized) | 35,817 | −255 |
| + `/quit` disconnect confirmation guard | 35,844 | −228 |
| + overlay exit RX reset hardening | 35,844 | −228 |
| + `ABOUT` keepalive fix | 35,861 | −211 |
| + `/mode` wrapper | 36,025 | −47 |
| + post-`/mode` shrink round | 36,006 | −66 |
| + Search UX + split/shrink ASM + PM reply/notice + UART shrinks | 36,178 | +106 |
| + 30_rendering audit (R01/R02) + shrink (M01-M05 + S01) | 36,136 | +64 |
| + Printer Buffer scratch reloc + overlay exit fragment-discard fix | 36,144 | +72 |
| current local 1.3.8 worktree after ASM/C audit passes | 35,736 | −336 |

### Current local build state

- Resident trimmed: **35,656B**
- TAP: **35,736B**
- BSS slack before `ring_buffer`: **478B**
- Overlays: **2019 / 2037 / 1982 / 2041B**

### Functional fixes

#### Help overlay pagination
- **Bug**: when the help text filled an exact number of pages, pressing a key on the last page rendered an empty extra page.
- **Fix**: `help_render_ovl()` now wraps `help_page >= total_pages` before calculating the first line to render.
- **Check**: `src/SPECTALK_HELP.txt` has all primary commands from `USER_COMMANDS` plus the direct `/0-9` switch; with 48 lines and 12 lines/page it now cycles cleanly over 4 pages.
- **Verificación**: `make` OK el 2026-04-24, `build/SpecTalkZX.tap` = 36,259B, BSS slack = 45B, overlays `1396/2037/1593/1986`.

#### IRC session bookmarks / autojoin
- **Feature**: `!save` now persists the open IRC session: configured server/port plus active non-query channels.
- **Controls**: `autoconnect=0|1` now controls only server connection; new `!autojoin` / `autojoin=0|1` controls whether saved channels are joined after registration.
- **Restore path**: config `channels=#chan,#other` is loaded as transient startup state and replayed as a single IRC `JOIN` after `001 RPL_WELCOME`; explicit `/server host` clears the pending session restore.
- **UI**: `!config` shows both `autojoin=` and the saved `channels=` list, and autojoin uses `Autojoining #chan...` instead of the manual join wording.
- **Scope**: this is the resident-safe MVP. Named multi-server bookmark selection stays deferred to an overlay/shrink pass.
- **Verificación**: `make` OK el 2026-04-24, `build/SpecTalkZX.tap` = 36,259B, BSS slack = 45B, overlays `1396/2037/1593/1986`.

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

#### `/mode` wrapper
- **Feature**: añade `/mode` como wrapper IRC ligero
- **Comportamiento**:
  - sin args: consulta `MODE` del canal actual
  - con args que empiezan por `+` o `-`: aplica esos modos al canal actual
  - con cualquier otro argumento: pasa el target explícito tal cual a `MODE`
- **Fix de protocolo**: valida el target implícito antes de tocar UART, evitando dejar un `MODE ` colgado si el usuario lanza `/mode` desde Server o desde una query
- **UX mínima**: `324 RPL_CHANNELMODEIS` ya no es silencioso; imprime una línea legible además de actualizar la barra de estado
- **Help**: la entrada de `/mode` se añadió al help y el texto fuente pasó a `src/SPECTALK_HELP.txt`, que `tools/bpe_build.py` inyecta en `SPECTALK.DAT` para que la ayuda quede versionada
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 36,025B
- **Coste**: +164B vs la build anterior de 35,861B

#### Post-`/mode` shrink round
- **Shrink**:
  - `src/irc_handlers.c` añade `notify3()` para secuencias repetidas `nb_init/nb/nb/NB_END`
  - `src/user_cmds.c` hace que `/join` reutilice `notify2()` en vez de reconstruir la misma notificación a mano
  - `src/user_cmds.c` colapsa el default duplicado de `irc_port` en `cmd_connect()`
- **Verificación**: `make` OK el 2026-04-22, `build/SpecTalkZX.tap` = 36,006B
- **Ahorro medido**: `36,025B -> 36,006B` (`-19B`)

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
- El TAP queda en **36,025B** tras añadir `/mode`; `SPECTALK.OVL` sigue empaquetado a tamaño fijo

### Search UX polish (2026-04-22)

- La salida de búsqueda pasa a línea nueva y ya no rompe el flujo del chat al mezclar resultado y prompt.
- Se limpia el marcador de cancelación/incomplete y se introduce throttling para evitar flood visual con resultados rápidos o repetidos.
- `NOTICE` se enruta al área principal y se filtra salida de parser obsoleta/estancada durante la UX de búsqueda.

### ASM split, renderer/runtime shrink, and SNTP retry (2026-04-22..2026-04-23)

- `asm/spectalk_asm.asm` queda como raíz fina y el código residente se reparte por módulos de dominio en `asm/spectalk_asm/`.
- Se añade retry de SNTP al conectar para evitar el reloj clavado en `00:00` cuando el ESP8266 aún no ha resuelto NTP en el primer intento.
- Varias rondas de shrink posteriores recortan renderer, redraw/input, text/numeric helpers, status bar y UI runtime sin cambiar overlays.
- `print_line64_fast()` y el redraw incremental quedan realineados para evitar el desfase vertical de una scanline entre texto redibujado y texto escrito carácter a carácter.

### PM reply/notice and status fixes (2026-04-23)

- Se añade `/reply` usando el último nick de PM recordado.
- Se añade `/notice` saliente sin abrir una ruta paralela grande; se apoya en los paths IRC ya existentes.
- Se corrige la regresión de contadores absurdos en la status bar (`0/0`, `4/4`, `7/7`), causada por una ruptura de ABI en `_sb_put_u8_2d()`.
- El compromiso final de esta ronda mantiene también reset del `last_pm_nick` al desconectar y rendering compacto de WHOIS (`311`/`319`) dentro del presupuesto residente.

### UART and resident ASM shrink rounds (2026-04-23)

- `asm/divmmc_uart.asm`: eliminados `_poked_byte`, `_byte_buff` y la rama `uartRead_retBuff`; los wrappers públicos se colapsan sobre el contrato real de `uartRead` (`A=0` / `A=byte`).
- Shrink seguro adicional en `20_rx_ring_uart.asm`, `40_text_numeric_screen.asm`, `50_main_output.asm` y `70_input_lookup.asm`: fuera preservaciones redundantes de `IX/IY`, tails fusionados en `_read_key()`, simplificación del overflow probe en `_try_read_line_nodrain()`, compresión de `64 - main_col` y `jr` local en `_cls_fast()`.
- Estado actual medido: **36,178B TAP**, `__data_compiler_tail = $EAC2`, `__BSS_END_tail = $F4A9`, **87B** de slack antes del overlay slot.

### Late 1.3.8 ASM audit/shrink rounds (2026-04-25..2026-04-26)

- Se corrigió la conversión UTF-8 Latin-1 después de que los shrinks desplazaran `u8a_tbl_c0` cerca de un cruce de página: `Ñ/ñ` vuelven a mapear al glifo interno `127`, y las vocales acentuadas se normalizan de forma estable a ASCII.
- Se corrigieron dos bugs reales en `40_text_numeric_screen.asm`: el test de dígitos IRC ya devuelve carry set para `0..9`, y `_fast_fill_attr()` devuelve `HL` apuntando al byte posterior al bloque rellenado.
- Se aplicaron shrinks seguros en `30_rendering.asm`, `80_ui_runtime.asm`, `20_rx_ring_uart.asm`, `60_protocol_storage.asm`, `10_core_helpers.asm`, `70_input_lookup.asm`, `overlay_loader.asm` y `divmmc_uart.asm`, manteniendo los contratos de pila, `IX/IY`, `EXX`, ring buffer y layout VRAM.
- El loader de overlays ya no recarga `_esx_count` en skips, compacta la limpieza RX post-carga y usa `pop bc` para retirar parámetros callee-clean.
- La UART divMMC/ZX-Uno evita recargas de puerto redundantes alternando `B=$FC/$FD` y limpia `_is_recv` sin preservar el byte recibido en `E`.
- El CRT zero-fill protege también el caso `BSS size == 1`, evitando un `LDIR` con `BC=0`; `_esx_fread()` carga el buffer directamente en `IX`.
- La auditoría C posterior endurece `wait_for_prompt_char()` para terminar `rx_line` también en la salida de éxito y reduce el parser `/0-9` con una comprobación `uint8_t` de rango.
- Build local actual: **35,736B TAP**, **478B** libres antes de `ring_buffer`, overlays **2019/2037/1982/2041B**.

### Pending opportunities (rendimientos decrecientes)

- `jp` → `jr` en `main_print_wrapped_ram` (~1–3B)
- Reescritura del tracker de último espacio en `main_print_wrapped_ram()` para dejar de pagar prefijos `IX`; ya no es low-risk.

### Known open bugs

---

### Internal shrink checkpoint — 2026-04-08 (committed `98321e1`, formerly labelled `v1.3.7.1`)

TAP: 35,719B (−353B vs v1.3.7). Shrink-only checkpoint, cero cambios funcionales. Se considera parte del desarrollo de `v1.3.8`, no una versión estable separada.

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
