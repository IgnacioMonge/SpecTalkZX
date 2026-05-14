;; overlay_entry2.asm — Entry table for SPCTLK2.OVL
SECTION code_user

EXTERN _about_render_ovl
EXTERN _about_close_ovl
EXTERN _about_packet_slot
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_fread
EXTERN _esx_result
EXTERN _earth_apply_frame_delta
EXTERN _earth_apply_attr_delta
EXTERN _esx_fseek_set
EXTERN _earth_draw_frame

DEFC EARTH_PACKET_SIZE  = 467
DEFC EARTH_FRAME_COUNT  = 24
DEFC EARTH_DELTA_OFFSET = 2128

PUBLIC _globe_tick_ovl
PUBLIC _earth_ready
PUBLIC _frame_idx

    dw 3                      ; entry_count = 3
    dw _about_render_ovl      ; entry 0 → about
    dw _about_close_ovl       ; entry 1 → close about DAT handle
    dw _globe_tick_ovl        ; entry 2 → globe animation tick

; ==============================================================================
; ENTRY 2 — Fast ASM Tick (transplanted from C)
; ==============================================================================
_globe_tick_ovl:
    ld a, (_earth_ready)
    or a
    ret z

    ; Draw current buffer FIRST. Anchors visible repaint to consistent
    ; timing right after frame_wait. Variable fread latency is absorbed
    ; into the post-draw window of this same tick, so display update no
    ; longer jitters with disk I/O. Buffer was prepared either by
    ; _about_render_ovl (frame 0) or by previous tick.
    call _earth_draw_frame

    ld hl, _about_packet_slot
    ld (_esx_buf), hl
    ld hl, EARTH_PACKET_SIZE
    ld (_esx_count), hl
    call _esx_fread

    ld hl, EARTH_PACKET_SIZE
    or a
    sbc hl, bc
    jr nz, _about_close_ovl       ; tail-call if read fails

    ; Apply frame delta: earth_apply_frame_delta(about_packet_slot + 2)
    ld hl, _about_packet_slot + 2
    call _earth_apply_frame_delta

    ; Apply attr delta: earth_apply_attr_delta(about_packet_slot + len + 3)
    ld hl, (_about_packet_slot)   ; reads little-endian 16-bit len
    ld de, _about_packet_slot + 3
    add hl, de
    call _earth_apply_attr_delta

    ; Increment frame and reset stream when wrapped
    ld hl, _frame_idx
    inc (hl)
    ld a, (hl)
    cp EARTH_FRAME_COUNT
    ret nz

    ld hl, EARTH_DELTA_OFFSET
    call _esx_fseek_set
    dec l
    jr nz, _about_close_ovl       ; tail-call if seek fails
    ld a, l
    ld (_frame_idx), a
    ret

_earth_ready: db 0
_frame_idx:   db 0
