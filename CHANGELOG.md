# SpecTalkZX Changelog

## [v1.3.9] - Juno

This release is the main development line after `v1.3.8: Hermes`. The headline
is native support for the **SpectraNext cartridge** (the Spectranet/XFS cart,
not the ZX Spectrum Next), delivered as a second compile-time build. The
Classic divMMC + ESP-AT product is unchanged: same screens, same IRC commands,
same three runtime files. Everything below is new or fixed since the published
1.3.8 tag.

The in-app What's New screen (`!changelog`) is headed `1.3.9. Juno` and lists:

- Native SpectraNext support
- Guided SpectraNext installer
- More reliable UART transfers
- Safe recovery from UDP failures
- Reliable configuration loading
- Safer friend and ignore lists
- File access preserves the interface
- More robust About/Earth screen
- Interrupt-safe chat scrolling
- Clearer registration errors
- Polished clock and status UI
- And much, much more!

The rest of this section is that list expanded against the actual 1.3.8 → 1.3.9
work.

Final release-profile verification recorded on 2026-08-28:

- Command: `make release NO_COLOR=1`
- Classic TAP: **35,386 bytes**.
- Classic BSS guard: **0xEFDD < 0xF500**, **1,315 bytes free** before
  `ring_buffer`.
- Classic overlays: **1700 / 1902 / 1739 / 1745 / 1934 / 1798 / 1816 /
  1893 bytes**.
- Classic packed `SPECTALK.OVL`: **14,591 bytes**.
- SpectraNext TAP: **36,474 bytes**.
- SpectraNext BSS guard: **0xF3F5 < 0xF500**, **267 bytes free** before
  `ring_buffer`.
- SpectraNext overlays: **1700 / 1902 / 1787 / 1828 / 1490 / 1920 / 1816 /
  1885 bytes**.
- SpectraNext packed `SPECTALK.OVL`: **14,392 bytes**.
- Shared `SPECTALK.DAT`: **16,469 bytes**.
- Installer: `SPECTALK.INS` **3,247 bytes**, `SPECTALK.PKG` **43,670 bytes**,
  loading screen **6,912 bytes**.
- Published 1.3.8 Classic TAP: **36,181 bytes**.
- Published 1.3.8 `SPECTALK.DAT`: **15,704 bytes**.
- Net Classic TAP delta: **-795 bytes**, despite the SpectraNext platform
  seams, UART/UDP recovery, configuration and About hardening, and the new
  What's New artwork stored in DAT.

### User-visible features

#### Native SpectraNext support

- Added a dedicated `PLATFORM=spectranext` build for ZX Spectrum models that
  the SpectraNext cartridge supports.
- IRC uses the cartridge ROM sockets and DNS directly. This binary does not
  link UART, ESP-AT, or a runtime backend selector.
- Configuration lives in local XFS as `/CFG/SPECTALK.CFG` and survives
  power-off. Classic still uses `/SYS/CONFIG/SPECTALK.CFG` with `/SYS/` as
  fallback.
- All five bookmark slots persist on the cartridge as `/CFG/SPTBM1.CFG`
  through `/CFG/SPTBM5.CFG`. Classic remains `/SYS/CONFIG/SPTBM1.CFG` through
  `SPTBM5.CFG`.
- The displayed clock is UDP/SNTP (`pool.ntp.org`) with the configured numeric
  timezone, taken **before** the IRC TCP socket opens.
- The cartridge exposes no RTC syscall to Z80 software. `!tz rtc` therefore
  falls back to the last numeric timezone; time still comes from SNTP.
- SpecTalkZX owns one cartridge socket at a time, so clock sync only runs
  while IRC is closed. A retry also waits if another overlay currently owns
  the shared buffer.
- Startup detection reports `REQUIRES SPECTRANEXT!` instead of
  `REQUIRES DIVMMC!`.
- The title banner is target-specific: Classic remains “IRC Client for ZX
  Spectrum”; the cartridge build reports “IRC Client for Spectranext”.
- `!init` closes the active cartridge socket and reinitializes the native
  backend. It does not send ESP-AT text into the IRC stream.
