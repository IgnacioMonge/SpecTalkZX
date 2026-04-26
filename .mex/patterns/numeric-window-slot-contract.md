# Numeric Window Slot Contract

## Context
The direct `/0`..`/9` command path and the live switcher both address physical channel slots, not visible ordinal positions.

## Rule
- Keep numeric window labels and numeric jumps in the same coordinate system.
- The switcher renders `buf[pos + 1] = '0' + sw_map[i]`, so the displayed number is the physical slot id.
- Direct switcher key handling must continue to find the typed slot inside `sw_map` before switching.
- `parse_user_input()` may use `channels[idx]` for `/0`..`/9` because that command also means "slot idx", not "nth visible tab".
- `remove_channel()` defragments `channels[]`; do not assume long-lived holes after closing a channel.

## Shrink Note
For single-byte numeric command checks where the input byte is already `uint8_t`, prefer:

```c
if ((uint8_t)(c0 - '0') <= 9 && cmd_str[1] == 0) {
```

over two-sided comparisons. Under the current SDCC/z88dk build this recovered resident bytes in `parse_user_input()`.

## Applied In
- `src/user_cmds.c` `parse_user_input()`
- `src/spectalk.c` switcher rendering and key dispatch
