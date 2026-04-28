; =============================================================================
; TEXT BUFFER MANIPULATION (Replaces memmove dependency)
; =============================================================================

; void text_shift_right(char *addr, uint16_t count) __z88dk_callee
; Desplaza memoria hacia ARRIBA (hace hueco). Usa LDDR.
; Entrada: HL = addr (origen), DE = count
; Nota: Callee convention, no pushear nada extra si es posible o limpiar stack.
; Para simplificar en C wrapper: usaremos fastcall pasando puntero y count global o struct?
; Mejor usamos __z88dk_callee est?ndar: Stack: [RetAddr] [Addr] [Count]
PUBLIC _text_shift_right
_text_shift_right:
    pop bc          ; Ret address
    pop hl          ; Addr (Inicio del hueco)
    pop de          ; Count (Bytes a mover)
    push bc         ; Restore Ret

    push iy         ; sdcc_iy ABI: preserve frame pointer
    
    ld b, d
    ld c, e         ; BC = Count
    ld a, b
    or c
    jr z, tsr_done
    
    add hl, bc      ; HL = Base + Count
    dec hl          ; HL = ?ltimo byte origen
    
    ld d, h
    ld e, l
    inc de          ; DE = Destino (HL + 1)
    
    lddr
tsr_done:
    pop iy
    ret

; void text_shift_left(char *addr, uint16_t count) __z88dk_callee
; Desplaza memoria hacia ABAJO (borra hueco). Usa LDIR.
; Stack: [RetAddr] [Addr] [Count]
; Copia desde addr+1 hacia addr.
PUBLIC _text_shift_left
_text_shift_left:
    pop bc          ; Ret
    pop de          ; Addr (Destino)
    pop hl          ; Count
    push bc         ; Restore Ret

    push iy         ; sdcc_iy ABI: preserve frame pointer
    
    ld b, h
    ld c, l         ; BC = Count
    ld a, b
    or c
    jr z, tsl_done
    
    ld h, d
    ld l, e         ; HL = Destino
    inc hl          ; HL = Origen (Destino + 1)
    
    ldir
tsl_done:
    pop iy
    ret

; =============================================================================
; STRING COPY WITH LENGTH LIMIT (Replaces strncpy dependency)
; =============================================================================

; void st_copy_n(char *dst, const char *src, uint8_t max_len)
; Copies src to dst, max (max_len-1) chars, always null-terminates
; Stack (stdcall): [ret] [dst] [src] [max_len]
; FIX1: POP AF pone byte bajo en F, no en A
PUBLIC _st_copy_n
_st_copy_n:
    pop af          ; ret en AF (temporal)
    pop de          ; dst
    pop hl          ; src
    pop bc          ; max_len en BC (C = byte bajo)
    push bc
    push hl
    push de
    push af         ; restaurar ret
    
    ld a, c         ; max_len real en A
    or a
    ret z           ; max_len == 0 ? nothing to do
    dec a           ; max_len - 1 = max chars to copy
    ld b, a         ; B = loop counter
    jr z, stcn_term ; If max_len was 1, just write \0
    
stcn_loop:
    ld a, (hl)
    ld (de), a      ; Copy NUL too; then we can return immediately
    or a
    ret z           ; Source ended, terminator already written
    inc hl
    inc de
    djnz stcn_loop
    
stcn_term:
    xor a
    ld (de), a      ; Null-terminate
    ret

; =============================================================================
; FAST CONSECUTIVE STRING OUTPUT (Saves ~4 bytes per call site)
; =============================================================================

; void main_puts2(const char *a, const char *b) __z88dk_callee
; Prints two strings consecutively - saves one CALL overhead per site
PUBLIC _main_puts2
EXTERN _main_puts
EXTERN _main_print
_main_puts2:
    pop bc          ; ret
    pop hl          ; a
    pop de          ; b
    push bc

    push iy         ; sdcc_iy ABI: preserve frame pointer
    push de
    call _main_puts
    pop hl
    pop iy
    jp _main_puts   ; tail call

; OPT: main_puts3 eliminated (no longer called from C)

; =============================================================================
; IRC SEND COMMAND HELPERS
; =============================================================================
EXTERN _connection_state
EXTERN _S_SP_COLON
; S_CRLF removed ? use uart_send_crlf instead
EXTERN _uart_send_crlf

; Common string " " for param separator
isc_space: defb ' ', 0

; void irc_send_cmd_internal(const char *cmd, const char *p1, const char *p2)
; Sends: CMD [p1] [ :p2]\r\n
; Stack: [ret] [cmd] [p1] [p2]
;
; ABI WARNING: clobbers IX (used as scratch for p2). Safe in resident build
; (--fomit-frame-pointer → IX free), but overlay C code uses IX as SDCC
; frame pointer. Call ONLY through _irc_send_cmd1 / _irc_send_cmd2 wrappers
; in 80_ui_runtime.asm — they save/restore IX. Do NOT call from overlay code.
; IY is preserved (sdcc_iy clib reserves it).
PUBLIC _irc_send_cmd_internal
_irc_send_cmd_internal:
    ; Check connection state first
    ld a, (_connection_state)
    cp 2                    ; STATE_TCP_CONNECTED
    ret c                   ; Return if not connected

    pop bc                  ; ret
    pop hl                  ; cmd
    pop de                  ; p1
    pop ix                  ; p2 (clobbers IX — see ABI warning above)
    push ix
    push de
    push hl
    push bc
    
    ; Send cmd (audit W02: preserve DE=p1 ? ay_uart_send clobbers DE)
    push de
    call _uart_send_string
    pop de

    ; Check p1
    ld a, d
    or e
    jr z, isci_check_p2
    ld a, (de)
    or a
    jr z, isci_check_p2
    
    ; Send " " + p1
    push de
    ld hl, isc_space
    call _uart_send_string
    pop hl
    call _uart_send_string
    
