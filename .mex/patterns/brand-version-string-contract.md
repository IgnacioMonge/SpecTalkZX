# Brand Version String Contract

## Rule

- UI-facing app/version strings use `SpecTalkZX 1.3.8` style:
  - `SpecTalkZX` has no space before `ZX`.
  - Numeric release versions do not need a leading `v` in the normal banner.
  - Keep the short app name as `SpecTalkZX`.
- All-caps big-font banner strings may use `SPECTALKZX`, but must preserve the
  same no-space/no-`v` contract.
- If the banner title length changes, re-check the following subtitle column so
  `IRC Client for ZX Spectrum` stays directly after the ` - ` separator.

## Applies To

- `src/spectalk.c` `S_APPNAME`, `S_APPSHORT`
- `overlay/spectalk_ovl.c` `banner_render_ovl()`
