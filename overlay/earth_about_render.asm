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
PUBLIC _about_render_ovl
PUBLIC _about_close_ovl
PUBLIC _about_packet_slot

EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _overlay_slot
EXTERN _ikkle_packed
EXTERN _theme_attrs
EXTERN _current_theme
EXTERN _K_DAT
EXTERN _clear_zone
EXTERN _rb_head
EXTERN _rb_tail
EXTERN _rx_pos
EXTERN _rx_last_len
EXTERN _rx_overflow
EXTERN _earth_ready
EXTERN _frame_idx

DEFC EARTH_FRAME0_OFFSET = 969
DEFC EARTH_FRAME0_SIZE = 587
DEFC EARTH_ATTR0_SIZE  = 44
DEFC EARTH_ATTR0_OFFSET = 1556
DEFC EARTH_DELTA_OFFSET = 2560
DEFC EARTH_LOGO_PRELOAD_ROWS = EARTH_LOGO_H - 1
DEFC EARTH_LOGO_PRELOAD_SIZE = EARTH_LOGO_PRELOAD_ROWS * EARTH_LOGO_W_BYTES
DEFC TA_MSG_CHAN  = 2
DEFC TA_MAIN_BG   = 5
DEFC TA_MSG_SYS   = 9
DEFC TA_MSG_NICK  = 11
DEFC TA_MSG_TOPIC = 13

_about_close_ovl:
        ld a,(_esx_handle)
        or a
        jr z,about_close_ready
        call _esx_fclose
        xor a
        ld (_esx_handle),a
about_close_ready:
        xor a
        ld (_earth_ready),a
        ret

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

;; LUT-based unpack: 16-byte table built once per about_render_ovl entry.
;; Eliminates per-cell theme_attr call (~40-50T saved per attr cell).
;; SMC sites patched by _build_theme_attr_lut with low/high of LUT base.
;; AUDIT-L01 FIX: `adc a, 0` propagates carry from `add a, nibble` into H,
;; so a LUT base whose low byte > $F0 still resolves correctly.
earth_unpack_attr_span:
        ld a,(hl)
        inc hl
        push hl                          ; save source ptr (HL repurposed for LUT)
        ld c,a                           ; save raw byte for low nibble pass
        rrca
        rrca
        rrca
        rrca
        and $0F                          ; high nibble (0..15)
smc_lut_lo_hi:
        add a,0                          ; SMC: + low(_theme_attr_lut)
        ld l,a
smc_lut_hi_hi:
        ld a,0                           ; SMC: high(_theme_attr_lut)
        adc a,0                          ; +carry from add a, nibble
        ld h,a
        ld a,(hl)
        ld (de),a
        inc de
        dec b
        jr z,earth_unpack_done
        ld a,c
        and $0F                          ; low nibble
smc_lut_lo_lo:
        add a,0                          ; SMC: + low(_theme_attr_lut)
        ld l,a
smc_lut_hi_lo:
        ld a,0                           ; SMC: high(_theme_attr_lut)
        adc a,0                          ; +carry
        ld h,a
        ld a,(hl)
        ld (de),a
        inc de
        pop hl                           ; restore source ptr
        djnz earth_unpack_attr_span
        ret
earth_unpack_done:
        pop hl
        ret

;; _build_theme_attr_lut — populate _theme_attr_lut[16] for current theme.
;; Mapping per nibble: pre-decode (0..7 → 0..7, 8..15 → $40..$47), then apply
;; theme transform once. Patches SMC sites in earth_unpack_attr_span with the
;; LUT base address. Called from _about_render_ovl before first _earth_draw_frame.
_build_theme_attr_lut:
        ld a,(_theme_attrs + TA_MAIN_BG)
        and $38
        ld c,a                           ; C = bg_paper bits (PAPER from theme)

        ld a,(_current_theme)
        ld d,a                           ; D = theme number (1..3)

        ld hl,_theme_attr_lut
        ld b,16                          ; loop counter
        xor a                            ; A = nibble (0..15)

blut_loop:
        push af                          ; save nibble counter
        cp 8
        jr c,blut_no_bright
        add a,$38                        ; nibbles 8..15 → $40..$47 (BRIGHT+INK)
blut_no_bright:
        ld e,a                           ; E = raw_decoded

        ld a,d
        cp 2
        jr z,blut_terminal
        cp 3
        jr z,blut_commander_check

blut_default:
        ld a,e
        and $47
        or c
        jr blut_store

blut_terminal:
        ld a,e
        and $40
        or c
        or 4
        jr blut_store

blut_commander_check:
        ld a,e
        and 7
        cp 1
        jr nz,blut_default               ; not INK 1 → default formula
        ld a,e
        and $40
        or c
        or 5

blut_store:
        ld (hl),a
        inc hl
        pop af
        inc a
        djnz blut_loop

        ; Patch SMC sites with LUT base address (4 sites: 2 low + 2 high)
        ld a,_theme_attr_lut & $FF
        ld (smc_lut_lo_hi + 1),a
        ld (smc_lut_lo_lo + 1),a
        ld a,_theme_attr_lut >> 8
        ld (smc_lut_hi_hi + 1),a
        ld (smc_lut_hi_lo + 1),a
        ret

