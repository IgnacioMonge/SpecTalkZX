# What's New Release Assets

## Context
The `!changelog` / What's New overlay is generated from release assets rather than edited directly:

- `release/version.txt`
- `release/changes.txt`
- `release/logo.png`
- generated `overlay/whatsnew_data.h`

## Rule
- Update `include/spectalk.h` and `overlay/overlay_api.h` `VERSION` alongside `release/version.txt`.
- Keep `release/changes.txt` to the visible list budget; the last entry is rendered without a bullet and in the highlight/magenta attribute, so reserve it for `And much much more!`.
- `tools/gen_whatsnew.py` currently allows a 96px-wide monochrome logo. With `WN_LOGO_WB=12`, `overlay/spectalk_ovl3.c` places the text at column 26 via `(WN_LOGO_WB + 1) * 2`.
- After changing any release asset, run `python tools/gen_whatsnew.py` or `make`, then run full `make` to verify SPCTLK3 still fits under 2048B.

## Applied In
- `release/version.txt`
- `release/changes.txt`
- `release/logo.png`
- `overlay/whatsnew_data.h`
- `tools/gen_whatsnew.py`
