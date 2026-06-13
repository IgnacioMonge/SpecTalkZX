# Frameless ASM Helper Reuse

When a resident frameless ASM wrapper already has a helper that performs the same byte-copy/build step, prefer calling the helper instead of re-inlining the loop, as long as the wrapper's own saved return/context can stay deeper on the stack than the helper's temporary `[ret][arg...]` frame.

- Safe shape: keep the wrapper's real return address and any saved local context below the helper arguments, then push only the helper's parameters above that saved state before `call helper`.
- After the helper returns, the deeper wrapper state is still intact because the helper only pops its own return address and arguments.
- This paid off in `asm/spectalk_asm/80_ui_runtime.asm`: `sys_puts_print()` shrank by calling `_sys_puts()` directly instead of restating `set_attr_sys()+main_puts()`. The earlier resident `_cfg_kv()`/`_cfg_put()` pair used the same shape, but those helpers later moved into `SPCTLK4` because only the save overlay still needed them.
- Related micro-rule: if the same 8-bit value is both a zero-extended offset and a loop/count register, use `ld b,0 / add hl,bc` instead of cloning it into `DE`.
