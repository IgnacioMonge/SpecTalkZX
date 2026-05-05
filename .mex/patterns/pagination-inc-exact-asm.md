# Pagination Inc Exact ASM

`pagination_inc()` may be kept in naked ASM only while it preserves the C contract exactly:

- if `pagination_count < PAGINATION_MAX_COUNT`, increment it and return `0`
- if `pagination_count >= PAGINATION_MAX_COUNT`, call `cancel_search_state()`, print the exact same `Result limit` user-facing text via `ui_err()`, and return `1`
- do not change `PAGINATION_MAX_COUNT`, the message text, or the overflow side effects

The 2026-05-05 rewrite measured only `-1B` in the Gemini shrink worktree (`35693B -> 35692B`, BSS guard `0xF308 -> 0xF307`). Treat it as optional: revert freely if future edits make this local ASM inconvenient.
