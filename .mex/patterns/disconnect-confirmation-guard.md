# Disconnect Confirmation Guard

When a user command is about to tear down an active IRC/TCP connection, route it through a shared confirmation helper instead of disconnecting immediately.

## Rule
- Ask `Disconnect (y/n)?` using error/red styling so the destructive action is visually explicit.
- Reuse one helper for all command paths that need the prompt, rather than duplicating polling loops.
- If a caller needs context before the prompt, print that prefix with `main_puts()` and no newline so the shared prompt remains on the same line.
- Keep the existing input-drain behavior while waiting, so UART traffic does not leave stale lines in the parser state.
- Treat timeout the same as `n`: print `Cancelled` and leave the session intact.

## Applied In
- `src/user_cmds.c` `cmd_quit()`
- `src/user_cmds.c` `cmd_connect()` when already connected
