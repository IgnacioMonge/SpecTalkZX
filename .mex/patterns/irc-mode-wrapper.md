# IRC Mode Wrapper

For IRC commands like `MODE`, prefer a thin wrapper over inventing a parser-heavy subsystem.

## Rule
- With no args, use the current channel as the implicit target and query its mode.
- If args start with `+` or `-`, treat them as channel modes for the current channel and prepend the current channel automatically.
- Otherwise, pass args through unchanged as an explicit IRC MODE target, so `/mode #chan +m`, `/mode nick +i` and `/mode #chan` all work without extra parsing.
- Validate any implicit target before touching UART; never emit a partial `MODE ` prefix and then bail out on `REQUIRE_CHAN()`.
- If a MODE query uses numeric replies like `324`, surface a visible line to the user; updating only local status-bar state makes the feature feel broken even when the wire protocol is correct.
- Do not build a full mode parser unless the client later needs structured validation or local state changes from the outgoing command itself.

## Applied In
- `src/user_cmds.c` `cmd_mode()`
- `src/irc_handlers.c` `h_numeric_324()`