;; Everything from _about_packet_slot up to _about_packet_slot_end is needed only
;; while ABOUT opens. Once entry 0 returns, globe ticks reuse this dead code area
;; as a 481-byte packet buffer, so keep close/tick/seek/apply/draw code above.
_about_packet_slot:
_about_render_ovl:
        call _about_close_ovl

        ld a,(_theme_attrs + TA_MAIN_BG)
        ld c,a
        ld b,0
        push bc
        ld bc,(19 << 8) | 2
        push bc
        call _clear_zone
        pop bc
        pop bc
        call _earth_draw_separator

        ld hl,_K_DAT
        call _esx_fopen
        ld a,(_esx_handle)
        or a
        jp z,about_fail

        ld hl,EARTH_FRAME0_OFFSET
        call _earth_seek
        ld a,l
        or a
        jp z,about_fail

        ld hl,_earth_frame_buffer
        ld de,EARTH_FRAME0_SIZE
        call about_read_exact
        jr nz,about_fail

        ld hl,_earth_attr_buffer
        ld de,EARTH_ATTR0_SIZE
        call about_read_exact
        jr nz,about_fail

        call _build_theme_attr_lut       ; theme is locked while !about open
        call _earth_draw_frame
        call _earth_read_logo
        ld a,l
        or a
        jr z,about_fail

        ld hl,EARTH_DELTA_OFFSET
        call _earth_seek
        ld a,l
        or a
        jr z,about_fail

        xor a
        ld (_frame_idx),a
        inc a
        ld (_earth_ready),a

        ld a,(_current_theme)
        cp 1
        jr nz,about_theme_attr
        ld hl,about_s_line1
        ld d,16
        ld e,10
        ld c,$46
        call _earth_ikkle_draw
        ld hl,about_s_line2
        ld d,17
        ld e,11
        ld c,$47
        jr about_draw_line2

about_theme_attr:
        ld hl,about_s_line1
        ld d,16
        ld e,10
        ld a,(_theme_attrs + TA_MSG_NICK)
        ld c,a
        call _earth_ikkle_draw
        ld hl,about_s_line2
        ld d,17
        ld e,11
        ld a,(_theme_attrs + TA_MSG_CHAN)
        ld c,a

about_draw_line2:
        call _earth_ikkle_draw
        ld hl,about_s_foot
        ld d,20
        ld e,18
        ld a,(_theme_attrs + TA_MSG_SYS)
        ld c,a
        call _earth_ikkle_draw
        jr about_reset_rx

about_fail:
        call _about_close_ovl
        ld hl,about_s_foot
        ld d,20
        ld e,18
        ld a,(_theme_attrs + TA_MSG_SYS)
        ld c,a
        call _earth_ikkle_draw

about_reset_rx:
        xor a
        ld hl,0
        ld (_rb_head),hl
        ld (_rb_tail),hl
        ld (_rx_pos),hl
        ld (_rx_last_len),hl
        ld (_rx_overflow),a
        ret

about_read_exact:
        ld (_esx_buf),hl
        ld (_esx_count),de
        call _esx_fread
        ld hl,(_esx_result)
        ld de,(_esx_count)
        or a
        sbc hl,de
        ret

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
        jr z,earth_logo_theme1
        cp 3
        jr z,earth_logo_theme3
        ld a,(_theme_attrs + TA_MSG_NICK)
        ret

earth_logo_theme1:
        ld a,$47
        ret

earth_logo_theme3:
        ld a,(_theme_attrs + TA_MAIN_BG)
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
        ld a,(_theme_attrs + TA_MAIN_BG)
earth_sep_attr_loop:
        ld (hl),a
        inc l
        djnz earth_sep_attr_loop
        ret

;; Input: HL = string, D = row, E = 64-col column, C = attr.
_earth_ikkle_draw:
        ld a,d
        ld (earth_ikkle_row + 1),a
        ld a,c
        ld (earth_ikkle_attr + 1),a
        ld c,e

earth_ikkle_char_loop:
        ld a,(hl)
        or a
        jp z,earth_ikkle_done
        push hl

        ld a,c
        srl a
        ld b,a
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

earth_ikkle_row:
        ld a,0
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
        ld a,(earth_ikkle_row + 1)
        call earth_attr_base
        ld a,b
        add a,l
        ld l,a
earth_ikkle_attr:
        ld a,0
        ld (hl),a

        pop hl
        inc hl
        inc c
        jp earth_ikkle_char_loop

earth_ikkle_done:
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

about_s_line1:
        db "SPECTALKZX 1.3.8: IRC CLIENT FOR ZX SPECTRUM",0
about_s_line2:
        db "GITHUB.COM/IGNACIOMONGE/SPECTALKZX GPL-2.0",0
about_s_foot:
        db "N: What's New  Any key: Exit",0
_about_packet_slot_end:
IF _about_packet_slot_end - _about_packet_slot < EARTH_PACKET_SIZE
        defb 1 / 0
ENDIF

        INCLUDE "earth_overlay_spans.asm"
        INCLUDE "earth_logo.asm"

SECTION bss_user
_earth_frame_buffer:
        DS EARTH_FRAME0_SIZE
_earth_attr_buffer:
        DS EARTH_ATTR0_SIZE
_theme_attr_lut:
        DS 16
