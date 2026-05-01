# Overlay Cold Status Text Guard

Do not shorten established user-facing overlay text just to recover bytes.

## Rule
- Preserve visible labels and status words in existing screens unless the user explicitly approves the wording change.
- Shrink pressure must first come from code structure, dead data, shared constants, or overlay relocation.
- Temporary debug-only wording may be shortened or removed when it is not part of the normal UI.
- Measure individual `SPCTLK*.OVL` sizes after any restoration or approved wording change.

## Applied
- The previous `!status` recuts (`Server:` -> `Srv:`, `Network:` -> `Net:`, `IRC ready` -> `IRC`, etc.) were reverted after user report. Treat that change as rejected.
- `overlay/spectalk_ovl4.c` may keep non-semantic save output like `Saving config... OK`, but not shortened screen labels in `!status`.
