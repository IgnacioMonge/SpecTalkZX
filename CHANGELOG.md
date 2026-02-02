# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.2.0] - 2026-02-02 "The Final Polish"

### Added

#### Auto-Away System
- **New command**: `/autoaway N` (alias `/aa`) sets automatic away after N minutes of inactivity
  - Range: 1-60 minutes, or 0/off to disable
  - `/autoaway` without arguments shows current status
- **Smart behavior**: Auto-away clears automatically when you send a message
- **Manual away preserved**: `/away message` sets manual away that does NOT auto-clear
- **New variables**: `autoaway_minutes`, `autoaway_counter`, `autoaway_active`

#### Away Status Server Sync
- **New handler**: `h_numeric_305_306()` processes server confirmations
  - **305 (RPL_UNAWAY)**: "You are no longer marked as being away"
  - **306 (RPL_NOWAWAY)**: "You have been marked as being away"
- **Benefit**: Away indicator now stays synchronized even if local state diverges from server

#### Smart Fallback Handler
- Generic numeric response parser in `irc_handlers.c`
- Displays output from `/raw` commands (like `/raw version`, `/raw info`, `/raw stats`)
- Intelligently parses command parameters that were previously discarded
- Fixes visibility for servers like InspIRCd that send info in params

#### Unity Build System
- Switched to single compilation unit strategy (`main_build.c`)
- Allows SDCC to perform global cross-function inlining
- Aggressive optimization across entire C codebase

### Fixed

#### Private Message Color Inversion (Critical)
- **Problem**: Incoming private messages (`<<`) used same colors as outgoing (`>>`), making them indistinguishable
- **Expected behavior**:
  - `>> NICK: MSG` (outgoing) → Nick in green, message in yellow
  - `<< NICK: MSG` (incoming) → Nick in yellow, message in green
- **Solution**: Swapped `ATTR_MSG_PRIV` and `ATTR_MSG_SELF` in incoming message handler
- **Files**: `irc_handlers.c` lines 274-280 and 191-196

#### Connection Indicator Logic
- **Problem**: `draw_connection_indicator()` only showed 2 states (red/green)
- **Solution**: Now properly shows 3 states:
  - 🔴 **Red**: No WiFi (`STATE_DISCONNECTED`)
  - 🟡 **Yellow**: WiFi OK but no TCP (`STATE_WIFI_OK`)
  - 🟢 **Green**: Connected (`STATE_TCP_CONNECTED` or `STATE_IRC_READY`)

#### /search Command on Freenode
- **Problem**: `/search #linux` returned no results on freenode.net
- **Root cause**: Servers that don't send 321 (LISTSTART) before 322 responses were ignored
- **Solution**: Commands now activate `pagination_active` state before sending request

#### Server Noise Filtering
- Added filtering for noisy server numerics: 002-005 (server info), 250-266 (stats), 396 (host hidden)
- These are now handled silently unless explicitly requested via `/raw`

#### UI Stability
- **Pagination Freeze**: Fixed freeze when pressing `EDIT` to cancel long listing/search
- **Ghost Character**: Fixed visual bug where deleting last character left pixel trail

### Optimized

#### Assembly Rendering Routines (`spectalk_asm.asm`)
- **Scroll**: Rewrote `scroll_main_zone` with efficient block transfers via lookup table
- **Memory Moves**: Custom `text_shift_right` (LDDR) and `text_shift_left` (LDIR) in assembly
- Eliminated dependency on generic `memmove` C library function

#### Memory Footprint
- Converted local command strings in `user_cmds.c` to `static const`
- Moves data from stack to flash/static memory, saving code size

### Removed (Dead Code Cleanup)

#### Unused Variables
- `ATTR_MSG_PRIV_INV` - declared but never used (spectalk.c, spectalk.h)
- `g_input_safe` - static variable never referenced (spectalk.c)
- Redundant `ind_attr` calculation in `draw_status_bar_real()` - recalculated internally anyway

#### Copyright Header Consistency
- Changed all headers from "SpecTalk ZX v1.0" to "SpecTalk ZX" (versionless)
- Prevents desync with `#define VERSION "1.2"`

### Technical Notes
- New external variables in `spectalk.h`: `autoaway_minutes`, `autoaway_counter`, `autoaway_active`
- Handler table entries added: 305, 306
- Command table entry added: AUTOAWAY with alias AA
- Help screen updated with `/away /autoaway N` documentation

---

## [1.1.1] - 2026-01-27 "Sleepless Guardian"

### Fixed

#### Keep-Alive System (Silent Disconnection Detection)
- **Problem**: Client remained in "connected" state after silent server disconnection
- Server would timeout and close connection, but client wouldn't notice until user tried to send
- Particularly affected overnight idle connections

