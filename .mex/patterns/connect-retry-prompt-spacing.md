# Connect Retry Prompt Spacing

`ui_err()` prints through `main_print()`, and `main_print()` already advances to
the next line. When a connection helper prints an error and returns to
`cmd_connect_retry()`, do not add an extra `main_newline()` before
`prompt_yn("Retry (y/n)?")`.

`prompt_yn()` prints the prompt with `main_puts()`, echoes a valid `y` or `n`
on the same line after one blank (`Retry (y/n)? y`), then emits the newline
itself. Do not keep a second newline in the retry wrapper; otherwise an
accepted `y` creates a blank row before the next `Connecting...` attempt.

The retry prompt is only for real connection attempts, not missing prerequisites.
Require WiFi-ready state, a saved server, and a non-empty nick before showing it;
otherwise `/server host` with no prior `/nick` prints only `Set /nick first`.