- IRC is plaintext on the configured server port. Cartridge TLS is a fixed
  port-443 service, so there is no IRC TLS on 6697.

#### Guided SpectraNext installer

- Public install is an HTTPS resource. Do not copy the Classic GitHub Release
  zip onto an SD card for this target.
- Canonical root: `https://ignaciomonge.github.io/SpecTalkZX/`.
- At the BASIC prompt:

```text
%umount 2
%mount 2, "https://ignaciomonge.github.io/SpecTalkZX/"
%fs 2
%cat
%load "boot.zx"
```

- `boot.zx` is tokenized BASIC. Do not `%tapein` it.
- The loading screen keeps the installer panel at the bottom of the display.
- The installer validates the package, writes `SPECTALK.tap`, `SPECTALK.OVL`,
  `SPECTALK.DAT` and `SPECTALK.ZX` to local XFS slot 0, then launches.
- `SPECTALK.ZX` is created on the cartridge during install; it is not a hosted
  file. Afterwards start that local copy from XFS.
- A later `%load "boot.zx"` from the same HTTPS root replaces the program
  files and leaves `/CFG/SPECTALK.CFG` and the five bookmark files alone.
- A GitHub Release zip is not a mountable SpectraNext resource.

#### More reliable UART transfers

- Classic UART byte send no longer spins forever if TX-ready never arrives.
  After about 16 ms (192 port polls) a stuck UART drops that byte and returns
  to the caller.
- A missed PONG still disconnects as usual; the timeout only covers a stuck
  BUSY line, not a healthy transfer.
- ESP-AT command writes use a bounded busy wait as well.
- Hardware check of the UART timeout: no behaviour change in normal operation.

#### Safe recovery from UDP failures

- The Classic raw-UDP path (NTP/AT over UART) uses a bounded transmission
  wait instead of an open-ended spin.
- If a datagram is only partly sent, SpecTalkZX stops and recovers. 1.3.8
  could keep writing and emit a truncated NTP or AT frame.
- On SpectraNext, SNTP rejects a short packet, a reply from the wrong host, a
  kiss-of-death / invalid-stratum reply, and a zero transmit timestamp. A zero
  timestamp would have locked the clock at midnight.

#### Reliable configuration loading

- Config keys must match in full against the 23 real names: `nick`,
  `nickpass`, `nickcolor`, `nickserv`, `server`, `port`, `pass`, `theme`,
  `autojoin`, `autoconnect`, `autoaway`, `friends`, `ignores`, `channels`,
  `countsync`, `beep`, `click`, `traffic`, `ts`, `tz`, `tzlast`, `divider`,
  `notif`.
- 1.3.8 accepted a truncated prefix. The nick-family keys all started with
  `ni`, so a short or mistyped key could apply the wrong setting. A truncated
  or unknown key is now ignored.

#### Safer friend and ignore lists

- The `!config` Friends row wraps at two nicks per line instead of running
  off the 64-column display.
- The Ignores row wraps at three names per line.
- Empty lists still show the established “not set” placeholder.

#### File access preserves the interface

- After every runtime esxDOS/XFS file call the keyboard input cache is
  invalidated, so the next keypress is not a leftover from the file
  operation.
- On SpectraNext, saving config or bookmarks no longer lets directory scratch
  wipe live UI state (theme, PM state, render cache).
- Overlay exit still discards reused parser/ring memory so loaded overlay
  bytes are not parsed as IRC.

#### More robust About/Earth screen

- Earth frame and attribute packets from `SPECTALK.DAT` are bounds-checked
  before they can be painted. A short or corrupt packet is rejected instead
  of being drawn.
- DAT reads during the animation run with interrupts off, matching the
  esxDOS `RST 8` contract.
- On SpectraNext, About used to issue up to 128 cartridge ROM calls per
  animation frame. The pump is now at most eight calls per frame. Connected
  About no longer stutters that way on hardware.
- PING/PONG, peer close and keepalive still run while About owns the screen.

#### Interrupt-safe chat scrolling

- Chat scroll no longer parks the CPU stack in display RAM to blit rows. An
  NMI during that fake stack could previously corrupt the copy.
