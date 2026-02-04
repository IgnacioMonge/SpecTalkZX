;;
;; spectalk_asm.asm - Optimized assembly routines for SpecTalk ZX
;; SpecTalk ZX - IRC Client for ZX Spectrum
;; Copyright (C) 2026 M. Ignacio Monge Garcia
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License version 2 as
;; published by the Free Software Foundation.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program. If not, see <https://www.gnu.org/licenses/>.
;;
;; STYLE NOTES:
;; - Local labels use function prefix (e.g., p64_loop_L instead of _loop_L)
;; - All functions preserve IY (required by z88dk)
;; - Stack parameters are documented with offset from IX

SECTION code_user

; =============================================================================
; functions PÚBLICAS (visibles desde C)
; =============================================================================
PUBLIC _set_border
PUBLIC _notification_beep
PUBLIC _check_caps_toggle
PUBLIC _key_shift_held
PUBLIC _input_cache_invalidate
PUBLIC _print_str64_char
PUBLIC _print_line64_fast
PUBLIC _draw_indicator
PUBLIC _clear_line
PUBLIC _clear_zone
PUBLIC _ldir_copy_fwd
PUBLIC _screen_row_base
PUBLIC _screen_line_addr
PUBLIC _attr_addr
PUBLIC _str_append
PUBLIC _str_append_n
PUBLIC _st_stricmp
PUBLIC _st_stristr
PUBLIC _u16_to_dec
PUBLIC _u16_to_dec3
PUBLIC _str_to_u16
PUBLIC _skip_to
PUBLIC _uart_send_string
PUBLIC _strip_irc_codes
PUBLIC _rb_pop
PUBLIC _rb_push
PUBLIC _try_read_line_nodrain
PUBLIC _reapply_screen_attributes
PUBLIC _cls_fast
PUBLIC _uart_drain_to_buffer
PUBLIC _scroll_main_zone
PUBLIC _tokenize_params
PUBLIC _sb_append

; =============================================================================
; variables Y functions EXTERNAS (definidas en C u otros .asm)
; =============================================================================
EXTERN _caps_lock_mode
EXTERN _caps_latch
EXTERN _g_ps64_y
EXTERN _g_ps64_col
EXTERN _g_ps64_attr
; _font64 eliminado - ahora usamos fuente comprimida integrada
EXTERN _input_cache_char
EXTERN _input_cache_attr
EXTERN _ay_uart_send
EXTERN _BORDER_COLOR
EXTERN _ATTR_BANNER
EXTERN _ATTR_MAIN_BG
EXTERN _ATTR_MSG_CHAN
EXTERN _ATTR_STATUS
EXTERN _ATTR_INPUT_BG
EXTERN _current_attr
EXTERN _irc_params
EXTERN _irc_param_count
EXTERN _force_status_redraw
EXTERN _status_bar_dirty
EXTERN _uart_drain_limit
EXTERN _ay_uart_ready   ; Retorna L=1 si hay datos
EXTERN _ay_uart_read    ; Retorna byte en L
EXTERN _rb_push         ; Fastcall: byte en L. Retorna L=1 (ok), 0 (fail)
EXTERN _irc_is_away

; Ring buffer (para rb_pop y try_read_line_nodrain)
EXTERN _ring_buffer
EXTERN _rb_head
EXTERN _rb_tail
EXTERN _rx_line
EXTERN _rx_pos
EXTERN _rx_overflow

; =============================================================================
; RING BUFFER CONSTANTS
; Size: 2048 bytes
; =============================================================================
DEFC RB_MASK_H = 0x07   ; Solo se usa la máscara alta (la baja hace overflow natural)

; =============================================================================
; VARIABLES BSS (Buffer para descompresión de fuente)
; =============================================================================
SECTION bss_user
glyph_buffer: defs 8    ; Buffer temporal para glifo descomprimido

SECTION code_user

; =============================================================================
; UTILIDADES BÁSICAS
; =============================================================================

; -----------------------------------------------------------------------------
; void set_border(uint8_t color) __z88dk_fastcall
; input: L = color
; -----------------------------------------------------------------------------
_set_border:
    ld a, l
    out (0xfe), a
    ret

; -----------------------------------------------------------------------------
; void notification_beep(void)
; Genera un pitido audible usando el altavoz del Spectrum
; -----------------------------------------------------------------------------
_notification_beep:
    push iy
    
    ld hl, 4000        ; Duración aumentada para beep audible (era 1000)
    
beep_loop:
    ld a, l
    and 0x10           ; Generar onda cuadrada (bit 4)
    or 0x08            ; Asegurar MIC on
    ld b, a            ; Guardamos los bits de audio en B
    
    ; --- INYECTAR COLOR DEL TEMA ---
    ld a, (_BORDER_COLOR) 
    and 0x07           ; Solo bits 0-2 (color)
    or b               ; Mezclamos: Audio (bits 3-4) + Borde (bits 0-2)
    ; ---------------------------------------
    
    out (0xFE), a      ; Salida: Suena el altavoz y el borde SE MANTIENE
    
    dec hl
    ld a, h
    or l
    jr nz, beep_loop
    
    pop iy
    ret

; -----------------------------------------------------------------------------
; void check_caps_toggle(void)
; Verifica Caps Shift + 2 y actualiza _caps_lock_mode
; -----------------------------------------------------------------------------
_check_caps_toggle:
    ld bc, 0xFEFE
    in a, (c)
    bit 0, a
    jr nz, caps_no_combo

    ld bc, 0xF7FE
    in a, (c)
    bit 1, a
    jr nz, caps_no_combo

    ld hl, _caps_latch
    ld a, (hl)
    or a
    ret nz

    ld hl, _caps_lock_mode
    ld a, (hl)
    xor 1
    ld (hl), a
    
    ld hl, _caps_latch
    ld (hl), 1
    ret

caps_no_combo:
    ld hl, _caps_latch
    ld (hl), 0
    ret

; -----------------------------------------------------------------------------
; uint8_t key_shift_held(void)
; Retorna 1 (en HL) si Caps Shift está pulsado limpiamente
; -----------------------------------------------------------------------------
_key_shift_held:
    ld bc, 0xFEFE
    in a, (c)
    bit 0, a
    jr nz, shift_not_held

    ld bc, 0xF7FE
    in a, (c)
    and 0x1F
    cp 0x1F
    jr nz, shift_not_held

    ld bc, 0xEFFE
    in a, (c)
    and 0x1F
    cp 0x1F
    jr nz, shift_not_held

    ld hl, 1
    ret

shift_not_held:
    ld hl, 0
    ret

; =============================================================================
; CACHE DE INPUT
; =============================================================================

; -----------------------------------------------------------------------------
; void input_cache_invalidate(void)
; Rellena input_cache_char (128 bytes) y input_cache_attr (64 bytes) con 0xFF
; -----------------------------------------------------------------------------
_input_cache_invalidate:
    ld hl, _input_cache_char
    ld (hl), 0xFF
    ld d, h
    ld e, l
    inc de
    ld bc, 127
    ldir
    
    ld hl, _input_cache_attr
    ld (hl), 0xFF
    ld d, h
    ld e, l
    inc de
    ld bc, 63
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
; Retorna: HL = byte leído (0-255), o -1 si buffer vacío.
; -----------------------------------------------------------------------------
_rb_pop:
    ; 1. Comprobar si hay datos (Head != Tail)
    ld hl, (_rb_head)
    ld de, (_rb_tail)
    or a                    ; Limpiar carry
    sbc hl, de
    jr z, rb_pop_empty      ; Si resultado es 0, buffer vacío -> salir
    
    ; 2. Leer el byte: ring_buffer[tail]
    ld hl, _ring_buffer
    add hl, de              ; HL = dirección real en memoria (&ring_buffer + tail)
    ld l, (hl)              ; Leemos el byte
    ld h, 0                 ; H=0 para retornar un int16 limpio
    
    ; 3. Avanzar Tail: tail = (tail + 1) & MASK
    inc de                  ; Incrementamos Tail
    ; NOTA: No hacemos AND E, 0xFF porque es redundante
    ld a, d
    and RB_MASK_H           ; Aplicamos máscara solo a la parte alta (0x07)
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
; Retorna: L=1 (Éxito), L=0 (Buffer Lleno)
; -----------------------------------------------------------------------------
_rb_push:
    push hl             ; Guardamos el dato (L) en la pila momentáneamente
    
    ; 1. Calcular dónde iría el NUEVO Head = (head + 1) & MASK
    ld hl, (_rb_head)
    ld d, h             ; DE = head actual (guardamos para escribir después)
    ld e, l
    inc hl
    
    ; Aplicar máscara solo a la parte alta (la baja hace overflow natural a 0)
    ld a, h
    and RB_MASK_H       
    ld h, a
    
    ; HL contiene ahora el "Futuro Head"
    ; DE contiene el "Head Actual" (donde escribiremos)
    
    ; 2. Comprobar colisión: ¿El Futuro Head choca con el Tail actual?
    ld bc, (_rb_tail)
    ld a, h
    cp b
    jr nz, _rb_push_ok
    ld a, l
    cp c
    jr z, _rb_push_full ; Si son iguales, el buffer está lleno. Abortar.
    
