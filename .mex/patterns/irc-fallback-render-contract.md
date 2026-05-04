# IRC Fallback Render Contract

Some real IRC traffic can still reach fallback paths: unknown uppercase commands
through `h_default_cmd()` and non-channel/non-self `MODE` through the tail of
`h_mode()`. Do not treat that traffic as disposable just because it is not in
the preferred handler path.

## Rule

- Fallback IRC output that reaches the main area must call
  `main_print_time_prefix()` before printing text.
- Keep the line visible. Do not "fix" these cases by dropping `PRIVMSG`, `MODE`,
  or other real protocol lines unless a separate RX/parser proof shows they are
  corrupted fragments.
- Timestamp prefix sets `wrap_indent=6`, so even fallback output that uses
  `main_puts()` / `main_print()` will continue under the normal timestamp
  indent instead of restarting at column 0.
- This is presentation containment only. It does not change RX drain policy,
  `/names`, `_frame_wait()`, or IRC command dispatch semantics.

## Applied In

- `src/irc_handlers.c` `h_mode()`
- `src/irc_handlers.c` `h_default_cmd()`