- The visible chat area still scrolls the same rows; only the implementation
  changed.

#### Clearer registration errors

- A server `ERROR` during nick registration is recognised as the whole
  five-byte token, followed by end-of-line or a space.
- 1.3.8 looked at `E`, `R` and a later `R`, so other words could be treated
  as a failed register, and a real `ERROR` without the extra byte was missed.
  Failed register now shows “Server error”.

#### Polished clock and status UI

- Status-bar clock shifted half a character to column 55, matching
  NetChessZX.
- Successful status results keep the intended leading space. 1.3.8 could
  print `[OK]` or a clipped `[ OK` after a cache miss on that space.
- The three established themes are unchanged.

### Reliability and protocol fixes

- Bounded Classic UART TX-ready polling; drop the byte on timeout rather than
  hang the machine.
- Bounded ESP-AT and raw-UDP transmission waits; fail-stop on a partial UDP
  frame instead of sending a truncated datagram.
- SpectraNext SNTP validation as above; clock retry defers while another
  overlay owns the shared buffer.
- Exact configuration-key matching for all 23 keys.
- Input-cache invalidate after every runtime file I/O path that touches
  esxDOS/XFS.
- SpectraNext XFS scratch no longer clobbers live UI across config and
  bookmark saves.
- About Earth packet bounds, plus `DI` around every About esxDOS call.
- Registration `ERROR` token and boundary.
- Status-result leading space preserved across the fast print path.
- Persistent notification / NAMES state moved out of Printer RAM into
  compiler BSS, so it no longer sits in a firmware-sensitive window.

### Performance, size, and architecture

- Classic TAP **35,386 bytes** versus 1.3.8 **36,181 bytes** (−795).
- What's New streams a new 64×88 packed logo from `SPECTALK.DAT` (after the
  Earth block, 765 packed bytes) instead of consuming SPCTLK3 overlay code.
  Dense dither did not fit the overlay budget.
- The twelve-line What's New list always ends with the magenta
  “And much, much more!” line.
- Network and clock are compile-time platform files. Product IRC and UI stay
  shared; there is no runtime backend switch.
- Memory-layout check refuses to ship a build whose BSS reaches
  `ring_buffer`, whose overlays exceed 2K, or whose Printer/CHANS / XFS
  scratch bindings overlap.
- Classic leaves `$5B80..$5BBF` free. SpectraNext binds XFS state at
  `$5B80..$5B93` and persistent theme/PM/render-cache at `$5B94..$5BBE`.
- `!tz rtc` dispatch moved into the cold overlay to recover Classic TAP and
  BSS; the command itself is unchanged.
- `build/toolchain.version` records the z88dk used. The tree no longer pins
  or rejects a particular z88dk release.
- TAP, OVL and DAT from one build stay a set. Mixing 1.3.8 and 1.3.9 files
  will break help, About, bookmarks or What's New.

### SpectraNext limits

- IRC uses plaintext on the configured server port. There is no IRC TLS on
  6697.
- `tz=rtc` is not a cartridge RTC.
- One cartridge socket at a time: clock sync only while IRC is closed.
- Successful config and bookmark saves persist. The compact XFS writer is not
  an atomic rollback if power or I/O fails during an existing-file overwrite.

### Hardware status

Verified on a physical SpectraNext cartridge during release preparation:

- Guided HTTPS install reaches and launches SpecTalkZX 1.3.9.
- Updating preserves the existing `/CFG/SPECTALK.CFG`.
- Saving configuration survives restart and reloads the expected settings.
- The What's New artwork and twelve-line list render.
- Connected About animation is smoother after the bounded ROM pump.

Classic UART TX timeout: hardware-checked with no behaviour change in normal
operation.

These observations do not silently promote untested UART BUSY/turbo fault
injection, long soak, every theme, maximum friend/ignore lists, unexpected
NMI, or a power-cut in the middle of an XFS overwrite.

### Current release files

- Classic (GitHub Release archive `spectalk_divmmc_v1.3.9.zip`), required
  together:
  - `SpecTalkZX.tap`
  - `SPECTALK.OVL`
  - `SPECTALK.DAT`
