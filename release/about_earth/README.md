# About Earth Assets

Generated from the GloboTerraqueoZX Earth generator for the SpecTalkZX `!about`
overlay.

Current asset pass:
- `FRAME_COUNT = 24`
- `TEMPORAL_HYSTERESIS = 0.025`
- Playback direction: west-to-east Earth rotation, generated as frame phases
  `0, -15, -30...` degrees.
- Globe layout: `ZX_X_BYTE = 10`, `ZX_Y_LINE = 24`
- Logo layout: `LOGO_X_BYTE = 5`, `LOGO_Y_LINE = 104`
- Logo size: `176x24` pixels
- Post-process: `tools/about_earth_polish.py` fills single black pinholes
  fully surrounded by lit pixels; attributes/colors are unchanged.
- Current generated interleaved delta packet size: `481` bytes

Shipped files:
- `earth_frame0.compact.bin`: first compact bitmap frame
- `earth_attr0.compact4.bin`: first packed attribute frame
- `earth_frame_deltas.bin`: compact bitmap SKIP/COPY deltas
- `earth_attr_deltas.compact4.bin`: packed attribute SKIP/COPY deltas
- `earth_logo.bin`: 1-bit logo bitmap
- `earth_overlay_spans.asm`: screen/attribute span tables for the overlay renderer
- `earth_logo.asm`: logo screen/attribute addresses
