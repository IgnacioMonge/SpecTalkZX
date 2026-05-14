# IRC Parser Space-Split Fastpath

## Superseded

This pattern described the old space-split parser and repeated IRCv3 line-head cleanup. It is intentionally superseded by `.mex/patterns/irc-parser-canonical.md`.

Do not revive the old model where `pkt_par` sometimes meant the whole raw param tail and handlers repaired IRCv3 shapes locally. Current parser policy is one canonical parse before dispatch:

- one optional transport marker (`>` or contiguous `>>`),
- one optional IRCv3 `@tags` block,
- optional prefix with nick isolation,
- `pkt_par` as first param,
- `pkt_rest` as remaining middle params,
- `pkt_txt` as trailing text only,
- lazy append tokenization from `pkt_rest`.

Historical measurements from this file still apply only as warnings: the C `switch(cmd_id)` dispatcher grew sharply, inline 3-digit numeric parsing grew sharply, and `irc_params_ensure()` measured smaller as a normal C helper than as an inline/naked ASM rewrite.

## Applies To

- `src\irc_handlers.c`
- `asm\spectalk_asm\40_text_numeric_screen.asm`