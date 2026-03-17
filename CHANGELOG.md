# Changelog

All notable changes to SpecTalk ZX will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.3.5] - 2026-03-16 "Terminal Glow"

### Added

#### System RAM Hijacking + IM2 (+602 bytes BSS freed)
- **IM2 interrupt mode**: Replaces IM1 to prevent divMMC automapper from triggering on every interrupt at 0x0038. Custom ISR at 0xFDFD increments FRAMES for HALT-based timing. Vector table at 0xFC00 (257 bytes, runtime). Previously, Printer Buffer and CHANS hijacking crashed under IM1 — divMMC's interrupt hook corrupted those areas
- **Printer Buffer (0x5B00-0x5BBF, 192 bytes)**: `input_cache_char/attr` moved from BSS. esxDOS RST 8 still corrupts this area during file I/O — `input_cache_invalidate()` called after every esxDOS operation to force clean redraw. `irc_pass`, `nickserv_pass`, `network_name` kept in BSS (must survive esxDOS calls)
- **CHANS workspace (0x5CB6-0x5DB5, 256 bytes)**: `line_buffer`, `temp_input` moved from BSS
- **UDG area (0xFF58-0xFFF1, 154 bytes)**: `friend_nicks`, `away_message`, `names_target_channel` moved from BSS
- All three regions zeroed by CRT init (3× LDIR). Safe: no ZX Printer, no ROM I/O, no UDGs used

#### esxDOS Detection
- Safe detection using ERR_SP trick — tries M_GETSETDRV via RST 8, catches ROM error if no divMMC
- `has_esxdos` flag guards config_load() and cmd_save()
- Without divMMC: program boots normally with defaults, `/save` shows "No esxDOS"

#### Activity Indicator (Theme 2)
- **Blinking `<<<`**: When a PRIVMSG is received, the `<<<` badge blinks by toggling INK green/black every 16 frames (~0.32s cycle)
- Any keypress stops the blink and restores normal display
- Zero CPU cost — just 4 XOR writes every 16th frame in the main loop

#### Help Screen State-Based Rewrite
- Help was a blocking loop that froze the main loop: PING/PONG not processed, clock stopped, keepalive inactive → server disconnected
- Now state-based: renders page and returns to main loop. IRC processing continues between pages
- Ring buffer occupied only during page read (~5ms SD access)

### Changed

#### Banner Redesign (NetManZX Style)
- **Text aligned to column 0** (was column 1) — maximizes usable banner width
- **Theme-dependent background**: Theme 1 & 2 black, Theme 3 red (was blue for all)
- **Double-height font restored** for banner (`draw_big_char` + `print_big_str`)
- **BRIGHT split**: Top row BRIGHT 1, bottom row BRIGHT 0 for text — depth effect
- `draw_big_char` ASM: removed hardcoded attribute writing — `clear_line()` controls BRIGHT per theme

#### Badge System Redesign
- **Theme 1 (Default)**: 5-cell dithered rainbow (black→red→yellow→green→blue) with wrap transition
- **Theme 2 (The Terminal)**: `<<<` text badge, green on black — minimalist terminal prompt
- **Theme 3 (The Commander)**: `_ [] X` text badge, white on red — Norton Commander window buttons
- Badge struct: `badge[5]` replaces `badge[4] + badge_count` (same 25-byte theme size)
- Badge always BRIGHT on both rows, independent of text BRIGHT split

### Fixed

#### Reconnect Regression
- Connecting to another server while connected hung at "Registering..."
- **Root cause**: PONG format during registration — `irc_send_pong()` sends `PONG server :token` which some servers reject during registration
- **Fix**: Registration loop reverted to v1.3.3.1 format (echo params directly)
- Removed QUIT in `force_disconnect()` before `+++` — caused throttle on reconnect
- Added absolute timeout (~60s) in registration loop to prevent indefinite hang

#### Pagination at End of Listing
- `pagination_active` set to 0 BEFORE printing summary, preventing spurious "Any key: more"

