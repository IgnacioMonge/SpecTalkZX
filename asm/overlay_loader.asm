;; overlay_loader.asm — Load and execute overlays from esxDOS
;; All overlays packed in SPECTALK.OVL (4 × 2048B blocks).
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
    jp z, ovl_fail

    ; Set buffer to ring_buffer, count = 2048
    ld hl, _ring_buffer
    ld (_esx_buf), hl
    ld hl, 2048
    ld (_esx_count), hl

    ; Skip ovl_id blocks (each 2048B) by reading into ring_buffer
    ld a, (ix+4)        ; ovl_id
    or a
    jr z, ovl_read      ; ovl_id=0: no skip
ovl_skip:
    push af
    call _esx_fread     ; read 2048B (skip — overwrites ring_buffer, will be overwritten again)
    pop af
    dec a
    jr nz, ovl_skip

ovl_read:
    ; Read the actual OVL block (2048B into ring_buffer)
    call _esx_fread
    call _esx_fclose

    ; W3 fix: verify read succeeded (at least 64 bytes)
    ld hl, (_esx_result)
    ld a, h
    or a
    jr nz, ovl_read_ok   ; >= 256B, OK
    ld a, l
    cp 64
    jp c, ovl_fail        ; < 64B, corrupted/truncated
ovl_read_ok:

    ; W7 fix: validate entry_id < entry_count
    ld a, (ix+5)        ; entry_id
    ld hl, _ring_buffer
    cp (hl)             ; compare entry_id vs entry_count (low byte)
    jp nc, ovl_fail     ; entry_id >= entry_count -> invalid

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
    ld de, 2048
    ; CF=0 guaranteed (jr c not taken, ld de preserves flags)
    sbc hl, de          ; offset >= 2048?
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
    ld a, (_rx_overflow)
    jr z, ovl_check_rx_pos
    ld a, 1
ovl_check_rx_pos:
    ld hl, (_rx_pos)
    or h
    or l
    jr z, ovl_write_rx_overflow
    ld a, 1
ovl_write_rx_overflow:
    ld (_rx_overflow), a

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

ovl_fail:
    pop ix
    pop de              ; ret addr
    pop bc              ; remove 2 bytes of params (callee cleanup)
    push de
    call _overlay_exit_full
    ld hl, ovl_err_msg
    jp _ui_err          ; tail call

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
    ld      de, 2048
    ; CF=0 guaranteed (jr c not taken, ld de preserves flags)
    sbc     hl, de
    jr      nc, ovl_call_bad
    pop     hl
    jp      (hl)             ; jump — overlay's ret returns to caller

ovl_call_bad:
    pop     de
    ret

ovl_filename:
    DEFM "SPECTALK.OVL"
    DEFB 0

ovl_err_msg:
    DEFM "Overlay load failed"
    DEFB 0
