# Parser Keyboard Early Yield

To improve input responsiveness during MOTD/NAMES/channel bursts, prefer a
minimal receive-loop yield before adding resident keyboard state.

Current low-risk shape: in `process_irc_data()`, after selecting the
backlog-based `max_lines`, call raw `in_inkey()` and cap that pass to 4 parsed
lines if a key is down. Do not call `read_key()` there; `read_key()` owns
debounce, repeat timing, and actual key consumption in the main loop.

Rejected shrink-lab shape: replacing the threshold ladder with
`6 + ((uint8_t)(backlog >> 8) << 3)` saved bytes, but it is not
behavior-identical. With a 2048B ring, a nearly full backlog can set
`max_lines` around 62 instead of the old cap of 32. User rejected this
tradeoff; keep the conservative threshold ladder unless explicitly reopened.

This bounds parser burst latency without adding BSS. The stricter every-4-lines
check cost more bytes, and a naked ASM helper measured worse because the call
overhead outweighed the tiny test. If hardware still loses short key taps, then
consider a one-byte pending-key latch as a second step.