- SpectraNext (GitHub Pages; not that zip):
  - `https://ignaciomonge.github.io/SpecTalkZX/`
  - hosted: `boot.zx`, `SPECTALK.INS`, `SPECTALK.PKG`, `SPECTALK.SCR`,
    `package.json`, `index.txt`
  - Pages-only extras, not listed in `index.txt`: `index.html`, `.nojekyll`
  - created on the cartridge during install: `SPECTALK.ZX`
- Generated release metadata:
  - `release/version.txt` -> `1.3.9. Juno`
  - `release/changes.txt` -> in-app What's New summary
  - `overlay/whatsnew_data.h` -> generated payload used by the overlay
- README screenshots under `images/` are cropped captures from this release.
  No 1.3.8 gallery image is reused.

---

## [v1.3.8] - Hermes - Release candidate

This release is the main development line after `v1.3.7: Artemis II`. It is a major maturity release: the user-visible feature set is much larger, the IRC session model is safer, the overlay/data architecture is more capable, and the low-level receive/render paths have had multiple audit and hardware-validation rounds.

Final local verification recorded on 2026-06-25:

- Command: `make NO_COLOR=1 PYTHON=C:/Progra~1/Python311/python.exe`
- Result: **BUILD OK / HW OK** for the promoted size batch.
- TAP: **36,181 bytes**.
- BSS guard: **0xF2B4 < 0xF500**, **588 bytes free** before `ring_buffer`.
- Overlays: **1702 / 1770 / 1938 / 1748 / 1901 / 1645 / 1816 / 1881 bytes**.
- Packed `SPECTALK.OVL`: **14,465 bytes**.
- `SPECTALK.DAT`: **15,704 bytes**.
- Baseline `v1.3.7` TAP: **36,072 bytes**.
- Net TAP delta: **+109 bytes**, despite the added bookmark manager, RTC path, expanded overlays, new IRC commands, rewritten help/data pipeline, animated About assets, and many reliability fixes.

### User-visible features

#### IRC bookmarks and session restore

- Added `!bm` / `!bookmarks`, a five-slot SD-backed IRC bookmark manager.
- Bookmark controls: `UP/DOWN` selects, `ENTER` connects, `S` stores, `A` marks automatic startup, `D` deletes, `BREAK` saves/exits.
- Bookmark files are stored as `/SYS/CONFIG/SPTBM1.CFG` through `/SYS/CONFIG/SPTBM5.CFG`.
- `!save` now saves the active IRC session, not only static settings.
- Saved sessions can include server, port, server password state, active joined channels, and startup policy.
- `!autoconnect` now means "connect to the saved server on startup".
- New `!autojoin` controls whether saved channels are joined after IRC registration.
- Autojoin waits for the IRC registration end (`376` or `422`) and for any auto-identify grace period before sending `JOIN`.
- Loading a bookmark while connected uses the shared disconnect confirmation/feedback path instead of silently tearing down the session.
- Fixed bookmark channel contamination: loading one bookmark can no longer connect to that bookmark's server while replaying channels from a different bookmark.
- Deleting an empty bookmark slot no longer truncates or rewrites an empty file.

#### New IRC commands and PM workflow

- Added `/mode` wrapper:
  - no args: query current channel modes;
  - args starting with `+` or `-`: apply modes to current channel;
  - other args: pass explicit IRC target unchanged.
- `324 RPL_CHANNELMODEIS` now produces visible mode output instead of only updating internal status.
- Added `/reply message`, using the last remembered incoming PM sender.
- Added `/notice target text` without adding a parallel large send path.
- `/msg nick text` activates the target query before local echo, so outgoing PMs appear in the expected window.
- `/0` through `/9` switch directly to physical window slots, matching the numbers shown by the tab/switcher UI.
- `/quit` and reconnect paths now share a guarded disconnect confirmation.

#### Better `/names`, `/list`, search, and channel counts