#### Solution: Proactive Keep-Alive
- Added `server_silence_frames` counter tracking frames since last server activity
- After 3 minutes (9000 frames) of silence, client sends `PING :keepalive`
- If no response within 30 seconds (1500 frames), connection considered dead
- User sees "Connection timeout (no response)" message
- Counter resets on any server activity

### Added

#### Critical IRC Event Handlers
- **ERROR**: Server closing connection (e.g., "Closing Link: Connection timeout")
  - Now properly disconnects and shows reason
  - Previously: message shown but client stayed "connected"
  
- **KICK**: Channel expulsion handling
  - If kicked: shows reason, closes channel window automatically
  - If someone else kicked: shows notification, decrements user count
  
- **KILL**: Server expulsion handling
  - Shows killer and reason
  - Forces full disconnect and channel reset
  
- **451 (Not registered)**: Session expiration
  - Detects when server no longer recognizes session
  - Forces disconnect with "Use /server to reconnect" message

### Optimized

#### Code Size Reduction
- Removed dead code: `wait_for_response_any()` function (~30 lines)
- Eliminated `server_tmp[64]` buffer in `cmd_connect()` - parse directly to globals
- Eliminated `short_topic[21]` buffer in `/list` handler - paint characters directly

### Technical Notes
- New variables: `server_silence_frames`, `keepalive_ping_sent`, `keepalive_timeout`
- Keep-alive logic runs in main loop at 50Hz, only when `STATE_IRC_READY`
- New handlers in IRC_CMD_TABLE: KICK, KILL, ERROR
- New handler in IRC_NUM_TABLE: 451

---

## [1.1.0] - 2026-01-26 "Lean Machine"

### Optimized

#### Binary Size Reduction (-2019 bytes, -5%)

Applied `__z88dk_fastcall` calling convention to high-frequency functions:
- `main_puts()`, `main_print()`, `main_putc()`
- `uart_send_line()`, `esp_send_line_crlf_nowait()`
- `parse_irc_message()`, `parse_user_input()`
- All `cmd_*` and `sys_*` command handlers
- `is_ignored()`, `add_ignore()`, `remove_ignore()`

Applied `__z88dk_callee` to multi-parameter graphics functions:
- `print_char64()`, `print_str64()`
- `main_run()`, `main_run_char()`, `main_run_u16()`

#### Status Bar Optimization
- New `sb_append()` helper consolidates redundant string copy loops

#### Clock System Rewrite
- Replaced 32-bit arithmetic with incremental 8-bit counters
- Eliminated `uint32_t` variables (`uptime_frames`, `time_sync_frames`)
- New tick-based tracking: `tick_counter`, `time_second`, `time_minute`, `time_hour`
- Removed dependency on 32-bit division library

#### IRC Registration Phase
- Refactored to use direct `uart_send_string()` calls
- Eliminated `tx_buffer` dependency during NICK/USER/PASS registration
- Streamlined IRCv3 CAP negotiation

### Technical Notes
- Final binary: 38,297 bytes (down from 40,316 bytes)
- No functionality removed - all features preserved
- Clock updates incrementally at 50Hz instead of calculating from frame count

---

## [1.0.0] - 2026-01-25 "First Contact"

### Added

#### Core Features
- Full IRC protocol support with table-driven command dispatch
- 64-column display using custom 4-pixel wide font (768 bytes)
- Multi-window interface supporting up to 10 simultaneous channels/queries
- Ring buffer (2KB) for reliable UART reception
- ESP8266 AT command interface for WiFi connectivity

#### IRC Protocol
- Complete message tokenizer with formal IRC parsing
- Numeric reply handling (001-433+)
- Command support: JOIN, PART, QUIT, NICK, PRIVMSG, NOTICE, TOPIC, MODE, KICK, BAN, WHO, WHOIS, LIST, NAMES, AWAY
- CTCP responses: VERSION, PING, TIME, ACTION
- NickServ automatic identification
- Keep-alive system with automatic PING

#### User Interface
- 3 visual themes (Default, Terminal, Colorful)
- Status bar with connection state indicators
- Activity notification for inactive channels
- Message history search functionality
- Tab-based window navigation
- Input line with cursor support

#### Hardware Support
- divTIESUS/divMMC hardware UART driver (115200 baud)
- AY-3-8912 bit-bang UART driver (9600 baud)
- Compatible with ZX Spectrum 48K/128K/+2/+3

#### Performance Optimizations
- Critical rendering paths in Z80 assembly
- Input cell caching to reduce screen updates
- Optimized string operations (stricmp, stristr, append)
- Assembly-based UART send/receive routines
- Efficient screen address calculations

### Technical Notes
- Built with z88dk and SDCC compiler
- Memory footprint optimized for 48K machines
- Modular codebase: separate C and ASM compilation units

---

*First public release - connecting the ZX Spectrum to IRC since 2025*
