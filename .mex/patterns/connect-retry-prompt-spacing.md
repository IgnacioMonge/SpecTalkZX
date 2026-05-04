# Connect Retry Prompt Spacing

`ui_err()` prints through `main_print()`, and `main_print()` already advances to
the next line. When a connection helper prints an error and returns to
`cmd_connect_retry()`, do not add an extra `main_newline()` before
`prompt_yn("Retry (y/n)?")`.

Keep the retry wrapper's newline before a new retry attempt. That separates the
accepted `y` response from the next `Connecting...` line without adding a blank
line between the original error and the prompt.

The retry prompt is only for real connection attempts, not missing prerequisites.
Require WiFi-ready state, a saved server, and a non-empty nick before showing it;
otherwise `/server host` with no prior `/nick` prints only `Set /nick first`.
