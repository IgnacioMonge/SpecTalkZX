;; overlay_loader.asm — Load and execute overlays from esxDOS
;; All overlays packed in SPECTALK.OVL as a variable-length atlas.
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
EXTERN _esx_fseek_set
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

OVL_ATLAS_HEADER_LEN EQU 64

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

    ; Read atlas header into ring_buffer.
    ld hl, _ring_buffer
    ld (_esx_buf), hl
    ld hl, OVL_ATLAS_HEADER_LEN
    ld (_esx_count), hl
    call _esx_fread

    ; Header must be complete before we trust table offsets/sizes.
    ld hl, (_esx_result)
    ld a, h
    or a
    jr nz, ovl_fail_close
    ld a, l
    cp OVL_ATLAS_HEADER_LEN
    jr nz, ovl_fail_close

    ; Resolve ovl_id to payload offset/size, then seek to that payload.
    call ovl_atlas_select
    jr c, ovl_fail_close
    call _esx_fseek_set
    jr c, ovl_fail_close

ovl_read:
    ; Read the exact overlay payload into ring_buffer.
    call _esx_fread
    call _esx_fclose
    call _input_cache_invalidate

    ; Never execute a partial/stale ring load.
    ld hl, (_esx_result)
    ld de, (ovl_loaded_len)
    or a
    sbc hl, de
    jr nz, ovl_fail
ovl_read_ok:

    ; W7 fix: validate entry_id < entry_count
    ld a, (ix+5)        ; entry_id
    ld hl, _ring_buffer
    cp (hl)             ; compare entry_id vs entry_count (low byte)
    jr nc, ovl_fail     ; entry_id >= entry_count -> invalid

    ; Look up entry point: ring_buffer[2 + entry_id*2]
    add a, a            ; *2
    add a, 2
    ld l, a             ; H already high(_ring_buffer) from entry_count check
    ld e, (hl)
    inc hl
    ld d, (hl)          ; DE = absolute entry address
    ; Reject corrupted entry pointers outside the loaded overlay body.
    call ovl_entry_in_loaded
    jr c, ovl_fail
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

; Header format: "STOA", version, count, header_len, then <offset,size> words.
; Output: HL = payload offset, ovl_loaded_len/_esx_count = payload size.
; Carry set means invalid atlas/ovl_id.
ovl_atlas_select:
    ld hl, _ring_buffer
    ld a, (hl)
    cp 'S'
    jr nz, ovl_atlas_bad
    inc hl
    ld a, (hl)
    cp 'T'
    jr nz, ovl_atlas_bad
    inc hl
    ld a, (hl)
    cp 'O'
    jr nz, ovl_atlas_bad
    inc hl
    ld a, (hl)
    cp 'A'
    jr nz, ovl_atlas_bad
    inc hl
    ld a, (hl)
    cp 1
    jr nz, ovl_atlas_bad
    inc hl
    ld a, (ix+4)        ; ovl_id
    cp (hl)             ; ovl_id < overlay_count?
    jr nc, ovl_atlas_bad

    add a, a
    add a, a            ; table offset = 8 + ovl_id * 4
    add a, 8
    ld l, a             ; H still high(_ring_buffer)
    ld e, (hl)
    inc hl
    ld d, (hl)          ; DE = payload offset
    inc hl
    ld c, (hl)
    ld a, c
    ld (ovl_loaded_len), a
    ld (_esx_count), a
    inc hl
    ld b, (hl)
    ld a, b
    ld (ovl_loaded_len+1), a
    ld (_esx_count+1), a
    or c
    jr z, ovl_atlas_bad
    ld a, b
    cp 8
    jr c, ovl_atlas_size_ok
    jr nz, ovl_atlas_bad
    ld a, c
    or a
    jr nz, ovl_atlas_bad
ovl_atlas_size_ok:
    ex de, hl           ; HL = payload offset
    or a
    ret

ovl_atlas_bad:
    scf
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
    add     a, 2
    ld      l, a             ; H already high(_ring_buffer) from entry_count check
    ld      e, (hl)
    inc     hl
    ld      d, (hl)          ; DE = entry function address
    ; Reject corrupt entry pointers outside the resident overlay body.
    call    ovl_entry_in_loaded
    ret     c
    ex      de, hl
    jp      (hl)             ; jump — overlay's ret returns to caller

; DE = candidate entry address. Carry set means outside the loaded body.
; Fixed-format overlays initialize this to 2048; atlas loading will narrow it.
ovl_entry_in_loaded:
    push    de
    ex      de, hl
    ld      de, _ring_buffer
    or      a
    sbc     hl, de           ; HL = entry - ring_buffer
    jr      c, ovl_entry_bad
    ld      de, (ovl_loaded_len)
    or      a
    sbc     hl, de
    jr      nc, ovl_entry_bad ; offset >= loaded_len
    pop     de
    or      a
    ret

ovl_entry_bad:
    pop     de
    scf
    ret

ovl_loaded_len:
    DEFW    2048

; void overlay_call_timed(uint8_t entry_id) __z88dk_fastcall
; Same ABI as overlay_call, but enables IM1 interrupts while the overlay entry
; runs. Use only for ABOUT animation ticks: this lets ROM FRAMES advance during
; long DAT/draw work while keeping the normal mainline DI contract elsewhere.
; Timed entries must not call resident render/text routines that use IYL.
; esxDOS RST 8 under EI relies on wrappers preserving IY and esxDOS critical sections.
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
