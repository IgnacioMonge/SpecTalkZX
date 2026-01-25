;;
;; spectalk_asm.asm - Optimized assembly routines for SpecTalk ZX
;; SpecTalk ZX v1.0 - IRC Client for ZX Spectrum
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

; =============================================================================
; variables Y functions EXTERNAS (definidas en C u otros .asm)
; =============================================================================
EXTERN _caps_lock_mode
EXTERN _caps_latch
EXTERN _g_ps64_y
EXTERN _g_ps64_col
EXTERN _g_ps64_attr
EXTERN _font64
EXTERN _input_cache_char
EXTERN _input_cache_attr
EXTERN _ay_uart_send
EXTERN _BORDER_COLOR
EXTERN _ATTR_BANNER
EXTERN _ATTR_MAIN_BG
EXTERN _ATTR_MSG_CHAN
EXTERN _ATTR_STATUS
EXTERN _ATTR_INPUT_BG
EXTERN _force_status_redraw
EXTERN _status_bar_dirty
EXTERN _uart_drain_limit
EXTERN _rb_dropped
EXTERN _ay_uart_ready   ; Retorna L=1 si hay datos
EXTERN _ay_uart_read    ; Retorna byte en L
EXTERN _rb_push         ; Fastcall: byte en L. Retorna L=1 (ok), 0 (fail)

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
DEFC RB_MASK_L = 0xFF
DEFC RB_MASK_H = 0x07

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
; Genera un pitido usando la rutina de la ROM
; -----------------------------------------------------------------------------
_notification_beep:
    push iy
    
    ld hl, 1000        ; Duración del pitido
    
beep_loop:
    ld a, l
    and 0x10           ; Generar onda cuadrada (bit 4)
    or 0x08            ; Asegurar MIC on
    ld b, a            ; Guardamos los bits de audio en B
    
    ; --- CAMBIO: INYECTAR COLOR DEL TEMA ---
    ld a, (_BORDER_COLOR) 
    and 0x07           ; Nos aseguramos de limpiar basura, solo bits 0-2
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
    inc hl
    
    ; Aplicar máscara solo a la parte alta (la baja hace overflow natural a 0)
    ld a, h
    and RB_MASK_H       
    ld h, a
    
    ; HL contiene ahora el "Futuro Head"
    
    ; 2. Comprobar colisión: ¿El Futuro Head choca con el Tail actual?
    ld de, (_rb_tail)
    or a            
    sbc hl, de      
    jr z, _rb_push_full ; Si son iguales, el buffer está lleno. Abortar.
    
    ; 3. Si hay sitio, necesitamos recuperar el Head ACTUAL para escribir
    ; (HL tenía el futuro head, no nos sirve para escribir AHORA)
    ld de, (_rb_head)   ; DE = head actual
    
    ; Pero ya podemos guardar el Futuro Head en la variable global, porque es seguro
    ; Para esto necesitamos recuperar el valor calculado antes de la resta (sbc).
    ; Truco: Volvemos a calcularlo o usamos el hecho de que ya validamos.
    ; Lo más rápido: Recalcularlo sobre la marcha es lento.
    ; Mejor estrategia: Recuperar el valor futuro.
    
    ; Recalculamos el futuro head rápidamente para guardarlo
    ld hl, (_rb_head)
    inc hl
    ld a, h
    and RB_MASK_H
    ld h, a             ; HL = Futuro Head validado
    
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
; Consume el buffer buscando un \n sin bloquear la CPU.
; Optimizado para usar las nuevas máscaras.
; -----------------------------------------------------------------------------
_try_read_line_nodrain:
    push iy
    
trln_loop:
    ; Check empty
    ld hl, (_rb_head)
    ld de, (_rb_tail)
    or a
    sbc hl, de
    jr z, trln_return_0
    
    ; Read byte at tail
    ld hl, _ring_buffer
    add hl, de
    ld c, (hl)          ; C = byte leído
    
    ; Advance tail logic (Optimized)
    inc de
    ld a, d
    and RB_MASK_H       ; Máscara 0x07 solo en High
    ld d, a
    ld (_rb_tail), de
    
    ; Analizar carácter
    ld a, c
    cp 0x0D             ; Ignorar \r
    jr z, trln_loop
    
    cp 0x0A             ; Fin de línea \n
    jr z, trln_newline
    
    ; Check overflow del buffer de línea RX
    ld hl, (_rx_pos)
    ld de, 510          ; Margen de seguridad
    or a
    sbc hl, de
    jr nc, trln_overflow
    
    ; Guardar carácter en rx_line
    ld hl, (_rx_pos)
    ld de, _rx_line
    add hl, de
    ld (hl), c
    
    ; Incrementar posición
    ld hl, (_rx_pos)
    inc hl
    ld (_rx_pos), hl
    jr trln_loop