isci_check_p2:
    ; Check p2
    push ix
    pop hl
    ld a, h
    or l
    jr z, isci_crlf
    ld a, (hl)
    or a
    jr z, isci_crlf
    
    ; Send " :" + p2
    push hl
    ld hl, _S_SP_COLON
    call _uart_send_string
    pop hl
    call _uart_send_string
    
isci_crlf:
    jp _uart_send_crlf      ; tail call (saves 3B: no string load needed)



; =============================================================================
; esxDOS FILE I/O - Parameter passing via globals (zero ABI risk)
;
; All parameters are set in C globals before calling.
; No stack manipulation whatsoever. IY preserved via push/pop.
; IX set = HL for F_OPEN (esxDOS requirement).
; =============================================================================

; -----------------------------------------------------------------------------
; uint8_t esx_detect(void)
; Returns 1 if esxDOS/divMMC is present, 0 otherwise.
; Uses M_GETSETDRV (RST 8 / 0x89) with a custom error handler
; to catch the case where RST 8 falls through to ROM ERROR_1.
; -----------------------------------------------------------------------------
_esx_detect:
    push iy
    ld hl, (23613)        ; save ERR_SP
    push hl
    ld hl, esx_det_fail   ; set our error handler
    push hl
    ld (23613), sp        ; ERR_SP = SP (points to our handler addr)
    ld a, 0xFF            ; get current drive (no change)
    rst 8
    defb 0x89             ; M_GETSETDRV
    ; If we get here, esxDOS is present (RST 8 was trapped by divMMC)
    pop hl                ; remove our handler from stack
    ld a, 1               ; return 1 (detected)
    jr esx_det_end
esx_det_fail:
    ; ROM ERROR_1 jumped here via ERR_SP ? no esxDOS
    xor a                 ; return 0 (not detected)
esx_det_end:
    pop hl                ; restore original ERR_SP (was pushed before handler)
    ld (23613), hl
    pop iy
    ld l, a
    ret

PUBLIC _esx_fopen
PUBLIC _esx_fread
PUBLIC _esx_fclose
PUBLIC _esx_fcreate
PUBLIC _esx_fwrite

; Globals for parameter passing (set from C before calling)
PUBLIC _esx_handle
PUBLIC _esx_buf
PUBLIC _esx_count
PUBLIC _esx_result

SECTION bss_user
_esx_handle:  defs 1
_esx_buf:     defs 2
_esx_count:   defs 2
_esx_result:  defs 2

SECTION code_user

; -----------------------------------------------------------------------------
; void esx_fopen(const char *path) __z88dk_fastcall
; Input:  HL = path string
; Output: _esx_handle = file handle (0 on error)
; Preserves IY. Sets IX = HL (esxDOS needs both).
; -----------------------------------------------------------------------------
_esx_fopen:
    ld b, 0x01          ; FA_READ
    jr esx_open_common

_esx_fcreate:
    ld b, 0x0E          ; FA_WRITE | FA_CREATE_AL (0x02 write + 0x0C create/trunc)

esx_open_common:
    push iy
    push ix
    push hl
    pop ix              ; IX = HL = path (esxDOS wants both)
    ld a, '*'           ; default drive
    rst 8
    defb 0x9A           ; F_OPEN
    jr nc, esx_open_ok
    xor a               ; error ? handle = 0
esx_open_ok:
    ld (_esx_handle), a
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; void esx_fread(void)
; Input:  _esx_handle = file handle
;         _esx_buf    = destination pointer
;         _esx_count  = max bytes to read
; Output: _esx_result = bytes actually read (0 on error)
; Preserves IY and IX.
; -----------------------------------------------------------------------------
_esx_fread:
    push iy
    push ix
    ld a, (_esx_handle)
    ld ix, (_esx_buf)   ; esxDOS F_READ needs buffer destination in IX
    ld bc, (_esx_count)
    rst 8
    defb 0x9D           ; F_READ
    jr nc, esxfr_ok
    ld bc, 0
esxfr_ok:
    ld (_esx_result), bc
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; void esx_fclose(void)
; Input:  _esx_handle = file handle
; Preserves IY and IX.
; -----------------------------------------------------------------------------
_esx_fclose:
    push iy
    push ix
    ld a, (_esx_handle)
    rst 8
    defb 0x9B           ; F_CLOSE
    pop ix
    pop iy
    ret

; esx_fcreate: now merged with esx_fopen above (esx_open_common)

; -----------------------------------------------------------------------------
; void esx_fwrite(void)
; Input:  _esx_handle = file handle
;         _esx_buf    = source pointer
;         _esx_count  = bytes to write
; Preserves IY and IX.
; -----------------------------------------------------------------------------
_esx_fwrite:
    push iy
    push ix
    ld a, (_esx_handle)
    ld hl, (_esx_buf)
    push hl
    pop ix              ; IX = source buffer
    ld bc, (_esx_count)
    rst 8
    defb 0x9E           ; F_WRITE
    jr nc, efw_ok
    ld bc, 0            ; Error: 0 bytes written
efw_ok:
    ld (_esx_result), bc ; Save bytes actually written
    pop ix
    pop iy
    ret



