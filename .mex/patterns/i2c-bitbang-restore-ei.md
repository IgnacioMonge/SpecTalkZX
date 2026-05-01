# I2C bit-bang DI/EI balance

Software-timed I2C protocols on Z80 must disable interrupts to keep clock/data line transitions deterministic. The DI block is fine inside the protocol — but the matching `ei` must run before returning to the C caller, or the rest of the program (text rendering, overlay return, status bar updates) silently runs with interrupts disabled until the next `frame_wait`.

## Symptoms when EI is missing

- FRAMES counter at $5C78 stops advancing during the affected window.
- Keyboard scan misses one ROM IM1 tick.
- Any code between the DI and the next `ei; halt; di` in `frame_wait` that depends on interrupt state malfunctions silently — there is no crash, just degraded timing.
- Symptom is fragile to refactors: today's harmless DI leak becomes tomorrow's bug as soon as someone adds an interrupt-dependent step (animation tick, lag counter sample, etc.) into the DI window.

## Rule

If you `di` at the start of an I2C/SPI/PS2/serial bit-bang block, you must `ei` after the last bus transition and before any `ret` or `jp` that exits the bit-bang block.

In `overlay/rtc_seed_ovl.asm` the canonical placement is:

```asm
    call i2c_stop
    ei                        ; restore interrupts after I2C bit-bang
    jp pcf_validate_commit
```

Validation/parsing of the read bytes runs with interrupts enabled — that work doesn't touch the bus and doesn't need DI. Earlier-failure paths (e.g. NACK detection, mask retries) must also EI before returning.

## Don't

- Don't rely on the caller's eventual `frame_wait` to restore interrupts. The DI window then includes overlay return code, C cleanup, and any rendering done before re-entering the main loop.
- Don't `ei` *before* `i2c_stop`: the last STOP transition still needs deterministic timing.
- Don't move `ei` into the C caller via inline asm unless the alternative is a measurable resident byte regression — the symmetry inside the ASM is easier to keep correct under future refactors.
