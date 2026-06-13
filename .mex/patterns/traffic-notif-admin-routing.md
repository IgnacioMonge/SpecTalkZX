# Traffic And Notification Routing

`!traffic` controls visible user-presence movement (`JOIN`, `PART`, and
`QUIT`). Administrative channel events are visible only for the current channel.

## Rule

- Gate active-channel `JOIN`/`PART`/`QUIT` render with `show_traffic`.
  Counter changes, status redraws, friend notifications, and query cleanup still
  happen even when traffic display is off.
- `QUIT` is channel-independent for state: still close matching query windows
  and keep the accepted single-channel user-count decrement even when hidden.
- Route current-channel administrative events through `notify()`: `KICK`,
  channel `MODE`, ban/unban (`MODE +b/-b`), and numeric channel mode replies
  such as `324`.
- Hide the same administrative events for non-current channels after applying
  their state change. Do not mark unread/activity for suppressed `PART`,
  non-self `KICK`, `MODE`, or `324`.
- `notify()` is the single user preference for visible admin events:
  `!notif 1` uses the footer notification, while `!notif 0` renders classic
  inline output in the main area.
- Classic inline `notify()` output must call `main_print_time_prefix()` so it
  gets the normal timestamp/wrap indent contract instead of raw column-0 text.
- The shared `nb()` notification builder is footer-capped when `!notif 1`, but
  must use the full `temp_input` budget when `!notif 0`; otherwise classic
  inline rendering receives already-truncated admin text such as long KICK
  reasons.
- Classic inline `notify()` may use word-wrap only when `msg == temp_input`.
  `main_print_wrapped_clean()` temporarily writes NULs while wrapping, so do not
  feed it string literals or other const storage.
- Do not add persistent telemetry or RX scheduling changes to solve routing
  mistakes. This is presentation policy only.

## Applied In

- `src/irc_handlers.c` `h_part()`
- `src/irc_handlers.c` `h_kick()`
- `src/irc_handlers.c` `h_mode()`
- `src/irc_handlers.c` `h_numeric_324()`
- `src/spectalk.c` `notify()`
