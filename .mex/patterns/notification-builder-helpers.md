# Notification Builder Helpers

When several call sites build short ikkle notifications with the same `nb_init/nb/.../NB_END/notify` shape, factor the common arity into a tiny helper and reuse existing helpers before open-coding another temp buffer copy.

## Rule
- Use `notify2()` when the second argument may point into mutable input and needs the defensive copy into `temp_input + 64`.
- Use a smaller direct builder such as `notify3()` only for stable pointers that do not need that extra copy.
- If a call site is just `st_copy_n(temp_input + 64, x, 56)` plus `nb_init(prefix); nb(temp_input + 64); ...`, prefer `notify2(prefix, x, attr)` instead of rebuilding the same pattern.
- For a one-off four/five-piece message such as channel MODE text, reuse inline `nb_init()/nb()/NB_END()/notify()` instead of adding a new `notify4/notify5()` helper unless another measured callsite appears.
- Re-measure with `make`; these helpers are only worth keeping if the build proves a net saving.

## Applied In
- `src/irc_handlers.c` `notify2()`
- `src/irc_handlers.c` `notify3()`
- `src/user_cmds.c` `cmd_join()`