_rb_push_ok:
    ; 3. Guardar el Futuro Head en la variable global
    ld (_rb_head), hl   ; ACTUALIZAMOS EL PUNTERO GLOBAL
    
    ; 4. Escribir el dato en la memoria: buffer[head_viejo]
    ld hl, _ring_buffer
    add hl, de          ; HL = dirección de escritura (buffer + head_viejo)
    
    pop de              ; Recuperamos el dato original de la pila (estaba en L, ahora en E)
    ld (hl), e          ; Escribimos en memoria
    
    ld l, 1             ; Retornar 1 (Éxito)
    ret

_rb_push_full:
    pop hl              ; Limpiar la pila (tiramos el dato que queríamos guardar)
    ld l, 0             ; Retornar 0 (Fallo/Lleno)
    ret

; -----------------------------------------------------------------------------
; _try_read_line_nodrain
; Consume el buffer buscando un \n. Si hay desbordamiento, descarta bytes
; de forma segura sin escribir en memoria hasta encontrar el \n.
; -----------------------------------------------------------------------------
_try_read_line_nodrain:
    push iy
    
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
    ld c, (hl)          ; C = byte leído
    
    ; 3. Avanzar Tail
    inc de
    ld a, d
    and RB_MASK_H       ; Máscara 0x07 solo en High
    ld d, a
    ld (_rb_tail), de
    
    ; 4. Analizar carácter
    ld a, c
    cp 0x0D             ; Ignorar \r
    jr z, trln_loop
    
    cp 0x0A             ; Fin de línea \n
    jr z, trln_newline
    
    ; 5. Chequear desbordamiento (SAFE CHECK)
    ld hl, (_rx_pos)
    ld de, 511          ; Margen de seguridad (RX_LINE_SIZE - 1)
    or a
    sbc hl, de
    jr nc, trln_overflow_state ; Si rx_pos >= 510, saltar a descarte seguro
    
    ; 6. Guardar carácter (Solo si no hay overflow)
    ld hl, (_rx_pos)
    ld de, _rx_line
    add hl, de
    ld (hl), c
    
    ; 7. Incrementar posición
    ld hl, (_rx_pos)
    inc hl
    ld (_rx_pos), hl
    jr trln_loop

trln_overflow_state:
    ; Marcar flag de overflow
    ld hl, 1
    ld (_rx_overflow), hl
    ; IMPORTANTE: NO escribir C en memoria, ni incrementar rx_pos.
    ; Simplemente volvemos al loop para consumir el siguiente byte.
    jr trln_loop

trln_newline:
    ; Terminar string con NULL
    ld hl, (_rx_pos)
    ld de, _rx_line
    add hl, de
    ld (hl), 0          
    
    ; Comprobar si esta línea tuvo overflow
    ld hl, (_rx_overflow)
    ld a, l
    or h
    jr z, trln_check_valid
    
    ; Si hubo overflow, DESCARTAR línea completa y resetear
    ld hl, 0
    ld (_rx_overflow), hl
    ld (_rx_pos), hl
    jr trln_loop        ; Volver a buscar siguiente línea

trln_check_valid:
    ; Si longitud > 0, tenemos línea válida
    ld hl, (_rx_pos)
    ld a, l
    or h
    jr z, trln_loop     ; Ignorar líneas vacías
    
    ; Éxito: Reset rx_pos y retornar 1
    ld hl, 0
    ld (_rx_pos), hl
    pop iy
    ld l, 1
    ret

trln_return_0:
    pop iy
    ld l, 0
    ret

; =============================================================================
; SIStheme GRÁFICO - TABLAS
; =============================================================================

_screen_row_base:
    defw 0x4000, 0x4020, 0x4040, 0x4060, 0x4080, 0x40A0, 0x40C0, 0x40E0
    defw 0x4800, 0x4820, 0x4840, 0x4860, 0x4880, 0x48A0, 0x48C0, 0x48E0
    defw 0x5000, 0x5020, 0x5040, 0x5060, 0x5080, 0x50A0, 0x50C0, 0x50E0

; Tabla de direcciones de atributos (24 filas)
; Elimina cálculo repetido de 0x5800 + y*32
_attr_row_base:
    defw 0x5800, 0x5820, 0x5840, 0x5860, 0x5880, 0x58A0, 0x58C0, 0x58E0
    defw 0x5900, 0x5920, 0x5940, 0x5960, 0x5980, 0x59A0, 0x59C0, 0x59E0
    defw 0x5A00, 0x5A20, 0x5A40, 0x5A60, 0x5A80, 0x5AA0, 0x5AC0, 0x5AE0

; =============================================================================
; FUENTE COMPRIMIDA - NIBBLE PACKED (Ahorra ~390 bytes vs font64 original)
; =============================================================================
; Solo 10 valores únicos en toda la fuente original:
; 0x00, 0x22, 0x44, 0x55, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0xFF
; Líneas 6 y 7 de cada glifo son SIEMPRE 0x00 (se regeneran en runtime)
; Cada glifo: 3 bytes comprimidos (6 nibbles = líneas 0-5)

font_lut:
    defb 0x00, 0x22, 0x44, 0x55, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0xFF

