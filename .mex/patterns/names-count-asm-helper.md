# NAMES Count ASM Helper

`h_numeric_353()` can use `_names_count_line` for the hot NAMES user-count
pass. The helper preserves the old C behavior exactly: an empty line counts as
`0`; any non-empty line counts as `1 + number of spaces`.

Do not silently switch this to token counting. Token counting is probably more
correct for malformed multiple-space input, but it changes the old semantics and
must be treated as a behavior change for `/names` and JOIN user totals.

The C fused count+friend version is not currently viable in resident code: it
built too large and overflowed the BSS tail into `_ring_buffer`. If revisiting
T9, prefer a smaller ASM/token helper or a call-site restructure that preserves
manual `/names` cancel/render ordering.

T10-style mode-prefix fast-fail in C measured larger in the current layout. Keep
the existing prefix loop unless an ASM form proves a net win.
