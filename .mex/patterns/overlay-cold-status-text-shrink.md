# Overlay Cold Status Text Shrink

Cold overlay status messages may be shortened when the shorter text still communicates the same state and the freed bytes buy useful safety margin in a nearly full overlay.

## Rule
- Apply only to cold overlay-only output such as save/config/about status, not IRC protocol text or hot chat rendering.
- Keep error messages specific; shorten only success/progress text where the surrounding context already identifies the action.
- Measure individual `SPCTLK*.OVL` sizes, because the TAP and resident binary may not change.
- Do not remove information that is needed for debugging unless the overlay is close enough to the 2048-byte cap to justify it.

## Applied
- `overlay/spectalk_ovl4.c` now prints `Saving config... OK` instead of `Saving config... OK (<bytes> bytes)`.
- Measured result: SPCTLK4 drops from `2044B` to `2007B`; resident `make` stays at `35674B` trimmed / `35754B` TAP / `461B` BSS slack.
