# divMMC SD I/O Optimization

Use this when proposing SD read/write speed work for the divMMC/esxDOS build.

## Boundary

- `DISK_FILEMAP ($85)`, `DISK_STRMSTART ($86)`, `DISK_STRMEND ($87)`, and the low-level stream helper path are NextZXOS-only in this repo.
- Classic esxDOS still has useful raw media calls: `DISK_INFO`, one-sector `DISK_READ`, one-sector `DISK_WRITE`, plus normal file calls (`F_OPEN`, `F_READ`, `F_WRITE`, `F_SEEK`, `F_FSTAT`).
- Do not describe esxDOS `DISK_READ` as equivalent to NextZXOS filemap streaming. It is a different primitive: raw sector I/O, not a file extent stream with a prepared file map.

## Safe First Tier

- Keep one data file open while an overlay consumes it.
- Prefer sequential/monotonic reads; avoid open/close and many small backward seeks.
- Use `F_SEEK` for large jumps, never dummy-read skips into fixed 512B scratch.
- If multiple overlays need absolute `F_SEEK` with a 16-bit offset, use resident `_esx_fseek_set()` instead of carrying duplicate overlay-local naked helpers. This grows resident code but can recover enough SPCTLK1/SPCTLK2 headroom when funded by resident shrink.
- Overlay bundle loads can seek to the selected 2048B block before reading it: `F_SEEK(ovl_id*2048)` plus one `F_READ(2048)` is HW OK when TAP + `SPECTALK.OVL` + `SPECTALK.DAT` are copied as a matching set. A prior apparent regression was artifact/setup-related, not accepted evidence against the seek path. After the read, require exactly 2048 bytes before validating the entry table or jumping; shorter reads can leave stale `ring_buffer` bytes executable.
- Do not assume a paginated overlay can redraw pages with `overlay_call()` just because it was loaded once; in `!help`, the first attempt showed page 1 but keypress did not advance in hardware, so keep page redraws on the proven `overlay_exec()` path unless the resident-overlay lifetime is re-proven.
- For segmented DAT payloads, compute `base + segment*segment_size` and seek directly; do not seek to base and read-discard earlier segments.
- Read in 512B or larger chunks when RAM allows; design packet formats so packets do not cross sector boundaries.
- For media-like streams that may later move to raw sector reads, align the stream start to 512B and pad each packet to exactly 512B. Accept this only when the extra DAT bytes are acceptable and the live packet buffer has a compile-time size guard.
- If DAT layout constants are generated, patch every consumer in the same build step. For ABOUT Earth this means `overlay_api.h`, `overlay_entry2.asm`, and `earth_about_render.asm`; otherwise init and tick paths can seek to different offsets and desynchronize the stream.
- Put hot overlay/media data in one DAT in access order, so normal FAT/file reads have fewer discontinuities.
- For writes, coalesce into fixed-size buffers and call `F_SYNC` sparingly. Do not write raw sectors for normal config/log data.

## Experimental Tier

A divMMC "turbo DAT" can be investigated only as a probe-gated acceleration path:

1. Open `SPECTALK.DAT` normally.
2. Use `F_FSTAT` to capture drive/device/size metadata.
3. Build or load a sector map for the hot DAT span.
4. Validate file identity before trusting the map: size, timestamp if available, internal DAT checksum/version, expected packet header, and device/path.
5. Use `DISK_READ` only for whole validated 512B sectors.
6. Fall back to normal `F_SEEK`/`F_READ` on any mismatch, fragmentation, unsupported device, or read error.

Raw `DISK_WRITE` is not acceptable for shipped data/config unless the target is a deliberately reserved raw area or a rigorously validated fixed file extent. Wrong LBA writes can corrupt FAT or unrelated files.

## Why Demos Feel Instant

Fast loaders/demos usually win by file/data design, not by magic file calls:

- contiguous prepared files or SD images;
- sequential access with little or no filesystem work in the hot path;
- 512B sector framing;
- compression so fewer bytes cross the SD path;
- buffering/double-buffering and work hidden behind music/effects;
- on 128K/extended hardware, preloading into banks before the visible scene.

For SpecTalkZX, translate that into small, measured changes: packet alignment, one-handle streaming, read-ahead into dead overlay slots, and optional validated raw-sector reads. Keep IRC parser/ring-buffer contracts visible when borrowing scratch RAM.
