# SpecTalkZX Router

## Project State
- Date: 2026-04-22
- Recent task: SAFE + SPECULATIVE shrink round after `/shrink-z80 scan`. Applied MICRO-01 (2× redundant `or a` overlay_loader), extended MICRO-02 (2× `call _rx_pos_reset` in hand-written ASM), REF-05 (CMD_TABLE direct pointer to `irc_check_friends_online`), REF-02 (9 sites `set_attr_err+main_print` → `ui_err`), REF-06 (new `reset_rx_state` ASM helper, 3 sites), REF-16 (`enter_overlay_mode` C helper, 5 sites), DUP-02 (`S_MODE_SP_SCR` BPE-compressed shared const, 2 sites in irc_handlers), DATA-09 (indicator patterns overlap with shared 0x00 boundary bytes, 40B→35B). Net **-63B**.
- Latest committed checkpoint: `27e581b` (`Shrink notifications and join/connect paths`)
- Build status: `make` completes successfully. All overlays fit, BSS guard OK.
- Current verified TAP from `make`: `35943B` on 2026-04-22 (`-63B` vs start of session `36006B`, `-129B` vs `v1.3.7`).
- Current overlay sizes: unchanged (`SPCTLK1.OVL` 1385B, `SPCTLK2.OVL` 1968B, `SPCTLK3.OVL` 1591B, `SPCTLK4.OVL` 1796B).
- Audit highlight: SAFE-only pass recovered 53B without changing behavior. Discarded: DATA-01/03/04/07 (conflict with BPE pipeline or overlay API), ARCH-05 (+4B — boolean cost > dedup), REF-19 (`__z88dk_callee` ABI composition non-trivial).

## Resume Here
- Build green, TAP 35953B. Changes still in working tree (uncommitted).
- Key changes: new `_reset_rx_state` and `enter_overlay_mode` helpers, `ui_err` consolidation, 2× `_rx_pos_reset` reuse in hand-written ASM, CMD_TABLE direct `irc_check_friends_online`, `K_MODE_SP`/`S_MODE_SP_SCR` shared consts, indicator patterns with overlapped storage (shared 0x00 rows).
- Pending hardware test checklist (CLAUDE.md): banner, `/connect`, channel join, send/recv, `!about`/`!config`/`!status`, EDIT switcher, esxDOS, 2-3 min stability.
- Next likely work items if development resumes in 48K mode: real auto-reconnect, more useful WHOIS output, less-limited `/list`, or tiny IRC wrappers like `/invite` and `/notice`.

## Patterns
- [`irc-mode-wrapper.md`](patterns/irc-mode-wrapper.md): simple IRC wrappers should prefer passthrough semantics with only the minimum implicit target logic needed for convenience, but query numerics should still produce a visible reply.
- [`help-text-source.md`](patterns/help-text-source.md): command help that ships inside `SPECTALK.DAT` should come from a tracked text source, not from hand-edited local DAT payloads.
- [`notification-builder-helpers.md`](patterns/notification-builder-helpers.md): deduplicate short notification builders by arity, but only keep the helper if `make` proves a net byte win.
- [`part-notification-replace.md`](patterns/part-notification-replace.md): channel leave notifications must cancel the current footer notification, and any prefix routed through `notify2()` must stay out of BPE unless that builder explicitly expands tokens.
- [`disconnect-confirmation-guard.md`](patterns/disconnect-confirmation-guard.md): commands that tear down an active connection should share the same guarded `y/n` confirmation helper instead of open-coding input loops.
- [`local-artifacts-gitignore.md`](patterns/local-artifacts-gitignore.md): local tooling files, backups and build outputs that are not part of the shipped program should be hidden via `.gitignore`, not left as noisy untracked entries.
- [`overlay-about-keepalive.md`](patterns/overlay-about-keepalive.md): if `ABOUT` reuses `ring_buffer`, keepalive must continue without full IRC parsing; accept `PONG` in the lightweight scanner and close the overlay before surfacing timeout UI.
- [`overlay-exit-rx-reset.md`](patterns/overlay-exit-rx-reset.md): overlay exit paths must discard parser/ring state if `ring_buffer` has been reused as overlay code storage.
- [`overlay-resident-string-dedup.md`](patterns/overlay-resident-string-dedup.md): overlays should reuse resident exported string constants when the text is identical, to recover overlay headroom without changing UI or growing TAP.