- Manual `/names` now owns the main area while active and suppresses unrelated traffic centrally.
- `353` replies render as a fixed-width four-column nick grid instead of raw wrapped IRC payload text.
- Long `/names` output supports pagination, cancellation, and incomplete-summary reporting when data is lost under buffer pressure.
- `/names` now accepts `366` only for the target channel, avoiding stale replies being committed to the wrong window.
- Automatic join-time NAMES counting still updates channel user counts without stealing the UI.
- Added a hardware-validated first-letter friend gate for NAMES bursts, reducing expensive friend matching during joins without changing count semantics.
- Added optional long-session user-count resync via `!countsync` / `!cs`.
- Silent count refresh uses `LIST #channel` only when the runtime is idle enough, avoiding eager traffic in busy channels.
- `/search` output now starts cleanly, avoids prompt collisions, and throttles repeated rapid visual updates.
- IRC fallback output remains visible but is timestamped/indented instead of being dropped or printed raw at column 0.

#### Time and RTC

- Added `!tz rtc` as an opt-in local RTC clock source.
- The cold RTC overlay can seed time from local RTC paths and preserve the last numeric timezone for fallback.
- ESP/SNTP time remains the default for normal setups.
- SNTP now retries during connect, avoiding the common ESP8266 `1970` placeholder when the first NTP attempt has not resolved yet.
- Status-bar clock updates were kept in the common main-loop tick path so overlays do not stale the clock.
- CTCP `TIME` avoids pretending to have a valid clock when no RTC/time source is available.

#### UI and display polish

- Added `!divider` to toggle channel context separators.
- Channel separators make interleaved channel/query context changes easier to read in long sessions.
- Shift-arrow cursor navigation now keeps the CAPS indicator stable and avoids cursor flicker.
- The status screen shows nick, server, network, state, latency, uptime, and a two-column window/channel list.
- `!config` includes new startup/session controls such as `autojoin`, saved channels, `divider`, `countsync`, RTC timezone state, friends, and ignores.
- The help text is now sourced from `src/SPECTALK_HELP.txt` and generated into `SPECTALK.DAT`, instead of being an untracked hand-edited data blob.
- Help pagination was fixed when the help text exactly filled a page count; it no longer shows an empty extra page.
- The What's New screen is generated from `release/version.txt`, `release/changes.txt`, and `release/logo.png`.

#### About / Earth overlay

- Replaced the old globe path with a compact animated Earth/About renderer.
- About assets are stored in `release/about_earth/` and packed into runtime data.
- The About screen processes pending IRC lines before entering animation.
- During About, a bounded overlay keepalive path handles PING/PONG and prevents false disconnects.
- About rendering is theme-adaptive: Earth colours and PAPER handling follow the active theme instead of assuming one palette.
- About exit resets overlay/RX state safely so reused overlay memory is not parsed as IRC traffic.

#### Notifications and input

- PM notifications still support ENTER-to-open and BREAK-to-dismiss.
- Friend notifications are batched after NAMES completion instead of spamming per nick.
- PART/leave notifications cancel the footer notification before printing the inline leave text.
- Notification slide-in is guarded by live timeout state so stale animation counters cannot revive a cancelled row.
- `!notif`, `!beep`, and `!click` are handled in the cold local-command overlay to save resident space.
- Word navigation and word delete remain available through Symbol Shift cursor/delete combinations.

### Reliability and protocol fixes

- Fixed auto-identify for non-standard NickServ service names such as `NiCK`.
- Added bounds checks to malformed config-key parsing so short keys cannot read past the NUL terminator.
- Config save now checks overlay-slot capacity before writes can pass the 512-byte scratch buffer.
- Raw string consumers no longer receive BPE-compressed `SB_*` strings after the resident string import shrink.
- Direct render paths and string compares keep raw ASCII literals where required.
- Overlay exit now discards parser/ring state when overlay memory reused `ring_buffer`.
- Overlay exits that reuse `overlay_slot` only force RX discard when a partial line existed before the call; valid first replies after an overlay are not swallowed.
- Prompt capture helpers NUL-terminate `rx_line` on timeout and success exits.
- IRC format stripping and digit tests were corrected after an ASM audit found flag-contract bugs.
- UTF-8 Latin-1 conversion was fixed after table movement near a page boundary; `Ñ/ñ` map to the internal display glyph again and accented vowels normalize stably.
- `/connect` retry prompt spacing was fixed so retry prompts do not gain extra blank lines.
- `force_disconnect()` now resets transient latency, post-cancel, and count-sync state, preventing ghost state after reconnect.
- BSS zero-fill now guards the `size == 1` case as well as `size == 0`, avoiding a Z80 `LDIR` wrap hazard.
- esxDOS buffer pointers can load directly into `IX` in the current assembler path, reducing fragile pointer shuffling.
- Several ABI-sensitive ASM helpers were audited for `IX`/`IY`, stack cleanup, carry/flag contracts, and callee argument removal.

