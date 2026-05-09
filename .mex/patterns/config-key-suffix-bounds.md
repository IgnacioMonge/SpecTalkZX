# Config Key Suffix Bounds

## Rule

- Config keys are NUL-terminated in place at `=`/`:` while the value starts immediately after that NUL. Short malformed keys such as `ni=foo` therefore have value bytes at `key[3+]`, so never probe `key[4]` unless the key has at least four real key bytes.
- For compact suffix dispatch on known key families, use a helper that returns `0` unless `key[2]` and `key[3]` are non-NUL, then compare the returned suffix byte. This keeps malformed short keys from selecting fields based on value bytes.
- Do not zero or patch `key[4]` to make short keys safe; for short keys it can be part of the value buffer and would corrupt the value being applied.

## Applied In

- `src/spectalk.c` `cfg_apply()`
