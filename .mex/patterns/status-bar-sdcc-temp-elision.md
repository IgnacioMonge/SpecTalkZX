# Status Bar SDCC Temp Elision

For `src/spectalk.c` status-bar/UI builders, prefer reading one-use state directly where it is consumed instead of materializing short-lived locals, but measure every control-flow rewrite.

## Rule
- If a temporary is only used once in `draw_status_bar_real()`-style code, first try reading the source directly at the branch or call site; SDCC often emits smaller code than when it has to spill/cache a byte local.
- This paid off for `has_other_mention()` and `chan_flags`: direct use beat the cached locals `has_mention` and `cur_flags`.
- Treat larger “cleanups” with suspicion. Reordering nested branches to look nicer can easily bloat the generated code even when the logic is equivalent.

## Rejected Here
- Rewriting `extract_network_short()` as manual loops grew the resident by 34 bytes; keep the library-call version.
- Reordering the center-section chooser in `draw_status_bar_real()` to nest the `current_channel_idx == 0` case looked cleaner, but grew the binary by 16 bytes.

## Applied In
- `src/spectalk.c` `draw_status_bar_real()`
