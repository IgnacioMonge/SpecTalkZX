# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-02-02 "The Final Polish"

### Added
- **Smart Fallback Handler**: Implemented a generic numeric response parser in `irc_handlers.c`.
  - **New Feature**: Now displays output from `/raw` commands (like `/raw version`, `/raw time`) even if the client doesn't have a specific handler for that numeric code.
  - **Detail**: It intelligently parses and prints command parameters that were previously discarded, fixing visibility for servers like InspIRCd that send info in params.
- **Unity Build System**: Switched to a single compilation unit strategy (`main_build.c`).
  - Allows SDCC to perform global cross-function inlining and aggressive optimization across the entire C codebase.

### Optimized
- **Assembly Rendering Routines** (`asm/spectalk_asm.asm`):
  - **Scroll**: Rewrote `scroll_main_zone` to use efficient block transfers via a lookup table, eliminating C function call overhead for the bottom line clear routine.
  - **Memory Moves**: Implemented custom `text_shift_right` (using `LDDR`) and `text_shift_left` (using `LDIR`) in assembly, removing the dependency on the generic `memmove` C library function.
- **Memory Footprint**:
  - Converted local command string arrays in `user_cmds.c` to `static const`. This moves data from runtime stack construction to flash/static memory, saving code size and preventing stack overflows.
  - Removed unused theme attributes (e.g., `msg_priv_inv`) to save RAM.

### Fixed
#### UART Timing Precision (Critical)
- **Problem**: Baud rate timing calculation (`_baud/2`) was being recalculated on every byte read.
- **Context**: This consumed ~20 T-states per byte unnecessarily during bit-banging timing-critical code.
- **Solution**: Added `_baud_half` pre-calculated variable in `ay_uart_init()`.
- **Result**: More stable UART timing, reduced risk of bit errors at 9600 baud (AY driver only).

#### /search Command on Freenode
- **Problem**: `/search #linux` returned no results on freenode.net.
- **Root cause**: `/list` and `/search` didn't activate pagination before sending LIST command. Servers that don't send 321 (LISTSTART) before 322 responses were being ignored.
- **Solution**: Both commands now activate `pagination_active` state immediately before sending the request.

#### Server Noise Filtering
- **Problem**: High-traffic servers flood the status window with non-critical info on connect.
- **Solution**: Added filtering for noisy server numerics: 002-005 (server info), 250-266 (stats), 396 (host hidden). These are now handled silently unless explicitly requested.

#### UI Stability
- **Pagination Freeze**: Fixed a freeze when pressing `EDIT` to cancel a long listing / search.
- **Ghost Character**: Fixed a visual bug where deleting the last character in the input line sometimes left a pixel trail.

---

## [1.1.1] - 2026-01-27 "Sleepless Guardian"

### Fixed

#### Keep-Alive System (Silent Disconnection Detection)
- **Problem**: Client could remain in "connected" state indefinitely after silent server disconnection
- The server would timeout and close the connection, but the client wouldn't notice until user tried to send a message
- Particularly affected overnight idle connections

#### Solution: Proactive Keep-Alive
- Added `server_silence_frames` counter tracking frames since last server activity
- After 3 minutes (9000 frames) of silence, client sends `PING :keepalive`
- If no response within 30 seconds (1500 frames), connection is considered dead
- User sees "Connection timeout (no response)" message
- Counter resets on any server activity (PING response, messages, etc.)

### Added

#### Critical IRC Event Handlers
- **ERROR**: Server closing connection (e.g., "Closing Link: Connection timeout")
  - Now properly disconnects and shows reason
  - Previously: message shown but client stayed in "connected" state
  
- **KICK**: Channel expulsion handling
  - If kicked: shows reason, closes channel window automatically
  - If someone else kicked: shows notification, decrements user count
  
- **KILL**: Server expulsion handling
  - Shows killer and reason
  - Forces full disconnect and channel reset
  
- **451 (Not registered)**: Session expiration
  - Detects when server no longer recognizes our session
  - Forces disconnect with helpful "Use /server to reconnect" message
  - This was exactly the error seen after overnight idle

### Optimized

#### Code Size Reduction (Phase 3)
- Removed dead code: `wait_for_response_any()` function (~30 lines)
- Eliminated `server_tmp[64]` buffer in `cmd_connect()` - parse directly to global variables
- Eliminated `short_topic[21]` buffer in `/list` handler - paint characters directly

### Technical Notes
- New variables: `server_silence_frames`, `keepalive_ping_sent`, `keepalive_timeout`
- Keep-alive logic runs in main loop at 50Hz, only when `STATE_IRC_READY`
- Minimal overhead: just counter increments when server is active
- New handlers added to IRC_CMD_TABLE: KICK, KILL, ERROR
- New handler added to IRC_NUM_TABLE: 451

---

## [1.1.0] - 2026-01-26 "Lean Machine"

### Optimized

#### Binary Size Reduction (-2019 bytes, -5%)
- Applied `__z88dk_fastcall` calling convention to high-frequency functions:
  - `main_puts()`, `main_print()`, `main_putc()`
  - `uart_send_line()`, `esp_send_line_crlf_nowait()`
  - `parse_irc_message()`, `parse_user_input()`
  - All `cmd_*` and `sys_*` command handlers
  - `is_ignored()`, `add_ignore()`, `remove_ignore()`
  
- Applied `__z88dk_callee` calling convention to multi-parameter graphics functions:
  - `print_char64()`, `print_str64()`
  - `main_run()`, `main_run_char()`, `main_run_u16()`

#### Status Bar Optimization
- New `sb_append()` helper function consolidates redundant string copy loops in `draw_status_bar_real()`

#### Clock System Rewrite
- Replaced 32-bit arithmetic with incremental 8-bit counters
- Eliminated `uint32_t` variables (`uptime_frames`, `time_sync_frames`)
- New tick-based time tracking: `tick_counter`, `time_second`, `time_minute`, `time_hour`
- Removed dependency on 32-bit division library (`l_div`, `l_mod`)

#### IRC Registration Phase
- Refactored server connection to use direct `uart_send_string()` calls instead of `str_append()` + `esp_send_line_crlf_nowait()`
- Eliminated `tx_buffer` dependency during NICK/USER/PASS registration
- Streamlined IRCv3 CAP negotiation response
- Optimized PING/PONG handling during registration loop

### Technical Notes
- Final binary: 38,297 bytes (down from 40,316 bytes)
- No functionality removed - all features preserved
- Clock now updates incrementally at 50Hz instead of calculating from frame count

---

*Leaner, meaner, same great taste - now with 2KB more room for activities*

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

*First public release - connecting the ZX Spectrum to IRC since 2026*
