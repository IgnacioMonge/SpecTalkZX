# Autojoin Handshake Gate

Restored-session autojoin must not fire on `001 RPL_WELCOME`. That numeric only confirms registration; most networks still send MOTD, service notices, NickServ prompts, and other welcome traffic immediately afterwards.

## Rule

- Keep `001` focused on marking `STATE_IRC_READY`, updating the confirmed nick, and redrawing status.
- Replay the pending `channels=` list at `376 RPL_ENDOFMOTD` or `422 ERR_NOMOTD`.
- If SpecTalkZX has sent an automatic NickServ identify during the same handshake, defer the replay until the identify round-trip is acknowledged by `900 RPL_LOGGEDIN` or by a following recognized service NOTICE.
- Clear `search_pattern[0]` after sending the pending comma-list `JOIN`, because `search_pattern` is shared transient state for search, switcher snapshots, and overlay messages.
- Reset the autojoin defer flags on disconnect before a new connect attempt starts.

## Applies To

- `src/irc_handlers.c` `h_numeric_1()`, `h_motd_done()`, `h_logged_in()`, `h_privmsg_notice()`
- `src/user_cmds.c` `cmd_connect()`
- `src/spectalk.c` `autojoin`, `autojoin_defer_flags`, `force_disconnect()`
