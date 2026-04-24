# Part Notification Replace

## Problem
When ikkle footer notifications are enabled, `You have left ...` has two failure modes:
1. It can append onto an already active footer notification.
2. It can render as `????` if the prefix is routed through a BPE-compressed constant, because `notify2()` concatenates raw bytes and the footer path does not decode BPE tokens.

## Rule
Before showing `You have left ...`, call `notif_cancel_current()` so the next notification replaces the active footer state instead of merging with it. `notif_cancel_current()` must clear the physical notification row as well as the timer/buffer state; otherwise `/quit` or a quick disconnect can leave an old ikkle notification visible after the logical state is gone.
Do not solve this by adding a generic BPE decoder to `notify()` unless multiple footer call sites truly need it; that bloats the resident core.
Keep the `You have left ` prefix out of `SAFE_CONSTANTS` in `tools/bpe_build.py`, because `notify2()` must receive plain ASCII if it is going to concatenate directly into `temp_input`.

## Applied In
- `src/user_cmds.c` for local `/part`
- `src/user_cmds.c` for immediate `/quit` footer cleanup before transparent-mode disconnect waits
- `src/irc_handlers.c` for server-confirmed self `PART`
- `src/spectalk.c` for switcher-driven `PART`
- `src/spectalk.c` `notif_cancel_current()` / `force_disconnect()` for physical row cleanup on disconnect
- `tools/bpe_build.py` to exclude `S_YOU_LEFT` from the BPE-safe constant rename list
