# IRC Long Session Count Resync

Busy IRC channels can drift user counts upward during long unattended sessions.
`JOIN` is attributable to a channel, but `QUIT` only names the user and does not
list every shared channel. Without resident member lists, multi-channel `QUIT`
cannot be decremented exactly.

Keep the exact path for one joined channel: decrement that channel on `QUIT`.
For multiple joined channels, do not add resident member tables.

If a one-channel session still drifts, do not blame the ambiguous multi-channel
`QUIT` rule first. In that case every server `QUIT` should be attributable to
the current channel, so investigate receive loss or skipped parser work:

- normal `process_irc_data()` drains only `DRAIN_NORMAL` bytes before parsing,
  and does not update `buffer_pressure`/`search_data_lost` like the manual
  pagination path does;
- `_uart_drain_to_buffer()` stops when `_rb_push()` reports full, but the normal
  chat path has no counter or visible flag for that loss;
- `h_part()` must not render active-channel PART lines with `show_traffic=0`;
  `!traffic off` should remove presence-message render pressure while keeping
  counters and status redraws correct.

Before changing count policy, prefer a temporary diagnostic build with small
counters for JOIN increments, PART/KICK/QUIT decrements, QUIT skipped because of
multi-channel state, ring full stops, and line-overflow discards.

The first accepted diagnostic build had only 45 resident bytes and 2 bytes in
SPCTLK4 left, so keep diagnostic UI brutally small. A new resident `!diag`
command was too expensive; decimal formatting in SPCTLK4 was also too large.
The working shape is: counters live in resident BSS, hot-path increments are
direct, and `!status` calls a tiny SPCTLK4-local ASM formatter that writes a
hex line into `overlay_slot`, e.g. `Qss/dd Rff DllLoo`.

Read `!status` only after noting the status-bar user count at the end of a test
window. The status overlay still resets RX buffers on exit, so it is not a
non-invasive live probe during the measurement.

In the first hardware diagnostic sample, `Q0F/0F R00 D40L00` with a large
448 -> 539 drift and `/names` returning 452 ruled out single-channel QUIT
attribution for that run. `D40` means the normal 32-byte UART drain limit was
hit 64 times while ring-full and line-overflow stayed at zero. The next clean
experiment is to change only `DRAIN_NORMAL` from 32 to 64 and repeat the same
single-channel measurement.

The second sample with `DRAIN_NORMAL=64` still drifted badly and showed
`QBC/BC R00 DDCL00`. This again ruled out parsed QUIT attribution and showed
the larger drain limit still hitting its ceiling 220 times. The next diagnostic
build should set `DRAIN_NORMAL=0`, which the ASM drain loop treats as 255
iterations, before designing a final adaptive strategy. If 255 still drifts,
the problem is likely that draining happens too late/too rarely relative to
render/parser work, not merely that the per-call byte limit is too low.

The third sample with max drain still drifted and showed ring-full drops
(`Q09/09 R42 D32L00`). In that case, push the fix toward scheduling: drain
cooperatively between parsed IRC lines, and make sure `!traffic off` really
suppresses presence rendering. `h_part()` must still decrement and dirty the
status bar, but should not call `print_departure()` while `show_traffic=0`.

After parser-cache plus scroll stack optimizations, hardware still drifted in a
busy single-channel room: after 10 minutes status count `549`, manual `/names`
`496`. The next non-diagnostic build uses a resident-only cooperative drain in
`process_irc_data()` after each handled IRC line. Keep it out of `_frame_wait()`
and out of `_try_read_line_nodrain()`: the parser stays no-drain by contract,
and overlays can execute from `ring_buffer`.

Follow-up hardware after that resident cooperative parse-line drain was reported
OK. Treat that build shape as the accepted baseline before further performance
work: no persistent telemetry, no `/names` drain, no generic `_frame_wait()`
drain, `DRAIN_NORMAL=32`, and `RX_TICK_PARSE_BYTE_BUDGET=512`.

The fourth sample after cooperative parse-line drain still drifted and made
manual `/names` incomplete: `QCD/CD R22 D2EL00`. That reinforced that the
problem is live RX pressure, not parsed QUIT attribution. In very busy rooms,
channel `MODE` storms and `notify()` animation/work are plausible contributors
even with `!traffic off`, because bans/unbans use `MODE #channel +b/-b` and the
current notification path is independent from the traffic flag. For the next
diagnostic, keep normal message output unchanged but add compact counters for
`notify()`, channel `MODE`, kick decrements, and parse-budget breaks. The
temporary SPCTLK4 status overlay may drop the channel list to fit the diagnostic
line; document that it is a diagnostic layout tradeoff.

Do not auto-run `NAMES` directly from `QUIT` in very large channels. Even a
quiet NAMES refresh can create a near-blocking burst and worsen the user
experience in busy rooms. Prefer explicit/manual refresh, an idle-only strategy,
or a cheap stale-count marker over automatic NAMES on traffic hot paths.

`h_numeric_353()` must treat the first `353` of any fresh batch as a reset point
for `names_count_acc`, even if the batch was not pre-armed by JOIN or manual
`/names`. Otherwise repeated server or automatic NAMES batches accumulate over
the prior total.

For rapid self-joins, a single global `names_target_channel` is not enough:
server `353/366` chunks may interleave between channels. The compact accepted
fix uses the spare `ChannelInfo.flags` bit `CH_FLAG_NAMING` only for automatic
JOIN/NAMES bursts: self-`JOIN` sets the bit and zeroes that channel's count;
the current/manual target keeps the old accumulator and commit-on-clean-`366`
path; only off-target `353` chunks for flagged channels add directly to
`channels[ci].user_count`; off-target `366` clears the bit for its own
`msg_chan`. This covers the real rapid-join interleave without rewriting the
manual `/names` path or paying for a full pending-channel scanner.

Do not "fix" manual zero counts by blindly removing the `names_count_acc > 0`
guard. `366` without a preceding visible `353` means "no count data reached the
client"; for manual refresh that should preserve the previous known count, not
overwrite it with zero. Automatic self-join is different because `h_join()` has
already reset the count and `CH_FLAG_NAMING` is the authoritative in-progress
state.

The full "keep `names_pending` alive while any channel has `CH_FLAG_NAMING` and
clear all flags on timeout" variant measured too expensive for this resident
budget. The compact variant relies on late `366`, `remove_channel()`, or full
channel reset to clear stale flags. This leaves a narrow edge case if a
non-target `366` is permanently lost, but avoids spending nearly the whole BSS
slack on a rare cleanup path.

Late or malformed numerics after registration must not update identity state:
`001` is a handshake event only. Once `connection_state >= STATE_IRC_READY`,
ignore additional `001` lines so fragmentary command tokens cannot clobber
`irc_nick`.

Always rebuild after touching this path. Resident/BSS margin is tight, and a
straight C guard can overflow into `_ring_buffer`.

After the diagnostic run is over, remove telemetry completely before starting
performance work. The accepted clean baseline has no `dbg_*` globals, no
diagnostic `!status` line, no max-drain override, no `_frame_wait()` drain, no
`/names` row-drain experiment, `DRAIN_NORMAL=32`, and
`RX_TICK_PARSE_BYTE_BUDGET=512`.

Do not drain UART from generic `_frame_wait()`: overlays can execute from
`ring_buffer`, and pushing RX bytes into that same storage while an overlay is
active can corrupt overlay code/data. If more drain points are needed, place
them only in proven-safe resident paths where the ring is owned by RX.
