# SpecTalkZX Router

## Project State
- Date: 2026-04-21
- Recent task: removed the obsolete `HANDOFF.md` handoff note from repo usage and kept `.mex/` as the current project-resume source of truth.
- Build status: `make` completes successfully in this workspace and produces `build/SpecTalkZX.tap`; the earlier `_SB_COLON_SP` failure was not reproduced in the normal full pipeline.

## Patterns
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and the footer notification path must expand BPE tokens before rendering.