#### Attribute Bleed in /search
- Replaced `st_strlen` parity check with `main_col & 1` pad, fixing magenta/white ink mixing

#### channel_flags_ptr Link-Time Bug
- `ld de, EXTERN + constant` produced wrong addresses in z80asm. Changed to runtime evaluation

#### Help Screen Output Leak (pre-existing)
- **Problem**: IRC messages rendered on top of help pages in high-traffic channels
- **Root cause**: `help_active` guard only in `main_print` (C) — `main_puts`, `main_putc`, `main_newline` (ASM) had no guard, and IRC handlers call these directly
- **Fix**: Added `help_active` check to `main_puts`, `main_putc`, `main_newline` in ASM. Output suppressed during help; IRC processing (PING/PONG) continues normally

#### TAP Loader
- Added `--blockname SpecTalkZX` and `--usraddr` to appmake for proper loader name and autorun

#### cmd_save Ring Buffer Corruption (Critical, Gemini audit)
- `cmd_save()` used `ring_buffer` as temp but didn't reset `rb_head/rb_tail/rx_pos`
- `process_irc_data()` would parse config data as IRC

#### flush_all_rx_buffers rx_overflow (Gemini audit)
- Changed `rx_overflow = 1` to `rx_overflow = 0` — empty buffer after flush means clean start

#### process_irc_data drain order (ChatGPT audit)
- Moved `uart_drain_to_buffer()` BEFORE measuring backlog — prevents jitter on bursty traffic

#### esx_result clamp (ChatGPT audit)
- Added bounds check before NUL terminator write in `cfg_try_read` and `help_render_page`

### Optimized

#### Code Size Reduction (8 optimization passes, -1,520 bytes)
- **Phase 1**: String dedup (`S_SP_COLON`, `S_SP_PAREN`, `S_TIMEOUT`), tail call optimization, 24 jp→jr conversions. -42 bytes
- **Phase 2**: copt subroutine factoring — `hl_mul32` (53 sites, -90B), `rx_pos_reset` (25 sites, -68B). -165 bytes
- **Phase 3**: C functions rewritten in ASM — `find_empty_channel_slot` (-23B), `print_char64` (-14B), `nav_push` (-24B). -61 bytes
- **Phase 4**: `sb_append` callee convention (-27B), KEY_UP/DOWN merge (-11B), XOR caps branchless, CLOSED 16-bit check. -45 bytes
- **Phase 5**: Cache `cmd_str[0]` (-15B), 8-bit loop counter (-18B), `sys_puts_print` unify (-12B), EX DE,HL in CRT, OUT (C),L in UART. -59 bytes
- **Phase 6**: copt `channel_flags_ptr` (-46B), C pointer caching in 5 functions (-53B), uppercase loop, `S_AUTOAWAY` dedup. -108 bytes
- **Phase 7**: copt `a_sext_mul32` (9 sites), `l_mul32` (12 sites), IX prologue→`___sdcc_enter_ix` (24 sites), IX epilogue→`jp _leave_ix` (28 sites). -179 bytes
- **Phase 8**: copt IX-relative load/store helpers — `ld_hl_ix4` (30 sites), `ld_hl_ixm2` (23 sites), `ld_hl_ix6` (18 sites), `ld_hl_ixm4` (14 sites), `st_hl_ix4` (11 sites), `st_hl_ixm2` (10 sites), `l_channel_flags_ptr` (5 sites). -300 bytes

#### cur_chan_ptr Optimization (-140 bytes)
- Cached `ChannelInfo *cur_chan_ptr` eliminates idx×32 multiplication at 25 call sites
- Macros `irc_channel`, `chan_mode`, `chan_user_count`, `chan_flags` use `cur_chan_ptr->`

#### SD Offload (-725 bytes ROM)
- Font 64-col moved to SPECTALK.DAT (BSS + SD load at startup). -269 bytes
- Theme data moved to SPECTALK.DAT. -91 bytes
- Help text moved to SPECTALK.DAT (loaded on demand per page). -365 bytes

