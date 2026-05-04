# Status Clock Overlay Tick

The status-bar clock is part of the always-visible chrome, not overlay content.

## Rule
- Keep `time_second`/`time_minute` advancement in the common main-loop tick path, before overlay key handling.
- Do not guard the minute redraw with `!overlay_mode`; overlays may own rows 2..20, but the status bar remains visible on row 21.
- Apply minute/hour wrap before calling `draw_clock()`. Drawing before wrap can briefly show invalid times such as `[12:60]`, and if no later status redraw happens the wrong value can persist.
- `draw_status_bar_real()` may run while overlays are open when `status_bar_dirty` is set; keep its left-side bounded renderer from touching the clock columns.
- Mainline normally runs with interrupts disabled except inside `frame_wait()`, so expensive overlay work makes the clock late even if the tick path is outside the overlay branch. Animated overlays should avoid many small esxDOS calls and long per-frame work, but batching alone is not a correctness fix.
- For the `ABOUT` Earth animation, use `overlay_call_timed()` for the animation tick only. It preserves caller `IY`, sets `IY=0x5C3A`, enables IM1 interrupts while the overlay entry runs, disables interrupts again, then restores `IY`. That lets ROM `FRAMES` advance during long DAT/draw work without changing the normal mainline `DI` contract.
- esxDOS calls can still hide interrupt time internally. For long-running animated overlays, minimize `F_READ` frequency and avoid close/open/skip loops in cyclic streams; use `F_SEEK` on the already-open DAT handle when rewinding is needed.
- If the asset has temporal dithering, ticking every other main-loop frame can both cut SD I/O and reduce perceived shimmer; keep the status clock itself driven by elapsed `FRAMES`, not by animation ticks.
- Do not replace generic `overlay_call()` with the timed variant without auditing the target overlay entry: it must not rely on `IY`, must not borrow `SP`, and must tolerate ROM ISR register preservation while it runs.

## Applied In
- `src/spectalk.c` main loop one-second tick
- `asm/overlay_loader.asm` `overlay_call_timed()` for `OVERLAY_ABOUT` animation ticks
- `src/spectalk.c` gates `OVERLAY_ABOUT` visual ticks to every other main-loop frame.
- `tools/bpe_build.py` pads `!about` Earth deltas to the generated maximum packet size (`481B` in the current pass) so `globe_tick_ovl()` uses one DAT read per visual animation tick.
- `overlay/earth_about_render.asm` exposes an overlay-local `F_SEEK` helper for rewinding the delta stream without close/open/skip churn.
- `overlay/earth_about_render.asm` preloads the 176x24 about logo in two reads instead of 24 row reads.
