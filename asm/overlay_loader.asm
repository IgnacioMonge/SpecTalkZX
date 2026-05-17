;; overlay_loader.asm — Load and execute overlays from esxDOS
;; All overlays packed in SPECTALK.OVL (2048B blocks).
;; OVL code loaded into ring_buffer (2048B) for execution.
;; overlay_slot (512B, aliased to rx_line) available as scratch data buffer for overlays.

SECTION code_user

EXTERN _ring_buffer
EXTERN _rb_head
EXTERN _rb_tail
EXTERN _rx_pos
EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _ui_err
EXTERN _uart_drain_to_buffer
EXTERN _rx_overflow
EXTERN _overlay_exit_full
EXTERN _input_cache_invalidate
EXTERN ___sdcc_enter_ix

; void overlay_exec(uint8_t ovl_id, uint8_t entry_id) __z88dk_callee
; SDCC stack: [IX+4]=ovl_id, [IX+5]=entry_id
PUBLIC _overlay_exec
_overlay_exec:
    call ___sdcc_enter_ix

    ; Drain UART before overlay (ring_buffer will be overwritten)
    call _uart_drain_to_buffer

    ; Open SPECTALK.OVL
    ld hl, ovl_filename
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, ovl_fail

    ; Set buffer to ring_buffer, count = 2048
    ld hl, _ring_buffer
    ld (_esx_buf), hl
    ld hl, 2048
    ld (_esx_count), hl

    ; Seek to ovl_id block (each 2048B). Avoid reading previous overlay
    ; blocks into ring_buffer just to discard them.
    ld a, (ix+4)        ; ovl_id
    or a
    jr z, ovl_read      ; ovl_id=0: already at start
    call ovl_seek_block
    jr c, ovl_fail_close

ovl_read:
    ; Read the actual OVL block (2048B into ring_buffer)
    call _esx_fread
    call _esx_fclose
    call _input_cache_invalidate

    ; Overlay blocks are fixed-size; never execute a partial/stale ring load.
    ld hl, (_esx_result)
    ld a, h
    sub 8
    or l
    jr nz, ovl_fail
ovl_read_ok:

    ; W7 fix: validate entry_id < entry_count
    ld a, (ix+5)        ; entry_id
    ld hl, _ring_buffer
    cp (hl)             ; compare entry_id vs entry_count (low byte)
    jr nc, ovl_fail     ; entry_id >= entry_count -> invalid

    ; Look up entry point: ring_buffer[2 + entry_id*2]
    add a, a            ; *2
    ld e, a
    ld d, 0
    ld hl, _ring_buffer + 2
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)          ; DE = absolute entry address
    ; Reject corrupted entry pointers outside the loaded 2K overlay block.
    push de
    ex de, hl
    ld de, _ring_buffer
    or a
    sbc hl, de          ; HL = entry - ring_buffer
    jr c, ovl_bad_entry
    ld a, h
    cp 8                ; offset >= 2048?
    jr nc, ovl_bad_entry
    pop de
    push de             ; preserve entry address while RX cleanup uses DE

    ; The overlay has overwritten ring_buffer. Drop any pre-load UART bytes
    ; that were in the ring, and mark overflow if that discarded a fragment.
    ld hl, (_rb_head)
    ld de, (_rb_tail)
    ld (_rb_tail), hl
    or a
    sbc hl, de
    ld a, h
    or l
    ld hl, (_rx_pos)
    or h
    or l
    ld hl, _rx_overflow
    or (hl)
    jr z, ovl_write_rx_overflow
    ld a, 1
ovl_write_rx_overflow:
    ld (hl), a

    ; Callee cleanup: restore IX, remove params, jump to overlay
    pop hl              ; HL = entry address
    pop ix              ; restore IX
    pop de              ; DE = ret addr (caller's return)
    pop bc              ; remove 2 bytes of params (callee cleanup)
    push de             ; push caller's return address back
    jp (hl)             ; jump to overlay — its ret goes back to caller

ovl_bad_entry:
    pop de
    jr ovl_fail

ovl_fail_close:
    call _esx_fclose
    jr ovl_fail

ovl_fail:
    pop ix
    pop de              ; ret addr
    pop bc              ; remove 2 bytes of params (callee cleanup)
    push de
    call _overlay_exit_full
    ld hl, ovl_err_msg
    jp _ui_err          ; tail call

; A = ovl_id (1..n). Seek SPECTALK.OVL to ovl_id * 2048.
; F_SEEK takes 32-bit BCDE distance and whence=SET in IXL.
ovl_seek_block:
    add a, a
    add a, a
    add a, a            ; A = ovl_id * 8, high byte of low word
    ld d, a
    ld e, 0
    ld bc, 0
    ld a, (_esx_handle)
    push ix
    ld ix, 0
    rst 8
    defb 0x9F           ; F_SEEK
    pop ix
    ret

; void overlay_call(uint8_t entry_id) __z88dk_fastcall
; Call entry in ALREADY-LOADED overlay (ring_buffer). No disk I/O.
; Used for per-frame animation ticks while overlay is resident.
PUBLIC _overlay_call
_overlay_call:
    ld      a, l             ; entry_id (fastcall: param in L)
    ld      hl, _ring_buffer
    cp      (hl)             ; entry_id < entry_count?
    ret     nc               ; invalid/corrupt table -> ignore safely
    add     a, a             ; *2 (entry table is word-indexed)
    ld      e, a
    ld      d, 0
    ld      hl, _ring_buffer + 2
    add     hl, de
    ld      e, (hl)
    inc     hl
    ld      d, (hl)          ; DE = entry function address
    ; Reject corrupt entry pointers outside the resident overlay block.
    push    de
    ex      de, hl
    ld      de, _ring_buffer
    or      a
    sbc     hl, de
    jr      c, ovl_call_bad
    ld      a, h
    cp      8
    jr      nc, ovl_call_bad
    pop     hl
    jp      (hl)             ; jump — overlay's ret returns to caller

ovl_call_bad:
    pop     de
    ret

; void overlay_call_timed(uint8_t entry_id) __z88dk_fastcall
; Same ABI as overlay_call, but enables IM1 interrupts while the overlay entry
; runs. Use only for ABOUT animation ticks: this lets ROM FRAMES advance during
; long DAT/draw work while keeping the normal mainline DI contract elsewhere.
PUBLIC _overlay_call_timed
_overlay_call_timed:
    push    iy
    ld      iy, 0x5C3A        ; ROM ISR expects IY = system variables
    ei
    call    _overlay_call
    di
    pop     iy
    ret

ovl_filename:
    DEFM "SPECTALK.OVL"
    DEFB 0

ovl_err_msg:
    DEFM "Overlay load failed"
    DEFB 0
