# NAMES Count ASM Helper

`h_numeric_353()` can use `_names_count_line` for the hot NAMES user-count
pass. The helper preserves the old C behavior exactly: an empty line counts as
`0`; any non-empty line counts as `1 + number of spaces`.

Fast-loop experiment, 2026-05-05: the accepted test form pre-decrements `HL`
after proving the line is non-empty, then increments `HL` at loop entry. This
makes the common non-space byte a direct back-edge while preserving a leading
space if malformed input ever starts with one. Do not use the shorter-looking
`ret z` empty fast exit unless `L` is explicitly set to zero; otherwise it
returns the low byte of the input pointer, not count zero. Measured build cost
on branch `codex/names-count-line-fast`: TAP/resident `+1B`, BSS unchanged in
meaning (`0xF00B < 0xF500`, `1269B` free).

Do not silently switch this to token counting. Token counting is probably more
correct for malformed multiple-space input, but it changes the old semantics and
must be treated as a behavior change for `/names` and JOIN user totals.

The C fused count+friend version is not currently viable in resident code: it
built too large and overflowed the BSS tail into `_ring_buffer`. If revisiting
T9, prefer a smaller ASM/token helper or a call-site restructure that preserves
manual `/names` cancel/render ordering.

T10-style mode-prefix fast-fail in C measured larger in the current layout. Keep
the existing prefix loop unless an ASM form proves a net win.

Background automatic JOIN/NAMES counting uses `_names_count_line` only for
off-target `353` chunks where `CH_FLAG_NAMING` is set for `msg_chan`. The
current/manual target keeps the old single accumulator scan. Avoid a full
"always count first, then reuse" shape: it measured smaller than the first C
version but still larger than the off-target-only fix.
