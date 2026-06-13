# Keepalive Activity Timeout Guard

When a local keepalive or lag PING is pending, do not force-disconnect solely
because the matching `PONG` has not been parsed yet if other server traffic is
still arriving.

`PONG` remains the only event that clears `keepalive_ping_sent` and provides a
latency measurement. Other server lines reset `server_silence_frames`, proving
the TCP/IRC stream is still alive. The disconnect condition should therefore
require both:

- `keepalive_timeout >= KEEPALIVE_TIMEOUT_FRAMES`
- `server_silence_frames >= KEEPALIVE_TIMEOUT_FRAMES`

This avoids false `Connection timeout (no response)` while visible channel
traffic is still being received, without reverting to the older looser behavior
where any traffic cleared the pending lag probe.