trln_overflow:
    ld hl, 1
    ld (_rx_overflow), hl
    jr trln_loop

trln_newline:
    ; Terminar string con NULL
    ld hl, (_rx_pos)
    ld de, _rx_line
    add hl, de
    ld (hl), 0          
    
    ; Si hubo overflow previo, descartar línea
    ld hl, (_rx_overflow)
    ld a, l
    or h
    jr z, trln_check_pos
    
    ; Reset por overflow
    ld hl, 0
    ld (_rx_overflow), hl
    ld (_rx_pos), hl
    jr trln_loop

trln_check_pos:
    ; Si longitud > 0, tenemos línea válida
    ld hl, (_rx_pos)
    ld a, l
    or h
    jr z, trln_reset_continue
    
    ; Éxito: Reset rx_pos y retornar 1
    ld hl, 0
    ld (_rx_pos), hl
    pop iy
    ld l, 1
    ret

trln_reset_continue:
    jr trln_loop

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
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ld a, (ix+6)
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    
    ld de, 0x5800
    add hl, de
    
    ld a, (ix+7)
    add a, l
    ld l, a
    jr nc, aa_done
    inc h
aa_done:
    pop ix
    pop iy
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
    ; Calcular dirección fuente: font64 + (ch-32)*8
    sub 32
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, _font64
    add hl, de
    push hl                 ; Guardar puntero fuente
    
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
    
    pop de                  ; DE = puntero fuente
    
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
; Stack: [IX+6]=y, [IX+7,8]=s, [IX+9]=attr
; -----------------------------------------------------------------------------
_print_line64_fast:
    push iy
    push ix
    ld ix, 0
    add ix, sp
    
    ; Configurar contexto global
    ld a, (ix+6)
    ld (_g_ps64_y), a
    ld a, (ix+9)
    ld (_g_ps64_attr), a
    xor a
    ld (_g_ps64_col), a
    
    ; HL = puntero al string
    ld l, (ix+7)
    ld h, (ix+8)

pl64_print_loop:
    ld a, (hl)
    or a
    jr z, pl64_fill_rest
    
    ld c, a                 ; Guardar carácter
    ld a, (_g_ps64_col)
    cp 64
    jr nc, pl64_done
    
    push hl
    ld l, c
    call _print_str64_char
    
    ld hl, _g_ps64_col
    inc (hl)
    pop hl
    inc hl
    jr pl64_print_loop

pl64_fill_rest:
    ; Rellenar resto con espacios
    ld a, (_g_ps64_col)
    cp 64
    jr nc, pl64_done
    
    ld l, 32
    call _print_str64_char
    
    ld hl, _g_ps64_col
    inc (hl)
    jr pl64_fill_rest

pl64_done:
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
    ld c, (ix+8)            ; attr (guardar para luego)
    
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
    add hl, de              ; HL = dirección exacta
    
    ; Dibujar círculo (8 scanlines)
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
    
    ; Atributo: 0x5800 + y*32 + phys_x
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

    ; 1. Configurar contador de bucle (BC)
    ld a, (_uart_drain_limit)
    or a
    jr z, drain_setup_safety
    
    ; Caso con límite (ej: 32 bytes)
    ld c, a
    ld b, 0
    jr drain_loop_start

drain_setup_safety:
    ; Caso límite=0 -> Usar seguridad de 4096 iteraciones
    ld bc, 4096

drain_loop_start:
    ; CRÍTICO: Guardamos BC porque ay_uart_read destruye B y C
    push bc
    
    ; 2. ¿Hay datos disponibles?
    call _ay_uart_ready     ; Retorna L=1 (Sí) o 0 (No)
    ld a, l
    or a
    jr z, drain_exit_pop    ; Si L=0, no hay más datos -> Salir
    
    ; 3. Leer byte
    call _ay_uart_read      ; Retorna byte en L
    
    ; 4. Meter en Ring Buffer
    ; _rb_push es fastcall, espera el dato en L (ya lo tenemos ahí)
    call _rb_push           ; Retorna L=1 (Éxito) o 0 (Fallo/Lleno)
    
    ld a, l
    or a
    jr nz, drain_next       ; Si L!=0 (Éxito), continuar
    
    ; 5. Gestión de error (Buffer lleno)
    ; rb_dropped++
    ld hl, (_rb_dropped)
    inc hl
    ld (_rb_dropped), hl

drain_next:
    pop bc                  ; Recuperar contador del bucle
    dec bc                  ; Decrementar
    ld a, b
    or c
    jr nz, drain_loop_start ; Si BC > 0, seguir
    
    pop iy                  ; Restaurar IY y salir
    ret

drain_exit_pop:
    pop bc                  ; Limpiar pila (el push bc del inicio del loop)
    pop iy
    ret