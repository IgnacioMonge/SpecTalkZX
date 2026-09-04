# About Earth assets

Generated graphics used by the animated `!about` screen. The binary and
assembly files form one matching set and are packed into `SPECTALK.DAT` during
the normal build. Do not edit generated data by hand.

## Files

- `earth_frame0.compact.bin`: first bitmap frame.
- `earth_attr0.compact4.bin`: first packed colour frame.
- `earth_frame_deltas.bin`: bitmap changes for later frames.
- `earth_attr_deltas.compact4.bin`: colour changes for later frames.
- `earth_logo.bin`: one-bit SpecTalkZX logo.
- `earth_overlay_spans.asm`: screen and colour ranges used by the renderer.
- `earth_logo.asm`: logo screen and colour addresses.
