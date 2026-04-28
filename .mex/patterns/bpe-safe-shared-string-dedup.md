# BPE-Safe Shared String Dedup

## Rule

- Pool repeated screen-only C literals as shared `S_` constants only when every use goes through a BPE-aware screen path or notification builder that ultimately uses screen text.
- Add new screen-only pooled constants to `tools\bpe_build.py` `SAFE_CONSTANTS`, so the build-time BPE pass renames `S_` to `SB_` and compresses the single shared payload.
- Do not mark UART, IRC protocol, file path, config key, compare-token, or direct-render strings as BPE-safe. Keep those as plain `S_` or `K_` constants.
- Measure with `make NO_COLOR=1`; BPE dictionary changes can make obvious-looking dedup smaller or larger.

## Current Example

- Shared screen strings: `S_IN_SP`, `S_QUIT_SUFFIX`, `S_SP_LBRACKET`, `S_CHANNEL_WORD`, `S_CLOSED_SP`, `S_USAGE_NOTICE`.
- Shared UART strings: `S_AT_CMD`, `S_JOIN_CMD`; these are deliberately not in `SAFE_CONSTANTS`.

## Applies To

- `src\spectalk.c`
- `include\spectalk.h`
- `src\irc_handlers.c`
- `src\user_cmds.c`
- `tools\bpe_build.py`
