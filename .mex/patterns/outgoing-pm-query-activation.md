# Outgoing PM Query Activation

Explicit private-message commands should activate the conversation window before sending when that is the user-facing command contract.

## Rule

- `/msg nick text` should create or find the query and switch to it before calling `irc_send_privmsg()`, so the local echo is rendered as `me> text` in the active PM window.
- Keep `irc_send_privmsg()` itself navigation-neutral unless a broader UX decision is made; it is also used by normal input and `/reply`, where implicit switching would be a separate behavior change.
- Do not switch for channel targets handled by the existing channel path (`#...`), because those are not query windows.
- The command parser calls handlers with `temp_input` after `input_clear()`, so switching/redrawing input inside `cmd_msg()` does not destroy the parsed `target` and `msg` pointers.

## Applies To

- `src/user_cmds.c` `cmd_msg()`
- `src/spectalk.c` `irc_send_privmsg()`
