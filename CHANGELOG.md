# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.3.0] - 2026-02-22 "Social Network"

### Added

#### Auto-Connect System
- **New config option**: `autoconnect=1` connects automatically to configured server on startup
- Combined with `nickpass=`, provides fully automated login experience
- Eliminates manual `/server` command for regular users

#### NickServ Auto-Identification
- **New config option**: `nickpass=` for automatic NickServ identification after connect
- Separate from server password (`pass=`) for clarity
- Sends `IDENTIFY <password>` to NickServ immediately after registration completes

#### Friends Detection System
- **New config options**: `friend1=`, `friend2=`, `friend3=` to define up to 3 friend nicks
- Automatic ISON polling every 30 seconds to detect online friends
- Status bar indicator shows when friends are online
- WHOIS information displayed when friend comes online

#### Timestamps Toggle
- **New command**: `/timestamps` (alias `/ts`) toggles timestamp display
- **New config option**: `timestamps=1` to enable by default
- Timestamps now shown consistently on all message types when enabled

#### Ping Latency Indicator
- Server response time measured and displayed after PING/PONG
- Helps diagnose connection quality issues
- Displayed in status messages when relevant

#### Automatic Nick Recovery (433 Handling)
- If nickname is in use during registration, automatically appends `_` and retries
- Prevents connection failure due to nick collision
- User notified of alternate nick being used

#### UTF-8 Support
- Incoming UTF-8 text automatically converted to displayable ASCII
- Accented characters (áéíóúñüç) converted to base letters
- Emoji and other multi-byte sequences replaced with `?`
- Enables readable chat with international users

### Fixed

#### Reentrant Disconnection Bug
- Fixed race condition where multiple disconnect triggers could corrupt state
- Added `disconnecting_in_progress` guard flag
- Prevents crashes during network instability

#### Search Command Reliability
- Fixed `rx_overflow` not being reset in `send_pending_search_command`
- Prevents stale overflow state from corrupting search results

#### WHO/Search Parsing
- Implemented protocol-agnostic parsing for numeric 352 (WHO reply)
- Works correctly with different IRC server implementations
- Fixes `/who` and `/search` on non-standard servers

#### Message Formatting
- `wrap_indent=0` now correctly applied after QUIT/KICK/KILL messages
- Prevents visual artifacts in subsequent messages

### Optimized

#### Deep Code Optimization (4 passes)
- **Library function elimination**: Replaced `strstr`, `strncmp`, `strcmp` with specialized inline checks
- **String deduplication**: Consolidated 20+ duplicate string literals into shared constants
- **ASM hot paths**: Converted `print_char64`, `print_str64`, `irc_param` to optimized assembly
- **SDCC codegen improvements**: Applied `__z88dk_fastcall` and `__z88dk_callee` conventions throughout
- **Table compression**: Optimized command dispatch tables and help text pool
- **Call fusion**: Combined consecutive `main_puts` calls with `main_puts2` helper

#### Memory Footprint
- Reduced command history buffer from 128 to 96 bytes per entry (128 bytes BSS saved)
- Eliminated unused static buffers
- Consolidated toggle command implementations

### Changed

#### Configuration File Format
- `!config` display now shows all new options with formatted output
- Config parser extended for new keywords
- Maintains backward compatibility with v1.2 config files

### Technical Notes
- Multiple optimization passes applied (see optimization reports r1-r4)
- Total estimated savings: ~500-800 bytes ROM across all optimizations
- New ASM routines: `_utf8_to_ascii`, `_st_strchr`
- New handlers: friends ISON/WHOIS processing
- Config parser extended for: `autoconnect`, `nickpass`, `friend1-3`, `timestamps`

---

## [1.2.2] - 2026-02-15 "Identity Crisis"

### Added

#### NickServ Quick Identification (`/id`)
- **New command**: `/id [password]` for fast NickServ identification
- If password provided: sends `IDENTIFY <password>` to NickServ
- If no password: uses the password from config file (`pass=...`)
- Replaces tedious `/msg NickServ identify password` workflow
- Visual confirmation: "Identifying with NickServ..."

#### Configuration Viewer (`!config`)
- **New command**: `!config` (alias `!cfg`) displays current configuration
- Shows all configurable variables and their values:
  - `nick=`, `server=`, `port=`, `pass=` (hidden if set)
  - `theme=`, `autoaway=`, `beep=`, `quits=`, `tz=`
