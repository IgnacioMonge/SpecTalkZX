# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
