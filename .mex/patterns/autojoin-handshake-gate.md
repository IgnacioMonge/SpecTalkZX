# Autojoin Handshake Gate

Restored-session autojoin must not fire on `001 RPL_WELCOME`. That numeric only confirms registration; most networks still send MOTD, service notices, NickServ prompts, and other welcome traffic immediately afterwards.

## Rule

- Keep `001` focused on marking `STATE_IRC_READY`, updating the confirmed nick, and redrawing status.
- If `autojoin` and `nickserv_pass` are configured, set the identify wait flag at `001`, before MOTD completion can replay JOIN. Do this in both registration paths: the normal parser handler and `cmd_connect()`'s early `001` interception. Waiting until the NickServ prompt arrives is too late on networks where MOTD ends first, and checking `search_pattern` at `001` is unreliable because it is shared transient state.
- In `cmd_connect()`'s early registration loop, do not call `reset_rx_state()` after a successful `001`. The server is usually already sending `002/003/004/005/MOTD`; clearing ring/RX state at that boundary can cut a line in flight and later leak a fragment through `h_default_cmd()`. Preserve pending bytes for the normal parser; reset RX only on abort/failure.
- Replay the pending `channels=` list at `376 RPL_ENDOFMOTD` or `422 ERR_NOMOTD`.
- Track whether automatic identify was actually sent, not just configured. If `AUTOJOIN_IDENT_WAIT` was armed at `001` but no NickServ-style prompt caused `send_identify()` before MOTD completion, start a short grace timer instead of clearing the wait immediately. Some networks deliver the NickServ prompt just after `376`/`422`; clearing the wait at that point lets autojoin interleave channel JOIN traffic before login acceptance.
- If the grace timer expires with no identify command sent, clear `AUTOJOIN_IDENT_WAIT` and replay autojoin so networks without auto-identification do not block forever.
- If SpecTalkZX has sent automatic NickServ identify during the same handshake, defer the replay until a recognized service/NOTICE success text has been rendered. Do not release the gate on `900 RPL_LOGGEDIN` alone, because some networks deliver it before the visible acceptance message and the pending JOIN then interleaves with that message.
- Treat a self `NICK` change while `AUTOJOIN_IDENT_SENT` is active as an identify-complete event after rendering/updating the nick, because some services restore the registered nick via a `NICK` message instead of only using a service NOTICE.
- Clear the autojoin defer flags after sending the pending comma-list `JOIN`. Do not clear `autojoin_channels[]`; it is the persistent loaded `channels=` list and is reused by later config/save paths.
- Reset the autojoin defer flags on disconnect before a new connect attempt starts.

## Applies To

- `src/irc_handlers.c` `h_nick()`, `h_numeric_1()`, `h_motd_done()`, `h_logged_in()`, `h_privmsg_notice()`, `is_ident_success_notice()`
- `src/user_cmds.c` `cmd_connect()` early registration loop
- `include/spectalk.h` `AUTOJOIN_MOTD_DONE`, `AUTOJOIN_IDENT_WAIT`, `AUTOJOIN_IDENT_SENT`
- `src/spectalk.c` `autojoin`, `autojoin_defer_flags`, `autojoin_ident_grace`, main-loop frame timer, `force_disconnect()`
