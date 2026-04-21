# Part Notification Replace

## Problem
When ikkle footer notifications are enabled, `You have left ...` has two failure modes:
1. It can append onto an already active footer notification.
2. It can render as `????` if the prefix is routed through a BPE-compressed `SB_` constant, because the footer copies raw bytes and does not run the BPE decoder.

## Rule
Before showing `You have left ...`, call `notif_cancel_current()` so the next notification replaces the active footer state instead of merging with it.
Expand BPE tokens inside `notify()` before storing text in `notif_buf`, so footer notifications can render both plain ASCII and BPE-compressed constants correctly.
Keep the `You have left ` prefix out of `SAFE_CONSTANTS` in `tools/bpe_build.py` as a belt-and-suspenders guard.

## Applied In
- `src/user_cmds.c` for local `/part`
- `src/irc_handlers.c` for server-confirmed self `PART`
- `src/spectalk.c` for switcher-driven `PART`
- `src/spectalk.c` `notify()` path to expand BPE tokens before ikkle rendering
- `tools/bpe_build.py` to exclude `S_YOU_LEFT` from the BPE-safe constant rename list
