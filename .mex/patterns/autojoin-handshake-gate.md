# Autojoin Handshake Gate

Restored-session autojoin must not fire on `001 RPL_WELCOME`. That numeric only confirms registration; most networks still send MOTD, service notices, NickServ prompts, and other welcome traffic immediately afterwards.

## Rule

- Keep `001` focused on marking `STATE_IRC_READY`, updating the confirmed nick, and redrawing status.
- If `autojoin` and `nickserv_pass` are configured, set the identify wait flag at `001`, before MOTD completion can replay JOIN. Do this in both registration paths: the normal parser handler and `cmd_connect()`'s early `001` interception. Waiting until the NickServ prompt arrives is too late on networks where MOTD ends first, and checking `search_pattern` at `001` is unreliable because it is shared transient state.
- Replay the pending `channels=` list at `376 RPL_ENDOFMOTD` or `422 ERR_NOMOTD`.
- Track whether automatic identify was actually sent, not just configured. If `AUTOJOIN_IDENT_WAIT` was armed at `001` but no NickServ-style prompt caused `send_identify()`, clear the wait at MOTD completion so networks without auto-identification do not block autojoin forever.
- If SpecTalkZX has sent automatic NickServ identify during the same handshake, defer the replay until a recognized service/NOTICE success text has been rendered. Do not release the gate on `900 RPL_LOGGEDIN` alone, because some networks deliver it before the visible acceptance message and the pending JOIN then interleaves with that message.
- Clear the autojoin defer flags after sending the pending comma-list `JOIN`. Do not clear `autojoin_channels[]`; it is the persistent loaded `channels=` list and is reused by later config/save paths.
- Reset the autojoin defer flags on disconnect before a new connect attempt starts.

## Applies To

- `src/irc_handlers.c` `h_numeric_1()`, `h_motd_done()`, `h_logged_in()`, `h_privmsg_notice()`, `is_ident_success_notice()`
- `src/user_cmds.c` `cmd_connect()` early registration loop
- `include/spectalk.h` `AUTOJOIN_MOTD_DONE`, `AUTOJOIN_IDENT_WAIT`, `AUTOJOIN_IDENT_SENT`
- `src/spectalk.c` `autojoin`, `autojoin_defer_flags`, `force_disconnect()`