#### Badge Rendering Performance
- Dither 100% ASM (`draw_badge_dither` renders both rows in one call). -40 C pointer writes eliminated
- Attribute writes unrolled — no C loops, values cached in locals
- ~15,000 → ~4,000 cycles

#### Inline Space Rendering in main_puts
- Spaces (~25% of IRC text) now rendered inline: clear nibble + set attr directly
- Skips full `print_str64_char` pipeline (unpack_glyph + pixel merge)
- Cache-validated: falls back to full path on first char after newline/scroll
- ~50 T-states per space vs ~650 for full path (~12x faster, ~2.7ms saved per line)

#### Build System
- `.tap` and `SPECTALK.DAT` now output to `build/` directory

### Technical Notes
- SPECTALK.DAT layout: `[10 LUT][288 glyphs][75 theme data][help text]` (1,077 bytes)
- `draw_big_char` ASM: 15 instructions removed (attr writes now handled by clear_line)
- copt rules: COPT-1 `hl_mul32`, COPT-2 `rx_pos_reset`, COPT-4 `channel_flags_ptr`, COPT-5 `a_sext_mul32`, COPT-6 `l_mul32`, COPT-7 IX prologue, COPT-8 IX epilogue, COPT-9 `l_channel_flags_ptr`, COPT-10..13 IX load helpers, COPT-14..15 IX store helpers
- New ASM functions: `esx_detect`, `find_empty_channel_slot`, `nav_push`, `badge_flash_on/off`, `leave_ix`, `a_sext_mul32`, `l_mul32`, `l_channel_flags_ptr`, `ld_hl_ix4/ixm2/ix6/ixm4`, `st_hl_ix4/ixm2`
- IM2: vector table at 0xFC00 (257B runtime), ISR template copied to 0xFDFD (12B runtime)
- System RAM regions zeroed by extended CRT init (3× LDIR blocks)
- `help_active` made non-static for ASM access; guards added to `main_puts`, `main_putc`, `main_newline`
- esxDOS Printer Buffer corruption: `input_cache_invalidate()` after `esx_fclose` in `help_render_page` and `cmd_save`
- Binary TAP size: 35,786 bytes (vs 37,166 baseline v1.3.4 = **-1,380 bytes, -3.7%**)
- BSS_END: 0xF98E (vs 0xFEEE in v1.3.4 = **-1,376 bytes BSS**). Growth margin: 626 bytes. Stack: 334 bytes

---

## [1.3.4] - 2026-03-07 "Solid Ground"

### Added

#### Channel Switcher Bar
- **New overlay**: Press EDIT to open a visual tab bar showing all active channels
- Navigate with LEFT/RIGHT arrows, select with ENTER, or press a digit key for direct access
- Active channel highlighted, unread channels marked with `*`, mentions marked with `!`
- Auto-hides after 20 seconds of inactivity
- Live refresh: unread/mention indicators update in real-time while the switcher is open
- Dynamic rebuild: channels added or removed while the switcher is open are reflected immediately

#### Help Screen Improvements
- Exit key changed from SPACE to BREAK (CS+SPACE) — avoids accidental dismissal when typing
- Removed blank line before `/0-9` entry for better layout

### Fixed

#### IRC Color Strip Bug (Critical)
- **Problem**: `strip_is_digit` in ASM returned "is digit" for characters below ASCII `'0'` (including NUL terminator)
- **Impact**: A bare `^C` color reset at the end of an IRC message caused reads past the NUL terminator, corrupting output with garbage bytes
- **Root cause**: `cp '0'` sets carry when A < 0x30 (underflow), and `ret c` returned with carry set — but the convention is carry set = IS a digit
- **Solution**: Added explicit `jr c, sid_false` branch that clears carry before returning

#### Lowercase IRC Command Handling
- **Problem**: The command dispatcher normalized the hash to uppercase for routing, but `pkt_cmd` itself was not modified — handlers comparing `pkt_cmd[0]` or `pkt_cmd[2]` against uppercase literals silently failed for lowercase commands
- **Impact**: Bouncers/gateways sending lowercase commands (e.g., `notice`, `kick`) caused NOTICE to be misidentified as PRIVMSG (wrong rendering, broken auto-identify, spurious query windows) and KICK/KILL to be silently dropped (channel not removed, stale data)
- **Solution**: Normalize `pkt_cmd[0..2]` to uppercase in-place before dispatch, so all downstream handlers see consistent uppercase

