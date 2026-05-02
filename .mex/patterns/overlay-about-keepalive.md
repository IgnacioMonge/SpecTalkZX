# Overlay About Parser Pump

`OVERLAY_ABOUT` reuses `ring_buffer` for `SPCTLK2.OVL`, so the normal ring-buffer IRC path cannot run while the overlay is resident. The accepted experiment is not a second mini-parser: it is a direct UART-to-`rx_line` pump that calls the normal parser. Earth packets must not use `rx_line`; they are read into a private dead-code packet slot inside SPCTLK2 after ABOUT init has finished.

## Rule
- Do not write `ring_buffer` while `OVERLAY_ABOUT` is active. `SPCTLK2.OVL` lives there.
- During ABOUT, drain UART directly into `rx_line` with the same line contract as the normal RX path: complete LF-terminated line, NUL terminator, `_rx_last_len`, `_rx_overflow`, then `parse_irc_message(rx_line)`.
- `rx_line` still aliases the generic `_overlay_slot`, but ABOUT animation must not use that alias for frame packets. `SPCTLK2` exposes `_about_packet_slot` over init-only code (`about_render_ovl`, logo/text drawing, strings). After entry 0 returns this code is dead and may be overwritten by each generated Earth packet.
- The dead packet slot must stay large enough for `EARTH_PACKET_SIZE`. Current build emits 459B packets; live tick/close/seek/apply/draw code and frame/attr buffers must stay outside that range.
- `_globe_tick_ovl` must read packets from `_about_packet_slot`, not `_overlay_slot`, and must not gate on `_rx_pos`. The C scheduler can keep the simple 25Hz about gate because packets no longer clobber partial IRC lines.
- Keep the ABOUT pump bounded. Current divMMC shape uses a `224B` byte budget and short inter-byte gap bridging (`C=64`).
- On divMMC, the ABOUT pump hot path should call `uartRead` directly and use its `CF=1/A=byte` contract, rather than calling `_ay_uart_ready` and `_ay_uart_read` for every byte. Keep `uartRead` exported and preserve the pump's own budget/gap registers across that call.
- Cache the next `rx_line` write pointer in `DE` inside `_about_pump`; `uartRead` preserves `DE` but clobbers `HL`, so do not cache `rx_pos` in `HL` across the UART call. Store `rx_pos` back only when returning with a partial line.
- The pump must yield immediately after parsing or discarding one complete LF-terminated line. Do not continue polling into the next line after `_rx_pos_reset()`, or the pump can return with a new partial line and starve animation during NAMES bursts.
- Before loading ABOUT, process complete lines already present in `ring_buffer` with the normal parser while `overlay_mode=OVERLAY_ABOUT` suppresses rendering. This preserves pending `353/366` lines that would otherwise be overwritten by the overlay binary.
- Do not reintroduce a separate ABOUT `PING/353/366` mini-parser. It grows resident code and loses normal handler semantics such as JOIN/PART count updates.
- Parser work that is purely display-cosmetic may be skipped while `overlay_mode != 0`, because overlay rendering suppresses main output. Current accepted skips are the fused `utf8_to_ascii(pkt_txt)` display-sanitize pass, channel mention scans in `PRIVMSG`, JOIN/QUIT friend notifications, and the NAMES friend-notification scan inside `353`; do not skip user-count accumulation, `353/366` state, PING/PONG, JOIN/PART/QUIT count updates, PM/query state, status-bar state, or any required connection mutation.
- Friend lookups should first reject on a cheap case-folded first-character compare before calling `st_stricmp()`. This preserves semantics while avoiding most expensive string compares during large NAMES/JOIN bursts.
- Keep local client keepalive timeout/probe suppression during ABOUT until hardware proves the direct parser path handles long sessions cleanly. Server `PING`/`PONG` traffic is handled by the normal parser through the pump.
- If parser execution inside ABOUT can enter a path that reuses `ring_buffer`, close ABOUT first. `force_disconnect()` must call the ABOUT close entry and `overlay_exit_full()` before any `flush_all_rx_buffers()` or ESP command wait.
- Do not use fixed post-ABOUT `flush_frames` drains. They block input and produce visible backlog bursts.
- Manual `/names` pagination does not need a special ABOUT renderer. The goal is full parser state correctness and status-bar updates, not drawing paginated lists over the ABOUT screen.
- Do not keep debug profile counters in the release path. The temporary `ABOUT_PROFILE`/`ABP` instrumentation was removed after proving the remaining `/names` lag is real parser+Earth work, not a pump/scheduler pathology.

## Applied In
- `src/spectalk.c` main overlay branch, keepalive block, `force_disconnect()`
- `src/user_cmds.c` `sys_about` pending-line pre-drain
- `asm/spectalk_asm/60_protocol_storage.asm` `_about_pump`
- `overlay/overlay_entry2.asm` `_globe_tick_ovl`
- `overlay/earth_about_render.asm` `_about_packet_slot` dead-code region
