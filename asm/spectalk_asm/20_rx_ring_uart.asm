; =============================================================================
; CACHE DE INPUT
; =============================================================================

; -----------------------------------------------------------------------------
; void input_cache_invalidate(void)
; Rellena input_cache_char (128 bytes) con 0xFF
; -----------------------------------------------------------------------------
_input_cache_invalidate:
    ld hl, _input_cache_char
    ld (hl), 0xFF
    ld d, h
    ld e, l
    inc de
    ld bc, 127
    ldir
    ret

; =============================================================================
; RING BUFFER
; Size: 2048 bytes (0x0800)
; Mask: 0x07FF (Low: 0xFF, High: 0x07)
; =============================================================================

; -----------------------------------------------------------------------------
; int16_t rb_pop(void)
; Saca un byte del buffer.
; Retorna: HL = byte le?do (0-255), o -1 si buffer vac?o.
; -----------------------------------------------------------------------------
_rb_pop:
    ; 1. Comprobar si hay datos (Head != Tail)
    ld hl, (_rb_head)
    ld de, (_rb_tail)
    or a                    ; Limpiar carry
    sbc hl, de
    jr z, rb_pop_empty      ; Si resultado es 0, buffer vac?o -> salir
    
    ; 2. Leer el byte: ring_buffer[tail]
    ld hl, _ring_buffer
    add hl, de              ; HL = direcci?n real en memoria (&ring_buffer + tail)
    ld l, (hl)              ; Leemos el byte
    ld h, 0                 ; H=0 para retornar un int16 limpio
    
    ; 3. Avanzar Tail: tail = (tail + 1) & MASK
    inc de                  ; Incrementamos Tail
    ; NOTA: No hacemos AND E, 0xFF porque es redundante
    ld a, d
    and RB_MASK_H           ; Aplicamos m?scara solo a la parte alta (0x07)
    ld d, a
    ld (_rb_tail), de       ; Guardamos nuevo Tail
    
    ret

rb_pop_empty:
    ld hl, -1
    ret

; -----------------------------------------------------------------------------
; uint8_t rb_push(uint8_t b) __z88dk_fastcall
; Mete un byte en el buffer.
; Input: L = byte a guardar
; Retorna: L=1 (?xito), L=0 (Buffer Lleno)
; -----------------------------------------------------------------------------
_rb_push:
    push hl             ; Guardamos el dato (L) en la pila moment?neamente
    
    ; 1. Calcular d?nde ir?a el NUEVO Head = (head + 1) & MASK
    ld hl, (_rb_head)
    ld d, h             ; DE = head actual (guardamos para escribir despu?s)
    ld e, l
    inc hl
    
    ; Aplicar m?scara solo a la parte alta (la baja hace overflow natural a 0)
    ld a, h
    and RB_MASK_H       
    ld h, a
    
    ; HL contiene ahora el "Futuro Head"
    ; DE contiene el "Head Actual" (donde escribiremos)
    
    ; 2. Comprobar colisi?n: ?El Futuro Head choca con el Tail actual?
    ld bc, (_rb_tail)
    ld a, h
    cp b
    jr nz, _rb_push_ok
    ld a, l
    cp c
    jr z, _rb_push_full ; Si son iguales, el buffer est? lleno. Abortar.
    
_rb_push_ok:
    ; 3. Guardar el Futuro Head en la variable global
    ; NOTA: En un sistema con ISR concurrente esto ser?a incorrecto (head antes de write),
    ;       pero aqu? todo es polling s?ncrono, sin riesgo de race condition.
    ld (_rb_head), hl
    
    ; 4. Escribir el dato en la memoria: buffer[head_viejo]
    ld hl, _ring_buffer
    add hl, de              ; HL = direcci?n de escritura (buffer + head_viejo)
    
    pop de                  ; Recuperamos el dato original de la pila (estaba en L, ahora en E)
    ld (hl), e              ; Escribimos en memoria
    
    ld l, 1                 ; Retornar 1 (?xito)
    ret

_rb_push_full:
    pop hl              ; Limpiar la pila (tiramos el dato que quer?amos guardar)
    ld l, 0             ; Retornar 0 (Fallo/Lleno)
    ret

; -----------------------------------------------------------------------------
; _try_read_line_nodrain
; Consume el buffer buscando un \n. Si hay desbordamiento, descarta bytes
; de forma segura sin escribir en memoria hasta encontrar el \n.
; -----------------------------------------------------------------------------
_try_read_line_nodrain:
trln_loop:
    ; 1. Comprobar si hay datos (Head != Tail)
    ld hl, (_rb_head)
    ld de, (_rb_tail)
    or a
    sbc hl, de
    jr z, trln_return_0
    
    ; 2. Leer byte del anillo
    ld hl, _ring_buffer
    add hl, de
    ld c, (hl)          ; C = byte le?do
    
    ; 3. Avanzar Tail
    inc de
    ld a, d
    and RB_MASK_H       ; M?scara 0x07 solo en High
    ld d, a
    ld (_rb_tail), de
    
    ; 4. Analyze character
    ld a, c
    cp 0x0D             ; Ignorar \r
    jr z, trln_loop
    
    cp 0x0A             ; End of line \n
    jr z, trln_newline
    
    ; 5. CR?TICO: Chequear desbordamiento ANTES de escribir (HARDENED)
    ld hl, (_rx_pos)
    ld de, RX_LINE_MAX  ; == RX_LINE_SIZE - 2 (espacio para \0)
    or a
    sbc hl, de
    jr nc, trln_overflow_state ; Si rx_pos >= 510, descartar
    
    ; 6. Save character (no hay overflow)
    ld de, _rx_line + RX_LINE_MAX
    add hl, de          ; HL = &rx_line[rx_pos]
    ld (hl), c
    
    ; 7. Incrementar posici?n
    ld hl, (_rx_pos)
    inc hl
    ld (_rx_pos), hl
    jr trln_loop

trln_overflow_state:
    ; Marcar flag de overflow (ahora es uint8_t)
    ld a, 1
    ld (_rx_overflow), a
    ; IMPORTANTE: NO escribir C en memoria, ni incrementar rx_pos.
    ; Simply return to loop para consumir el siguiente byte.
    jr trln_loop

trln_newline:
    ; Terminar string con NULL
    ld hl, (_rx_pos)
    ld de, _rx_line
    add hl, de
    ld (hl), 0          
    
    ; Check if line had overflow (ahora es uint8_t)
    ld hl, _rx_overflow
    ld a, (hl)
    or a
    jr z, trln_check_valid
    
    ; If overflow, DISCARD line completa y resetear
    ld (hl), 0
    call _rx_pos_reset
    jr trln_loop        ; Look for next line

trln_check_valid:
    ; If length > 0, valid line
    ld hl, (_rx_pos)
    ld a, l
    or h
    jr z, trln_loop     ; Ignore empty lines
    
    ; ?xito: Reset rx_pos y retornar 1
    ; Guardar longitud de la l?nea (rx_pos) para evitar strlen() en C
    ld (_rx_last_len), hl

    call _rx_pos_reset
    ld l, 1
    ret

trln_return_0:
    ld l, 0
    ret