### Performance, size, and architecture

- Resident ASM was split from one large `spectalk_asm.asm` into domain modules:
  - `10_core_helpers.asm`
  - `20_rx_ring_uart.asm`
  - `30_rendering.asm`
  - `40_text_numeric_screen.asm`
  - `50_main_output.asm`
  - `60_protocol_storage.asm`
  - `70_input_lookup.asm`
  - `80_ui_runtime.asm`
- Overlay count grew to eight logical SPCTLK pages while staying below the 2K executable-page limit for each page.
- Overlay loader now validates the fixed `STOA` + version header with a short loop rather than open-coded compares.
- The overlay atlas uses variable-length metadata, reducing dead fixed-size layout overhead.
- Generated BPE dictionary sizing is exact rather than conservatively overallocated.
- The 6x8 font is packed and decoded on demand with corrected glyph offset math.
- `_notif_buf` / `names_friend_buf` moved from compiler BSS to fixed Printer Buffer space, freeing 64 bytes of compiler BSS without overlapping input cache or scratch areas.
- Cold local commands moved to overlays where possible: password, ignore, settings toggles, autoaway, friend list, bookmarks, RTC, and related config/status work.
- `_main_puts` gained a hardware-validated fast path for safe even-column ASCII chunks.
- `print_line64_fast()` alignment was corrected so bulk-rendered text matches per-character rendering vertically.
- UART/divMMC hot paths were shrunk by removing dead local state, redundant port reloads, and unnecessary preserve/restore sequences.
- IRC numeric dispatch no longer needs the old zero sentinel; it uses `CMD_TABLE_COUNT`.
- The parser and input lookup paths gained small fixed-index ASM wrappers and peephole rules where build measurement proved a win.
- Status/config/bookmark overlays were repeatedly deduplicated for headroom without shortening user-visible labels.
- Many shrink candidates were rejected and recorded when they grew the binary, risked UX, or needed hardware evidence.

### Rejected or deferred prototypes

- Scanline-level UART drain during visible scroll built successfully but was rejected on hardware because scrolling felt irregular and "under water".
- Status-bar backlog deferral was rejected because it could delay clock/status freshness and make the UI feel stale.
- A larger NAMES friend-scan ASM rewrite was reverted after hardware showed slower or less reliable connects after `AT+CIPSTART OK`.
- Raw `theme_raw` cold-load BSS experiment was rejected because DAT I/O overhead ate nearly all BSS savings.
- Unioning `autojoin_channels` with `search_pattern` was rejected because the lifetimes are not actually disjoint.
- Several proposed string-shortening and label-shortening ideas were rejected because established user-facing text should not be cut for tiny byte wins without explicit approval.

### Current release files

- Runtime files required together:
  - `SpecTalkZX.tap`
  - `SPECTALK.OVL`
  - `SPECTALK.DAT`
- Generated release metadata:
  - `release/version.txt` -> `v1.3.8: Hermes`
  - `release/changes.txt` -> in-app What's New summary
  - `overlay/whatsnew_data.h` -> generated payload used by the overlay
- New README screenshots are copied into `images/` as cropped `snapshot-01.png` through `snapshot-17.png` plus refreshed banners.

---

## [v1.3.7] - Artemis II

Baseline stable release before the 1.3.8 development line.

- TAP: **36,072 bytes**.
- Core features at that point included nick colouring, smart notifications, word navigation, config save detection, C-compiled overlays, prompt marker, Ikkle-4 mini font, improved esxDOS file handling, What's New, and the first large public size-optimization pass.
