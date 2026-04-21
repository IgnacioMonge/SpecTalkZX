# SpecTalkZX Router

## Project State
- Date: 2026-04-21
- Recent task: synced `CHANGELOG.md` with the ikkle footer `PART`/BPE fix and recorded that a normal `make` succeeds in this workspace.
- Build status: `make` completes successfully in this workspace and produces `build/SpecTalkZX.tap`; the earlier `_SB_COLON_SP` failure was not reproduced in the normal full pipeline.

## Patterns
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and the footer notification path must expand BPE tokens before rendering.
