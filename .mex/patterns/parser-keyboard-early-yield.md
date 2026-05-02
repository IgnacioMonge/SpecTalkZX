# Parser Keyboard Early Yield

To improve input responsiveness during MOTD/NAMES/channel bursts, prefer a
minimal receive-loop yield before adding resident keyboard state.

Current low-risk shape: in `process_irc_data()`, after selecting the
backlog-based `max_lines`, call raw `in_inkey()` and cap that pass to 4 parsed
lines if a key is down. Do not call `read_key()` there; `read_key()` owns
debounce, repeat timing, and actual key consumption in the main loop.

This bounds parser burst latency without adding BSS. The stricter every-4-lines
check cost more bytes, and a naked ASM helper measured worse because the call
overhead outweighed the tiny test. If hardware still loses short key taps, then
consider a one-byte pending-key latch as a second step.
