# SpecTalkZX changelog

Only user-visible changes and compatibility notes are listed here.

## [1.4.0] - Proteus - 2026-09-04

### Added

- Native ZX Spectrum Next support as a self-contained `SPECTALK.NEX` file.
- Use of the Spectrum Next internal ESP and real-time clock.
- Target-specific About screens for Classic ZX, native Spectrum Next and the
  Spectranext cartridge.

### Changed

- Spectranext now keeps secondary screens and commands separate from incoming
  IRC data through cartridge memory paging.
- Native Next embeds help, themes, What's New and About data in the NEX file;
  no separate OVL or DAT files are needed.
- Native Next startup and `!init` can recover from an inherited or unresponsive
  ESP state. BREAK cancels initialization.
- Classic ZX and native Next try `/SYS` for bookmark files when
  `/SYS/CONFIG` is unavailable.
- The Spectranext installer artwork and package have been updated for 1.4.0.

### Release verification

- Classic: TAP **35,386 bytes**; BSS ends at **0xEFDD**, leaving **1,315
  bytes** before the receive ring; overlays **1705 / 1902 / 1573 / 1745 / 1934
  / 1798 / 1816 / 1973 bytes**; packed `SPECTALK.OVL` **14,510 bytes**.
- Native Next: `SPECTALK.NEX` **147,968 bytes**; resident **35,466 bytes**; BSS
  ends at **0xF086**, leaving **1,146 bytes**; overlays **1688 / 1882 / 1581 /
  1745 / 1965 / 1787 / 1816 / 1994 bytes**; embedded data **16,633 bytes**.
- Spectranext: TAP **36,512 bytes**; BSS ends at **0xF420**, leaving **224
  bytes**; overlays **1705 / 1922 / 746 / 2532 / 1490 / 1937 / 1816 / 1906
  bytes**; packed `SPECTALK.OVL` **14,374 bytes**.
- Spectranext resource: `SPCTX.INS` **3,299 bytes**, `SPCTX.PKG` **43,768
  bytes**, `SPCTX.SCR` **6,912 bytes**.

### Compatibility

- Classic ZX remains compatible with the 1.3.9 configuration format and
  command set. It still requires matching `SpecTalkZX.tap`, `SPECTALK.OVL` and
  `SPECTALK.DAT` files from one build.
- Native Next requires NextZXOS and a configured internal ESP.
- Existing Classic and Spectranext configuration and bookmark files need no
  migration.
- Spectranext requires cartridge firmware `0.9-6fc153a3` or later.

## [1.3.9.1] - Spectranext installation fix - 2026-08-30

The client remains SpecTalkZX 1.3.9 Juno. This maintenance release changes only
the Spectranext installer and storage handling.

### Fixed

- Installed program files now remain available after a power cycle.
- Later configuration changes now remain available after a power cycle instead
  of reverting to an earlier saved copy.
- Installation now starts through **Load Resource URL** in the Spectranext menu.

The installer labels this package `1.3.9-2`, the three-component form accepted
by the cartridge package format. Classic ZX is unchanged.

## [1.3.9] - Juno - 2026-08-28

### Added

- Native support for ZX Spectrum models equipped with the Spectranext
  cartridge.
- Guided Spectranext installation from
  `https://ignaciomonge.github.io/SpecTalkZX/`.
- Cartridge storage for configuration and five bookmark slots under `/CFG`.
- UDP/SNTP clock synchronization for Spectranext.
- Target-specific startup and About identification.

### Improved

- UART and UDP operations no longer wait indefinitely when hardware stops
  responding.
- Partial UDP sends stop safely instead of continuing with an incomplete
  packet.
- Configuration keys must match their complete names; unknown or truncated
  names are ignored.
- Long friend and ignore lists fit the configuration screen.
- File access no longer leaves stale keyboard input or damages visible
  interface state.
- About rejects incomplete animation data and remains responsive to IRC
  connection traffic.
- IRC registration errors and status messages display more reliably.

### Compatibility

- The Classic release uses `SpecTalkZX.tap`, `SPECTALK.OVL` and
  `SPECTALK.DAT`; all three files must come from the same release.
- Spectranext IRC is plaintext. IRC TLS on port 6697 is not supported by this
  target.
- Spectranext has no Z80-visible RTC. Selecting `tz=rtc` uses the last numeric
  timezone with SNTP.
- A power loss during a configuration or bookmark write can leave the file
  incomplete.

## [1.3.8] - Hermes - 2026-06-25

### Added

- Five-slot IRC bookmark manager with store, connect, delete and automatic
  startup controls.
- Session saving for server, port, joined channels and startup policy.
- `/mode`, `/reply`, `/notice` and direct `/0` through `/9` window switching.
- Paginated four-column `/names` view.
- Optional long-session channel count refresh with `!countsync`.
- Optional channel context separators with `!divider`.
- Local RTC clock mode with `!tz rtc` where supported.
- Animated, theme-aware About screen.

### Improved

- Bookmark slots keep their channel lists isolated.
- Autojoin waits for IRC registration and NickServ identification.
- `/names`, `/list` and `/search` handle long results and paging more clearly.
- NickServ identification accepts non-standard service names.
- Help paging, command prompts, IRC formatting and accented text handling are
  more reliable.
- About continues processing keepalive traffic while open.
- Notifications, cursor movement, status information and key repeat are more
  consistent.

### Compatibility

- esxDOS/divMMC storage is required.
- `SpecTalkZX.tap`, `SPECTALK.OVL` and `SPECTALK.DAT` must be copied together.

## [1.3.7] - Artemis II - 2026-04-06

### Added

- On-demand Help, About, Config, Status and What's New screens.
- Compact notifications for private messages, mentions and friends.
- Per-nick colours, word navigation, key repeat and an animated globe.
- Expanded connection and channel status information.

### Compatibility

- esxDOS/divMMC became mandatory.
- Releases from this version onward require the TAP, OVL and DAT files from the
  same archive.

Earlier releases are available on the
[GitHub Releases page](https://github.com/IgnacioMonge/SpecTalkZX/releases).

[1.4.0]: https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.4.0
[1.3.9.1]: https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.3.9.1
[1.3.9]: https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.3.9
[1.3.8]: https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.3.8
[1.3.7]: https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.3.7
