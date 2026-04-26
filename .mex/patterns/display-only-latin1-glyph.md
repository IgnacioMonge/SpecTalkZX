# Display-Only Latin-1 Glyphs

SpecTalkZX keeps the text renderer in the internal `32..127` range. BPE tokens start at `128`, so display-only Latin-1 glyphs must not use bytes above `127`.

## Rule

- Use internal byte `127` for display-only `ñ`. It is accepted by the renderers but not typeable by `input_add_char()`, which keeps this feature receive-only.
- `utf8_to_ascii()` maps both UTF-8 `Ñ` (`C3 91`) and `ñ` (`C3 B1`) to byte `127`.
- Other Latin-1 accented letters are normalization-only, not custom glyphs: accented vowels fold to their base ASCII vowel, `ü/Ü` fold to `u/U`, and `ç/Ç` fold to `c/C`.
- C2 punctuation is also normalized for the 64-column font (`¡` -> `!`, `¿` -> `?`, `«/»` -> `</>`), while E2 smart quotes fold to ASCII quotes.
- The `u8a_tbl_c0` lookup must be page-safe. Nearby resident shrinks can move the table; do not rely on `table_low + 0x3F` staying below `$100`. Propagate carry into `H` after the low-byte add unless the table is explicitly aligned and documented.
- `tools/bpe_build.py` patches the ignored/local `SPECTALK.DAT` font header on every build so glyph slot `127` is packed as `47 66 66`.
- Do not assign BPE tokens, command text, or input semantics to byte `127` while it is used as the Spanish `ñ` display glyph.

## Applied In

- `asm/spectalk_asm/70_input_lookup.asm` `u8a_tbl_c0`
- `tools/bpe_build.py` DAT font-header patch
