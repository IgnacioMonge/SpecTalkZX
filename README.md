# SpecTalkZX

<p align="center">
  <img src="images/spectalkzx-banner.png" alt="SpecTalkZX" width="80%">
</p>

**IRC client for ZX Spectrum and the SpectraNext cartridge**

:es: [Leer en español](READMEsp.md)

**Installation:** [Classic ZX / divMMC](#classic-zx--divmmc) ·
[SpectraNext cartridge](#spectranext-cartridge)

![Platform](https://img.shields.io/badge/Platform-ZX%20Spectrum%20%7C%20SpectraNext-blue)
![License](https://img.shields.io/badge/License-GPLv2-green)
![Version](https://img.shields.io/badge/Version-1.3.9-orange)

SpecTalkZX 1.3.9 is the first release with native SpectraNext support.
It brings the complete IRC client to the cartridge through ROM sockets, XFS
storage and a guided HTTPS installer, while preserving the established
Classic ZX/divMMC build. This release also contains every reliability and UI
change completed since the published 1.3.8 release.

---

## Highlights in 1.3.9

- **Native SpectraNext target** using cartridge ROM sockets and DNS directly,
  with no UART/ESP-AT translation layer.
- **Guided SpectraNext installer** that verifies the package, installs a
  local XFS copy and preserves configuration and bookmarks during updates.
- **Persistent SpectraNext sessions** under <code>/CFG</code>, including all
  five bookmark slots, saved channels, autoconnect and autojoin.
- **UDP/SNTP clock for SpectraNext**, with numeric timezone handling before the
  single IRC socket is opened.
- **Classic remains fully supported** with the same three-file divMMC release
  and the same user interface and IRC command set.
- **Bounded UART, ESP-AT and raw UDP waits** replace paths that could previously
  stall indefinitely or continue after partial transmission.
- **Safer configuration and lists** through exact key matching, capacity checks
  and bounded friend/ignore rendering.
- **Storage-safe interface state** across config, bookmark and overlay file
  operations on both backends.
- **More robust About/Earth animation**, including packet validation,
  interrupt-safe file reads and responsive SpectraNext network pumping while
  connected.
- **Interrupt-safe chat scrolling** without using temporary screen RAM as a
  stack.
- **Clearer IRC registration failures** through exact <code>ERROR</code> token
  handling.
- **Presentation update** with complete artwork, twelve release entries,
  corrected status-result spacing and a banner identifying the active target.

See [CHANGELOG.md](CHANGELOG.md) for the complete change history, technical
limits and final build measurements.

---

## Screenshots

### Getting started and navigation

<table>
  <tr>
    <td align="center" valign="top" width="50%">
      <strong>Guided installer</strong><br>
      <a href="images/snapshot-spectranext-installer.png"><img src="images/snapshot-spectranext-installer.png" width="420" alt="SpecTalkZX guided installer on SpectraNext"></a><br>
      <sub>The HTTPS resource starts the guided installer. It verifies and writes the application to local XFS while leaving the user's <code>/CFG</code> data untouched.</sub>
    </td>
    <td align="center" valign="top" width="50%">
      <strong>Choose your nick</strong><br>
      <a href="images/snapshot-nick.png"><img src="images/snapshot-nick.png" width="420" alt="Choosing an IRC nick"></a><br>
      <sub>First-run nick selection before opening a server connection.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="50%">
      <strong>Connected server</strong><br>
      <a href="images/snapshot-connected.png"><img src="images/snapshot-connected.png" width="420" alt="Connected to Libera Chat"></a><br>
      <sub>The server window after the native SpectraNext socket reaches IRC.</sub>
    </td>
    <td align="center" valign="top" width="50%">
      <strong>Window navigation</strong><br>
      <a href="images/snapshot-options.png"><img src="images/snapshot-options.png" width="420" alt="Server and channel windows"></a><br>
      <sub>The tab bar keeps the server and active channel contexts one key away.</sub>
    </td>
  </tr>
</table>

### Conversation and discovery

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Joining a channel</strong><br>
      <a href="images/snapshot-joining.png"><img src="images/snapshot-joining.png" width="280" alt="Joining an IRC channel"></a><br>
      <sub>Join progress and channel context remain visible during registration.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Live conversation</strong><br>
      <a href="images/snapshot-chat.png"><img src="images/snapshot-chat.png" width="280" alt="Normal IRC conversation"></a><br>
      <sub>64-column chat with timestamps, nick colours, modes and unread state.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Fast private reply</strong><br>
      <a href="images/snapshot-fast-reply.png"><img src="images/snapshot-fast-reply.png" width="280" alt="Fast reply to a private message"></a><br>
      <sub>PM notifications support immediate ENTER-to-open reply workflow.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Friends online</strong><br>
      <a href="images/snapshot-friends-online.png"><img src="images/snapshot-friends-online.png" width="280" alt="Friend detection"></a><br>
      <sub>Friends found during NAMES are collected into one compact notification.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Channel users</strong><br>
      <a href="images/snapshot-users.png"><img src="images/snapshot-users.png" width="280" alt="Channel user list"></a><br>
      <sub>The paginated four-column /names view keeps long nick lists readable.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Channel search</strong><br>
      <a href="images/snapshot-channel-search.png"><img src="images/snapshot-channel-search.png" width="280" alt="Channel search results"></a><br>
      <sub>Search and paging present large channel lists without colliding with input.</sub>
    </td>
  </tr>
</table>

### Management and information

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Bookmarks</strong><br>
      <a href="images/snapshot-bookmarks.png"><img src="images/snapshot-bookmarks.png" width="280" alt="IRC bookmark manager"></a><br>
      <sub>Five independent slots store server, port, channels and startup policy.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Configuration</strong><br>
      <a href="images/snapshot-config.png"><img src="images/snapshot-config.png" width="280" alt="Configuration overview"></a><br>
      <sub>The complete active configuration is inspectable without leaving the client.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Connection status</strong><br>
      <a href="images/snapshot-status.png"><img src="images/snapshot-status.png" width="280" alt="Connection status"></a><br>
      <sub>Network state, latency, uptime and open windows are summarized together.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Command help</strong><br>
      <a href="images/snapshot-help.png"><img src="images/snapshot-help.png" width="280" alt="Built-in command help"></a><br>
      <sub>The generated five-page help covers local and IRC commands inside the ZX.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>About</strong><br>
      <a href="images/snapshot-about.png"><img src="images/snapshot-about.png" width="280" alt="Animated About screen"></a><br>
      <sub>The Earth animation keeps IRC PING/PONG and connection handling alive.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>What's New</strong><br>
      <a href="images/snapshot-changes.png"><img src="images/snapshot-changes.png" width="280" alt="SpecTalkZX What's New screen"></a><br>
      <sub>The 1.3.9 release screen combines the SpecTalkZX artwork with twelve concise changes.</sub>
    </td>
  </tr>
</table>

### Themes

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Theme 1</strong><br>
      <a href="images/snapshot-theme-1-away.png"><img src="images/snapshot-theme-1-away.png" width="280" alt="Theme 1 with away state"></a><br>
      <sub>Default palette showing away state, notifications and coloured nicks.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Theme 2</strong><br>
      <a href="images/snapshot-theme-2.png"><img src="images/snapshot-theme-2.png" width="280" alt="Theme 2"></a><br>
      <sub>Green terminal-style palette with the same full IRC interface.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Theme 3</strong><br>
      <a href="images/snapshot-theme-3.png"><img src="images/snapshot-theme-3.png" width="280" alt="Theme 3"></a><br>
      <sub>High-contrast blue/red palette demonstrating theme-safe rendering.</sub>
    </td>
  </tr>
</table>

---

## Requirements

| Target | Computer | Storage | Network |
|---|---|---|---|
| Classic | ZX Spectrum 48K, 128K, +2, +2A, +3 or compatible | divMMC/esxDOS SD storage | Supported 115200-baud UART with ESP8266 or compatible ESP-AT bridge |
| SpectraNext | ZX Spectrum model supported by the SpectraNext cartridge firmware | Local cartridge XFS | Native cartridge Wi-Fi and ROM sockets |

Both builds use a matching runtime set: <code>SpecTalkZX.tap</code>,
<code>SPECTALK.OVL</code> and <code>SPECTALK.DAT</code>. Never mix files from
different builds. A stale atlas or data file can break help, About,
configuration, bookmarks or What's New.

---

## Installation

### Classic ZX / divMMC

1. Download the **Classic** archive from the GitHub release.
2. Copy <code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> and
   <code>SPECTALK.DAT</code> into the same directory on the SD card.
3. Configure the ESP-AT bridge for **115200 baud**. Wi-Fi credentials can be
   prepared with [NetManZX](https://github.com/IgnacioMonge/NetManZX) or an
   equivalent ESP-AT setup tool.
4. Load <code>SpecTalkZX.tap</code>, wait for the network indicator and connect
   to IRC.

### SpectraNext cartridge

SpectraNext does **not** use the Classic divMMC copy procedure. Do not put
<code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> or
<code>SPECTALK.DAT</code> on an SD card. The public install is an HTTPS
resource. The canonical root, hosted on this repository's GitHub Pages, is:

<code>https://ignaciomonge.github.io/SpecTalkZX/</code>

1. Configure the cartridge and Wi-Fi using the
   [official SpectraNext instructions](https://docs.spectranext.net/tutorials/setting-up-mounts).
2. At the BASIC prompt:

```text
%umount 2
%mount 2, "https://ignaciomonge.github.io/SpecTalkZX/"
%fs 2
%cat
%load "boot.zx"
```

   <code>boot.zx</code> is tokenized BASIC. Do not <code>%tapein</code> it.
3. The guided installer validates the package, writes
   <code>SPECTALK.tap</code>, <code>SPECTALK.OVL</code>,
   <code>SPECTALK.DAT</code>, <code>SPECTALK.ZX</code> and the version marker
   to local XFS slot 0, then launches the client.
4. Afterwards start <code>SPECTALK.ZX</code> from local XFS. To update, mount
   the same HTTPS root and load <code>boot.zx</code> again;
   <code>/CFG/SPECTALK.CFG</code> and the five bookmark files are preserved.

A GitHub Release zip is not a mountable resource. Maintainer hosting steps:
[Publishing the SpectraNext resource](#publishing-the-spectranext-resource).

---

## Quick Start

```text
/nick YourNick
/server irc.libera.chat 6667
/join #spectrum
```

Useful first setup:

```text
!theme 1
!timestamps smart
!notif on
!nickcolor on
!save
```

To save a complete session, open `!bm`. In bookmarks: **UP/DOWN** selects a slot, **S** stores the current server/channel snapshot, **A** marks it for startup, **ENTER** connects, **D** deletes, and **BREAK** saves/exits.

---

## Interface

- **64-column chat display** with a custom 4-pixel font.
- **Up to 10 windows**: server window `0` plus channel/query windows `1` to `9`.
- **Direct window switching** with `!0` through `!9` or `/0` through `/9`.
- **EDIT switcher** with unread/mention markers and direct numeric selection.
- **Three themes** with distinct badges and colour behaviour.
- **Nick colouring** based on stable nick hashing, with `!nickcolor`.
- **Smart notifications** using the Ikkle-4 mini-font at the bottom row.
- **PM quick reply**: ENTER on a PM notification opens that sender's query.
- **Optional timestamps**: off, on, or smart.
- **Channel context dividers** with `!divider`.
- **Status bar** with nick, current window, network/modes, user count, clock, away marker, and three-state connection indicator.


## Keyboard Controls

| Key | Action |
|-----|--------|
| **ENTER** | Send message, run command, accept overlay action |
| **EDIT** | Open/close the channel switcher |
| **DELETE** | Delete character behind cursor |
| **LEFT/RIGHT** | Move cursor or overlay selection |
| **UP/DOWN** | Command history or overlay row selection |
| **Symbol Shift + LEFT/RIGHT** | Word-by-word cursor movement |
| **Symbol Shift + DELETE** | Delete previous word |
| **BREAK** | Dismiss notification, cancel paging, or leave overlay |
| **ENTER on PM notification** | Open the sender query window |

---


## IRC Behaviour

- Supports the usual IRC workflow: `JOIN`, `PART`, `QUIT`, `NICK`, `PRIVMSG`, `NOTICE`, `TOPIC`, `MODE`, `KICK`, `WHO`, `WHOIS`, `LIST`, and `NAMES`.
- Supports CTCP `VERSION`, `PING`, `TIME`, and `ACTION`.
- NickServ can be used manually with `/id` or automatically with `nickpass=`.
- `nickserv=` can override the service nick when a network does not use standard `NickServ` naming.
- Friends are monitored through `!friend`; join/NAMES batches generate compact notifications.
- Ignores are managed with `/ignore`, including `-nick` removal.
- Away state supports manual `/away` and idle `!autoaway`.
- Keepalive detects silent disconnects and is also active during the About overlay.
- Long sessions keep channel user counts more stable through NAMES handling and optional `!countsync`.

---
## Commands

### Local Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `!help` | `!h` | Show command help |
| `!status` | `!s` | Show connection, latency, uptime, and window status |
| `!init` | `!i` | Reset the active network backend |
| `!config` | `!cfg` | Show all current settings |
| `!theme N` | | Switch theme `1`, `2`, or `3` |
| `!about` | | Animated About screen |
| `!changelog` | | What's New screen |
| `!bookmarks` | `!bm` | Open the IRC bookmark manager |
| `!save` | `!sv` | Save config and current session |
| `!autoconnect` | `!ac` | Toggle startup server connection |
| `!autojoin` | | Toggle replay of saved channels after registration |
| `!tz` | | Show/set timezone `-12`..`+12`; `rtc` uses the Classic RTC |
| `!timestamps` | `!ts` | Cycle off/on/smart timestamp modes |
| `!notif` | `!nf` | Toggle notifications |
| `!beep` | | Toggle mention sound |
| `!click` | | Toggle key click |
| `!traffic` | | Toggle JOIN/PART/QUIT presence noise |
| `!divider` | | Toggle channel context separators |
| `!countsync` | `!cs` | Toggle idle user-count resync |
| `!autoaway` | `!aa` | Auto-away after N minutes, `0` disables |
| `!friend` | | List or toggle tracked friends |
| `!nickcolor` | `!nc` | Toggle per-nick colours |
| `!clear` | `!cls` | Clear chat area |

Toggle commands with no argument alternate their state; they accept `on`/`off`/`1`/`0`.
`!timestamps` also accepts `smart`.

### IRC Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `/server [host[:port]]` | `/connect` | No args: show state or reconnect saved server; otherwise connect to `host[:port]` |
| `/nick [name]` | | View or set nick |
| `/pass [password\|clear\|none]` | | View/set stored password; `clear`/`none` remove it for the next connection |
| `/id [password]` | | Identify with NickServ or detected service nick |
| `/join channel\|#channel\|&channel` | `/j` | Join a bare, `#`- or `&`-prefixed channel |
| `/part [#channel\|&channel] [message]` | `/p` | Leave the current or named `#`/`&` channel |
| `/msg nick text` | `/m` | Send private message |
| `/reply text` | | Reply to the last PM sender |
| `/notice target text` | | Send IRC NOTICE |
| `/query nick` | `/q` | Open a private query window |
| `/close` | | Close current query or part current channel |
| `/quit [message]` | | Disconnect, with confirmation guard |
| `/me action` | | Send CTCP ACTION |
| `/away [message]` | | Set or clear away |
| `/raw command` | | Send raw IRC command |
| `/whois nick` | `/wi` | Show WHOIS information |
| `/who [channel\|nick]` | | Search users; defaults to the current channel |
| `/list pattern` | `/ls` | List channels |
| `/names [#channel\|&channel]` | | Paginated grid of users in the current or named channel |
| `/topic [#channel] [text]` | | View/set topic; target and text are optional |
| `/mode [args]` | | View/set channel or target modes |
| `/search #pattern\|nick` | | Search LIST channels with `#pattern` or WHO users with `nick` |
| `/ignore [nick]` | | List, add, or remove ignored nicks (`-nick`) |
| `/kick nick [reason]` | `/k` | Kick from current channel |
| `/channels` | `/w` | List open windows |
| `/0`..`/9` | | Switch to a physical window slot |

`/pass` only updates the stored password used when opening the next connection; it
does not send an immediate IRC `PASS` command. The numeric `!0`..`!9` and
`/0`..`/9` forms are dispatcher shortcuts for physical window slots, not aliases.

---

## Configuration

SpecTalkZX writes the current configuration with `!save`. Classic loads `SPECTALK.CFG` from `/SYS/CONFIG/` with `/SYS/` as fallback. SpectraNext loads `/CFG/SPECTALK.CFG` from local cartridge XFS.

```ini
nick=MyNick
server=irc.libera.chat
port=6667
pass=
nickpass=myNickServPassword
nickserv=NickServ
autoconnect=1
autojoin=1
channels=#spectrum,#zx
theme=1
timestamps=2
autoaway=15
beep=1
click=1
traffic=1
divider=1
countsync=1
tz=1
notif=1
nickcolor=1
friends=Friend1,Friend2
ignores=NoisyNick
```


Supported settings:

| Setting | Values | Notes |
|---------|--------|-------|
| `nick` | IRC nick | Default nick |
| `server` | Hostname/IP | IRC server |
| `port` | `1`-`65535` | Default IRC port is usually `6667` |
| `pass` | Text or empty | Server password |
| `nickpass` | Text or empty | NickServ password for `/id` / auto-identify |
| `nickserv` | Nick or empty | Service nick override, for non-standard networks |
| `autoconnect` | `0`/`1` | Connect to saved server at startup |
| `autojoin` | `0`/`1` | Join saved `channels` after IRC registration |
| `channels` | Comma-separated channels | Session restore channel list |
| `theme` | `1`, `2`, `3` | Colour theme |
| `timestamps` | `0`, `1`, `2` | Off, on, smart |
| `beep` | `0`/`1` | Mention sound |
| `click` | `0`/`1` | Key click sound |
| `traffic` | `0`/`1` | JOIN/PART/QUIT traffic display |
| `divider` | `0`/`1` | Channel context separators |
| `countsync` | `0`/`1` | Idle user-count resync |
| `notif` | `0`/`1` | Bottom-row notifications |
| `nickcolor` | `0`/`1` | Per-nick colours |
| `autoaway` | `0`-`60` | Idle minutes, `0` disables |
| `tz` | `-12`..`+12` or `rtc` | SNTP timezone; `rtc` is Classic RTC mode |
| `tzlast` | `-12`..`+12` | Last numeric timezone used when leaving RTC mode |
| `friends` | Comma-separated nicks | Up to five tracked friends |
| `ignores` | Comma-separated nicks | Up to five ignored nicks |

Notable settings:

- `autoconnect=1` connects to the saved server on startup.
- `autojoin=1` replays the saved `channels=` list after IRC registration and after any required NickServ grace period.
- On Classic, `tz=rtc` uses the local RTC seed path and numeric values use ESP/SNTP time. SpectraNext has no Z80-visible RTC: it uses UDP/SNTP and falls back from `rtc` to `tzlast`.
- `divider=0` hides future channel context separators.
- `countsync=0` disables idle count refresh after long sessions.
- `friends=` and `ignores=` hold up to five nicks each.

Bookmark files are stored separately as `/SYS/CONFIG/SPTBM1.CFG` through `SPTBM5.CFG` on Classic and `/CFG/SPTBM1.CFG` through `SPTBM5.CFG` on SpectraNext.

---

## SpectraNext limits

- IRC uses plaintext on the configured server port. The cartridge exposes TLS
  only on its fixed port-443 service, so IRC TLS on port 6697 is unavailable.
- The cartridge exposes no RTC to Z80 software. SpectraNext uses UDP/SNTP;
  `tz=rtc` falls back to the last numeric timezone stored in `tzlast`.
- SpecTalkZX owns one cartridge socket at a time. Clock synchronization runs
  before the IRC socket opens.
- Successful configuration and bookmark saves persist in XFS. An I/O failure or
  power loss during an overwrite is not an atomic rollback transaction in this
  release.

---

## Build

Requirements: z88dk with SDCC support, GNU Make, and Python 3.8 or newer.

```sh
make NO_COLOR=1
make release NO_COLOR=1
make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=/path/to/SpectraNext/driver
make clean
```

Build outputs:

- `build/SpecTalkZX.tap`
- `build/SPECTALK.OVL`
- `build/SPECTALK.DAT`

The SpectraNext installer is a separate compilation of those same artifacts.
See [Publishing the SpectraNext resource](#publishing-the-spectranext-resource).

The project uses a unity C build plus hand-written Z80 modules. Generated data
includes compressed strings, help, overlay metadata, What's New, the compact
font and About/Earth animation assets. The compiler version is recorded in
`build/toolchain.version`; the project records it for reproducibility but
does not pin or reject a specific z88dk release.

---

## Publishing the SpectraNext resource

This is the maintainer path. End users install by mounting the GitHub Pages
HTTPS directory; they do not copy TAP files onto the cartridge and they do
not wait for the cartridge author's catalogue. The official Resource Index is
optional later publicity. The binding host procedure is
[Publishing a Resource](https://docs.spectranext.net/publishing/publish-a-resource).

### 1. Build and test the release artifacts

Build both targets from the same source, then compile the **release**
installer **without** `--force-install`:

```sh
make release NO_COLOR=1 PLATFORM=classic
make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=/path/to/SpectraNext/driver
/path/to/SpectraNext/tools/dev installer packaging/spectranext/installer.json \
  build/spectranext-resource
```

On Windows PowerShell use `tools\dev.cmd`. The output directory must be new or
empty. `--force-install` is for local reinstalls of the same version only; never
host that installer.

The published folder is a flat HTTPS directory:

| File | Role |
|---|---|
| `boot.zx` | Remote entry. Tokenized BASIC. Never `%tapein`. |
| `SPECTALK.INS` | Generic installer runtime. |
| `SPECTALK.PKG` | SPXI package with TAP, OVL and DAT. |
| `SPECTALK.SCR` | Loading screen. |
| `package.json` | Resource name, version, sizes and SHA-256. |
| `index.txt` | HTTP filesystem directory listing. |
| `index.html` | GitHub Pages root only; not listed in `index.txt`. |
| `.nojekyll` | GitHub Pages host extra. |

`SPECTALK.ZX` is created on the cartridge during install. It is not a hosted
file. The remote `boot.zx` must stay on the server; it must not be copied to
XFS slot 0, where `boot.zx` is the owner's global power-on file.

Use the same TAP/OVL/DAT set recorded in [CHANGELOG.md](CHANGELOG.md). Do not
substitute a later development rebuild.

### 2. Host the directory over HTTPS

Public resources must be served from a stable `https://` root. Plain HTTP is
only for local testing. A GitHub Release zip is **not** a mountable resource.

The public host for SpecTalkZX is GitHub Pages on the **stable** repository
(`https://ignaciomonge.github.io/SpecTalkZX/`), not the development checkout.
Upload the generated directory **byte-for-byte**. The installer preview PNG is
a local review file and is not part of the resource.

GitHub Pages returns 404 for a directory URL unless `index.html` exists at
that root. The cartridge mounts that URL, so a missing `index.html` becomes
`No such file or directory` even when `boot.zx` and `index.txt` are present.
Add a small Pages-only `index.html`; do not list it in `index.txt`. Verify
the root URL itself returns HTTP 200 in a desktop browser before mounting.

### 3. Verify the hosted files in a desktop browser

Open:

- `https://ignaciomonge.github.io/SpecTalkZX/` (must be HTTP 200, not GitHub's 404 page)
- `https://ignaciomonge.github.io/SpecTalkZX/index.txt`
- `https://ignaciomonge.github.io/SpecTalkZX/boot.zx`
- `https://ignaciomonge.github.io/SpecTalkZX/package.json`

Confirm the listing format, the `boot.zx` bytes, and that every SHA-256 in
`package.json` matches the uploaded file. See the
[HTTP(s) filesystem](https://docs.spectranext.net/filesystem/https-fs)
`index.txt` contract.

### 4. Share the HTTPS root

That GitHub Pages root is what the cartridge mounts. Users follow the
[install steps](#spectranext-cartridge). Do not tell them to `%tapein`
`boot.zx`. An optional TinyURL-class alias may help typing; it is not the
canonical URL.

### 5. Optional later listing in the official Resource Index

GitHub Pages is enough for public install. Submitting
`https://ignaciomonge.github.io/SpecTalkZX/` at
[spectranext.net/submit-resource.html](https://spectranext.net/submit-resource.html)
is optional catalogue publicity by the cartridge author. Do not block a
release on that review. Update the user install notes only after it appears.

---

## Troubleshooting

| Problem | Check |
|---|---|
| Classic indicator stays red | ESP-AT bridge wiring, power and 115200 baud |
| SpectraNext cartridge is not detected | Cartridge present and local XFS `/CFG` available |
| Indicator is ready but IRC will not connect | Wi-Fi credentials, hostname and plaintext IRC port |
| Startup stops on esxDOS/DAT | Classic divMMC mounted; all three files together and from one build |
| Help/About/bookmarks fail | `SPECTALK.OVL` or `SPECTALK.DAT` is missing or stale |
| SpectraNext install does not start | Mount the GitHub Pages HTTPS root; do not `%tapein` `boot.zx` |
| Clock remains at `00:00` | SNTP access and numeric timezone; Classic may also use `!tz rtc` |
| NickServ identify fails | Use `/id`, `nickpass=` or the `nickserv=` override |
| Too much JOIN/PART noise | Toggle `!traffic` |
| Channel counts drift | Keep `!countsync` enabled or run `/names` |
| Accented text looks odd | UTF-8 is converted to the ZX display character set |
| `/reply` has no target | Receive a PM first so its sender can be remembered |

---

## License

SpecTalkZX is free software released under the **GNU General Public License
v2.0**.

Includes code derived from:

- UART driver work by **Nihirash**.
- **Ikkle-4** mini font by Jack Oatley.

---

## Author

**M. Ignacio Monge Garcia** — 2025–2026

*Connecting the ZX Spectrum to IRC since 2025.*
