# SpecTalkZX Changelog

## [v1.3.9] - Juno - Release candidate

Juno is the complete development line after `v1.3.8: Hermes`. Its headline
feature is native support for the **SpectraNext cartridge**, delivered as a
separate compile-time target without removing or weakening the Classic
ZX/divMMC build. The release also includes every network, storage,
configuration, overlay, scrolling and presentation change completed since the
published 1.3.8 tag.

Final release-profile verification recorded on 2026-08-28:

- Classic command: `make release NO_COLOR=1 PLATFORM=classic`.
- SpectraNext command:
  `make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=C:/dev/SpectraNext/driver`.
- Result: **both builds OK**; driver, network, storage, configuration and clock
  contracts **OK** against SpectraNext authority `a4ae350`.
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
- Net Classic TAP delta: **-795 bytes**, despite the added safety work, Juno
  screen/data and the common platform seams used by the new target.

### User-visible features

#### Native SpectraNext support

- Added a dedicated `PLATFORM=spectranext` build for ZX Spectrum models
  supported by the SpectraNext cartridge firmware.
- Networking uses cartridge ROM sockets and DNS directly. No UART, ESP-AT
  command stream or runtime backend selector is linked into this target.
- Configuration and all five bookmark slots persist in local XFS under `/CFG`.
- The displayed clock is obtained through UDP/SNTP before the IRC TCP socket is
  opened and uses the configured numeric timezone.
- Startup detection and fatal diagnostics identify the required cartridge/XFS
  environment instead of reporting Classic divMMC requirements.
- The title banner is target-specific: Classic remains “IRC Client for ZX
  Spectrum”; the cartridge build reports “IRC Client for Spectranext”.
- `!init` closes the active cartridge socket and reinitializes the native
  backend without sending ESP-AT text into the IRC stream.

#### Resource Index installer and updates

- Added the official manifest-driven SpectraNext resource: remote `boot.zx`,
  product installer, SPXI package, loading screen, `package.json` and
  `index.txt`.
- The loading artwork keeps the installation panel at the bottom of the screen.
- The installer validates the package, writes the application to local XFS slot
  0 and launches the installed product.
- Normal updates replace immutable application files while preserving
  `/CFG/SPECTALK.CFG` and `/CFG/SPTBM1.CFG` through `SPTBM5.CFG`.
- The release installer is built without the development
  `--force-install` override.
- Added managed FuseX direct and installer launch paths for repeatable emulator
  checks. Public hardware installation remains the Resource Index route.

#### Juno What's New and visual presentation

- Added the complete Juno artwork to `release/logo.png` and streams its packed
  64x88 image from `SPECTALK.DAT` instead of consuming overlay code space.
- The What's New screen contains twelve curated 1.3.9 entries and always ends
  with the magenta “And much, much more!” line.
- Replaced the obsolete 1.3.8 screenshot gallery with nineteen fresh Juno
  captures, grouped and captioned by installation, IRC workflow, tools/session
  management and themes.
- Corrected status-result rendering so successful operations use the intended
  leading-space form instead of `[OK]` or a clipped `[ OK`.
- Polished the status-clock placement and target banner without changing the
  three established themes.

#### About/Earth while connected

- Hardened Earth packet bounds and delta validation before animation data can be
  applied.
- Kept esxDOS reads inside the required interrupt state and reset shared RX
  state safely when overlays exit.
- The SpectraNext About pump now performs at most eight cartridge ROM calls per
  frame instead of as many as 128, removing the dominant connected-session
  animation jitter observed on hardware.
- PING/PONG, peer close and keepalive state remain serviced while About owns the
  screen.

#### Network and recovery reliability

- Bounded Classic UART TX-ready polling; a stuck BUSY state no longer waits
  forever.
- Bounded ESP-AT command and raw UDP transmission paths.
- Partial raw UDP writes now stop and recover instead of continuing with a
  malformed datagram.
