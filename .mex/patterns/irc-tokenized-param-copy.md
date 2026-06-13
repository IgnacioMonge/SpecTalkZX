# IRC Tokenized Param Copy

After `irc_params_ensure()`, `irc_params[]` entries point into `rx_line` tokens that have already been split in place with NUL terminators.

For values inside those params, such as `NETWORK=...` in numeric `005`, do not re-scan for spaces before bounded copy. Use `st_copy_n(dst, token_value, sizeof(dst))` when the destination wants the same truncation-and-terminate contract.

Guardrail: this applies only to tokenized params. Raw `pkt_par`/`pkt_txt` paths may still contain spaces and must keep their own parsing rules.