#### `/friend` Space Truncation
- **Problem**: `/friend nick extra` stored `"nick extra"` instead of just `"nick"`, unlike every other command that accepts a nick argument
- **Solution**: Added space truncation before processing, matching `cmd_ignore` pattern

#### Switcher Bar Stale Map After Channel Removal
- **Problem**: If a channel was removed (kick/part) while the switcher was open, key handling on the next frame used stale `sw_map[]` data because the dirty-rebuild check ran after key processing
- **Solution**: Moved `if (sw_dirty) switcher_render()` to run before key handling, ensuring the map is always current

#### Switcher Bar Crash on EDIT Key
- **Problem**: Pressing EDIT to open the channel switcher caused an immediate crash to BASIC with "J Invalid I/O device" error under certain compilation conditions
- **Root cause**: `print_str64()` used `__z88dk_callee` calling convention with inline ASM containing `push ix`/`pop ix`. Without aggressive SDCC optimization (`--max-allocs-per-node`), SDCC generates IX-based frame pointer code whose prologue/epilogue conflicted with the inline ASM's IX save/restore, corrupting the return stack
- **Solution**: Rewrote `print_str64()` as pure C loop (no inline ASM) — SDCC now handles callee convention and frame pointer management internally without conflict. `--max-allocs-per-node200000` moved to release builds only

### Technical Notes
- New ASM label: `sid_false` in `strip_is_digit` (2 bytes added)
- `pkt_cmd` normalization adds 3 conditional subtracts (~18 bytes)
- `cmd_friend` space truncation reuses existing `strchr` pattern (~12 bytes)
- Switcher rebuild moved before key handler (no size change, just reorder)
- Binary TAP size: 37,176 bytes

---

## [1.3.3] - 2026-03-03 "Remember Me"

### Added

#### Persistent Ignore List
- **Ignore list now saved to config**: `/ignore nick` + `/save` persists the ignore list across restarts
- On startup, `SPECTALK.CFG` is parsed and the ignore list is restored automatically
- `!config` now displays `ignores=` alongside other settings

#### Colored Status Bar Banner
- **Top status bar** now renders with a colored background matching the current theme
- Provides a cleaner visual separation between the status bar and the main text area

### Changed

#### CSV Config Format for Lists
- **Friends and ignores** now use comma-separated format in config file:
  - Old: `friend1=Nick1`, `friend2=Nick2`, `friend3=Nick3` (one line per entry)
  - New: `friends=Nick1,Nick2,Nick3` (single line, comma-separated)
  - New: `ignores=Troll1,Troll2` (single line, comma-separated)
- Parser tolerates spaces after commas (`nick1, nick2`)
- Shared `csv_next_tok()` parser and `cfg_put_csv()` writer eliminate code duplication

#### Expanded Friends & Ignore Slots
- **Friends**: Increased from 3 to 5 slots (`MAX_FRIENDS`)
- **Ignores**: Reduced from 8 to 5 slots (`MAX_IGNORES`)
- Both lists now aligned at 5 entries for consistency
- All hardcoded `3` references replaced with `MAX_FRIENDS` constant

### Optimized

#### Redundant Zero-Initialization Removal
- **Significant binary size reduction** by removing explicit `= 0`, `= {0}`, `= ""` initializers from global variables
- The z88dk CRT startup code already zeroes the entire BSS segment before `main()` runs
- Explicit initializers caused variables to be placed in the DATA segment with redundant initialization code
- Removing them lets variables fall into BSS (zero-cost, zeroed by CRT), saving both code and data bytes

### Fixed

