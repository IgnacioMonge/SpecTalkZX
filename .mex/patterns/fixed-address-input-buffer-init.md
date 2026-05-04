# Fixed-address input buffer init

`_line_buffer` is not normal C BSS. It is mapped by ASM to `0x5CB6`, reusing
the Spectrum CHANS workspace:

- `asm/spectalk_asm/80_ui_runtime.asm`: `defc _line_buffer = 0x5CB6`
- `src/spectalk.c`: input rendering reads it through `redraw_input_full()`

Do not assume it starts as zeroed RAM. On cold startup it can contain ROM/BASIC
channel data, and `_print_line64_fast()` will render bytes until it finds a NUL.

Before the first input redraw, call `input_clear()` or explicitly set:

- `line_len = 0`
- `cursor_pos = 0`
- `line_buffer[0] = 0`

The accepted startup fix is in `main()`: after `draw_status_bar()`, call
`input_clear()` instead of `redraw_input_full()`. This preserves the same visual
prompt path but guarantees the backing string is terminated before the renderer
reads it.

When touching startup UI, test the disconnected WiFi-ready screen. Garbage below
the status bar usually means the fixed input workspace was rendered before being
terminated, not an IRC/parser bug.
