# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- Significant reduction in binary size (more room for future improvements!)
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
