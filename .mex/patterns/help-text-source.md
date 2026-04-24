# Help Text Source

If command help is rendered from `SPECTALK.DAT`, keep the editable help text in a tracked plain-text source and inject it during the BPE/DAT build step.

## Rule
- Do not rely on editing `src/SPECTALK.DAT` directly when that file is local or ignored.
- Store the canonical help text in a tracked file such as `src/SPECTALK_HELP.txt`.
- Make `tools/bpe_build.py` read that tracked help source when generating the final `SPECTALK.DAT`.
- When adding a user-facing command, update both the command implementation and the tracked help source in the same change.
- If help is paged from a computed line count, wrap `help_page >= total_pages` before calculating the page's starting line. Exact multiples of `LINES_PER_PAGE` otherwise render an empty page after the last real page.

## Applied In
- `src/SPECTALK_HELP.txt`
- `tools/bpe_build.py`
