# Scroll Border Profiling

## Context

`_scroll_main_zone()` is the dominant render spike: it copies 4096 bitmap bytes,
512 attribute bytes, and clears row 19. Its release path must stay free of debug
border writes, but the project needs an easy hardware-visible way to measure the
scroll burst.

## Rule

Use the optional assembler define `SCROLL_PROFILE` only for measurement builds:

```sh
make SCROLL_PROFILE=1
```

This adds phase-colored border writes inside `_scroll_main_zone()` and restores
the theme border before returning. Plain `make` must remain byte-identical to the
normal build.

Phase colors:

- red: bitmap copy;
- yellow: attribute copy;
- cyan: row-19 clear;
- normal theme border: scroll routine has returned.

## Test

For profiling on hardware or emulator:

- load `build/SpecTalkZX.tap` after `make SCROLL_PROFILE=1`;
- trigger continuous chat scroll, ideally with a flood or large NAMES/MOTD burst;
- observe/record which color dominates during each scroll;
- compare against RX behavior: missing lines, disconnects, or `rx_overflow`;
- run plain `make` again before release or packaging.

## Current Numbers

- Normal build: `35674B` trimmed / `35754B` TAP / `461B` slack.
- Profile build: `35691B` trimmed / `35771B` TAP / `444B` slack.

## Current Observation

On hardware, the scroll burst is mostly red, with much smaller yellow/cyan
bands. Treat the 4096-byte bitmap copy as the bottleneck. Attribute copy and
row-19 clear are visible but secondary; optimizing those tail phases cannot
materially change the perceived scroll spike.

## Rejected LDI Unroll Probe

An experimental build replaced only the 224-byte block-3 bitmap `LDIR` with an
unrolled `LDI` byte stream. This was the largest single contiguous bitmap block
and the best T-state saving per resident byte among simple `LDIR` -> `LDI`
probes.

Measured build impact:

- Normal release baseline: `35674B` trimmed / `35754B` TAP / `461B` slack.
- `SCROLL_LDI_UNROLL=1`: `36118B` trimmed / `36198B` TAP / `17B` slack.
- `SCROLL_PROFILE=1 SCROLL_LDI_UNROLL=1`: `36131B` trimmed / `36211B` TAP /
  `4B` slack.

Expected speed impact: about 224 bytes * 5T saved * 8 scanlines = ~8.9k
T-states per scroll before contention effects.

Hardware result: no apparent visual improvement; the border remained mostly red.
Because the byte cost is high and the slack becomes tiny, this shape is rejected.
Do not retry the same `LDIR` -> unrolled `LDI` block-3 conversion unless the
resident byte budget changes substantially.

## Stack Blit Experiment

`SCROLL_STACK_EXPERIMENT=1` enables an experimental stack+SMC replacement for the
same 224-byte block-3 bitmap copy. The broken `C`-counter version is rejected
because `POP BC` destroys the loop counter; the integrated variant uses `IXL`,
preserves `IX` inside the helper, and leaves interrupts disabled on exit. Do not
add `EI` here: normal IM1 is gated through `_frame_wait()`, which temporarily
sets `IY=0x5C3A` before enabling the ROM ISR.

Measured build impact after the 2026-04-29 shrink passes:

- Normal release: `35616B` trimmed / `35696B` TAP / `518B` slack.
- `SCROLL_STACK_EXPERIMENT=1`: `35692B` trimmed / `35772B` TAP / `442B` slack.
- `SCROLL_STACK_EXPERIMENT=1 SCROLL_PROFILE=1`: `35710B` trimmed / `35790B`
  TAP / `424B` slack before the no-`EI` fix; remeasure before relying on this
  combined number.
- Current combined `SCROLL_STACK_EXPERIMENT=1 RAW6_FONT_EXPERIMENT=1` after the
  low-byte destination SMC update: `35684B` trimmed / `35764B` TAP / `175B`
  slack. The destination update measured `-2B` versus the previous combined
  experiment and saves roughly `23T * 14 chunks * 8 scanlines = 2576T` per
  full scroll.

The scroll-only experiment should now fit the `+80B` cutoff by about six bytes
after the low-byte update; remeasure scroll-only before quoting a release
number. It is not release-approved until hardware profiling shows a visible
improvement and no UART or pagination loss during flood scroll.