#### `!theme` Usage Message Shows Wrong Prefix
- **Problem**: `!theme` without arguments displayed `Usage: /theme 1|2|3` (wrong prefix)
- **Cause**: `ui_usage()` always prepends `/`, but `!theme` is a system command
- **Solution**: Custom usage path with correct `Usage: !theme 1|2|3` message

#### Nick Underscore Confusion with Ellipsis
- **Problem**: "Autoconnecting as spectalk_..." visually merged the `_` with `...` on Spectrum font
- **Solution**: Added space separator: "Autoconnecting as spectalk_ ..."

### Technical Notes
- New defines: `MAX_FRIENDS 5` (was hardcoded `3`), `MAX_IGNORES` reduced from 8 to 5
- New helpers: `csv_next_tok()` (spectalk.c), `cfg_put_csv()` (user_cmds.c)
- `friend_nicks` declaration changed from `[3]` to `[MAX_FRIENDS]`
- Config keys renamed: `friend1`..`friend3` -> `friends`, new key `ignores`
- `!config` output now includes `ignores=` line

---

## [1.3.2] - 2026-02-25

### Fixed

#### WiFi Connection Check (false positive)
- **Problem**: At startup, `AT+CIFSR` could return a stale cached IP from the ESP8266's flash memory even when not connected to any WiFi network (e.g., after a power outage before the router has restarted)
- SpecTalkZX would report WiFi OK and attempt autoconnect, which then timed out
- **Solution**: Added `AT+CWJAP?` verification after `AT+CIFSR` — confirms actual AP association before setting `STATE_WIFI_OK`

#### Config Parser Hardening (cfg_apply)
- **Problem**: Key fields like `nick`/`nickpass`, `autoconnect`/`autoaway`, and `friend1-3` were accessed by fixed character index (e.g., `key[4]`, `key[6]`) without validating key length
- A malformed or truncated config line could cause accidental writes to wrong settings
- **Solution**: Added `st_strlen(key)` check; `nick` requires exactly 4 chars, `nickpass` requires ≥8, `autoconnect`/`autoaway` requires ≥5, `friendN` requires exactly 7

#### esxDOS File I/O Compilation
- **Problem**: `/save` command referenced `esx_fcreate` and `esx_fwrite` which were missing from ASM and header files, causing compilation failures
- **Solution**: Added `_esx_fcreate` (FA_WRITE | FA_CREATE_AL mode 0x0C) and `_esx_fwrite` (F_WRITE 0x9E) to `spectalk_asm.asm` with proper PUBLIC declarations and extern declarations in `spectalk.h`

#### Font Glyph: Capital N
- **Problem**: The capital N was visually ambiguous with W in the 4px-wide 64-column font
- **Solution**: Adopted Π-shape glyph (horizontal bar + two verticals) as suggested by community — reads unambiguously as N in context

#### Status Bar Clock Overlap
- **Problem**: When channel modes were displayed (e.g., `#channel(+Cnst)`), the user count could extend into columns 54-55 where the clock is rendered, causing visual corruption
- **Solution**: Changed `limit_end` from position 56 to 54 — left part of status bar now stops before the clock area

#### `!status` Disconnected State
- **Problem**: After `/quit`, `!status` still showed server info without indicating disconnected state
- **Solution**: `!status` now appends `(off)` next to server info when `connection_state < STATE_TCP_CONNECTED`

---

## [1.3.1] - 2026-02-23 "Persistence"

### Added

#### Configuration Save Command (`/save`)
- **New command**: `/save` (alias `/sv`) writes all current settings to SD card
- Saves to `/SYS/CONFIG/SPECTALK.CFG` (fallback `/SYS/SPECTALK.CFG`)
- Persists all settings: server, port, nick, passwords, theme, toggles, timezone, friends
- Reports bytes written on success
- **New ASM routines**: `esx_fcreate` (file create/truncate), `esx_fwrite` (file write)

#### Autoconnect Toggle (`/autoconnect`)
- **New command**: `/autoconnect` (alias `/ac`) toggles autoconnect on/off at runtime
- Previously only configurable via config file