font64_packed:
    defb 0x00, 0x00, 0x00  ; ' ' (ASCII 32)
    defb 0x55, 0x55, 0x05  ; '!' (ASCII 33)
    defb 0x06, 0x60, 0x00  ; '"' (ASCII 34)
    defb 0x06, 0x86, 0x86  ; '#' (ASCII 35)
    defb 0x28, 0x58, 0x18  ; '$' (ASCII 36)
    defb 0x61, 0x47, 0x56  ; '%' (ASCII 37)
    defb 0x26, 0x26, 0x74  ; '&' (ASCII 38)
    defb 0x25, 0x00, 0x00  ; ''' (ASCII 39)
    defb 0x25, 0x55, 0x52  ; '(' (ASCII 40)
    defb 0x52, 0x22, 0x25  ; ')' (ASCII 41)
    defb 0x06, 0x28, 0x26  ; '*' (ASCII 42)
    defb 0x02, 0x28, 0x22  ; '+' (ASCII 43)
    defb 0x00, 0x02, 0x25  ; ',' (ASCII 44)
    defb 0x00, 0x08, 0x00  ; '-' (ASCII 45)
    defb 0x00, 0x00, 0x77  ; '.' (ASCII 46)
    defb 0x11, 0x22, 0x55  ; '/' (ASCII 47)
    defb 0x26, 0x68, 0x62  ; '0' (ASCII 48)
    defb 0x27, 0x22, 0x28  ; '1' (ASCII 49)
    defb 0x26, 0x12, 0x58  ; '2' (ASCII 50)
    defb 0x81, 0x21, 0x62  ; '3' (ASCII 51)
    defb 0x56, 0x68, 0x11  ; '4' (ASCII 52)
    defb 0x85, 0x71, 0x17  ; '5' (ASCII 53)
    defb 0x45, 0x76, 0x62  ; '6' (ASCII 54)
    defb 0x81, 0x12, 0x22  ; '7' (ASCII 55)
    defb 0x26, 0x26, 0x62  ; '8' (ASCII 56)
    defb 0x26, 0x64, 0x17  ; '9' (ASCII 57)
    defb 0x00, 0x50, 0x05  ; ':' (ASCII 58)
    defb 0x02, 0x02, 0x25  ; ';' (ASCII 59)
    defb 0x01, 0x25, 0x21  ; '<' (ASCII 60)
    defb 0x00, 0x80, 0x80  ; '=' (ASCII 61)
    defb 0x05, 0x21, 0x25  ; '>' (ASCII 62)
    defb 0x26, 0x12, 0x02  ; '?' (ASCII 63)
    defb 0x26, 0x88, 0x52  ; '@' (ASCII 64)
    defb 0x26, 0x68, 0x66  ; 'A' (ASCII 65)
    defb 0x76, 0x76, 0x67  ; 'B' (ASCII 66)
    defb 0x45, 0x55, 0x54  ; 'C' (ASCII 67)
    defb 0x76, 0x66, 0x67  ; 'D' (ASCII 68)
    defb 0x85, 0x75, 0x58  ; 'E' (ASCII 69)
    defb 0x85, 0x75, 0x55  ; 'F' (ASCII 70)
    defb 0x26, 0x58, 0x62  ; 'G' (ASCII 71)
    defb 0x66, 0x86, 0x66  ; 'H' (ASCII 72)
    defb 0x82, 0x22, 0x28  ; 'I' (ASCII 73)
    defb 0x41, 0x11, 0x62  ; 'J' (ASCII 74)
    defb 0x66, 0x87, 0x66  ; 'K' (ASCII 75)
    defb 0x55, 0x55, 0x58  ; 'L' (ASCII 76)
    defb 0x68, 0x88, 0x66  ; 'M' (ASCII 77)
    defb 0x66, 0x68, 0x86  ; 'N' (ASCII 78)
    defb 0x26, 0x66, 0x62  ; 'O' (ASCII 79)
    defb 0x76, 0x67, 0x55  ; 'P' (ASCII 80)
    defb 0x26, 0x66, 0x74  ; 'Q' (ASCII 81)
    defb 0x76, 0x67, 0x66  ; 'R' (ASCII 82)
    defb 0x45, 0x21, 0x62  ; 'S' (ASCII 83)
    defb 0x82, 0x22, 0x22  ; 'T' (ASCII 84)
    defb 0x66, 0x66, 0x68  ; 'U' (ASCII 85)
    defb 0x66, 0x66, 0x62  ; 'V' (ASCII 86)
    defb 0x66, 0x88, 0x82  ; 'W' (ASCII 87)
    defb 0x66, 0x26, 0x66  ; 'X' (ASCII 88)
    defb 0x66, 0x62, 0x22  ; 'Y' (ASCII 89)
    defb 0x81, 0x22, 0x58  ; 'Z' (ASCII 90)
    defb 0x75, 0x55, 0x57  ; '[' (ASCII 91)
    defb 0x55, 0x22, 0x11  ; '\' (ASCII 92)
    defb 0x72, 0x22, 0x27  ; ']' (ASCII 93)
    defb 0x26, 0x00, 0x00  ; '^' (ASCII 94)
    defb 0x00, 0x00, 0x09  ; '_' (ASCII 95)
    defb 0x55, 0x20, 0x00  ; '`' (ASCII 96)
    defb 0x07, 0x14, 0x64  ; 'a' (ASCII 97)
    defb 0x55, 0x76, 0x67  ; 'b' (ASCII 98)
    defb 0x04, 0x55, 0x54  ; 'c' (ASCII 99)
    defb 0x11, 0x46, 0x64  ; 'd' (ASCII 100)
    defb 0x02, 0x68, 0x54  ; 'e' (ASCII 101)
    defb 0x26, 0x57, 0x55  ; 'f' (ASCII 102)
    defb 0x46, 0x64, 0x17  ; 'g' (ASCII 103)
    defb 0x55, 0x76, 0x66  ; 'h' (ASCII 104)
    defb 0x20, 0x72, 0x28  ; 'i' (ASCII 105)
    defb 0x10, 0x41, 0x62  ; 'j' (ASCII 106)
    defb 0x55, 0x67, 0x66  ; 'k' (ASCII 107)
    defb 0x55, 0x55, 0x62  ; 'l' (ASCII 108)
    defb 0x06, 0x88, 0x66  ; 'm' (ASCII 109)
    defb 0x07, 0x66, 0x66  ; 'n' (ASCII 110)
    defb 0x02, 0x66, 0x62  ; 'o' (ASCII 111)
    defb 0x07, 0x67, 0x55  ; 'p' (ASCII 112)
    defb 0x04, 0x64, 0x11  ; 'q' (ASCII 113)
    defb 0x04, 0x55, 0x55  ; 'r' (ASCII 114)
    defb 0x04, 0x52, 0x17  ; 's' (ASCII 115)
    defb 0x28, 0x22, 0x21  ; 't' (ASCII 116)
    defb 0x06, 0x66, 0x68  ; 'u' (ASCII 117)
    defb 0x06, 0x66, 0x62  ; 'v' (ASCII 118)
    defb 0x06, 0x88, 0x82  ; 'w' (ASCII 119)
    defb 0x06, 0x26, 0x66  ; 'x' (ASCII 120)
    defb 0x06, 0x64, 0x17  ; 'y' (ASCII 121)
    defb 0x08, 0x12, 0x58  ; 'z' (ASCII 122)
    defb 0x42, 0x52, 0x24  ; '{' (ASCII 123)
    defb 0x55, 0x00, 0x55  ; '|' (ASCII 124)
    defb 0x72, 0x12, 0x27  ; '}' (ASCII 125)
    defb 0x36, 0x00, 0x00  ; '~' (ASCII 126)
    defb 0x99, 0x96, 0x06  ; DEL (ASCII 127)

; =============================================================================
; DESCOMPRESOR DE GLIFO
; Input: A = char (ASCII 32-127)
; Output: HL = puntero a glyph_buffer con 8 bytes del glifo descomprimido
; Preserva: IY (requerido por z88dk)
; Destruye: AF, BC, DE
; =============================================================================
unpack_glyph:
    push bc
    push de
    
    ; Calcular offset: (char - 32) * 3
    sub 32
    ld l, a
    ld h, 0
    ld d, h
    ld e, l
    add hl, hl          ; *2
    add hl, de          ; *3
    ld de, font64_packed
    add hl, de          ; HL = puntero a glifo comprimido (3 bytes)
    
    ld de, glyph_buffer
    
    ; Desempaquetar 3 bytes -> 6 líneas (0-5)
    ld b, 3
unpack_loop:
    ld a, (hl)          ; Leer byte comprimido
    ld c, a             ; Guardar copia
    
    ; Nibble alto -> línea N
    srl a
    srl a
    srl a
    srl a               ; A = índice (0-9)
    push hl
    push bc
    ld hl, font_lut
    add a, l
    ld l, a
    jr nc, unpack_no_carry1
    inc h
unpack_no_carry1:
    ld a, (hl)          ; A = valor real desde LUT
    pop bc
    pop hl
    ld (de), a
    inc de
    
    ; Nibble bajo -> línea N+1
    ld a, c
    and 0x0F            ; A = índice bajo
    push hl
    push bc
    ld hl, font_lut
    add a, l
    ld l, a
    jr nc, unpack_no_carry2
    inc h
unpack_no_carry2:
    ld a, (hl)
    pop bc
    pop hl
    ld (de), a
    inc de
    
    inc hl              ; Siguiente byte comprimido
    djnz unpack_loop
    
    ; Líneas 6 y 7 siempre 0x00
    xor a
    ld (de), a
    inc de
    ld (de), a
    
    ld hl, glyph_buffer
    pop de
    pop bc
    ret

; =============================================================================
; SIStheme GRÁFICO - functions
; =============================================================================

; -----------------------------------------------------------------------------
; uint8_t* screen_line_addr(uint8_t y, uint8_t phys_x, uint8_t scanline)
; Stack: [IX+6]=y, [IX+7]=phys_x, [IX+8]=scanline
; -----------------------------------------------------------------------------
_screen_line_addr:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld a, (ix+6)
    add a, a
    ld l, a
    ld h, 0
    ld de, _screen_row_base
    add hl, de
    
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    
    ld a, (ix+8)
    add a, h
    ld h, a
    
    ld a, (ix+7)
    add a, l
    ld l, a
    jr nc, sla_done
    inc h
sla_done:
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; uint8_t* attr_addr(uint8_t y, uint8_t phys_x)
; Stack: [IX+6]=y, [IX+7]=phys_x
; -----------------------------------------------------------------------------
_attr_addr:
    ; OPTIMIZADO: Usa tabla precalculada en lugar de y*32+0x5800
    push ix
    ld ix, 0
    add ix, sp
    
    ; Lookup en tabla: _attr_row_base[y]
    ld a, (ix+4)        ; y
    add a, a            ; y*2 (tabla de words)
    ld l, a
    ld h, 0
    ld de, _attr_row_base
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a             ; HL = base de atributos de fila
    
    ; Añadir phys_x
    ld a, (ix+5)
    add a, l
    ld l, a
    jr nc, aa_done
    inc h
aa_done:
    pop ix
    ret

; -----------------------------------------------------------------------------
; Rutina interna para clear_line/clear_zone
; input: A = line Y, C = atributo
; Preserva: IX, IY (ya preservados por caller)
; -----------------------------------------------------------------------------
cli_internal:
    push bc
    push de
    push hl
    push af
    
    ; Calcular dirección de screen
    ld l, a
    ld h, 0
    add hl, hl
    ld de, _screen_row_base
    add hl, de
    
    ld e, (hl)
    inc hl
    ld d, (hl)
    
    ; Borrar 8 scanlines
    ld b, 8
cli_px_loop:
    push de
    push bc
    ld h, d
    ld l, e
    ld (hl), 0
    inc de
    ld bc, 31
    ldir
    pop bc
    pop de
    inc d
    djnz cli_px_loop
    
    ; Atributos
    pop af
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5800
    add hl, de
    
    ld (hl), c
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ldir
    
    pop hl
    pop de
    pop bc
    ret

; -----------------------------------------------------------------------------
; void clear_line(uint8_t y, uint8_t attr)
; Stack: [IX+6]=y, [IX+7]=attr
; -----------------------------------------------------------------------------
_clear_line:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld a, (ix+6)
    ld c, (ix+7)
    call cli_internal
    
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; void clear_zone(uint8_t start, uint8_t lines, uint8_t attr)
; Stack: [IX+6]=start, [IX+7]=lines, [IX+8]=attr
; -----------------------------------------------------------------------------
_clear_zone:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld a, (ix+6)        ; start
    ld b, (ix+7)        ; lines
    ld c, (ix+8)        ; attr
    
    ; Si lines == 0, salir
    ld d, a
    ld a, b
    or a
    jr z, cz_done
    ld a, d
    
cz_loop:
    call cli_internal
    inc a
    djnz cz_loop

cz_done:
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; void ldir_copy_fwd(void *dst, const void *src, uint16_t len)
; Stack: [IX+6,7]=dst, [IX+8,9]=src, [IX+10,11]=len
; -----------------------------------------------------------------------------
_ldir_copy_fwd:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld e, (ix+6)
    ld d, (ix+7)        ; DE = dst
    ld l, (ix+8)
    ld h, (ix+9)        ; HL = src
    ld c, (ix+10)
    ld b, (ix+11)       ; BC = len
    
    ld a, b
    or c
    jr z, lcf_done
    
    ldir

lcf_done:
    pop ix
    pop iy
    ret

; =============================================================================
; IMPRESIÓN 64 COLUMNAS
; =============================================================================

; -----------------------------------------------------------------------------
; void print_str64_char(uint8_t ch) __z88dk_fastcall
; Dibuja un carácter de 4 píxeles usando fuente 64 columnas
; input: L = carácter ASCII
; No preserva: AF, BC, DE, HL
; Preserva: IY (no lo usa)
; -----------------------------------------------------------------------------
_print_str64_char:
    push ix
    
    ; Validar carácter
    ld a, l
    cp 32
    jr c, p64_use_space
    cp 128
    jr c, p64_calc_font
p64_use_space:
    ld a, 32
    
p64_calc_font:
    ; A contiene el carácter validado (32-127)
    ; Descomprimir glifo a glyph_buffer
    call unpack_glyph       ; Input: A = char, Output: HL = glyph_buffer
    push hl                 ; Guardar puntero fuente descomprimida
    
    ; Calcular dirección screen
    ld a, (_g_ps64_y)
    add a, a
    ld e, a
    ld d, 0
    ld ix, _screen_row_base
    add ix, de
    ld l, (ix+0)
    ld h, (ix+1)            ; HL = base de fila
    
    ; Sumar col/2
    ld a, (_g_ps64_col)
    srl a
    ld e, a
    ld d, 0
    add hl, de              ; HL = dirección pantalla
    
    pop de                  ; DE = puntero fuente (glyph_buffer)
    
    ; Decidir lado izquierdo o derecho
    ld a, (_g_ps64_col)
    and 1
    jr nz, p64_right
    
    ; === LADO IZQUIERDO (columna par) ===
    ld a, (hl)
    and 0x0F                ; Conservar nibble derecho
    ld (hl), a
    inc h
    
    ld b, 7
p64_left_loop:
    ld a, (de)
    and 0xF0                ; Tomar nibble izquierdo de fuente
    ld c, a
    ld a, (hl)
    and 0x0F                ; Conservar nibble derecho de pantalla
    or c
    ld (hl), a
    inc de
    inc h
    djnz p64_left_loop
    jr p64_set_attr

p64_right:
    ; === LADO DERECHO (columna impar) ===
    ld a, (hl)
    and 0xF0                ; Conservar nibble izquierdo
    ld (hl), a
    inc h
    
    ld b, 7
p64_right_loop:
    ld a, (de)
    and 0x0F                ; Tomar nibble derecho de fuente
    ld c, a
    ld a, (hl)
    and 0xF0                ; Conservar nibble izquierdo de pantalla
    or c
    ld (hl), a
    inc de
    inc h
    djnz p64_right_loop
    
p64_set_attr:
    ; Calcular dirección atributo: 0x5800 + y*32 + col/2
    ld a, (_g_ps64_y)
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld bc, 0x5800
    add hl, bc
    
    ld a, (_g_ps64_col)
    srl a
    ld c, a
    ld b, 0
    add hl, bc
    
    ld a, (_g_ps64_attr)
    ld (hl), a
    
    pop ix
    ret

; -----------------------------------------------------------------------------
; void print_line64_fast(uint8_t y, const char *s, uint8_t attr)
; OPTIMIZED VERSION: Calcula dirección VRAM una vez y avanza linealmente
; NOTA: Compatible con _print_str64_char que salta scanline 0
; Stack: [IX+6]=y, [IX+7,8]=s, [IX+9]=attr
; -----------------------------------------------------------------------------
_print_line64_fast:
    push iy
    push ix
    ld ix, 0
    add ix, sp

    ; ----------------------------------------------------------
    ; FIX (punto 2): sincronizar contexto global usado por
    ; print_str64_char / _print_str64_char
    ; No cambia el renderizado de esta rutina.
    ; ----------------------------------------------------------
    ld a, (ix+6)            ; y
    ld (_g_ps64_y), a
    xor a
    ld (_g_ps64_col), a     ; col = 0 (inicio de línea)
    ld a, (ix+9)            ; attr
    ld (_g_ps64_attr), a
    ; ----------------------------------------------------------
    
    ; 1. CALCULAR DIRECCIÓN DE PANTALLA (Solo una vez)
    ld a, (ix+6)            ; Y (0-23)
    add a, a                ; Y * 2 (offset en tabla words)
    ld l, a
    ld h, 0
    ld de, _screen_row_base
    add hl, de
    
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a                 ; HL = Dirección base de la fila en VRAM
    
    ; 2. CALCULAR DIRECCIÓN DE ATRIBUTOS Y GUARDARLA
    ld a, (ix+6)            ; Y
    ld e, a
    ld d, 0
    ex de, hl               ; DE = VRAM base, HL = Y
    add hl, hl              ; Y*2
    add hl, hl              ; Y*4
    add hl, hl              ; Y*8
    add hl, hl              ; Y*16
    add hl, hl              ; Y*32
    ld bc, 0x5800
    add hl, bc              ; HL = Attr Address base
    push hl                 ; Guardar Attr Addr en Stack
    
    ex de, hl               ; HL = VRAM base
    
    ; Preparar registros para el bucle
    ld e, (ix+7)
    ld d, (ix+8)            ; DE = Puntero al string (char *s)
    ld c, (ix+9)            ; C = Atributo (guardar para luego)
    
    ld b, 32                ; B = Contador de columnas físicas (32 bytes = 64 chars)

pl64_loop:
    push bc                 ; Guardar contador columnas
    
    ; --- CARACTER IZQUIERDO (Bits 4-7) ---
    ld a, (de)              ; Leer char string
    or a
    jp z, pl64_pad          ; Fin de string -> Rellenar con espacios
    inc de
    
    push de                 ; Guardar puntero string
    push hl                 ; Guardar puntero VRAM
    
    ; Obtener glifo usando fuente comprimida
    ; A ya contiene el char (32-127)
    call unpack_glyph       ; Retorna HL = puntero a glyph_buffer
    push hl
    pop iy                  ; IY = Puntero al glifo descomprimido
    
    pop hl                  ; Recuperar VRAM
    
    ; COMPATIBILIDAD: Borrar scanline 0, luego dibujar 7 scanlines (1-7)
    ld a, (hl)
    and 0x0F                ; Conservar nibble derecho
    ld (hl), a
    inc h                   ; Saltar a scanline 1
    
    ; Dibujar 7 scanlines (Lado Izquierdo - nibble alto)
    ld b, 7
pl64_char1:
    ld a, (iy+0)            ; Leer fuente
    and 0xF0                ; Quedarse con parte izquierda
    ld c, a
    ld a, (hl)              ; Leer pantalla
    and 0x0F                ; Quedarse con parte derecha existente
    or c                    ; Combinar
    ld (hl), a              ; Escribir
    inc h                   ; Siguiente scanline
    inc iy
    djnz pl64_char1
    
    ; Corregir HL (volver a scanline 0: restamos 8 porque hicimos 1+7 inc h)
    ld a, h
    sub 8
    ld h, a
    
    pop de                  ; Recuperar puntero string

    ; --- CARACTER DERECHO (Bits 0-3) ---
    ld a, (de)
    or a
    jp z, pl64_pad_right    ; Fin de string justo en medio del byte
    inc de
    
    push de
    push hl
    
    ; Obtener glifo usando fuente comprimida
    ; A ya contiene el char (32-127)
    call unpack_glyph       ; Retorna HL = puntero a glyph_buffer
    push hl
    pop iy
    
    pop hl
    
    ; COMPATIBILIDAD: Borrar scanline 0, luego dibujar 7 scanlines (1-7)
    ld a, (hl)
    and 0xF0                ; Conservar nibble izquierdo
    ld (hl), a
    inc h                   ; Saltar a scanline 1
    
    ld b, 7
pl64_char2:
    ld a, (iy+0)            ; Fuente
    and 0x0F                ; Nibble derecho
    ld c, a
    ld a, (hl)              ; Pantalla
    and 0xF0                ; Nibble izquierdo
    or c
    ld (hl), a
    inc h
    inc iy
    djnz pl64_char2
    
    ; Corregir HL
    ld a, h
    sub 8
    ld h, a
    
    pop de                  ; Recuperar string
    
    ; --- AVANZAR Y CONTINUAR ---
    inc hl                  ; Avanzar 1 byte en VRAM (horizontal)
    
    pop bc                  ; Recuperar contador columnas
    djnz pl64_loop
    jp pl64_attrs

; --- PADDING: String terminó ---
pl64_pad:
    ; Borrar nibble izquierdo en todas las scanlines
    push hl
    ld b, 8
pl64_pad_L:
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    djnz pl64_pad_L
    pop hl
    
pl64_pad_right:
    ; Borrar nibble derecho en todas las scanlines
    push hl
    ld b, 8
pl64_pad_R:
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    djnz pl64_pad_R
    pop hl
    
    inc hl                  ; Siguiente byte VRAM
    
    pop bc                  ; Contador
    dec b
    jp z, pl64_attrs        ; Terminamos
    
    ; Continuar rellenando resto de línea con ceros
pl64_pad_full:
    push bc
    push hl
    
    ; Borrar byte completo (8 scanlines)
    ld b, 8
pl64_pad_byte:
    ld (hl), 0
    inc h
    djnz pl64_pad_byte
    
    pop hl
    inc hl
    pop bc
    djnz pl64_pad_full

pl64_attrs:
    ; 3. PINTADO RÁPIDO DE ATRIBUTOS (32 bytes con LDIR)
    pop hl                  ; Recuperar dirección base de atributos
    
    ld a, (ix+9)            ; Atributo
    ld (hl), a              ; Primer byte
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ldir                    ; Copiar a los 31 restantes
    
    pop ix
    pop iy
    ret

    
; -----------------------------------------------------------------------------
; void draw_indicator(uint8_t y, uint8_t phys_x, uint8_t attr)
; Dibuja un círculo de state 8x8
; Stack: [IX+6]=y, [IX+7]=phys_x, [IX+8]=attr
; -----------------------------------------------------------------------------
_draw_indicator:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld a, (ix+6)            ; y
    ld c, (ix+8)            ; attr
    
    ; Calcular dirección screen
    ld l, a
    ld h, 0
    add hl, hl
    ld de, _screen_row_base
    add hl, de
    
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a                 ; HL = base de fila
    
    ld e, (ix+7)            ; phys_x
    ld d, 0
    add hl, de              ; HL = dirección exacta pixel
    
    ; Decidir forma basada en estado AWAY
    ld a, (_irc_is_away)
    or a
    jr nz, draw_ind_away
    
draw_ind_online:
    ; Círculo Sólido
    ld (hl), 0x00
    inc h
    ld (hl), 0x3C
    inc h
    ld (hl), 0x7E
    inc h
    ld (hl), 0x7E
    inc h
    ld (hl), 0x7E
    inc h
    ld (hl), 0x7E
    inc h
    ld (hl), 0x3C
    inc h
    ld (hl), 0x00
    jr draw_ind_attr

draw_ind_away:
    ; Forma: Círculo medio lleno (Mitad derecha sólida, mitad izquierda vacía)
    ; Mantener el mismo diámetro que el círculo sólido (0x00 arriba/abajo)
    ld (hl), 0x00
    inc h
    ld (hl), 0x3C
    inc h
    ld (hl), 0x4E
    inc h
    ld (hl), 0x4E
    inc h
    ld (hl), 0x4E
    inc h
    ld (hl), 0x4E
    inc h
    ld (hl), 0x3C
    inc h
    ld (hl), 0x00
    
draw_ind_attr:
    ld a, (ix+6)            ; y
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5800
    add hl, de
    
    ld e, (ix+7)            ; phys_x
    ld d, 0
    add hl, de
    
    ld (hl), c              ; attr
    
    pop ix
    pop iy
    ret

; =============================================================================
; UTILIDADES DE STRING
; =============================================================================

; -----------------------------------------------------------------------------
; char* str_append(char *dst, const char *src)
; Copia src al final de dst, retorna puntero al NULL terminador
; Convención: cdecl (caller limpia pila)
; -----------------------------------------------------------------------------
_str_append:
    pop bc                  ; Retorno
    pop hl                  ; dst
    pop de                  ; src
    push de
    push hl
    push bc
    
sapp_loop:
    ld a, (de)
    ld (hl), a
    or a
    jr z, sapp_done
    inc hl
    inc de
    jr sapp_loop
    
sapp_done:
    ; HL apunta al NULL terminador
    ret

; -----------------------------------------------------------------------------
; char* str_append_n(char *dst, const char *src, uint8_t max)
; Copia hasta max chars de src a dst, siempre termina con NULL
; Stack: [IX+6,7]=dst, [IX+8,9]=src, [IX+10]=max
; -----------------------------------------------------------------------------
_str_append_n:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld l, (ix+6)
    ld h, (ix+7)            ; HL = dst
    ld e, (ix+8)
    ld d, (ix+9)            ; DE = src
    ld b, (ix+10)           ; B = max
    
    ld a, b
    or a
    jr z, sappn_terminate
    
sappn_loop:
    ld a, (de)
    or a
    jr z, sappn_terminate
    ld (hl), a
    inc hl
    inc de
    djnz sappn_loop
    
sappn_terminate:
    xor a
    ld (hl), a
    
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; int st_stricmp(const char *a, const char *b)
; Comparación case-insensitive
; Stack: [IX+6,7]=a, [IX+8,9]=b
; Retorna: 0 si iguales, <0 si a<b, >0 si a>b
; -----------------------------------------------------------------------------
_st_stricmp:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld l, (ix+6)
    ld h, (ix+7)            ; HL = a
    ld e, (ix+8)
    ld d, (ix+9)            ; DE = b

stricmp_loop:
    ld a, (hl)
    ld c, (de)
    
    ; Convertir A a minúscula
    cp 'A'
    jr c, stricmp_a_done
    cp 'Z' + 1
    jr nc, stricmp_a_done
    add a, 32
stricmp_a_done:
    ld b, a                 ; B = a normalizado

    ; Convertir C a minúscula
    ld a, c
    cp 'A'
    jr c, stricmp_b_done
    cp 'Z' + 1
    jr nc, stricmp_b_done
    add a, 32
stricmp_b_done:
    
    ; Comparar
    sub b                   ; A = b_norm - a_norm
    jr nz, stricmp_diff
    
    ; Iguales - ¿fin de string?
    ld a, b
    or a
    jr z, stricmp_equal
    
    inc hl
    inc de
    jr stricmp_loop

stricmp_diff:
    ; Extender signo a 16 bits
    ld l, a
    ld h, 0
    bit 7, l
    jr z, stricmp_done
    dec h
    jr stricmp_done

stricmp_equal:
    ld hl, 0
stricmp_done:
    pop ix
    pop iy
    ret

; -----------------------------------------------------------------------------
; const char* st_stristr(const char *hay, const char *needle)
; Búsqueda case-insensitive de substring
; Retorna: puntero a primera ocurrencia, o NULL
; Stack: [IX+6,7]=hay, [IX+8,9]=needle
; -----------------------------------------------------------------------------
_st_stristr:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ; Cargar argumentos
    ld l, (ix+6)
    ld h, (ix+7)            ; HL = hay
    ld e, (ix+8)
    ld d, (ix+9)            ; DE = needle
    
    ; Validar needle
    ld a, d
    or e
    jr z, sstr_ret_hay      ; needle NULL -> return hay
    ld a, (de)
    or a
    jr z, sstr_ret_hay      ; needle vacío -> return hay
    
sstr_outer:
    ld a, (hl)
    or a
    jr z, sstr_fail         ; Fin de hay sin match
    
    push hl                 ; Guardar inicio actual
    push de                 ; Guardar inicio de needle
    
sstr_inner:
    ld a, (de)
    or a
    jr z, sstr_found        ; Fin de needle -> match!
    
    ld c, a                 ; C = char de needle
    ld a, (hl)
    or a
    jr z, sstr_next_pos     ; Fin de hay en medio
    
    ; Normalizar A (hay) a mayúscula
    cp 'a'
    jr c, sstr_hay_done
    cp 'z' + 1
    jr nc, sstr_hay_done
    sub 32
sstr_hay_done:
    ld b, a
    
    ; Normalizar C (needle) a mayúscula
    ld a, c
    cp 'a'
    jr c, sstr_needle_done
    cp 'z' + 1
    jr nc, sstr_needle_done
    sub 32
sstr_needle_done:
    
    cp b
    jr nz, sstr_next_pos    ; No coinciden
    
    ; Coinciden, avanzar
    inc hl
    inc de
    jr sstr_inner
    
sstr_next_pos:
    pop de                  ; Restaurar needle
    pop hl                  ; Restaurar hay
    inc hl                  ; Siguiente posición
    jr sstr_outer
    
sstr_found:
    pop de                  ; Limpiar stack
    pop hl                  ; HL = posición del match
    pop ix
    pop iy
    ret

sstr_ret_hay:
    ; HL ya tiene hay
    pop ix
    pop iy
    ret

sstr_fail:
    ld hl, 0
    pop ix
    pop iy
    ret

; =============================================================================
; CONVERSIONES NUMÉRICAS
; =============================================================================

; -----------------------------------------------------------------------------
; char* u16_to_dec(char *dst, uint16_t v)
; Convierte uint16 a string decimal, retorna puntero al final
; Convención: cdecl
; -----------------------------------------------------------------------------
; =============================================================================
; CONVERSIONES NUMÉRICAS (CORREGIDO: STACK FIX + 5 DÍGITOS)
; =============================================================================

; -----------------------------------------------------------------------------
; char* u16_to_dec(char *dst, uint16_t v)
; Convierte uint16 a string decimal, retorna puntero al final
; Convención: cdecl
; -----------------------------------------------------------------------------
_u16_to_dec:
    pop bc                  ; Retorno
    pop de                  ; dst
    pop hl                  ; v
    push hl
    push de
    push bc
    
    push ix
    ld ix, 0                ; IXL = flag "ya imprimimos algo"
    
    ld bc, -10000
    call u16_digit
    ld bc, -1000
    call u16_digit
    ld bc, -100
    call u16_digit
    ld bc, -10
    call u16_digit
    
    ; Unidades (siempre se imprime)
    ld a, l
    add a, '0'
    ld (de), a
    inc de
    
    xor a
    ld (de), a              ; NULL terminator
    
    ex de, hl               ; Retornar puntero al final
    pop ix
    ret

; =============================================================================
; char* u16_to_dec3(char *dst, uint16_t v)
; Versión reducida: imprime hasta 4 dígitos (1000,100,10,1). Ideal para contadores.
; Retorna puntero al NULL (igual que u16_to_dec).
; =============================================================================

_u16_to_dec3:
    pop bc                  ; return
    pop de                  ; dst
    pop hl                  ; v
    push hl
    push de
    push bc

    push ix
    ld ix, 0                ; IXL = flag "ya imprimimos algo"

    ld bc, -1000
    call u16_digit
    ld bc, -100
    call u16_digit
    ld bc, -10
    call u16_digit

    ; Unidades (siempre)
    ld a, l
    add a, '0'
    ld (de), a
    inc de

    xor a
    ld (de), a              ; NULL

    ex de, hl               ; devolver puntero al final (HL)
    pop ix
    ret

; Subrutina: extrae un dígito restando BC repetidamente
; input: HL = valor, BC = -potencia, DE = buffer
; output: HL = residuo, DE avanzado si se imprimió
u16_digit:
    ld a, '0' - 1
u16_sub_loop:
    inc a
    add hl, bc
    jr c, u16_sub_loop
    
    ; Deshacer última resta
    sbc hl, bc
    
    ; ¿Imprimir?
    cp '0'
    jr nz, u16_do_print
    
    ; Es cero - ¿ya imprimimos algo?
    push af
    ld a, ixl
    or a
    jr nz, u16_print_zero
    pop af
    ret                     ; Suprimir cero inicial

u16_print_zero:
    pop af
u16_do_print:
    ld (de), a
    inc de
    ld ixl, 1               ; Marcar que ya imprimimos
    ret

; -----------------------------------------------------------------------------
; uint16_t str_to_u16(const char *s) __z88dk_fastcall
; Convierte string decimal a uint16
; input: HL = puntero al string
; Retorna: HL = valor
; -----------------------------------------------------------------------------
_str_to_u16:
    ld de, 0                ; Acumulador
    
stu16_loop:
    ld a, (hl)
    sub '0'
    jr c, stu16_done        ; < '0'
    cp 10
    jr nc, stu16_done       ; > '9'
    
    ; DE = DE * 10 + A
    push hl
    push af
    
    ; DE * 10 = DE * 8 + DE * 2
    ld l, e
    ld h, d
    add hl, hl              ; *2
    ld d, h
    ld e, l
    add hl, hl              ; *4
    add hl, hl              ; *8
    add hl, de              ; *10
    
    pop af
    ld e, a
    ld d, 0
    add hl, de
    ex de, hl               ; DE = nuevo acumulador
    
    pop hl
    inc hl
    jr stu16_loop
    
stu16_done:
    ex de, hl               ; Retorno en HL
    ret

; =============================================================================
; PARSING IRC
; =============================================================================

; -----------------------------------------------------------------------------
; char* skip_to(char *s, char c)
; Busca char c en s, reemplaza con NULL, retorna s+1
; Convención: cdecl
; -----------------------------------------------------------------------------
_skip_to:
    pop bc                  ; Retorno
    pop hl                  ; s
    pop de                  ; c (en E)
    push de
    push hl
    push bc
    
skpto_loop:
    ld a, (hl)
    or a
    jr z, skpto_end         ; Fin de string
    cp e
    jr z, skpto_found
    inc hl
    jr skpto_loop
    
skpto_found:
    ld (hl), 0              ; Terminar string aquí
    inc hl                  ; Retornar siguiente posición
    
skpto_end:
    ret

; =============================================================================
; CÓDIGOS IRC
; =============================================================================

; -----------------------------------------------------------------------------
; void strip_irc_codes(char *s)
; Elimina códigos de control IRC in-situ (^B, ^C, ^O, ^R, ^_)
; Stack: [IX+6,7]=s
; -----------------------------------------------------------------------------
_strip_irc_codes:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld l, (ix+6)
    ld h, (ix+7)            ; HL = lectura
    ld d, h
    ld e, l                 ; DE = escritura

strip_loop:
    ld a, (hl)
    or a
    jr z, strip_done
    
    ; Filtrar códigos de un byte
    cp 0x02                 ; Bold
    jr z, strip_skip
    cp 0x0F                 ; Reset
    jr z, strip_skip
    cp 0x16                 ; Reverse
    jr z, strip_skip
    cp 0x1F                 ; Underline
    jr z, strip_skip
    
    ; Color (^C + dígitos opcionales)
    cp 0x03
    jr z, strip_color
    
    ; Carácter normal - copiar
    ld (de), a
    inc de
strip_skip:
    inc hl
    jr strip_loop

strip_color:
    inc hl                  ; Saltar ^C
    
    ; Saltar hasta 2 dígitos de foreground
    ld a, (hl)
    call strip_is_digit
    jr nc, strip_loop
    inc hl
    ld a, (hl)
    call strip_is_digit
    jr nc, strip_check_bg
    inc hl

strip_check_bg:
    ld a, (hl)
    cp ','
    jr nz, strip_loop
    inc hl                  ; Saltar coma
    
    ; Saltar hasta 2 dígitos de background
    ld a, (hl)
    call strip_is_digit
    jr nc, strip_loop
    inc hl
    ld a, (hl)
    call strip_is_digit
    jr nc, strip_loop
    inc hl
    jr strip_loop

strip_done:
    xor a
    ld (de), a              ; NULL terminator
    pop ix
    pop iy
    ret

; Subrutina: ¿es A un dígito?
; Retorna: Carry set si es dígito
strip_is_digit:
    cp '0'
    ret c                   ; < '0' -> no es dígito (carry clear)
    cp '9' + 1
    ccf                     ; > '9' -> carry clear, else carry set
    ret

; =============================================================================
; UART
; =============================================================================

; -----------------------------------------------------------------------------
; void uart_send_string(const char *s) __z88dk_fastcall
; Envía string por UART
; input: HL = puntero al string
; -----------------------------------------------------------------------------
_uart_send_string:
usend_loop:
    ld a, (hl)
    or a
    ret z                   ; Fin de string
    
    push hl
    ld l, a
    call _ay_uart_send
    pop hl
    inc hl
    jr usend_loop

; =============================================================================
; void reapply_screen_attributes(void)
; =============================================================================
_reapply_screen_attributes:
    push iy

    ; 1. Borde
    ld a, (_BORDER_COLOR)
    out (0xFE), a

    ; 2. Banner (0x5800)
    ld hl, 0x5800
    ld bc, 32
    ld a, (_ATTR_BANNER)
    call _fast_fill_attr

    ; 3. Separador Superior (0x5820)
    ld hl, 0x5820
    ld bc, 32
    ld a, (_ATTR_MAIN_BG)
    call _fast_fill_attr

    ; 4. Área Chat (0x5840 - 18 líneas)
    ld hl, 0x5840
    ld bc, 576
    ld a, (_ATTR_MAIN_BG)
    call _fast_fill_attr

    ; 5. Separador Inferior (0x5A80)
    ld hl, 0x5A80
    ld bc, 32
    ld a, (_ATTR_MAIN_BG)
    call _fast_fill_attr

    ; 6. Barra Estado (0x5AA0)
    ld hl, 0x5AA0
    ld bc, 32
    ld a, (_ATTR_STATUS)
    call _fast_fill_attr

    ; 7. Input (0x5AC0)
    ld hl, 0x5AC0
    ld bc, 64
    ld a, (_ATTR_INPUT_BG)
    call _fast_fill_attr

    ; 8. AVISAR A TODOS LOS SISTEMAS DE REPINTADO
    ld hl, _force_status_redraw   ; Avisa al renderizador interno
    ld (hl), 1
    
    ld hl, _status_bar_dirty      ; Avisa al bucle Main de C <--- CRÍTICO
    ld (hl), 1

    pop iy
    ret

; Rutina auxiliar (Mantenla igual que antes)
_fast_fill_attr:
    ld (hl), a
    dec bc
    ld a, b
    or c
    ret z
    ld d, h
    ld e, l
    inc de
    ldir
    ret

; =============================================================================
; void cls_fast(void)
; Borrado completo de pantalla (Píxeles a 0 + Atributos del tema)
; Reutiliza las rutinas de reapply para ahorrar espacio.
; =============================================================================

_cls_fast:
    push iy
    
    ; 1. BORRAR PÍXELES (Bitmap: 0x4000, longitud 6144 bytes)
    ld hl, 0x4000       ; Inicio VRAM bitmap
    ld bc, 6144         ; 192 líneas * 32 bytes
    xor a               ; A = 0 (Pixels apagados)
    
    ; ¡REUTILIZACIÓN 1! Usamos el rellenador que ya creamos
    call _fast_fill_attr 
    
    ; 2. PINTAR COLORES
    ; ¡REUTILIZACIÓN 2! Llamamos a la rutina de temas
    pop iy              ; Restaurar IY antes de saltar (tail call optimization)
    jp _reapply_screen_attributes

; =============================================================================
; void uart_drain_to_buffer(void)
; Lee bytes del UART y los mete en el Ring Buffer lo más rápido posible.
; CRÍTICO: Minimiza la latencia entre bytes para evitar pérdida de datos en AY.
; =============================================================================

_uart_drain_to_buffer:
    push iy                 ; Preservar IY (estándar z88dk)

    ; Preparar contador en BC' (banco alternativo) para que las llamadas
    ; puedan destruir BC sin necesidad de push/pop por iteración.
    ld a, (_uart_drain_limit)
    or a
    jr z, drain_setup_safety

    ; Caso con límite (ej: 32 bytes)
    exx
    ld b, 0
    ld c, a                 ; BC' = limit
    exx
    jr drain_loop_start

drain_setup_safety:
    ; Caso límite=0 -> Usar seguridad de 4096 iteraciones
    exx
    ld bc, 4096             ; BC' = 4096
    exx

drain_loop_start:
    ; 2. ¿Hay datos disponibles?
    call _ay_uart_ready     ; Retorna L=1 (Sí) o 0 (No)
    ld a, l
    or a
    jr z, drain_exit        ; Si L=0, no hay más datos -> Salir

    ; 3. Leer byte
    call _ay_uart_read      ; Retorna byte en L

    ; 4. Meter en Ring Buffer
    call _rb_push           ; Retorna L=1 (Éxito) o 0 (Fallo/Lleno)
    ld a, l
    or a
    jr z, drain_exit        ; Si buffer lleno, PARAR para no romper framing

    ; Decrementar contador en BC' (banco alternativo)
    exx
    dec bc
    ld a, b
    or c
    exx
    jr nz, drain_loop_start ; Si BC' > 0, seguir

drain_exit:
    pop iy
    ret

; =============================================================================
; void scroll_main_zone(void)
; Scroll optimizado de la zona de chat (líneas 2-19 -> 2-18).
; Versión UNROLLED: elimina tabla y loop interno manteniendo exactamente
; los mismos 5 bloques y longitudes que la versión original.
; =============================================================================
_scroll_main_zone:
    push iy
    di

    xor a                   ; A = offset scanline (0..7)

smz_scanline_loop:
    ; ---------------------------------------------------------
    ; BLOQUE 1: Filas 3-7 -> 2-6 (Src: 0x4060, Dest: 0x4040, Len: 160)
    ; ---------------------------------------------------------
    ld h, 0x40
    ld l, 0x60
    ld d, 0x40
    ld e, 0x40

    ld c, a                 ; C = offset
    ld a, h
    add a, c
    ld h, a
    ld a, d
    add a, c
    ld d, a
    ld a, c                 ; restaurar A=offset

    ld bc, 160
    ldir

    ; ---------------------------------------------------------
    ; BLOQUE 2: Fila 8 -> 7 (Src: 0x4800, Dest: 0x40E0, Len: 32)
    ; ---------------------------------------------------------
    ld h, 0x48
    ld l, 0x00
    ld d, 0x40
    ld e, 0xE0

    ld c, a
    ld a, h
    add a, c
    ld h, a
    ld a, d
    add a, c
    ld d, a
    ld a, c

    ld bc, 32
    ldir

    ; ---------------------------------------------------------
    ; BLOQUE 3: Filas 9-15 -> 8-14 (Src: 0x4820, Dest: 0x4800, Len: 224)
    ; ---------------------------------------------------------
    ld h, 0x48
    ld l, 0x20
    ld d, 0x48
    ld e, 0x00

    ld c, a
    ld a, h
    add a, c
    ld h, a
    ld a, d
    add a, c
    ld d, a
    ld a, c

    ld bc, 224
    ldir

    ; ---------------------------------------------------------
    ; BLOQUE 4: Fila 16 -> 15 (Src: 0x5000, Dest: 0x48E0, Len: 32)
    ; ---------------------------------------------------------
    ld h, 0x50
    ld l, 0x00
    ld d, 0x48
    ld e, 0xE0

    ld c, a
    ld a, h
    add a, c
    ld h, a
    ld a, d
    add a, c
    ld d, a
    ld a, c

    ld bc, 32
    ldir

    ; ---------------------------------------------------------
    ; BLOQUE 5: Filas 17-19 -> 16-18 (Src: 0x5020, Dest: 0x5000, Len: 96)
    ; ---------------------------------------------------------
    ld h, 0x50
    ld l, 0x20
    ld d, 0x50
    ld e, 0x00

    ld c, a
    ld a, h
    add a, c
    ld h, a
    ld a, d
    add a, c
    ld d, a
    ld a, c

    ld bc, 96
    ldir

    ; Siguiente scanline (0..7)
    inc a
    cp 8
    jr nz, smz_scanline_loop

    ; Scroll atributos (17 filas: 3->2 ... 19->18) (idéntico a tu versión)
    ld de, 0x5840
    ld hl, 0x5860
    ld bc, 544
    ldir

    ; Limpiar última línea (19) píxeles + atributos (idéntico a tu versión)
    ld a, (_current_attr)
    ld c, a
    ld a, 19
    call cli_internal

    ei
    pop iy
    ret


; =============================================================================
; void tokenize_params(char *par, uint8_t max_params)
; Trocea un string IRC separando por espacios y rellenando el array global irc_params
; Modifica el string 'par' in-situ (reemplaza espacios por NULLs)
; Stack: [IX+6,7]=par, [IX+8]=max_params
; =============================================================================
_tokenize_params:
    push iy
    push ix
    ld ix, 0
    add ix, sp

    ; Inicializar contador a 0
    xor a
    ld (_irc_param_count), a

    ; HL = par (string a trocear)
    ld l, (ix+6)
    ld h, (ix+7)

    ; Comprobar string nulo
    ld a, h
    or l
    jp z, tp_exit
    ld a, (hl)
    or a
    jp z, tp_exit

    ; B = max_params (si es 0, usar 10 por defecto)
    ld a, (ix+8)
    or a
    jr nz, tp_check_max
    ld a, 10                ; IRC_MAX_PARAMS default

tp_check_max:
    cp 11                   ; > 10?
    jr c, tp_max_ok
    ld a, 10

tp_max_ok:
    ld b, a                 ; B = slots restantes (1..10)

    ; IY = puntero al array de punteros (_irc_params)
    ld iy, _irc_params

    ; --- Saltar espacios/colons iniciales ---
tp_skip_leading:
    ld a, (hl)
    or a
    jp z, tp_exit           ; fin de string
    cp ' '
    jr z, tp_skip_lead_next
    cp ':'
    jr nz, tp_main_loop     ; encontrado inicio de token

tp_skip_lead_next:
    inc hl
    jr tp_skip_leading

    ; --- BUCLE PRINCIPAL DE TOKENIZADO ---
    ; HL = puntero actual en el string
    ; IY = puntero actual en el array irc_params
    ; B  = slots restantes
tp_main_loop:
    ; Guard estricto: si no hay slots, NO escribir en _irc_params.
    ld a, b
    or a
    jr z, tp_exit

    ; Guardar inicio del token en irc_params[count]
    ld (iy+0), l
    ld (iy+1), h
    inc iy
    inc iy                  ; avanzar al siguiente slot (2 bytes por puntero)

    ; Incrementar cuenta
    ld a, (_irc_param_count)
    inc a
    ld (_irc_param_count), a

    ; Decrementar cupo max
    dec b
    jr z, tp_exit           ; si llegamos al max, paramos

    ; Buscar fin del token (espacio o NULL)
tp_scan_word:
    ld a, (hl)
    or a
    jp z, tp_exit           ; fin de string
    cp ' '
    jr z, tp_terminate
    inc hl
    jr tp_scan_word

tp_terminate:
    ld (hl), 0              ; terminar token con NULL
    inc hl                  ; avanzar

    ; Saltar espacios entre tokens
tp_skip_spaces:
    ld a, (hl)
    or a
    jp z, tp_exit           ; fin de string
    cp ' '
    jr nz, tp_main_loop     ; encontrado siguiente token
    inc hl
    jr tp_skip_spaces

tp_exit:
    pop ix
    pop iy
    ret


; =============================================================================
; char *sb_append(char *dst, const char *src, const char *limit)
; Concatena src en dst asegurando no pasar de limit. Retorna nuevo dst en HL.
; Stack: [IX+4,5]=dst, [IX+6,7]=src, [IX+8,9]=limit
; =============================================================================
_sb_append:
    push ix
    ld ix, 0
    add ix, sp
    
    ; Cargar argumentos
    ld l, (ix+4)
    ld h, (ix+5)            ; HL = dst
    ld e, (ix+6)
    ld d, (ix+7)            ; DE = src
    ld c, (ix+8)
    ld b, (ix+9)            ; BC = limit
    
sba_loop:
    ; 1. Chequear límite: if (dst >= limit) exit
    ; Comparamos HL con BC usando resta temporal
    push hl
    or a                    ; Clear carry
    sbc hl, bc              ; HL = dst - limit
    pop hl
    jr nc, sba_done         ; Si no carry, dst >= limit, salir
    
    ; 2. Leer fuente
    ld a, (de)
    or a
    jr z, sba_done          ; Si es NULL, fin del string
    
    ; 3. Copiar
    ld (hl), a
    inc hl
    inc de
    jr sba_loop
    
sba_done:
    ; HL ya contiene el nuevo dst (valor de retorno)
    pop ix
    ret

; =============================================================================
; OPTIMIZED C FUNCTIONS REPLACEMENT
; High-performance replacements for spectalk.c bottlenecks
; =============================================================================

; Variables externas definidas en C (spectalk.c)
EXTERN _main_newline
EXTERN _main_col
EXTERN _main_line
EXTERN _current_attr
EXTERN _g_ps64_y
EXTERN _g_ps64_col
EXTERN _g_ps64_attr
EXTERN _ignore_count
EXTERN _ignore_list

; Funciones externas necesarias
EXTERN _print_str64_char
EXTERN _st_stricmp

; -----------------------------------------------------------------------------
; void main_putc(char c) __z88dk_fastcall
; Imprime un caracter gestionando saltos de línea y coordenadas.
; L = c
; -----------------------------------------------------------------------------
PUBLIC _main_putc
_main_putc:
    ld a, l
    cp 10               ; ¿Es '\n'?
    jp z, _main_newline
    
    ld b, a             ; Guardar caracter en B
    
    ; Chequear límite de columna (SCREEN_COLS = 64)
    ld a, (_main_col)
    cp 64
    jr c, mputc_no_wrap
    
    ; Necesitamos wrap - guardar B antes de llamar a main_newline
    push bc
    call _main_newline
    pop bc
    
mputc_no_wrap:
    ; Configurar variables globales para el driver de video (bypasea print_char64 de C)
    ld a, (_main_line)
    ld (_g_ps64_y), a
    
    ld a, (_main_col)
    ld (_g_ps64_col), a
    
    ld a, (_current_attr)
    ld (_g_ps64_attr), a
    
    ; Llamar al driver (L = caracter)
    ld l, b
    call _print_str64_char
    
    ; Incrementar columna: main_col++
    ld hl, _main_col
    inc (hl)
    ret

; -----------------------------------------------------------------------------
; void main_puts(const char *s) __z88dk_fastcall
; Imprime una cadena usando la versión optimizada de putc.
; HL = string
; -----------------------------------------------------------------------------

PUBLIC _main_puts
_main_puts:
    ld d, h
    ld e, l             ; DE = puntero string
    
puts_loop:
    ld a, (de)
    or a
    ret z               ; Fin de cadena (\0)
    
    push de             ; Guardar puntero
    ld l, a             ; Argumento para main_putc (L)
    call _main_putc
    pop de              ; Recuperar puntero
    
    inc de
    jr puts_loop


; -----------------------------------------------------------------------------
; uint8_t is_ignored(const char *nick) __z88dk_fastcall
; Verifica si un nick está en la lista de ignorados (Array de 16 bytes stride).
; HL = nick
; Retorna L=1 (true) o L=0 (false)
; -----------------------------------------------------------------------------
PUBLIC _is_ignored
_is_ignored:
    ld a, (_ignore_count)
    or a
    jr z, ign_fail      ; Si count == 0, retornar 0
    
    ld b, a             ; B = contador de bucle
    ld de, _ignore_list ; DE = puntero al inicio de la lista
    
ign_loop:
    ; Guardar contexto
    push bc
    push de
    push hl
    
    ; Preparar llamada a st_stricmp(list_item, nick)
    ; Convención C (stack): push right-to-left
    push hl             ; Arg2: nick
    push de             ; Arg1: list_item
    call _st_stricmp
    pop de              ; Limpiar pila
    pop de
    
    ; Resultado en HL. Si es 0, hay coincidencia.
    ld a, h
    or l
    jr z, ign_match
    
    ; Restaurar contexto
    pop hl              ; nick
    pop de              ; list_ptr actual
    pop bc              ; contador
    
    ; Avanzar DE + 16 (tamaño fijo de cada entrada en ignore_list)
    ld a, e
    add a, 16
    ld e, a
    jr nc, ign_next
    inc d
    
ign_next:
    djnz ign_loop       ; Siguiente iteración
    
ign_fail:
    ld l, 0
    ret
    
ign_match:
    ; Limpiar pila de los push iniciales del loop
    pop hl
    pop de
    pop bc
    ld l, 1
    ret

; =============================================================================
; TEXT BUFFER MANIPULATION (Replaces memmove dependency)
; =============================================================================

; void text_shift_right(char *addr, uint16_t count) __z88dk_callee
; Desplaza memoria hacia ARRIBA (hace hueco). Usa LDDR.
; Entrada: HL = addr (origen), DE = count
; Nota: Callee convention, no pushear nada extra si es posible o limpiar stack.
; Para simplificar en C wrapper: usaremos fastcall pasando puntero y count global o struct?
; Mejor usamos __z88dk_callee estándar: Stack: [RetAddr] [Count] [Addr]
PUBLIC _text_shift_right
_text_shift_right:
    pop bc          ; Ret address
    pop hl          ; Addr (Inicio del hueco)
    pop de          ; Count (Bytes a mover)
    push bc         ; Restore Ret
    
    ld b, d
    ld c, e         ; BC = Count
    ld a, b
    or c
    ret z
    
    add hl, bc      ; HL = Base + Count
    dec hl          ; HL = Último byte origen
    
    ld d, h
    ld e, l
    inc de          ; DE = Destino (HL + 1)
    
    lddr
    ret

; void text_shift_left(char *addr, uint16_t count) __z88dk_callee
; Desplaza memoria hacia ABAJO (borra hueco). Usa LDIR.
; Stack: [RetAddr] [Count] [Addr]
; Copia desde addr+1 hacia addr.
PUBLIC _text_shift_left
_text_shift_left:
    pop bc          ; Ret
    pop de          ; Addr (Destino)
    pop hl          ; Count
    push bc         ; Restore Ret
    
    ld b, h
    ld c, l         ; BC = Count
    ld a, b
    or c
    ret z
    
    ld h, d
    ld l, e         ; HL = Destino
    inc hl          ; HL = Origen (Destino + 1)
    
    ldir
    ret