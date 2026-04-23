# Skill Progressive Disclosure And Evals

If a Codex skill keeps producing shallow audits or collapses to the first obvious optimization class, do not keep inflating the main `SKILL.md`; keep it as a thin orchestrator and move the detail into references, scripts and a regression corpus.

## Rule
- Keep `SKILL.md` focused on trigger, workflow, evidence thresholds, report contract and escalation rules.
- Move mode-specific heuristics and long checklists into one-level-deep `references/*.md` files so the agent only loads the material needed for the chosen mode.
- Add deterministic `scripts/` for evidence collection the model routinely skips or miscomputes, such as build-flag preflight, `.map` summaries, ABI boundary inventories, library-drag scans or duplicate-block reports.
- Run `quick_validate.py` after edits and keep frontmatter within the supported schema so the skill stays parseable by the current tooling.
- Maintain a small forward-test corpus from real project bugs and proven shrink wins; trust a skill upgrade only after it rediscovers those cases under fresh context.
- For multi-mode skills, require explicit category coverage or an evidence-backed `none found` so the agent cannot stop after the easiest class of findings.

## Applied In
- `C:\Users\ignac\.codex\skills\audit-z80`
- `C:\Users\ignac\.codex\skills\shrink-z80`
