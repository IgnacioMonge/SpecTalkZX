# Overlay-Exported Resident Constants

## Rule

- Do not delete resident `static` constants or public helpers just because `rg`
  shows no same-file use. This project is a single compilation unit and exports
  selected resident symbols to overlays through `tools\gen_overlay_defs.py`.
- Before purging or reusing a resident string/helper from an overlay, check:
  - `tools\gen_overlay_defs.py`
  - `overlay\overlay_api.h`
  - `overlay\spectalk_ovl*.c`
  - `SpecTalkZX.map`
- If overlays import the symbol, deletion requires either moving an equivalent
  constant/helper into each overlay or changing the overlay export set. Measure
  overlay headroom first; SPCTLK2 and SPCTLK5 are often near-full.

## Current Example

- `K_NICK`, `K_SERVER`, `K_PORT`, `K_PASS`, `K_NKPASS`, `K_AUTOCONN`, `K_THEME`, `K_AUTOAWAY`, `K_BEEP`, `K_NCOLOR`, `K_TRAFFIC`, `K_TS`, `K_TZ`, and `K_NOTIF` look dead inside `user_cmds.c`, but are imported by config/status overlays.
- Removing them saves resident bytes during the resident build, then breaks overlay link with missing symbols.
- Public helpers such as `_sys_puts` and `_reset_rx_state` can also be reused by
  overlays, but only after adding them to `tools\gen_overlay_defs.py`.
- Resident geometry helpers can be reused the same way when the ABI is exact.
  `earth_about_render.asm` now replaces its local `earth_screen_base` body with
  `jp _compute_screen_base`; the helper takes row in `A`, returns `HL`, and is
  whitelisted in `tools/gen_overlay_defs.py`.

## Applies To

- `src\user_cmds.c`
- `tools\gen_overlay_defs.py`
- `overlay\overlay_api.h`
- `overlay\spectalk_ovl*.c`
