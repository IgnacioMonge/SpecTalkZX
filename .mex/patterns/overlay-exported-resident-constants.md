# Overlay-Exported Resident Constants

## Rule

- Do not delete resident `static` constants just because `rg` shows no same-file use. This project is a single compilation unit and exports selected local symbols to overlays through `tools\gen_overlay_defs.py`.
- Before purging a resident string or helper, check:
  - `tools\gen_overlay_defs.py`
  - `overlay\overlay_api.h`
  - `overlay\spectalk_ovl*.c`
  - `SpecTalkZX.map`
- If overlays import the symbol, deletion requires either moving an equivalent constant into each overlay or changing the overlay ABI. Measure overlay headroom first; SPCTLK2 is often near-full.

## Current Example

- `K_NICK`, `K_SERVER`, `K_PORT`, `K_PASS`, `K_NKPASS`, `K_AUTOCONN`, `K_THEME`, `K_AUTOAWAY`, `K_BEEP`, `K_NCOLOR`, `K_TRAFFIC`, `K_TS`, `K_TZ`, and `K_NOTIF` look dead inside `user_cmds.c`, but are imported by config/status overlays.
- Removing them saves resident bytes during the resident build, then breaks overlay link with missing symbols.

## Applies To

- `src\user_cmds.c`
- `tools\gen_overlay_defs.py`
- `overlay\overlay_api.h`
- `overlay\spectalk_ovl*.c`
