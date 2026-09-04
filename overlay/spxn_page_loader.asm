;; Spectranext init-only overlay loader. Runs after the atlas header in the
;; first 320 bytes of ring_buffer and stages reads through the 512-byte slice.

SECTION code_user

PUBLIC _spxn_page_bootstrap

EXTERN _ring_buffer
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _esx_fseek_set
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _input_cache_invalidate
EXTERN _spxn_overlay_page
EXTERN _spxn_page_ready
EXTERN _spxn_overlay_len

OVL_ATLAS_HEADER_LEN EQU 64
OVL_CODE_BASE        EQU $2000
SPXN_PAGEIN          EQU $3FF9
SPXN_PAGEOUT         EQU $007C
SPXN_SET_PAGE_B      EQU $3E36
SPXN_RESERVE_PAGE    EQU $3E9F
SPXN_OVERLAY_OWNER   EQU $53
SPXN_STAGE_SIZE      EQU 512
SPXN_STAGE_BASE      EQU _ring_buffer + SPXN_STAGE_SIZE

; A=atlas id. Return carry clear on success, set on validation/I/O failure.
_spxn_page_bootstrap:
    ld (boot_id), a
    call boot_select
    jp c, boot_fail_close
    call _esx_fseek_set
    ld a, l
    or a
    jp z, boot_fail_close

    ld a, (_spxn_page_ready)
    or a
    jr nz, boot_copy_setup
    di
    call SPXN_PAGEIN
    ld a, SPXN_OVERLAY_OWNER
    call SPXN_RESERVE_PAGE
    jr c, boot_reserve_fail
    ld (_spxn_overlay_page), a
    call SPXN_PAGEOUT
    ld a, 1
    ld (_spxn_page_ready), a

boot_copy_setup:
    ld hl, (_spxn_overlay_len)
    ld (boot_remaining), hl
    ld hl, OVL_CODE_BASE
    ld (boot_copy_dst), hl

boot_read_loop:
    ld hl, (boot_remaining)
    ld a, h
    or l
    jr z, boot_success
    ld de, SPXN_STAGE_SIZE
    or a
    sbc hl, de
    jr c, boot_short_chunk
    ld bc, SPXN_STAGE_SIZE
    jr boot_chunk_ready
boot_short_chunk:
    add hl, de
    ld b, h
    ld c, l
boot_chunk_ready:
    ld (_esx_count), bc
    ld hl, SPXN_STAGE_BASE
    ld (_esx_buf), hl
    call _esx_fread
    ld hl, (_esx_result)
    ld de, (_esx_count)
    or a
    sbc hl, de
    jr nz, boot_fail_close

    di
    call SPXN_PAGEIN
    ld a, (_spxn_overlay_page)
    call SPXN_SET_PAGE_B
    ld hl, SPXN_STAGE_BASE
    ld de, (boot_copy_dst)
    ld bc, (_esx_count)
    ldir
    ld (boot_copy_dst), de
    call SPXN_PAGEOUT

    ld hl, (boot_remaining)
    ld de, (_esx_count)
    or a
    sbc hl, de
    ld (boot_remaining), hl
    jr boot_read_loop

boot_reserve_fail:
    call SPXN_PAGEOUT
boot_fail_close:
    scf
    jr boot_close
boot_success:
    or a
boot_close:
    push af
    call _esx_fclose
    call _input_cache_invalidate
    pop af
    ret

; Header format: STOA/version/count/header_len, then <offset,size> words.
boot_select:
    ld hl, _ring_buffer
    ld a, (hl)
    cp 'S'
    jr nz, boot_bad
    inc hl
    ld a, (hl)
    cp 'T'
    jr nz, boot_bad
    inc hl
    ld a, (hl)
    cp 'O'
    jr nz, boot_bad
    inc hl
    ld a, (hl)
    cp 'A'
    jr nz, boot_bad
    inc hl
    ld a, (hl)
    cp 1
    jr nz, boot_bad
    inc hl
    ld a, (boot_id)
    cp (hl)
    jr nc, boot_bad
    add a, a
    add a, a
    add a, 8
    ld e, a
    ld d, 0
    ld hl, _ring_buffer
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld a, b
    or c
    jr z, boot_bad
    ld a, b
    cp 16
    jr c, boot_size_ok
    jr nz, boot_bad
    ld a, c
    or a
    jr nz, boot_bad
boot_size_ok:
    ld (_spxn_overlay_len), bc
    ex de, hl
    or a
    ret
boot_bad:
    scf
    ret

boot_id:
    DEFB 0
boot_copy_dst:
    DEFW 0
boot_remaining:
    DEFW 0
