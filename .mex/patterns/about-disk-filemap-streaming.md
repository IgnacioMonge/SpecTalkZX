# ABOUT DISK_FILEMAP Streaming

Use the NextZXOS low-level streaming API only as an optional future acceleration path for `!about`. Keep it out of the current divMMC release build.

- Call `DISK_FILEMAP ($85)` immediately after opening `SPECTALK.DAT`; the API documentation says filemap must be obtained before other file access on that handle.
- Wrap the `DISK_FILEMAP` probe with an `ERR_SP` trap, same family as `_esx_detect()`. Unsupported esxDOS/divMMC implementations can route unknown `RST 8` calls into ROM/BASIC error handling; without the trap, `!about` can reset or leave the app.
- Keep the normal esxDOS `F_SEEK/F_READ` path as the fallback. Reject turbo if `DISK_FILEMAP` fails, returns no usable extent, the first extent does not cover the complete Earth stream, or the returned flags indicate byte-addressed card addressing instead of block addressing.
- For hardware triage, show an explicit mode marker in the about footer. `F` means fallback `F_READ`; `T` means the `DISK_FILEMAP` probe succeeded and the tick path will attempt `DISK_STRMSTART`. Without this marker, animation smoothness is not evidence that turbo is active.
- Generate the Earth delta stream as 512-byte packets and align `EARTH_DELTA_OFFSET` to a 512-byte boundary. This lets both paths consume identical packets: fallback reads 512 bytes through `F_READ`; turbo streams one sector directly.
- The current prototype intentionally starts and ends a one-block stream per tick: `DISK_STRMSTART ($86)`, two `INIR`s from port `$EB`, consume the two SD CRC bytes, then `DISK_STRMEND ($87)`. This is smaller and keeps filesystem calls legal between ticks. A persistent multi-block stream is faster in theory but did not fit in SPCTLK2 without larger shrink.
- Do not issue normal esxDOS filesystem calls while a low-level stream is open. If a future persistent stream is added, `about_close_ovl` must end the stream before `F_CLOSE`.
- Treat the streaming path as Next-only for now: it assumes the documented Next return port `$EB`. Non-Next/divMMC targets must fall back cleanly.
- This path reduces SD/FAT overhead for the animation tick. It does not by itself make the normal IRC parser run during ABOUT, because `ring_buffer` still contains overlay code.
- Current divMMC baseline deliberately reverted to generated-size `481B` packets and no footer `F/T` marker. The saved experimental state is preserved separately; do not reintroduce its 512B padding, sector alignment, or `RST 8` `$85/$86/$87` calls into the divMMC branch without a new Next-specific branch.
