# No-Visible-Change Callee Shrink

For the no-visible-change shrink lane, multi-argument C helpers may be switched to `__z88dk_callee` only when every declaration and definition is updated together and the helper is not called from hand-written ASM with the old ABI.

Measure each helper independently. Keep the change only if `make NO_COLOR=1` proves a TAP/resident win and the diff contains no text, parser, limit, timing, or protocol behavior edits.

Accepted experimental checkpoint: `notify(const char *msg, uint8_t attr)` became `__z88dk_callee` on 2026-05-05, with identical notification behavior. Hardware in the main checkout confirmed the startup clock still updates normally; the earlier failed run was an executable/SD-copy issue. Combined with the exact `extract_network_short()` ASM rewrite in `C:\tmp\SpecTalkZX-gemini-shrink`, `make NO_COLOR=1` measured TAP `35693B` and BSS guard `0xF308` (`504B` free). Keep the change in the experimental worktree until the combined build is hardware-confirmed.