#### Timezone Command (`/tz`)
- **New command**: `/tz [±N]` to view or set UTC timezone offset (-12 to +12)
- Without arguments: displays current timezone
- Previously only configurable via config file

#### Friends Management (`/friend`)
- **New command**: `/friend [nick]` to manage the friends list at runtime
- Without arguments: lists current friends
- With nick: toggles add/remove (max 3 friends)
- Previously only configurable via config file

#### NickServ Password Persistence
- `/id <password>` now saves the password to memory for later `/save`
- Inline C copy replaces `st_copy_n` ASM call, fixing truncation bug from stdcall convention
- `!config` now displays `nickpass=` status (set/not set)

### Fixed

#### Nick Validation Before Connect
- **Problem**: `/connect` without a nick set would establish TCP connection, then fail during IRC registration
- Connection entered half-connected state requiring restart
- **Solution**: Check `irc_nick[0]` before TCP connection attempt; shows "Set /nick first" if empty

### Optimized

#### `cmd_save` Size Reduction (835 → 508 bytes, -39%)
- Centralized `cfg_kv()` helper: writes key=value+CRLF in one function call
- Digit-as-pointer trick for single-digit numeric values (0-9)
- Eliminated `u16_to_str` function — reuses existing `fast_u8_to_str` and `u16_to_dec`
- Shortened feedback strings ("Saved (N bytes)" instead of "Config saved (N bytes)")
- Reduced `cmd_friend` and `cmd_tz` feedback messages

### Technical Notes
- New ASM public symbols: `_esx_fcreate`, `_esx_fwrite`, `_call_trampoline`
- New extern declarations in `spectalk.h`: `autoconnect`, `show_timestamps`, esxDOS functions
- esxDOS declarations consolidated in `spectalk.h` (removed duplicates from `spectalk.c`)
- `CMD_HELP_BASE` updated from 49 to 55 (6 new pool entries: save, sv, autoconnect, ac, tz, friend)
- `USER_COMMANDS` expanded from 33 to 37 entries
- Fixed `fast_u8_to_str` missing null terminator when used with `cfg_put`/`cfg_kv`
- Binary size: 37,959 bytes (+1,645 from v1.3.0 base, with 3,676 bytes stack headroom)

---

## [1.3.0] - 2026-02-22 "Social Network"

### Added

#### Auto-Connect System
- **New config option**: `autoconnect=1` connects automatically to configured server on startup
- Combined with `nickpass=`, provides fully automated login experience
- Eliminates manual `/server` command for regular users
- Cursor properly restored if autoconnect fails (e.g., no WiFi, no server configured)

#### NickServ Auto-Identification
- **New config option**: `nickpass=` for automatic NickServ identification after connect
- Separate from server password (`pass=`) for clarity
- Sends `IDENTIFY <password>` to NickServ immediately after registration completes

#### WiFi Connectivity Check
- `esp_init()` now verifies actual WiFi connection via `AT+CIFSR` after ESP8266 responds
- Detects "module powered but no WiFi" scenario (e.g., router not yet booted)
- Sets `STATE_DISCONNECTED` instead of false `STATE_WIFI_OK` when no IP assigned

#### Friends Detection System
- **New config options**: `friend1=`, `friend2=`, `friend3=` to define up to 3 friend nicks
- Automatic ISON polling every 30 seconds to detect online friends
- Status bar indicator shows when friends are online
- WHOIS information displayed when friend comes online

#### Timestamps (3 modes)
- **New command**: `/timestamps` (alias `/ts`) cycles through display modes
- **New config option**: `timestamps=N` — 0=off, 1=always on, 2=smart (shows on minute change)
- Smart mode reduces visual clutter while preserving time awareness
- Timestamps shown consistently on all message types when enabled

#### SNTP Time Sync
- Automatic time synchronization via SNTP after WiFi connection and after reconnection
- Retry logic for SNTP failures — retries 3 times before giving up
- Timezone support via `tz=` config option (UTC -12 to +12)

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

