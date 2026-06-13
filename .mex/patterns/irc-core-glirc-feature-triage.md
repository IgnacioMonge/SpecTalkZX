# irc-core/glirc Feature Triage

Use this when reviewing `glguy/irc-core` or `glirc` for SpecTalkZX ideas.

## Keep The Split Clear

- `irc-core` is protocol structure: raw parser/printer, CAP, commands, MODE parsing, rate limiting, identifiers.
- `glirc` is a full console client: retained IRC model, dynamic views, searchable lists, plugins, split windows, URL opener, macros, DCC, and broad auth support.
- For SpecTalkZX, import ideas only unless a small C/Z80 implementation can be measured. The Haskell architecture does not fit the resident/overlay budget.

## Direct Candidates

- STATUSMSG routing: normalize targets like `@#chan` or `+#chan` before current `target[0] == '#'` routing in `h_privmsg_notice()`. This avoids opening a bogus PM query for status-targeted channel messages and CTCP ACTIONs. Keep it local, no CAP negotiation needed. Current implementation handles `@/+/%/~` before `#chan`; extending this to status-targeted `&chan` needs a separate check because `&` is also a local-channel prefix.
- Outgoing flood guard: if bookmarks/autojoin/macros start sending bursts, use a tiny frame-based token bucket near IRC send helpers. Do not block render/RX paths for long waits.
- MODE splitting: if channel admin output needs accuracy, add a narrow splitter for visible user/channel modes instead of a generic mode engine.
- SASL PLAIN: useful for modern networks, but only as a measured handshake prototype. Do not port SCRAM/ECDSA/CERTFP machinery.

## Parked

- Full dynamic message model, `/grep`, detailed view, searchable retained views, split-screen, URL opener, `/exec`, Lua/extensions, DCC, and full IRCv3 tag storage are not resident-fit.
- Command macros should stay in the existing bookmark/session-restore lane, replayed through existing commands and config, not as a general scripting engine.
- Full CAP negotiation is not useful unless each requested capability has a handler and a visible SpecTalkZX payoff.

## Current Local Anchors

- `src/irc_handlers.c` `h_cap()` currently answers `CAP END` to `CAP LS`.
- `src/irc_handlers.c` `h_privmsg_notice()` currently routes channel traffic by literal `target[0] == '#'`.
- `src/user_cmds.c` command names live in the resident packed command pool, so every new command must justify parser/table bytes.
