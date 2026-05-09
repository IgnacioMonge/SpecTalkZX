# ASM Render Contract Guards

Resident render/output ASM is allowed to be small, but it must preserve the behavior promised by the C-facing API and by UI sentinel state.

## Rule
- After any output helper calls `_main_newline()`, reload `_main_col` and stop if it is still `64`; pagination cancel deliberately leaves `main_col=64` to protect the "Cancelled" line from later output.
- If deferred output is already active and enters its next step with `_main_col == 64`, treat that as the same cancellation sentinel: clear deferred-wrap state and return before deriving a `_print_line64_fast()` start byte.
- Ikkle/footer renderers receive logical 64-column positions. Bounds checks must stop at logical column `64` before shifting to physical byte coordinates; checking `32` there breaks right-half slide-in animation.
- If a C-facing renderer takes an `attr` parameter, either write the relevant attribute RAM in ASM or document/enforce that callers pre-fill attributes before drawing pixels.
- Do not treat a stored global such as `_g_ps64_attr` as proof that the visual contract is honored; verify a later path actually consumes it.
- When fixing these paths, check both direct character emitters (`_main_putc`) and bulk emitters (`_main_puts`, `print_line64_fast`, big-font renderers), because cancellation and color contracts often diverge between fast and slow paths.

## Applied In
- `asm/spectalk_asm/30_rendering.asm`
- `asm/spectalk_asm/50_main_output.asm`
- `asm/spectalk_asm/80_ui_runtime.asm`
