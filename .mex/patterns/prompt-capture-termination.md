# Prompt Capture Termination

## Context
`wait_for_prompt_char()` waits for a single serial prompt byte while accumulating preceding UART bytes into `rx_line`.

## Rule
- If a helper documents that captured serial data is inspectable, terminate the capture before every return path that may leave useful data in the buffer.
- Timeout and BREAK exits are not enough; the success path that sees the prompt must also write `rx_line[wp] = '\0'` before returning.
- Keep the prompt byte itself out of the captured string unless a caller explicitly needs it.
- Do not remove `disconnecting_in_progress` as a local cleanup: it is a cross-module teardown guard used by IRC parsing paths while AT disconnect commands are in flight.

## Applied In
- `src/spectalk.c` `wait_for_prompt_char()`