- Network and clock acquisition are separated behind compile-time seams, keeping
  IRC policy common while each target owns only its hardware transport.
- SpectraNext clock retries defer while another overlay owns the shared buffer.
- SNTP validation rejects malformed replies, mismatched peers and a zero
  transmit timestamp instead of accepting a permanently synchronized midnight.

#### Configuration, storage and UI safety

- Configuration keys require exact matches; malformed prefixes are no longer
  accepted as valid settings.
- Packed config, friend and ignore lists are bounded to their storage and screen
  capacities.
- Every firmware-sensitive file operation invalidates the shared input cache at
  the correct boundary.
- SpectraNext XFS scratch preservation protects live UI state during config and
  bookmark operations.
- `!save` and bookmark completion retain the interface and use the corrected
  status-result prefix.
- Replaced stack-based chat scrolling with an interrupt-safe implementation
  that does not borrow temporary display RAM as a stack.
- Registration-time `ERROR` recognition checks the complete token and boundary,
  avoiding prefix false positives.
- Overlay exit paths discard reused parser/ring state so loaded code or data
  cannot be consumed as IRC input.

### Build, layout and reproducibility

- Added fail-closed Classic/SpectraNext memory-layout checks for resident BSS,
  the overlay ring, stack, Printer/CHANS areas and XFS scratch/state bindings.
- Added source contracts for network, clock, storage, configuration, SNTP,
  installer, COPT control flow, registration errors and stale artifacts.
- The build records the selected z88dk version in `build/toolchain.version` for
  reproducibility but deliberately does not pin or reject a particular release.
- Generated `SPECTALK.DAT`, all eight overlays and the TAP are treated as one
  inseparable build set.
- The SpectraNext installer records exact file sizes and SHA-256 values in its
  generated manifest.

### SpectraNext limitations

- IRC uses plaintext on the configured server port. Cartridge TLS is exposed
  only through its fixed port-443 service, so IRC TLS on port 6697 is
  unavailable.
- The cartridge exposes no RTC syscall to Z80 software. `tz=rtc` therefore
  falls back to the last numeric timezone and time comes from UDP/SNTP.
- SpecTalkZX owns one cartridge socket at a time. Clock synchronization runs
  while the IRC socket is closed.
- Successful config/bookmark saves persist. The compact XFS compatibility
  writer is not an atomic rollback transaction if power or I/O fails during an
  existing-file overwrite.

### Hardware status

Verified on physical SpectraNext hardware during release preparation:

- Guided resource installation reaches and launches SpecTalkZX 1.3.9.
- Updating preserves the existing `/CFG/SPECTALK.CFG`.
- Saving configuration survives restart and reloads the expected settings.
- The Juno What's New artwork and release list render correctly.
- Connected About animation is materially smoother after the bounded pump
  change.

The platform-specific banners are verified in the release binaries; this is
build evidence, not promoted as a separate hardware observation.

These observations do not silently promote untested fault-injection, long soak,
all-theme, maximum-list, unexpected-NMI or power-cut cases; those remain
hardware-pending where applicable.

### Publication

Classic installation remains the GitHub release archive of TAP, OVL and DAT.
SpectraNext public installation is the Resource Index route, not that archive.

The generated resource directory (`boot.zx`, `SPECTALK.INS`, `SPECTALK.PKG`,
`SPECTALK.SCR`, `package.json`, `index.txt`) must be hosted unchanged over
HTTPS, verified in a desktop browser, then submitted at
[spectranext.net/submit-resource.html](https://spectranext.net/submit-resource.html).
A GitHub Release zip is not a mountable resource. The published installer must
be compiled without `--force-install`. The recorded 1.3.9 installer sizes are
`SPECTALK.INS` **3,247 bytes** and `SPECTALK.PKG` **43,670 bytes**. Resource
Index listing was still pending when this candidate was prepared. See
[README.md](README.md#publishing-the-spectranext-resource).

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
