# Resident Parser State Machine Shrink

## Context
Resident ASM parsers often spend more bytes on skip paths, common exits, and temporary pointer saves than on the actual checks. This showed up in `70_input_lookup.asm` in the UTF-8 normalizer, SNTP response scanner, key repeat state machine, nav history helper, and query lookup.

## Pattern
- Chain fixed-length skip tails from longest to shortest when they share the same truncation and replacement behavior. A 4-byte UTF-8 skip can do its first checked increment and then fall into the existing `skip3 -> skip2 -> skip1` chain.
- Move common exit labels near the densest branch cluster when that unlocks `jr` without changing fall-through behavior. Recheck every caller after moving the label because the win depends on final layout.
- When a scanner needs to inspect multiple offsets from the same base pointer, keep the logical index in an 8-bit register and use `DE` as the temporary pointer instead of `push hl` / `pop hl` around every probe.
- Use low-byte table or array indexing only with a documented bound and a map-confirmed no-carry range, then let `make` prove that no local jump range broke.

## Guardrails
- Do not apply the low-byte form to buffers that can move across a page or indexes that are only "normally" small.
- Keep retry/cancel sentinel behavior unchanged; parser shrink must not collapse distinct "retry later" and "done" exits unless the state update is identical.
- Treat every `jp` to `jr` conversion as build-confirmed, not inferred, because nearby size changes can move the range both ways.
