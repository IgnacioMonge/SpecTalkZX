; =============================================================================
; SpecTalkZX custom CRT - minimal startup without stdio/terminal drivers
; Saves ~2500 bytes by not linking stdin/stdout/stderr and their driver chain
; =============================================================================

SECTION CODE

PUBLIC __Start
PUBLIC __Exit
PUBLIC __sp_or_ret

EXTERN _main
EXTERN __bss_compiler_head
EXTERN __BSS_UNINITIALIZED_head

__Start:
    di
    ; Save return address / SP for potential clean exit
    ld (__sp_or_ret), sp

    ; Stack at end of BSS
    ld sp, __BSS_UNINITIALIZED_head

    ; Zero BSS (from bss_compiler_head to BSS_UNINITIALIZED_head)
    ld hl, __bss_compiler_head
    ld de, __bss_compiler_head + 1
    ld bc, __BSS_UNINITIALIZED_head - __bss_compiler_head - 1
    ld (hl), 0
    ld a, b
    or c
    jr z, _skip_bss     ; BSS is 0 or 1 byte
    ldir
_skip_bss:

    ; Enable interrupts (IM 1 - standard ZX Spectrum)
    ei

    ; Call main
    call _main

__Exit:
    ; Restore SP and return to BASIC (or halt)
    ld sp, (__sp_or_ret)
    ei
    ret

SECTION BSS_UNINITIALIZED
__sp_or_ret:
    defw 0