#### IRCv3 Extended JOIN Compatibility
- **Problem**: Libera.Chat sends IRCv3 extended-join (`JOIN #channel accountname :Real Name`) where `pkt_txt` contains the real name instead of the channel
- The old code `(*pkt_txt) ? pkt_txt : pkt_par` selected the real name as channel → failed to create window
- **Solution**: Always use `pkt_par` when it starts with `#` or `&`; fall back to `pkt_txt` only for trailing-only format

#### SNTP Time Validation
- **Problem**: Corrupted SNTP responses (e.g., during WiFi dropout) could produce invalid times like "10:60"
- **Solution**: Added range validation — rejects hour ≥ 24, minute ≥ 60, second ≥ 60

#### NAMES User Count Preservation
- **Problem**: `/names` command could overwrite the accurate user count (obtained at JOIN) with an incomplete count if the NAMES response was truncated
- **Solution**: Added `names_count_acc` accumulator — only updates `chan_user_count` when ENDOFNAMES (366) confirms complete list

#### Status Bar Mode/Network Priority
- **Problem**: Channel modes `(+Cnst)` consumed space needed for user count, overlapping with other elements
- **Solution**: Always drop modes first, then network name if still exceeding available space

#### Reentrant Disconnection Bug
- Fixed race condition where multiple disconnect triggers could corrupt state
- Added `disconnecting_in_progress` guard flag
- Prevents crashes during network instability

#### AT Command Leakage
- **Problem**: AT command responses could leak into IRC transparent mode under certain timing conditions
- **Solution**: Improved ESP8266 state machine handling to properly separate AT and transparent modes

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

#### Cursor Visibility
- Fixed cursor not restored after `/quit` command
- Fixed cursor not restored when autoconnect fails at startup

### Optimized

#### CRT Optimization (-startup=31)
- Switched from default CRT to `-startup=31` (no stdio FILE structures)
- Saves **3,062 bytes** — single largest optimization in the project history
- All existing functionality preserved (SpecTalkZX uses direct UART, not stdio)

#### Deep Code Optimization (12 passes)
- **C refactoring**: Attribute setter helpers (`set_attr_sys/err/priv/chan/nick/join`) — 335 bytes saved
- **Cursor wrappers**: Centralized cursor show/hide — ~100 bytes saved
- **ASM rewrites**: `sntp_process_response` (91 bytes), `read_key` (70 bytes), `find_query` (35 bytes)
- **String deduplication**: Consolidated 20+ duplicate string literals into shared constants — 49+ bytes
- **Library function elimination**: Replaced `strstr`, `strncmp`, `strcmp` with specialized inline checks
- **ASM hot paths**: Converted `print_char64`, `print_str64`, `irc_param` to optimized assembly
- **SDCC codegen improvements**: Applied `__z88dk_fastcall` and `__z88dk_callee` conventions throughout
- **Table compression**: Optimized command dispatch tables and help text pool
- **Call fusion**: Combined consecutive `main_puts` calls with `main_puts2`/`main_puts3` helpers
- **Status bar simplification**: Reduced branching in mode/network display logic — 36 bytes

#### Memory Footprint
- Reduced command history buffer from 128 to 96 bytes per entry (128 bytes BSS saved)
- Eliminated unused static buffers
- Consolidated toggle command implementations
- Total binary: 40,123 → 36,314 bytes (**3,809 bytes saved, 9.49%**)

### Changed

#### Configuration File Format
- `!config` display now shows all new options with formatted output
- Config parser extended for new keywords
- Maintains backward compatibility with v1.2 config files

### Technical Notes
- New ASM routines: `_utf8_to_ascii`, `_st_strchr`, `_sntp_process_response`, `_read_key`, `_find_query`
- New attribute setter ASM helpers: `_set_attr_sys`, `_set_attr_err`, `_set_attr_priv`, `_set_attr_chan`, `_set_attr_nick`, `_set_attr_join`
- New variable: `names_count_acc` (NAMES accumulator)
- New handlers: friends ISON/WHOIS processing
- Config parser extended for: `autoconnect`, `nickpass`, `friend1-3`, `timestamps`, `tz`

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
