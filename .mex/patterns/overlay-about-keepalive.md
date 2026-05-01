# Overlay About Keepalive

`OVERLAY_ABOUT` reuses `ring_buffer` for overlay code, so normal IRC parsing cannot run there. Keepalive still has to work while the overlay is open.

## Rule
- During `OVERLAY_ABOUT`, do not launch new client-originated keepalive/lag PINGs and do not expire an already pending local PING. Normal IRC parsing is intentionally paused while `ring_buffer` holds overlay code, so a self-timeout can be false-positive even though server PING/PONG traffic is still being scanned.
- The lightweight scanner must reply to server `PING`. Local `PONG` consumption is optional if ABOUT exit rebaselines local keepalive state.
- Do not make the lightweight scanner poll through UART inter-byte gaps unless it also preserves protocol state for the traffic it consumes. The gap-poll variant reached server PING faster, but it also drained and discarded full `353/366` NAMES bursts during `ABOUT`, so the normal parser never committed the channel user count.
- The scanner may poll through short UART inter-byte gaps only if it preserves protocol state for consumed traffic. Current divMMC shape uses `D=64` gap bridging but a fixed `224B` byte budget and runs before the ABOUT animation tick. Do not drain unbounded: hardware proved it makes `/names` count work but freezes the globe until `366` arrives.
- Do not store the lightweight scanner line in `rx_line` while an overlay also uses `overlay_slot`; `rx_line` aliases `overlay_slot`, so animation/file chunks can destroy partial `PING`/`PONG` lines. Use a non-aliased scratch buffer such as `temp_input` and cap writes to that buffer's size.
- Before loading the ABOUT overlay, process complete lines already present in `ring_buffer` with the normal parser while `overlay_mode=OVERLAY_ABOUT` suppresses rendering. This preserves pending `353/366` lines that would otherwise be overwritten by the overlay binary.
- Automatic JOIN/NAMES is part of the ABOUT keepalive contract. While `names_pending` is live, the scanner must advance automatic `353/366` state without rendering: count users from `353`, reset the NAMES timeout, commit `cur_chan_ptr->user_count` on `366`, and set `status_bar_dirty`.
- Keep the automatic NAMES scanner streaming and tiny. Current accepted shape counts whole-line tokens and uses `tokens - 5` for normal prefixed `353` lines (`server 353 nick = #chan :names`). It reuses existing RX globals for temporary state to avoid new BSS.
- Detect `366` as `36x`, not as the old broken `35x` branch. The `35x` path is only for `353`; otherwise ABOUT never commits the accumulated count while the overlay is open.
- Initialize the reused token state (`rx_last_len` and `rx_overflow`) when ABOUT starts; stale parser state can corrupt the first counted line.
- Do not try to support manual `/names` pagination inside ABOUT unless there is a broader parser redesign. Manual `/names` owns the main area and needs the normal renderer/summary path.
- On `ABOUT` exit, do not call `flush_all_rx_buffers()`: that throws away parser context and can break keepalive/backlog handling.
- Do not use a fixed post-ABOUT `flush_frames` drain. It blocks user input and still risks a visible burst if bytes keep arriving after the drain window.
- Do not preserve an arbitrary partial `rx_line` from the lightweight scanner across exit; if the line was really part of a long `353`/topic flood, restoring it leaks a chopped tail as `>< ...` once normal parsing resumes.
- `overlay_keepalive()` does not have to consume local PONG if ABOUT exit rebaselines local keepalive state (`server_silence_frames`, `keepalive_ping_sent`, `keepalive_timeout`, lag counter). Server PING replies are mandatory; local PONG consumption is optional under this contract.
- Accept that other non-keepalive IRC traffic can still be discarded during `ABOUT`; this pattern is for avoiding false disconnects and preserving automatic NAMES counts, not for full background IRC processing.

## Applied In
- `src/spectalk.c` main keepalive block
- `src/user_cmds.c` `sys_about` pending-line pre-drain
- `asm/spectalk_asm/60_protocol_storage.asm` `_overlay_keepalive`
