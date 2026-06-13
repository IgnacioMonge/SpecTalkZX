# ASM Callee Stack Path Proof

Any hand-written ASM replacement for a C helper with multiple early exits must prove that every return path has the same stack height as entry.

## Rule
- Count each `push`, `pop`, packed-byte `pop rr`, `dec sp`, and helper call in every branch, including NULL/empty-input exits.
- Be extra cautious with parsers that save both an input pointer and an output token pointer; an early return between those saves can leave the stack shifted.
- Do not replace startup config parsers with ASM unless the stack proof is simple enough to audit locally and the build is tested on hardware.

## Rejected Shape
- The first ASM `csv_next_tok(char **pp)` fastcall attempt returned early from NULL/empty-token paths without balancing all saved state in every path. It built cleanly but caused severe font/screen corruption on hardware, consistent with stack/memory corruption during startup config parsing.
- Keep `csv_next_tok()` in C unless a future version is much simpler and has explicit per-label stack-height notes.
