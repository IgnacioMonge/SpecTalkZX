# Traffic And Notification Routing

`!traffic` controls only visible user-presence movement in a channel:
`JOIN`, `PART`, and `QUIT`. It must not decide whether administrative channel
events are shown.

## Rule

- Gate only active-channel `JOIN`/`PART`/`QUIT` render with `show_traffic`.
  Counter changes, status redraws, friend notifications, and query cleanup still
  happen even when traffic display is off.
- Route channel administrative events through `notify()`: `KICK`, channel `MODE`,
  ban/unban (`MODE +b/-b`), and numeric channel mode replies such as `324`.
- `notify()` is the single user preference for those admin events:
  `!notif 1` uses the footer notification, while `!notif 0` renders classic
  inline output in the main area.
- Classic inline `notify()` output must call `main_print_time_prefix()` so it
  gets the normal timestamp/wrap indent contract instead of raw column-0 text.
- Do not add persistent telemetry or RX scheduling changes to solve routing
  mistakes. This is presentation policy only.

## Applied In

- `src/irc_handlers.c` `h_part()`
- `src/irc_handlers.c` `h_kick()`
- `src/irc_handlers.c` `h_mode()`
- `src/irc_handlers.c` `h_numeric_324()`
- `src/spectalk.c` `notify()`