- Useful for verifying settings loaded from config file

#### Mention Highlighting in Window List
- **Enhanced `/w` command**: channels with mentions now display in highlight color
- Channels where your nick was mentioned show in `ATTR_MSG_PRIV` (typically magenta)
- Makes it easier to spot which conversations need attention

### Optimized

#### Memory Footprint Reduction (~136 bytes BSS + ~26 bytes code)

- **Command history buffer**: Reduced `HISTORY_LEN` from 128 to 96 bytes per entry
  - 4 entries × 32 bytes saved = **128 bytes BSS**
  - IRC commands rarely exceed 96 characters in practice

- **Shift key mapping**: Replaced 4-way if/else chain with lookup table
  - `static const uint8_t shift_keys[] = {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT}`
  - Saves ~10 bytes of code, slightly faster execution

- **Status bar buffer**: Changed `user_buf[8]` from stack to static
  - Reduces stack frame setup overhead (~8 bytes code)

- **String literal deduplication**: Added `S_CONNECTED` constant
  - Shared across 2 call sites in `sb_pick_status()`
  - Saves ~8 bytes

### Technical Notes
- New commands in help system: `!config` (index 3), `/id` (index 9)
- Help system now shows `!` prefix for first 6 commands (was 5)
- `NUM_USER_CMDS` increased to 31
- Command table updated with new entries and aliases

---

## [1.2.1] - 2026-02-04 "Slim & Steady"

### Optimized

#### Nibble-Packed Font Compression (~390 bytes saved)
- **Discovery**: Original 768-byte font uses only 10 unique byte values
- **Discovery**: Lines 6 and 7 of every glyph are always 0x00
- **Solution**: Compress each glyph from 8 bytes to 3 bytes using nibble packing
- **Implementation**:
  - `font_lut`: 10-byte lookup table for unique values
  - `font64_packed`: 288 bytes (96 glyphs × 3 bytes)
  - `unpack_glyph`: ~80-byte decompression routine
- **Total**: 378 bytes vs 768 bytes original = **390 bytes saved**
- **Performance**: Adds ~50 T-states per character (~22ms per full screen redraw) - imperceptible

#### Attribute Address Lookup Table
- Added `_attr_row_base`: precalculated table of 24 attribute row addresses
- Eliminates repeated `y*32 + 0x5800` calculations throughout codebase
- Optimized `_attr_addr` function to use table lookup

#### Search Command Unification
- New `do_search_or_queue()` helper consolidates duplicate logic
- Simplified `cmd_list`, `cmd_who`, and `cmd_search` functions
- Removed ~60 bytes of redundant code

### Fixed

#### /search Returns "No results" Incorrectly (Critical)
- **Problem**: `/search` command frequently returned "No results" even when matches existed
- **Root cause 1**: Condition `rb_head != rb_tail` in search logic was too restrictive
- **Root cause 2**: Cancelling with EDIT activated drain mode (`search_draining=1`) but end-markers never arrived
- **Root cause 3**: Drain timeout reset whenever buffer had data (IRC always has traffic), so timeout never triggered
- **Solution**: 
  - Removed `rb_head != rb_tail` check from `do_search_or_queue()`
  - Changed `cancel_search_state(1)` to `cancel_search_state(0)` in `pagination_pause()` - manual cancel doesn't need drain
  - Changed drain timeout to absolute 3 seconds (150 frames) without resetting on buffer activity
- **Files**: `user_cmds.c` line 116, `spectalk.c` lines 682, 2331

#### /quit Does Not Update Connection State (Critical)
- **Problem**: After `/quit`, `!status` still showed "IRC ready" instead of "WiFi OK"
- **Root cause**: `cmd_quit()` called `force_disconnect()` but didn't update `connection_state`
- **Effect**: User believed they were still connected; subsequent commands would fail confusingly
- **Solution**: Added `connection_state = STATE_WIFI_OK;` after `force_disconnect()` in `cmd_quit()`
- **File**: `user_cmds.c` line 502

### Technical Notes
- Font data now integrated directly in `spectalk_asm.asm` (removed `font64_data.h` include)
- New BSS variable: `glyph_buffer` (8 bytes) for decompression workspace
- Removed `font64_packed.asm` as separate file - consolidated into main ASM
- Build command updated: remove `asm/font64_packed.asm` from compilation
- Drain timeout changed from conditional 5s to absolute 3s for reliability

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
