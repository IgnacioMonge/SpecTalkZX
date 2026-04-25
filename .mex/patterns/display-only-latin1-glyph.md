# Display-Only Latin-1 Glyphs

SpecTalkZX keeps the text renderer in the internal `32..127` range. BPE tokens start at `128`, so display-only Latin-1 glyphs must not use bytes above `127`.

## Rule

- Use internal byte `127` for display-only `ñ`. It is accepted by the renderers but not typeable by `input_add_char()`, which keeps this feature receive-only.
- `utf8_to_ascii()` maps both UTF-8 `Ñ` (`C3 91`) and `ñ` (`C3 B1`) to byte `127`.
- `tools/bpe_build.py` patches the ignored/local `SPECTALK.DAT` font header on every build so glyph slot `127` is packed as `47 66 66`.
- Do not assign BPE tokens, command text, or input semantics to byte `127` while it is used as the Spanish `ñ` display glyph.

## Applied In

- `asm/spectalk_asm/70_input_lookup.asm` `u8a_tbl_c0`
- `tools/bpe_build.py` DAT font-header patch
