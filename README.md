![SpecTalk Banner](images/white_banner.png)

# SpecTalk ZX

**IRC Client for ZX Spectrum with ESP8266 WiFi**

🇪🇸 [Leer en español](READMEsp.md)

![Platform](https://img.shields.io/badge/Platform-ZX%20Spectrum-blue)
![License](https://img.shields.io/badge/License-GPLv2-green)
![Version](https://img.shields.io/badge/Version-1.2-orange)

## Overview

SpecTalk ZX is a fully-featured IRC client for the ZX Spectrum. Using an ESP8266 WiFi module for connectivity, it provides a complete IRC experience on 8-bit hardware with a 64-column display and support for up to 10 simultaneous channel/query windows.

## Features

- **64-column display** using custom 4-pixel wide font
- **Multi-window interface**: Up to 10 simultaneous channels/queries
- **3 color themes**: Default, Terminal, Colorful
- **NickServ integration**: Automatic identification
- **CTCP support**: VERSION, PING, TIME, ACTION
- **Channel user counting** with real-time updates
- **Search functionality**: Find channels or users by pattern
- **Keep-alive system**: Automatic PING to prevent timeout
- **Activity indicators**: Visual notification for unread messages
- **Smart Protocol Handling** *(New)*:
  - Generic numeric parser allows viewing `/raw` command output (e.g., `/raw info`, `/raw stats`) and custom server errors.
  - Filters connection noise (MOTD, server modes) for a cleaner status bar.
- **Unity Build Architecture** *(New)*:
  - Entire client compiled as a single unit to maximize speed and fit within 48K RAM.
- **High-Performance UART**:
  - Ring Buffer (2KB) for reliable data reception.
  - Optimized drivers for divMMC (115200 baud) and AY (9600 baud).

[![SpecTalkZX](images/snap1.png)](images/snap1.png)


## Hardware Requirements

- **ZX Spectrum** 48K, 128K, +2, or +3.
- **ESP8266 WiFi Module** configured at the correct baud rate.

| Interface | Driver Used | Required Baud Rate |
|-----------|-------------|--------------------|
| **divMMC / divTIESUS** | Hardware UART / Fast Bit-bang | **115200** bps |
| **ZX-Uno / AY Interface** | AY-3-8912 Bit-banging | **9600** bps |

## Installation

1. Download the TAP file for your hardware
2. Load on your Spectrum (SD card, tape, etc.)
3. Configure WiFi with [NetManZX](https://github.com/IgnacioMonge/NetManZX) or similar tool

## Quick Start (example)

1.  **Connect**: On startup, the client will attempt to initialize the ESP8266.
    - Wait for `WiFi:OK` in the status bar.
2.  **Server**: Connect to an IRC server (default is configured in source, or use command):
    - `/server irc.libera.chat 6667`
3.  **Identify**: Set your nickname:
    - `/nick MyRetroNick`
4.  **Join**: Enter a channel:
    - `/join #spectrum`

[![SpectalkZX theme1](images/theme1.png)](images/theme1.png) [![SpectalkZX theme2](images/theme2.png)](images/theme2.png) [![SpectalkZX theme3](images/theme3.png)](images/theme3.png)

## Commands Reference

### 1. Controls & Key Bindings

| Key Combo | Action | Description |
|-----------|--------|-------------|
| **EXTEND** (CS+SS) | Toggle Mode | Switches between **Command Mode** (Typing) and **View Mode** (Scrolling). |
| **TRUE VIDEO** (CS+3)| Next Window | Cycles to the next active channel or query (Tab). |
| **INV VIDEO** (CS+4) | Prev Window | Cycles to the previous active channel or query. |
| **ENTER** | Send / Act | Sends the message or executes the command. |
| **EDIT** (CAPS+1) | Cancel / Clear | Clears the input line or cancels active searches. |
| **DELETE** (CAPS+0) | Backspace | Deletes character to the left. |
| **↑ / ↓** | History | Navigates command history. |
| **← / →** | Cursor | Moves cursor within the line. |

### 2. Slash Commands

All commands start with `/`.

| Category | Command | Description |
|----------|---------|-------------|
| **Session** | `/nick [name]` | Change nickname. |
| | `/quit [msg]` | Disconnect and exit. |
| | `/raw [cmd]` | Send raw IRC command (e.g., `/raw VERSION`). |
| | `/quote [cmd]` | Alias for `/raw`. |
| **Channel** | `/join [chan]` | Join a channel. |
| | `/part [chan]` | Leave a channel. |
| | `/topic [text]` | View or set channel topic. |
| | `/names` | List users in current channel. |
| | `/kick [user]` | Kick a user (Ops only). |
| | `/mode [args]` | Set channel/user modes. |
| **Messages** | `/msg [user] [txt]` | Send private message. |
| | `/query [user]` | Open a private chat window. |
| | `/me [action]` | Send action (e.g., `* User waves`). |
| | `/notice [tgt] [txt]`| Send notice. |
| **Tools** | `/windows` | List all open windows and IDs. |
| | `/clear` | Clear text in current window. |
| | `/search [str]` | Search active channels/users. |
| | `/list` | Download channel list (use with caution). |
| | `/1` ... `/9` | Jump to window ID. (/0 Server)|

## Building from Source

This project uses a **Unity Build** strategy to optimize for the Z80 target.

### Requirements
- **z88dk** (with SDCC support).
- **Make**.

### Build Commands

The `Makefile` supports different targets for different hardware backends:

bash
1. Standard Build (divMMC / divTIESUS - 115200 baud)
make

2. Legacy Build (AY Interface - 9600 baud)
make ay

3. Clean artifacts
make clean


### License

SpecTalk ZX is free software under **GNU General Public License v2.0**.

Includes code derived from:
- **BitchZX** - IRC client (GPLv2)
- **AY/ZXuno UART driver** by Nihirash

### Author

**M. Ignacio Monge Garcia** - 2026

### Acknowledgments

- BitchZX project for IRC protocol inspiration
- Nihirash for AY UART driver code
- z88dk team for the cross-compiler
- ZX Spectrum retro computing community
