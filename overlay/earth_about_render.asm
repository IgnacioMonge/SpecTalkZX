;; earth_about_render.asm -- SpecTalkZX !about Earth renderer.
;; Large bitmap/logo/delta payloads live in SPECTALK.DAT. This overlay keeps
;; only renderer code, compact buffers, and span metadata.

SECTION code_user

PUBLIC _earth_draw_frame
PUBLIC _earth_apply_frame_delta
PUBLIC _earth_apply_attr_delta
PUBLIC _earth_read_logo
PUBLIC _earth_ikkle_draw
PUBLIC _earth_draw_separator
PUBLIC _earth_seek
PUBLIC _earth_frame_buffer
PUBLIC _earth_attr_buffer

EXTERN _esx_fread
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _overlay_slot
EXTERN _ikkle_packed
EXTERN _theme_attrs
EXTERN _current_theme
EXTERN ___sdcc_enter_ix

DEFC EARTH_FRAME0_SIZE = 587
DEFC EARTH_ATTR0_SIZE  = 44
DEFC EARTH_LOGO_PRELOAD_ROWS = EARTH_LOGO_H - 1
DEFC EARTH_LOGO_PRELOAD_SIZE = EARTH_LOGO_PRELOAD_ROWS * EARTH_LOGO_W_BYTES
DEFC TA_MAIN_BG   = 5
DEFC TA_MSG_NICK  = 11
DEFC TA_MSG_TOPIC = 13

_earth_seek:
        push ix
        ld e,l
        ld d,h
        ld bc,0
        ld a,(_esx_handle)
        ld ix,0
        rst 8
        defb $9F                  ; F_SEEK, mode=SET in IXL
        pop ix
        ld hl,1
        ret nc
        dec hl
        ret

_earth_draw_frame:
        push ix
        ld hl,_earth_frame_buffer
        ld ix,earth_screen_spans
        call earth_draw_spans_packed

        ld hl,_earth_attr_buffer
        ld ix,earth_attr_spans
        call earth_draw_attr_spans_packed
        pop ix
        ret

_earth_apply_frame_delta:
        ld de,_earth_frame_buffer
        jr earth_apply_delta

_earth_apply_attr_delta:
        ld de,_earth_attr_buffer
        jr earth_apply_delta

earth_apply_delta:
        ld a,(hl)
        inc hl
        or a
        ret z
        jp m,earth_delta_copy
        ld c,a
        ld b,0
        ex de,hl
        add hl,bc
        ex de,hl
        jr earth_apply_delta

earth_delta_copy:
        and $7F
        inc a
        ld c,a
        ld b,0
        ldir
        jr earth_apply_delta

_earth_read_logo:
        ld hl,_overlay_slot
        ld (_esx_buf),hl
        ld hl,EARTH_LOGO_PRELOAD_SIZE
        ld (_esx_count),hl
        call _esx_fread
        ld hl,(_esx_result)
        ld de,EARTH_LOGO_PRELOAD_SIZE
        or a
        sbc hl,de
        jr nz,earth_logo_fail

        ld hl,_overlay_slot
        ld de,EARTH_LOGO_SCREEN_ADDR
        ld b,EARTH_LOGO_PRELOAD_ROWS

earth_logo_copy_row_loop:
        push bc
        push de
        ld bc,EARTH_LOGO_W_BYTES
        ldir
        pop de
        call earth_screen_down
        pop bc
        djnz earth_logo_copy_row_loop

        ld (_esx_buf),de
        ld hl,EARTH_LOGO_W_BYTES
        ld (_esx_count),hl
        call _esx_fread
        ld hl,(_esx_result)
        ld a,h
        or a
        jr nz,earth_logo_fail
        ld a,l
        cp EARTH_LOGO_W_BYTES
        jr nz,earth_logo_fail

        ld de,EARTH_LOGO_ATTR_ADDR
        ld a,EARTH_LOGO_ATTR_H

earth_logo_attr_row_loop:
        push af
        ld b,EARTH_LOGO_W_BYTES
        call earth_logo_attr_value

earth_logo_attr_byte_loop:
        ld (de),a
        inc de
        djnz earth_logo_attr_byte_loop
        ld a,32-EARTH_LOGO_W_BYTES
        add a,e
        ld e,a
        jr nc,earth_logo_attr_next
        inc d

earth_logo_attr_next:
        pop af
        dec a
        jr nz,earth_logo_attr_row_loop
        ld hl,1
        ret

earth_logo_fail:
        ld hl,0
        ret

earth_logo_attr_value:
        ld a,(_current_theme)
        cp 1
        ld a,$47
        ret z
        ld a,(_theme_attrs + TA_MSG_NICK)
        ret

_earth_draw_separator:
        ld hl,$4040
        ld b,32
        ld a,$FF
earth_sep_px_loop:
        ld (hl),a
        inc l
        djnz earth_sep_px_loop
        ld hl,$5840
        ld b,32
        ld a,(_theme_attrs + TA_MSG_TOPIC)
earth_sep_attr_loop:
        ld (hl),a
        inc l
        djnz earth_sep_attr_loop
        ret

earth_screen_down:
        inc d
        ld a,d
        and 7
        ret nz
        ld a,e
        add a,32
        ld e,a
        ret c
        ld a,d
        sub 8
        ld d,a
        ret

earth_draw_spans_packed:
        ld e,(ix+0)
        ld d,(ix+1)
        ld a,(ix+2)
        inc ix
        inc ix
        inc ix

earth_draw_first_span:
        ld c,a
        ld b,0
        push de
        ldir
        pop de

earth_draw_span_loop:
        ld a,(ix+0)
        inc ix
        or a
        ret z
        ld c,a

        call earth_screen_down

        ld a,c
        and $F0
        jr z,earth_bitmap_dx_done
        cp $10
        jr z,earth_bitmap_dx_inc
        dec de
        jr earth_bitmap_dx_done

earth_bitmap_dx_inc:
        inc de

earth_bitmap_dx_done:
        ld a,c
        and $0F
        jr earth_draw_first_span

earth_draw_attr_spans_packed:
        ld e,(ix+0)
        ld d,(ix+1)
        ld a,(ix+2)
        inc ix
        inc ix
        inc ix
        ld b,a
        push de
        call earth_unpack_attr_span
        pop de

earth_attr_span_loop:
        ld a,(ix+0)
        inc ix
        or a
        ret z
        ld c,a

        ld a,e
        add a,32
        ld e,a
        jr nc,earth_attr_apply_dx
        inc d

earth_attr_apply_dx:
        ld a,c
        and $F0
        jr z,earth_attr_dx_done
        cp $10
        jr z,earth_attr_dx_inc
        dec de
        jr earth_attr_dx_done

earth_attr_dx_inc:
        inc de

earth_attr_dx_done:
        ld a,c
        and $0F
        ld b,a
        push de
        call earth_unpack_attr_span
        pop de
        jr earth_attr_span_loop

earth_unpack_attr_span:
        ld a,(hl)
        inc hl
        ld c,a
        rrca
        rrca
        rrca
        rrca
        and $0F
        cp 8
        jr c,earth_attr_high_no_bright
        add a,$38

earth_attr_high_no_bright:
        push bc
        call earth_theme_attr
        ld (de),a
        pop bc
        inc de
        dec b
        ret z
        ld a,c
        and $0F
        cp 8
        jr c,earth_attr_low_no_bright
        add a,$38

earth_attr_low_no_bright:
        push bc
        call earth_theme_attr
        ld (de),a
        pop bc
        inc de
        djnz earth_unpack_attr_span
        ret

earth_theme_attr:
        ld c,a
        ld a,(_theme_attrs + TA_MAIN_BG)
        and $38
        ld b,a
        ld a,(_current_theme)
        cp 2
        ld a,c
        jr nz,earth_theme_not_terminal
        and $40
        or b
        or 4
        ret
earth_theme_not_terminal:
        and $47
        or b
        ret

;; void earth_ikkle_draw(uint8_t row, uint8_t col, const char *str, uint8_t attr)
;; cdecl stack: [IX+4]=row, [IX+5]=col, [IX+6,7]=str, [IX+8]=attr
_earth_ikkle_draw:
        call ___sdcc_enter_ix
        ld l,(ix+6)
        ld h,(ix+7)
        ld c,(ix+5)

earth_ikkle_char_loop:
        ld a,(hl)
        or a
        jp z,earth_ikkle_done
        push hl

        ld a,c
        srl a
        ld b,a                     ; physical byte column
        push bc
        ld a,(hl)
        cp 33
        jr c,earth_ikkle_set_attr
        cp 128
        jr nc,earth_ikkle_set_attr
        cp 96
        jr c,earth_ikkle_no_fold
        sub 32
earth_ikkle_no_fold:
        sub 32
        add a,a
        ld e,a
        ld d,0
        ld hl,_ikkle_packed
        add hl,de
        ld d,(hl)
        inc hl
        ld e,(hl)

        ld a,(ix+4)
        call earth_screen_base
        inc h
        inc h
        ld a,b
        add a,l
        ld l,a

        bit 0,c
        jr nz,earth_ikkle_odd_col

        ld a,d
        and $F0
        ld (hl),a
        ld a,d
        and $0F
        rlca
        rlca
        rlca
        rlca
        inc h
        ld (hl),a
        ld a,e
        and $F0
        inc h
        ld (hl),a
        ld a,e
        and $0F
        rlca
        rlca
        rlca
        rlca
        inc h
        ld (hl),a
        jr earth_ikkle_set_attr

earth_ikkle_odd_col:
        ld a,d
        rrca
        rrca
        rrca
        rrca
        and $0F
        or (hl)
        ld (hl),a
        ld a,d
        and $0F
        inc h
        or (hl)
        ld (hl),a
        ld a,e
        rrca
        rrca
        rrca
        rrca
        and $0F
        inc h
        or (hl)
        ld (hl),a
        ld a,e
        and $0F
        inc h
        or (hl)
        ld (hl),a

earth_ikkle_set_attr:
        pop bc
        ld a,(ix+4)
        call earth_attr_base
        ld a,b
        add a,l
        ld l,a
        ld a,(ix+8)
        ld (hl),a

        pop hl
        inc hl
        inc c
        jp earth_ikkle_char_loop

earth_ikkle_done:
        pop ix
        ret

earth_screen_base:
        ld h,a
        and $07
        rrca
        rrca
        rrca
        ld l,a
        ld a,h
        and $18
        or $40
        ld h,a
        ret

earth_attr_base:
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,$5800
        add hl,de
        ret

        INCLUDE "earth_overlay_spans.asm"
        INCLUDE "earth_logo.asm"

SECTION bss_user
_earth_frame_buffer:
        DS EARTH_FRAME0_SIZE
_earth_attr_buffer:
        DS EARTH_ATTR0_SIZE
