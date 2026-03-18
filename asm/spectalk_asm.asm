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

; =============================================================================
; BSS ZEROING - runs before main() via code_crt_init
; Allows the TAP binary to be truncated before BSS (saving ~4KB of zeros)
; =============================================================================
SECTION code_crt_init
EXTERN __data_compiler_tail
EXTERN __bss_compiler_tail
    ld hl, __bss_compiler_tail
    ld de, __data_compiler_tail
    or a
    sbc hl, de          ; HL = total size to zero
    ld b, h
    ld c, l             ; BC = size
    ex de, hl           ; HL = start address (1 byte vs 2)
    ld (hl), 0
    ld d, h
    ld e, l
    inc de              ; DE = start + 1
    dec bc              ; BC = size - 1
    ldir
    ; Zero Printer Buffer: 0x5B00..0x5BFF (256 bytes)
    ld hl, 0x5B00
    ld (hl), 0
    ld d, h
    ld e, l
    inc de
    ld bc, 255
    ldir
    ; Zero CHANS workspace: 0x5CB6..0x5DB5 (256 bytes)
    ld hl, 0x5CB6
    ld (hl), 0
    ld d, h
    ld e, l
    inc de
    ld bc, 255
    ldir
    ; Zero UDG area: 0xFF58..0xFFF1 (154 bytes)
    ld hl, 0xFF58
    ld (hl), 0
    ld d, h
    ld e, l
    inc de
    ld bc, 153
    ldir
    ; --- IM2 setup ---
    ; Fill vector table at 0xFC00 with 0xFD (257 bytes)
    ld hl, 0xFC00
    ld (hl), 0xFD
    ld d, h
    ld e, l
    inc de
    ld bc, 256
    ldir
    ; Copy ISR to 0xFDFD
    ld hl, _im2_isr_template
    ld de, 0xFDFD
    ld bc, _im2_isr_end - _im2_isr_template
    ldir
    ; Switch to IM2
    ld a, 0xFC
    ld i, a
    im 2
    ; Invalidar cache de fila screen/attr
    ld a, 0xFF
    ld (cache_row_y), a

SECTION code_user

; =============================================================================
; CONSTANTS - Video and Memory Addresses
; =============================================================================
VIDEO_BASE      EQU 0x4000      ; Screen memory base
ATTR_BASE       EQU 0x5800      ; Attribute memory base
SCREEN_WIDTH    EQU 32          ; Physical screen width in characters
SCREEN_HEIGHT   EQU 24          ; Physical screen height in rows

; =============================================================================
; CONSTANTS - Ring Buffer
; =============================================================================
; WARNING-M14: Estas constantes DEBEN coincidir con spectalk.h
; RING_BUFFER_SIZE=2048, RING_BUFFER_MASK=0x07FF
RING_SIZE       EQU 2048        ; == RING_BUFFER_SIZE  (spectalk.h:85)
RING_MASK       EQU 0x07FF      ; == RING_BUFFER_MASK  (spectalk.h:86)
RX_LINE_MAX     EQU 510         ; == RX_LINE_SIZE - 2  (spectalk.h:103)

; =============================================================================
; PUBLIC FUNCTIONS (visible from C)
; =============================================================================
PUBLIC _esx_detect
PUBLIC _hl_mul32
PUBLIC _a_sext_mul32
PUBLIC _l_mul32
PUBLIC _channel_flags_ptr
PUBLIC _l_channel_flags_ptr
PUBLIC _rx_pos_reset
PUBLIC _leave_ix
PUBLIC _ld_hl_ix4
PUBLIC _ld_hl_ixm2
PUBLIC _ld_hl_ix6
PUBLIC _ld_hl_ixm4
PUBLIC _st_hl_ix4
PUBLIC _st_hl_ixm2
PUBLIC _set_border
PUBLIC _check_caps_toggle
PUBLIC _key_shift_held
PUBLIC _input_cache_invalidate
PUBLIC _print_str64_char
PUBLIC _print_line64_fast
PUBLIC _draw_indicator
; draw_indicator_big removed (big mode eliminated)
PUBLIC _clear_line
PUBLIC _clear_zone
PUBLIC _screen_row_base
PUBLIC _screen_line_addr
PUBLIC _attr_addr
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
PUBLIC _main_newline
PUBLIC _tokenize_params
PUBLIC _sb_append
PUBLIC _draw_badge_dither
PUBLIC _in_inkey
PUBLIC asm_in_inkey
PUBLIC _nav_push
PUBLIC _find_empty_channel_slot
PUBLIC _print_char64
PUBLIC _draw_big_char
PUBLIC _font_lut

; =============================================================================
; EXTERNAL VARIABLES AND FUNCTIONS (defined in C or other .asm)
; =============================================================================
EXTERN _caps_lock_mode
EXTERN _caps_latch
EXTERN _beep_enabled
EXTERN _g_ps64_y
EXTERN _g_ps64_col
EXTERN _g_ps64_attr
EXTERN _ay_uart_send
EXTERN _theme_attrs
; theme_attrs[] offsets (must match spectalk.h #defines)
TA_BANNER   EQU 0
TA_STATUS   EQU 1
TA_MSG_CHAN  EQU 2
TA_MAIN_BG  EQU 5
TA_INPUT_BG EQU 7
TA_BORDER   EQU 19
EXTERN _current_attr
EXTERN _irc_params
EXTERN _irc_param_count
EXTERN _force_status_redraw
EXTERN _status_bar_dirty
EXTERN _uart_drain_limit
EXTERN _ay_uart_ready   ; Retorna L=1 si hay datos
; _line_buffer, _temp_input, _input_cache_char/attr: via defc at end of file
; (mapped to system RAM: Printer Buffer, CHANS workspace)
EXTERN _irc_pass
EXTERN _nickserv_pass
EXTERN _network_name
EXTERN _ay_uart_read    ; Retorna byte en L
EXTERN _irc_is_away
EXTERN _buffer_pressure
EXTERN _ping_latency
EXTERN _pagination_active
EXTERN _pagination_lines
EXTERN _pagination_pause
; _main_newline is implemented below in this file
; EXTERN _main_newline
EXTERN _main_col
EXTERN _main_line
EXTERN _wrap_indent

; Ring buffer (para rb_pop y try_read_line_nodrain)
EXTERN _ring_buffer
EXTERN _rb_head
EXTERN _rb_tail
EXTERN _rx_line
EXTERN _rx_pos
EXTERN _rx_overflow
EXTERN _rx_last_len

; Ignore list (para is_ignored)
EXTERN _ignore_list
EXTERN _ignore_count
; _big_status removed (big mode eliminated)

; =============================================================================
; RING BUFFER CONSTANTS
; Size: 2048 bytes (MUST match RING_BUFFER_SIZE in spectalk.h)
; =============================================================================
DEFC RB_MASK_H = 0x07   ; High byte mask for 2048 (0x0800)
                         ; WARNING: If RING_BUFFER_SIZE changes in C,
                         ; this MUST be updated: 2048→0x07, 4096→0x0F, etc.

; =============================================================================
; VARIABLES BSS (Buffer para descompresión de fuente)
; =============================================================================
SECTION bss_user
glyph_buffer: defs 8    ; Buffer temporal para glifo descomprimido
plf_left_buf: defs 8    ; Buffer temporal para nibbles izquierdos (print_line64_fast)
plf_str_ptr:  defs 2    ; String pointer temporal (print_line64_fast)
plf_attr_val: defs 1    ; Atributo a escribir (print_line64_fast)
plf_y_val:    defs 1    ; Fila Y (print_line64_fast)
cache_scr_base: defs 2  ; Screen base addr cacheada (print_str64_char)
cache_atr_base: defs 2  ; Attr base addr cacheada (print_str64_char)
cache_row_y:   defs 1   ; Fila Y del cache (0xFF = inválido)

; BPE decompression state (dict is after theme_raw for contiguous SPECTALK.DAT load)
bpe_rstack:   defs 16   ; Return stack for BPE expansion (8 levels x 2 bytes)
bpe_rsp:      defs 2    ; Current position in bpe_rstack


SECTION code_user

; =============================================================================
; UTILIDADES BÁSICAS
; =============================================================================

; -----------------------------------------------------------------------------
; hl_mul32: HL = HL * 32 (used by peephole rule for channels[] indexing)
; Replaces 5x add hl,hl (5 bytes) with call (3 bytes) saving 2 bytes/site
; -----------------------------------------------------------------------------
_hl_mul32:
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ret

; -----------------------------------------------------------------------------
; channel_flags_ptr: HL = &channels[L].flags
; copt replaces: 5x add hl,hl + ld de,_channels + add hl,de + ld de,0x001e + add hl,de
; Saves 8 bytes per site (11 -> 3 bytes call)
; -----------------------------------------------------------------------------
EXTERN _channels
_channel_flags_ptr:
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, _channels
    add hl, de
    ld de, 0x001e
    add hl, de
    ret

; -----------------------------------------------------------------------------
; a_sext_mul32: sign-extend A to HL, then HL *= 32
; copt replaces: ld l,a / rlca / sbc a,a / ld h,a / 5x add hl,hl (9 bytes -> 3)
; -----------------------------------------------------------------------------
_a_sext_mul32:
    ld l, a
    rlca
    sbc a, a
    ld h, a
    jp _hl_mul32

; -----------------------------------------------------------------------------
; l_mul32: zero-extend L to HL, then HL *= 32
; copt replaces: ld h,0x00 / 5x add hl,hl (7 bytes -> 3)
; -----------------------------------------------------------------------------
_l_mul32:
    ld h, 0x00
    jp _hl_mul32

; -----------------------------------------------------------------------------
; leave_ix: IX epilogue — ld sp,ix / pop ix / ret
; copt replaces the 3-instruction epilogue (5 bytes) with jp (3 bytes)
; -----------------------------------------------------------------------------
_leave_ix:
    ld sp, ix
    pop ix
    ret

; -----------------------------------------------------------------------------
; l_channel_flags_ptr: HL = &channels[L].flags (L variant of channel_flags_ptr)
; copt replaces: call _l_mul32 + ld de,_channels + add hl,de + ld de,0x001e + add hl,de
; -----------------------------------------------------------------------------
_l_channel_flags_ptr:
    ld h, 0x00
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, _channels
    add hl, de
    ld de, 0x001e
    add hl, de
    ret

; -----------------------------------------------------------------------------
; IX load helpers: HL = (ix+N/ix+N+1) — 7 bytes each, saves 3 bytes per site
; Safe: IX is frame pointer, unchanged across call. call uses stack, not IX.
; -----------------------------------------------------------------------------
_ld_hl_ix4:
    ld l,(ix+4)
    ld h,(ix+5)
    ret

_ld_hl_ixm2:
    ld l,(ix-2)
    ld h,(ix-1)
    ret

_ld_hl_ix6:
    ld l,(ix+6)
    ld h,(ix+7)
    ret

_ld_hl_ixm4:
    ld l,(ix-4)
    ld h,(ix-3)
    ret

; -----------------------------------------------------------------------------
; IX store helpers: (ix+N/ix+N+1) = HL — 7 bytes each, saves 3 bytes per site
; -----------------------------------------------------------------------------
_st_hl_ix4:
    ld (ix+4),l
    ld (ix+5),h
    ret

_st_hl_ixm2:
    ld (ix-2),l
    ld (ix-1),h
    ret

; -----------------------------------------------------------------------------
; rx_pos_reset: rx_pos = 0 (copt replaces ld hl,0/ld (_rx_pos),hl)
; Saves 3-6 bytes per site (25+ sites)
; -----------------------------------------------------------------------------
_rx_pos_reset:
    ld hl, 0
    ld (_rx_pos), hl
    ret

; -----------------------------------------------------------------------------
; void set_border(uint8_t color) __z88dk_fastcall
; input: L = color
; -----------------------------------------------------------------------------
_set_border:
    ld a, l
    out (0xfe), a
    ret

; -----------------------------------------------------------------------------
; Common beep core:
;   IN:  HL = duration loop count
;        C  = tone mask (e.g. 0x10 or 0x20)
; Uses: A,B,C,HL
; -----------------------------------------------------------------------------
_beep_core:
    push iy

beep_loop:
    ld a, l
    and 0x10
    or 0x08
    ld b, a

    ld a, (_theme_attrs + TA_BORDER)
    and 0x07
    or b

    out (0xFE), a

    dec hl
    ld a, h
    or l
    jr nz, beep_loop

    pop iy
    ret


; -----------------------------------------------------------------------------
; void mention_beep(void)  -- mención en canal (beep simple más grave)
; -----------------------------------------------------------------------------
PUBLIC _mention_beep
_mention_beep:
    ld a, (_beep_enabled)
    or a
    ret z                   ; Si beep_enabled == 0, salir
    ld hl, 2500             ; Más largo
    ld c, 0x18              ; Tono más grave
    jp _beep_core

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
    ; NOTA: En un sistema con ISR concurrente esto sería incorrecto (head antes de write),
    ;       pero aquí todo es polling síncrono, sin riesgo de race condition.
    ld (_rb_head), hl
    
    ; 4. Escribir el dato en la memoria: buffer[head_viejo]
    ld hl, _ring_buffer
    add hl, de              ; HL = dirección de escritura (buffer + head_viejo)
    
    pop de                  ; Recuperamos el dato original de la pila (estaba en L, ahora en E)
    ld (hl), e              ; Escribimos en memoria
    
    ld l, 1                 ; Retornar 1 (Éxito)
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
    
    ; 4. Analyze character
    ld a, c
    cp 0x0D             ; Ignorar \r
    jr z, trln_loop
    
    cp 0x0A             ; End of line \n
    jr z, trln_newline
    
    ; 5. CRÍTICO: Chequear desbordamiento ANTES de escribir (HARDENED)
    ld hl, (_rx_pos)
    push hl             ; Guardar rx_pos para paso 6
    ld de, RX_LINE_MAX  ; == RX_LINE_SIZE - 2 (espacio para \0)
    or a
    sbc hl, de
    jr nc, trln_overflow_pop ; Si rx_pos >= 510, descartar
    
    ; 6. Save character (no hay overflow)
    pop hl              ; HL = rx_pos (guardado)
    ld de, _rx_line
    add hl, de          ; HL = &rx_line[rx_pos]
    ld (hl), c
    
    ; 7. Incrementar posición
    ld hl, (_rx_pos)
    inc hl
    ld (_rx_pos), hl
    jr trln_loop

trln_overflow_pop:
    pop hl              ; Limpiar stack

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
    ld a, (_rx_overflow)
    or a
    jr z, trln_check_valid
    
    ; If overflow, DISCARD line completa y resetear
    xor a
    ld (_rx_overflow), a
    ld hl, 0
    ld (_rx_pos), hl
    jr trln_loop        ; Look for next line

trln_check_valid:
    ; If length > 0, valid line
    ld hl, (_rx_pos)
    ld a, l
    or h
    jr z, trln_loop     ; Ignore empty lines
    
    ; Éxito: Reset rx_pos y retornar 1
    ; Guardar longitud de la línea (rx_pos) para evitar strlen() en C
    ld hl, (_rx_pos)
    ld (_rx_last_len), hl

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
; GRAPHICS SYSTEM - TABLES
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
; Lines 6 and 7 of each glyph are ALWAYS 0x00 (se regeneran en runtime)
; Each glyph: 3 compressed bytes (6 nibbles = líneas 0-5)

; Data loaded from SPECTALK.DAT at startup (font + themes)
; Layout: [10 LUT][288 glyphs][75 theme_raw] = 373 bytes contiguous
SECTION bss_user
ALIGN 16
_font_lut:
font_lut:
    defs 10
font64_packed:
    defs 288
PUBLIC _theme_raw
_theme_raw:
    defs 75
; BPE dictionary: contiguous with font_lut+font64_packed+theme_raw for single SPECTALK.DAT read
; Layout in DAT: [10 LUT][288 glyphs][75 themes][318 bpe_dict][help text]
PUBLIC _bpe_dict
_bpe_dict:
bpe_dict:
    defs 318              ; 106 entries x 3 bytes (b1, b2, 0x00)

SECTION code_user

; =============================================================================
; GLYPH DECOMPRESSOR
; Input: A = char (ASCII 32-127)
; Output: HL = pointer to glyph_buffer with 8 bytes of decompressed glyph
; Preserves: IY (required by z88dk)
; Destroys: AF, BC, DE. Uses alternate HL'/E' via EXX (ISR safe: IM2 ISR only touches AF)
; Timing: ~136 t-states/iteration (EXX LUT access, no push/pop)
; =============================================================================
unpack_glyph:
    ; Calculate offset: (char - 32) * 3
    sub 32
    jr nz, unpack_not_space
    ; --- Space short-circuit: fill glyph_buffer with 0x00 (~50 t-states) ---
    ld hl, glyph_buffer
    xor a
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    inc hl
    ld (hl), a
    ld hl, glyph_buffer
    ret
unpack_not_space:
    ld l, a
    ld h, 0
    ld c, l
    ld b, h
    add hl, hl          ; *2
    add hl, bc          ; *3
    ld bc, font64_packed
    add hl, bc          ; HL = pointer to compressed glyph (3 bytes)

    ; Use DE as pointer to compressed source (faster than IX)
    ex de, hl            ; DE = compressed source, HL freed
    ld hl, glyph_buffer  ; HL = destination

    ; Setup alternate registers for LUT access (font_lut is ALIGN 16, no carry)
    exx
    ld hl, font_lut      ; H' = font_lut high byte (constant for ld a,(hl))
    ld e, l              ; E' = font_lut low byte (base for add)
    exx

    ; Unpack 3 bytes -> 6 lines (0-5)
    ld b, 3
unpack_loop:
    ld a, (de)           ; 7 T - Read compressed byte
    ld c, a              ; 4 T - Save copy

    ; High nibble -> line N
    rrca
    rrca
    rrca
    rrca                 ; 16 T - A = high nibble in low position
    and 0x0F             ; 7 T  - Clear upper garbage

    exx                  ; 4 T  - switch to alt set (H=lut_hi, E=lut_lo)
    add a, e             ; 4 T  - A = font_lut_lo + nibble (no carry: ALIGN 16)
    ld l, a              ; 4 T
    ld a, (hl)           ; 7 T  - LUT lookup
    exx                  ; 4 T  - back to main set
    ld (hl), a           ; 7 T  - store to glyph_buffer
    inc hl               ; 6 T

    ; Low nibble -> line N+1
    ld a, c              ; 4 T
    and 0x0F             ; 7 T  - A = low index

    exx                  ; 4 T
    add a, e             ; 4 T
    ld l, a              ; 4 T
    ld a, (hl)           ; 7 T
    exx                  ; 4 T
    ld (hl), a           ; 7 T
    inc hl               ; 6 T

    inc de               ; 6 T  - Next compressed byte
    djnz unpack_loop     ; 13/8 T

    ; Lines 6 and 7 always 0x00
    xor a
    ld (hl), a
    inc hl
    ld (hl), a

    ld hl, glyph_buffer
    ret

; =============================================================================
; GRAPHICS SYSTEM - FUNCTIONS
; =============================================================================

; -----------------------------------------------------------------------------
; uint8_t* screen_line_addr(uint8_t y, uint8_t phys_x, uint8_t scanline)
; Stack: [IX+6]=y, [IX+7]=phys_x, [IX+8]=scanline (sdcc_iy byte-packed)
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
    
    ld a, (ix+8)            ; scanline (ORIGINAL)
    add a, h
    ld h, a
    
    ld a, (ix+7)            ; phys_x (ORIGINAL)
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
; Stack: [IX+4]=y, [IX+6]=phys_x (FIX6: solo push ix, SDCC 16-bit args)
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
    ld a, (ix+6)        ; phys_x (FIX6: era IX+5)
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
; Stack: [IX+6]=start, [IX+7]=lines, [IX+8]=attr (sdcc_iy byte-packed)
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

; =============================================================================
; IMPRESIÓN 64 COLUMNAS
; =============================================================================

; -----------------------------------------------------------------------------
; Cache helpers: devuelven base de fila screen/attr usando cache si válido.
; Input: (none, usa _g_ps64_y y cache_row_y)
; Output: HL = base addr
; Destroys: AF, DE (solo si cache miss)
; -----------------------------------------------------------------------------
p64_get_scr_base:
    ld a, (cache_row_y)
    ld hl, _g_ps64_y
    cp (hl)
    jr nz, p64_scr_miss
    ld hl, (cache_scr_base)
    ret
p64_scr_miss:
    ld a, (hl)             ; A = g_ps64_y
    add a, a
    ld l, a
    ld h, 0
    ld de, _screen_row_base
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a                ; HL = screen row base
    ld (cache_scr_base), hl
    ; Actualizar cache_row_y y calcular attr base también
    ld a, (_g_ps64_y)
    ld (cache_row_y), a
    push hl                ; Guardar screen base
    add a, a
    ld l, a
    ld h, 0
    ld de, _attr_row_base
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    ld (cache_atr_base), hl
    pop hl                 ; Devolver screen base
    ret

p64_get_atr_base:
    ld a, (cache_row_y)
    ld hl, _g_ps64_y
    cp (hl)
    jr nz, p64_atr_miss
    ld hl, (cache_atr_base)
    ret
p64_atr_miss:
    ; Cache miss en attr pero no en screen: recalcular ambos
    call p64_get_scr_base  ; Esto cachea ambos
    ld hl, (cache_atr_base)
    ret

; -----------------------------------------------------------------------------
; void print_str64_char(uint8_t ch) __z88dk_fastcall
; Draw a 4-pixel character usando fuente 64 columnas
; Input: L = ASCII character
; No preserva: AF, BC, DE, HL
; Preserva: IY (no lo usa)
; -----------------------------------------------------------------------------
_print_str64_char:
    ; Validate character
    ld a, l
    cp 32
    jr c, p64_use_space
    cp 128
    jr c, p64_calc_font
p64_use_space:
    ld a, 32

p64_calc_font:
    ; A contains validated character (32-127)
    ; --- Space short-circuit: borrado directo sin unpack_glyph ---
    cp 32
    jr nz, p64_not_space

    ; Obtener screen base (cache o lookup)
    call p64_get_scr_base   ; HL = screen row base
    ld a, (_g_ps64_col)
    ld b, a
    srl a
    ld e, a
    ld d, 0
    add hl, de              ; HL = dirección pantalla

    bit 0, b
    jr nz, p64_space_right

    ; Espacio lado izquierdo: AND 0x0F en 8 scanlines
    ld b, 8
p64_space_left:
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    djnz p64_space_left
    jr p64_set_attr

p64_space_right:
    ; Espacio lado derecho: AND 0xF0 en 8 scanlines
    ld b, 8
p64_space_right_loop:
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    djnz p64_space_right_loop
    jr p64_set_attr

p64_not_space:
    ; Descomprimir glifo a glyph_buffer
    call unpack_glyph       ; Input: A = char, Output: HL = glyph_buffer
    push hl                 ; Guardar puntero fuente descomprimida

    ; Obtener screen base (cache o lookup)
    call p64_get_scr_base   ; HL = screen row base

    ; Sumar col/2
    ld a, (_g_ps64_col)
    ld b, a                 ; Guardar col (paridad) temporalmente
    srl a
    ld e, a
    ld d, 0
    add hl, de              ; HL = dirección pantalla

    pop de                  ; DE = puntero fuente (glyph_buffer)

    ; Decidir lado izquierdo o derecho
    bit 0, b
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
    ; Obtener attr base (cache o lookup)
    call p64_get_atr_base   ; HL = attr row base

    ld a, (_g_ps64_col)
    srl a
    ld c, a
    ld b, 0
    add hl, bc

    ; Escribir atributo solo si cambia (evita store redundante)
    ld a, (_g_ps64_attr)
    cp (hl)
    ret z
    ld (hl), a
    ret

; -----------------------------------------------------------------------------
; -----------------------------------------------------------------------------
; void draw_big_char(uint8_t ch) __z88dk_fastcall
; Double-height 4px character renderer for banner.
; Input: L = char. Uses: _g_ps64_y (top row), _g_ps64_col, _g_ps64_attr
; Destroys: AF, BC, DE, HL. Preserves: IY
_draw_big_char:
    ld a, l
    cp 32
    jr c, dbc_force_space
    cp 128
    jr c, dbc_valid
dbc_force_space:
    ld a, 32
dbc_valid:
    call unpack_glyph
    ex de, hl
    ld a, (_g_ps64_y)
    ld l, a
    ld h, 0
    add hl, hl
    ld bc, _screen_row_base
    add hl, bc
    ld (dbc_smt+1), hl
    inc hl
    inc hl
    ld (dbc_smb+1), hl
dbc_smt:
    ld hl, (_screen_row_base + 0)
    call dbc_add_col
    call dbc_render_top
dbc_smb:
    ld hl, (_screen_row_base + 2)
    call dbc_add_col
    jp dbc_render_bot           ; tail call (attrs set by clear_line)

dbc_add_col:
    ld a, (_g_ps64_col)
    srl a
    ld c, a
    ld b, 0
    add hl, bc
    ret

dbc_left_core:
    ld a, (de)
    and 0xF0
    ld c, a
    ld a, (hl)
    and 0x0F
    or c
    ld (hl), a
    inc h
    ld a, (hl)
    and 0x0F
    or c
    ld (hl), a
    inc h
    inc de
    djnz dbc_left_core
    ret

dbc_right_core:
    ld a, (de)
    and 0x0F
    ld c, a
    ld a, (hl)
    and 0xF0
    or c
    ld (hl), a
    inc h
    ld a, (hl)
    and 0xF0
    or c
    ld (hl), a
    inc h
    inc de
    djnz dbc_right_core
    ret

dbc_render_top:
    ld a, (_g_ps64_col)
    bit 0, a
    jr nz, dbc_rt_right
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    ld b, 3
    jp dbc_left_core
dbc_rt_right:
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    ld b, 3
    jp dbc_right_core

dbc_render_bot:
    ld a, (_g_ps64_col)
    bit 0, a
    jr nz, dbc_rb_right
    ld b, 3
    call dbc_left_core
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    ld a, (hl)
    and 0x0F
    ld (hl), a
    ret
dbc_rb_right:
    ld b, 3
    call dbc_right_core
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    ld a, (hl)
    and 0xF0
    ld (hl), a
    ret

; -----------------------------------------------------------------------------
; void print_line64_fast(uint8_t y, const char *s, uint8_t attr)
; Stack layout (con push ix):
;   [IX+4]=y, [IX+5,6]=s (pointer), [IX+7]=attr
;
; Optimized: procesa pares de columnas, calcula screen addr 1 vez,
; escribe bytes completos (sin AND/OR de preservación), attr fill al final.
; -----------------------------------------------------------------------------
_print_line64_fast:
    push ix
    ld ix, 0
    add ix, sp

    ; --- Calcular screen_row_base[y] una sola vez ---
    ld a, (ix+4)           ; y
    add a, a
    ld l, a
    ld h, 0
    ld bc, _screen_row_base
    add hl, bc
    ld c, (hl)
    inc hl
    ld b, (hl)             ; BC = screen base addr de la fila

    ; Guardar attr y string pointer
    ld a, (ix+7)
    ld (plf_attr_val), a   ; attr para después
    ld e, (ix+5)
    ld d, (ix+6)           ; DE = string pointer

    ; También guardar y para attr fill al final
    ld a, (ix+4)
    ld (plf_y_val), a

    push bc                ; Guardar screen base en stack
    pop hl                 ; HL = screen addr (col 0, scanline 0)

    ld b, 32               ; 32 pares de columnas (= 64 cols)

plf_pair_loop:
    push bc                ; Guardar contador de pares
    push hl                ; Guardar screen addr del byte actual

    ; --- Leer char izquierdo ---
    ld a, (de)
    or a
    jr z, plf_left_pad     ; NUL: no avanzar puntero, usar espacio
    inc de                 ; Avanzar puntero (char válido o inválido)
    cp 32
    jr c, plf_left_space   ; char < 32: tratar como espacio (puntero ya avanzó)
    cp 128
    jr c, plf_left_ok      ; char 32-127: OK
plf_left_space:
    ld a, 32
    jr plf_left_ok
plf_left_pad:
    ld a, 32
plf_left_ok:
    ; Guardar string pointer antes de unpack_glyph (destruye DE)
    ex de, hl
    ld (plf_str_ptr), hl   ; HL = string ptr saved; no need to swap back
    call unpack_glyph      ; A still has char; HL/DE don't matter (destroyed)
    ; Copiar nibbles altos a plf_left_buf
    ld hl, glyph_buffer
    ld de, plf_left_buf
    ld b, 8
plf_save_left:
    ld a, (hl)
    and 0xF0               ; Solo nibble alto (lado izquierdo)
    ld (de), a
    inc hl
    inc de
    djnz plf_save_left

    ; --- Leer char derecho ---
    ld hl, (plf_str_ptr)
    ex de, hl              ; DE = string pointer
    ld a, (de)
    or a
    jr z, plf_right_pad    ; NUL: no avanzar puntero, usar espacio
    inc de                 ; Avanzar puntero
    cp 32
    jr c, plf_right_space  ; char < 32: tratar como espacio
    cp 128
    jr c, plf_right_ok     ; char 32-127: OK
plf_right_space:
    ld a, 32
    jr plf_right_ok
plf_right_pad:
    ld a, 32
plf_right_ok:
    ex de, hl
    ld (plf_str_ptr), hl   ; HL = string ptr saved; no need to swap back
    call unpack_glyph      ; A still has char; HL/DE don't matter (destroyed)

    ; --- Combinar y escribir 8 scanlines ---
    pop hl                 ; HL = screen addr
    push hl                ; Re-guardar para avanzar después

    ld de, glyph_buffer
    ld ix, plf_left_buf
    ld b, 8
plf_write_loop:
    ld a, (ix+0)           ; Nibble alto del char izquierdo
    ld c, a
    ld a, (de)             ; Byte del char derecho
    and 0x0F               ; Solo nibble bajo (lado derecho)
    or c                   ; Combinar: left_high | right_low
    ld (hl), a             ; Escribir byte completo a pantalla
    inc ix
    inc de
    inc h                  ; Siguiente scanline
    djnz plf_write_loop

    ld hl, (plf_str_ptr)
    ex de, hl              ; DE = string pointer para siguiente iteración
    pop hl                 ; Recuperar screen addr
    inc hl                 ; Siguiente byte (siguiente par de columnas)

    pop bc                 ; Recuperar contador de pares
    djnz plf_pair_loop

    ; --- Attr fill: escribir 32 atributos de golpe ---
    ld a, (plf_y_val)
    add a, a
    ld l, a
    ld h, 0
    ld bc, _attr_row_base
    add hl, bc
    ld c, (hl)
    inc hl
    ld b, (hl)
    ; BC = attr base addr — fill 32 bytes with LDIR
    ld h, b
    ld l, c                ; HL = attr addr
    ld a, (plf_attr_val)
    ld (hl), a             ; Write first byte
    ld d, h
    ld e, l
    inc de                 ; DE = HL + 1
    ld bc, 31
    ldir                   ; Copy first byte to remaining 31

    ; Actualizar _g_ps64_col y _g_ps64_y para consistencia
    ld a, 64
    ld (_g_ps64_col), a
    ld a, (plf_y_val)
    ld (_g_ps64_y), a
    ld a, (plf_attr_val)
    ld (_g_ps64_attr), a

    pop ix
    ret


; -----------------------------------------------------------------------------
; void redraw_input_asm(void)
; Redibuja las 2 líneas de INPUT usando line_buffer y line_len
; Incluye prompt "> " y rellena con espacios
; Usa variables globales: _line_buffer, _line_len, theme_attrs[6]=INPUT, [8]=PROMPT
; Mucho más rápido que el loop C equivalente
; -----------------------------------------------------------------------------
PUBLIC _redraw_input_asm
EXTERN _line_len
; _ATTR_INPUT = _theme_attrs + 6, _ATTR_PROMPT = _theme_attrs + 8

DEFC INPUT_ROW_1 = 22
DEFC INPUT_ROW_2 = 23

_redraw_input_asm:
    ; === Línea 1: "> " + primeros 62 chars ===
    ld a, INPUT_ROW_1
    ld (_g_ps64_y), a
    xor a
    ld (_g_ps64_col), a
    
    ; Prompt ">" con ATTR_PROMPT
    ld a, (_theme_attrs + 8)  ; ATTR_PROMPT
    ld (_g_ps64_attr), a
    ld l, '>'
    call _print_str64_char
    ld hl, _g_ps64_col
    inc (hl)
    
    ; Espacio con ATTR_PROMPT
    ld l, ' '
    call _print_str64_char
    ld hl, _g_ps64_col
    inc (hl)
    
    ; Cambiar a ATTR_INPUT para el texto
    ld a, (_theme_attrs + 6)  ; ATTR_INPUT
    ld (_g_ps64_attr), a
    
    ; DE = puntero a line_buffer
    ld de, _line_buffer
    ; BC: B = contador de chars restantes en línea (62), C = line_len
    ld a, (_line_len)
    ld c, a                     ; C = line_len total
    ld b, 62                    ; 62 chars restantes en línea 1

    call ria_print_n_chars

    ; === Línea 2: siguientes 64 chars ===
    ld a, INPUT_ROW_2
    ld (_g_ps64_y), a
    xor a
    ld (_g_ps64_col), a

    ; B = 64 chars, C ya tiene chars restantes
    ld b, 64
    jp ria_print_n_chars

; Helper: pinta B chars desde (DE), rellenando con espacios si C=0
; B = num chars a pintar, C = chars restantes en buffer, DE = ptr buffer
ria_print_n_chars:
    ld a, c
    or a
    jr z, ria_pnc_space
    ld a, (de)
    inc de
    dec c
    jr ria_pnc_print
ria_pnc_space:
    ld a, 32
ria_pnc_print:
    ld l, a
    push bc
    push de
    call _print_str64_char
    pop de
    pop bc
    ld hl, _g_ps64_col
    inc (hl)
    djnz ria_print_n_chars
    ret


; -----------------------------------------------------------------------------
; void draw_indicator(uint8_t y, uint8_t phys_x, uint8_t attr)
; Dibuja un círculo de estado 8x8
; - away: medio círculo (prioridad)
; - buffer_pressure: círculo vacío
; - normal: círculo sólido
; Stack: [IX+6]=y, [IX+7]=phys_x, [IX+8]=attr
; TABLE-DRIVEN: saves ~20 bytes vs inline patterns
; -----------------------------------------------------------------------------

; Indicator patterns: 8 bytes each (shared first/last with 0x00)
ind_pattern_solid:    defb 0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00
ind_pattern_empty:    defb 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00
ind_pattern_away:     defb 0x00, 0x3C, 0x4E, 0x4E, 0x4E, 0x4E, 0x3C, 0x00
ind_pattern_medium:   defb 0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00  ; ~6px circle
ind_pattern_small:    defb 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00  ; ~4px dot

; Shared: select indicator pattern into DE based on connection state
; Preserves: BC, HL, IX, IY
ind_select_pattern:
    ld de, ind_pattern_solid
    ld a, (_irc_is_away)
    or a
    jr z, isp_ck_pr
    ld de, ind_pattern_away
    ret
isp_ck_pr:
    ld a, (_buffer_pressure)
    or a
    jr z, isp_ck_lat
    ld de, ind_pattern_empty
    ret
isp_ck_lat:
    ld a, (_ping_latency)
    or a
    ret z                        ; 0 = good, use solid
    cp 2
    jr nc, isp_lh
    ld de, ind_pattern_medium
    ret
isp_lh:
    ld de, ind_pattern_small
    ret

_draw_indicator:
    push iy
    push ix
    ld ix, 0
    add ix, sp

    ld a, (ix+6)            ; y
    ld c, (ix+8)            ; attr (byte-packed: y=+6, phys_x=+7, attr=+8)

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

    ld e, (ix+7)            ; phys_x (byte-packed)
    ld d, 0
    add hl, de              ; HL = dirección exacta pixel
    push hl                 ; Save screen address for later
    
    call ind_select_pattern     ; DE = pattern

di_draw_pattern:
    ; DE = pattern, HL = screen address
    ld b, 8
di_pattern_loop:
    ld a, (de)
    ld (hl), a
    inc de
    inc h                   ; Next scanline
    djnz di_pattern_loop

    ; Calcular dirección atributos
    pop hl                  ; Discard (we recalculate - cleaner)
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

    ld e, (ix+7)            ; phys_x (byte-packed)
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
; uint8_t st_strlen(const char *s) __z88dk_fastcall
; Retorna longitud del string en L (max 255)
; Entrada: HL = puntero al string
; Salida: L = longitud, H = 0
; -----------------------------------------------------------------------------
PUBLIC _st_strlen
_st_strlen:
    ; CPIR block scan: 21 T-states/byte vs 36 manual (+41% faster)
    ld d, h
    ld e, l               ; DE = start
    ld bc, 0x00FF         ; max 255 chars (uint8_t return)
    xor a                 ; A = 0 (search for NUL)
    cpir                  ; scan until match or BC=0
    ; HL = byte after NUL, DE = start
    or a
    sbc hl, de            ; HL = length + 1
    dec hl                ; HL = length
    ret

; -----------------------------------------------------------------------------
; int st_stricmp(const char *a, const char *b)
; Comparación case-insensitive
; Stack: [IX+4,5]=a, [IX+6,7]=b
; Retorna: 0 si iguales, <0 si a<b, >0 si a>b
; -----------------------------------------------------------------------------
_st_stricmp:
    push ix
    ld ix, 0
    add ix, sp

    ld l, (ix+4)
    ld h, (ix+5)            ; HL = a
    ld e, (ix+6)
    ld d, (ix+7)            ; DE = b

stricmp_loop:
    ld a, (hl)
    call irc_tolower        ; Convertir A según RFC 1459
    ld b, a                 ; B = a normalizado
    
    ld a, (de)
    call irc_tolower        ; Convertir según RFC 1459
    
    ; Comparar (NOTE-M12: retorna b-a, signo invertido vs strcmp.
    ; OK porque TODOS los callers solo chequean ==0)
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
    ret

; -----------------------------------------------------------------------------
; irc_tolower: Convierte carácter a minúscula según RFC 1459
; Input: A = carácter
; Output: A = lowercase character (incluyendo []\^ -> {}|~)
; Preserva: BC, DE, HL
; -----------------------------------------------------------------------------
irc_tolower:
    ; A-Z -> a-z
    cp 'A'
    jr c, irc_tl_check_special
    cp 'Z' + 1
    jr nc, irc_tl_check_special
    add a, 32
    ret
irc_tl_check_special:
    ; RFC 1459: [ -> {, \ -> |, ] -> }, ^ -> ~
    cp '['
    jr z, irc_tl_add32
    cp '\\'
    jr z, irc_tl_add32
    cp ']'
    jr z, irc_tl_add32
    cp '^'
    jr z, irc_tl_add32
    ret
irc_tl_add32:
    add a, 32
    ret

; -----------------------------------------------------------------------------
; const char* st_stristr(const char *hay, const char *needle)
; Búsqueda case-insensitive de substring
; Retorna: puntero a primera ocurrencia, o NULL
; Stack: [IX+4,5]=hay, [IX+6,7]=needle
; -----------------------------------------------------------------------------
_st_stristr:
    push ix
    ld ix, 0
    add ix, sp

    ld l, (ix+4)
    ld h, (ix+5)            ; HL = hay
    ld e, (ix+6)
    ld d, (ix+7)            ; DE = needle
    
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
    ret

sstr_ret_hay:
    ; HL ya tiene hay
    pop ix
    ret

sstr_fail:
    ld hl, 0
    pop ix
    ret

; =============================================================================
; CONVERSIONES NUMÉRICAS
; =============================================================================

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
    jr u16_common_1000      ; Saltar a código común

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

u16_common_1000:
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
    inc hl                  ; Return next position
    
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
    push ix
    ld ix, 0
    add ix, sp

    ld l, (ix+4)
    ld h, (ix+5)            ; HL = lectura
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
    ret

; Subrutina: ¿es A un dígito?
; Retorna: Carry set si es dígito
strip_is_digit:
    cp '0'
    jr c, sid_false         ; < '0' -> no es dígito
    cp '9' + 1
    ccf                     ; > '9' -> carry clear, else carry set
    ret
sid_false:
    or a                    ; clear carry
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
    ld a, (_theme_attrs + TA_BORDER)
    out (0xFE), a

    ; 2. Banner row 0 (0x5800 - 32 bytes) BRIGHT 1
    ld hl, 0x5800
    ld bc, 32
    ld a, (_theme_attrs + TA_BANNER)
    or 0x40
    call _fast_fill_attr
    ; 3. Banner row 1 (0x5820 - 32 bytes) BRIGHT 0
    ld hl, 0x5820
    ld bc, 32
    ld a, (_theme_attrs + TA_BANNER)
    and 0xBF
    call _fast_fill_attr

    ; 3. Separador Superior row 2 (0x5840)
    ld hl, 0x5840
    ld bc, 32
    ld a, (_theme_attrs + TA_MAIN_BG)
    call _fast_fill_attr

    ; 4. Área Chat rows 3-19 (0x5860 - 17 líneas = 544 bytes)
    ld hl, 0x5860
    ld bc, 544
    ld a, (_theme_attrs + TA_MAIN_BG)
    call _fast_fill_attr

    ; 5. Separador Inferior (0x5A80)
    ld hl, 0x5A80
    ld bc, 32
    ld a, (_theme_attrs + TA_MAIN_BG)
    call _fast_fill_attr

    ; 6. Barra Estado (0x5AA0)
    ld hl, 0x5AA0
    ld bc, 32
    ld a, (_theme_attrs + TA_STATUS)
    call _fast_fill_attr

    ; 7. Input (0x5AC0)
    ld hl, 0x5AC0
    ld bc, 64
    ld a, (_theme_attrs + TA_INPUT_BG)
    call _fast_fill_attr

    ; 8. AVISAR A TODOS LOS SISTEMAS DE REPINTADO
    ld hl, _force_status_redraw   ; Avisa al renderizador interno
    ld (hl), 1
    
    ld hl, _status_bar_dirty      ; Avisa al bucle Main de C <--- CRÍTICO
    ld (hl), 1

    pop iy
    ret

; -----------------------------------------------------------------------------
; Rutina auxiliar: _fast_fill_attr
; Rellena BC bytes en (HL) con el valor A.
; CORREGIDA Y OPTIMIZADA: Elimina el bug de stack (ix+1) y optimiza el flujo.
; -----------------------------------------------------------------------------
_fast_fill_attr:
    ld d, a                 ; Guardar valor a escribir
    ld a, b
    or c                    ; BC == 0?
    ret z
    ld (hl), d              ; Escribir primer byte (usar D directamente)
    dec bc
    ld a, b
    or c                    ; BC == 0 después de dec?
    ret z
    ld d, h
    ld e, l                 ; DE = HL
    inc de                  ; DE = HL+1
    ldir
    ret



; =============================================================================
; void cls_fast(void)
; Borrado completo de pantalla con estrategia "Chunked LDIR".
; 1. Borra el bitmap (6144 bytes) en bloques de 128 bytes para no bloquear UART.
; 2. Repinta los atributos usando el sistema de temas.
; =============================================================================

_cls_fast:
    push iy

    ; --- BORRADO DE BITMAP (0x4000..0x57FF) = 6144 bytes ---
    ld hl, 0x4000
    xor a
    ld (hl), a
    ld de, 0x4001
    ld bc, 6143
    ldir

    ; --- ATRIBUTOS ---
    pop iy
    jp _reapply_screen_attributes


; =============================================================================
; void uart_drain_to_buffer(void)
; Lee bytes del UART y los mete en el Ring Buffer lo más rápido posible.
; CRÍTICO: Minimiza la latencia entre bytes para evitar pérdida de datos en AY.
; OPTIMIZED: No usa EXX, contador en IYL
; =============================================================================

_uart_drain_to_buffer:
    push iy                 ; Preservar IY (estándar z88dk)

    ld a, (_uart_drain_limit)
    or a
    jr z, drain_setup_safety

    ; Caso con límite (ej: 32 bytes) - usar IYL como contador
    ld iyl, a
    jr drain_loop_start

drain_setup_safety:
    ; Caso límite=0 -> Usar seguridad de 255 iteraciones (suficiente)
    ld iyl, 255

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

    ; Decrementar contador
    dec iyl
    jr nz, drain_loop_start

drain_exit:
    pop iy
    ret

; =============================================================================
; void scroll_main_zone(void)
; Optimized scroll of chat zone (lines 2-19 -> 2-18).
; UNROLLED version: eliminates table and inner loop maintaining exactly
; the same 5 blocks and lengths as original version.
; 
; Timing: ~12,000 t-states (~3.4ms @ 3.5MHz)
; Note: Interrupts disabled during scroll to avoid visual artifacts
; =============================================================================
_scroll_main_zone:
    push iy
    di

    ld iyl, 0              ; IYL = scanline offset (0..7)

smz_scanline_loop:
    ; Precompute: C = scanline offset for H/D additions
    ld c, iyl

    ; BLOQUE 1: Filas 4-7 -> 3-6 (Src: 0x4080, Dest: 0x4060, Len: 128)
    ld a, 0x40
    add a, c
    ld h, a
    ld d, a
    ld l, 0x80
    ld e, 0x60
    ld b, 0                ; BC = 128 (C is offset but we need BC=128)
    push bc                ; save offset
    ld bc, 128
    ldir
    pop bc                 ; restore C = offset

    ; BLOQUE 2: Fila 8 -> 7 (Src: 0x4800, Dest: 0x40E0, Len: 32)
    ld a, 0x48
    add a, c
    ld h, a
    ld a, 0x40
    add a, c
    ld d, a
    ld l, 0x00
    ld e, 0xE0
    push bc
    ld bc, 32
    ldir
    pop bc

    ; BLOQUE 3: Filas 9-15 -> 8-14 (Src: 0x4820, Dest: 0x4800, Len: 224)
    ld a, 0x48
    add a, c
    ld h, a
    ld d, a
    ld l, 0x20
    ld e, 0x00
    push bc
    ld bc, 224
    ldir
    pop bc

    ; BLOQUE 4: Fila 16 -> 15 (Src: 0x5000, Dest: 0x48E0, Len: 32)
    ld a, 0x50
    add a, c
    ld h, a
    ld a, 0x48
    add a, c
    ld d, a
    ld l, 0x00
    ld e, 0xE0
    push bc
    ld bc, 32
    ldir
    pop bc

    ; BLOQUE 5: Filas 17-19 -> 16-18 (Src: 0x5020, Dest: 0x5000, Len: 96)
    ld a, 0x50
    add a, c
    ld h, a
    ld d, a
    ld l, 0x20
    ld e, 0x00
    ld bc, 96
    ldir

    ; Siguiente scanline (0..7)
    inc iyl
    ld a, iyl
    cp 8
    jr nz, smz_scanline_loop

    ; Scroll atributos (16 filas: 4->3 ... 19->18)
    ld de, 0x5860
    ld hl, 0x5880
    ld bc, 512
    ldir

    ; Limpiar última línea (19) píxeles + atributos
    ld a, (_current_attr)
    ld c, a
    ld a, 19
    call cli_internal

    ei
    pop iy
    ret


; =============================================================================
; void main_newline(void)
; Identical behavior to the C version in src/spectalk.c
; Notes:
;  - pagination_pause() is C (sdcc_iy): preserve IY around the call.
;  - _print_str64_char clobbers BC/DE/HL/AF: preserve BC (loop counter) and HL.
;  - micro-opt: keep HL -> _g_ps64_col and store (hl)=c each iteration.
; =============================================================================
_main_newline:
    ; Suppress output during help overlay
    ld a, (_help_active)
    or a
    ret nz
    push ix
    push iy

    ; Invalidar cache de fila (la fila va a cambiar)
    ld a, 0xFF
    ld (cache_row_y), a

    ; main_col = 0
    xor a
    ld (_main_col), a

    ; if (pagination_active)
    ld a, (_pagination_active)
    or a
    jr z, mn_no_pagination

    ; pagination_lines++
    ld hl, _pagination_lines
    inc (hl)

    ; if (pagination_lines >= MAIN_LINES-1)  (17-1 = 16)
    ld a, (hl)
    cp 16
    jr c, mn_inc_line

    ; if (pagination_pause()) return;
    push iy
    call _pagination_pause
    pop iy
    ld a, l
    or a
    jr nz, mn_ret
    jr mn_indent

mn_no_pagination:
    ld a, (_main_line)
    cp 19               ; MAIN_END = 19
    jr c, mn_inc_line_do
    call _scroll_main_zone
    jr mn_indent

mn_inc_line:
    ld a, (_main_line)
    cp 19
    jr nc, mn_indent
mn_inc_line_do:
    inc a
    ld (_main_line), a

mn_indent:
    ; if (wrap_indent > 0) clear indent zone directly and set main_col
    ld a, (_wrap_indent)
    or a
    jr z, mn_ret

    ; Guardar wrap_indent para después
    ld c, a             ; C = wrap_indent (num cols)

    ; --- Calcular screen addr de la fila ---
    ld a, (_main_line)
    add a, a
    ld l, a
    ld h, 0
    ld de, _screen_row_base
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a             ; HL = screen base de la fila

    ; Bytes a borrar = (wrap_indent + 1) / 2  (redondeo arriba)
    ld a, c
    inc a
    srl a               ; A = bytes a borrar
    ld b, a             ; B = byte count

    ; Borrar 8 scanlines por cada byte
    push hl             ; Guardar screen base
    push bc             ; Guardar byte count + wrap_indent en C
mn_indent_scanline:
    push hl             ; Guardar inicio de scanline
    push bc             ; Guardar counters
    xor a
mn_indent_byte:
    ld (hl), a
    inc hl
    djnz mn_indent_byte
    pop bc              ; Restaurar counters
    pop hl              ; Restaurar inicio de scanline
    inc h               ; Siguiente scanline
    ld a, h
    and 0x07
    jr nz, mn_indent_scanline

    pop bc              ; Restaurar byte count (B) + wrap_indent (C)
    pop hl              ; (descartar screen base)

    ; --- Escribir atributos ---
    ld a, (_main_line)
    add a, a
    ld l, a
    ld h, 0
    ld de, _attr_row_base
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a             ; HL = attr base de la fila

    ld a, (_current_attr)
    ld b, c             ; B = wrap_indent (num cols)
    inc b
    srl b               ; B = ceil(wrap_indent/2) = attrs a escribir
    jr z, mn_indent_attr_done
mn_indent_attr:
    ld (hl), a
    inc hl
    djnz mn_indent_attr
mn_indent_attr_done:

    ; Actualizar main_col y g_ps64_col/y/attr para consistencia
    ld a, (_wrap_indent)
    ld (_main_col), a
    ld (_g_ps64_col), a
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_current_attr)
    ld (_g_ps64_attr), a

    ; Pre-warm row cache for next line's first character
    ; Saves ~600 T-states when line starts with spaces (wrap indent)
    call p64_get_scr_base

mn_ret:
    pop iy
    pop ix
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
    jr z, tp_exit               ; OPT: jp→jr
    ld a, (hl)
    or a
    jr z, tp_exit               ; OPT: jp→jr

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
    jr z, tp_exit           ; OPT: jp→jr
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
    jr z, tp_exit           ; OPT: jp→jr
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
    jr z, tp_exit           ; OPT: jp→jr
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
; void draw_badge_dither(uint8_t count) __z88dk_fastcall
; Dibuja patrón de triángulos en AMBAS filas del banner (0 y 1).
; count en L = número de celdas (5 fijo)
; phys_x = 32 - count (automático)
; =============================================================================
_draw_badge_dither:
    ld a, l
    push af                 ; guardar count para fila 1

    ld b, l                 ; B = count
    ld a, 32
    sub l
    ld c, a                 ; C = phys_x (preservado entre filas)

    ; --- Fila 0: base 0x4000 ---
    ld hl, 0x4000
    ld e, c
    ld d, 0
    add hl, de
    call dbd_row

    ; --- Fila 1: base 0x4020 ---
    pop af
    ld b, a                 ; restaurar count
    ld hl, 0x4020
    ld e, c                 ; C preservado
    ld d, 0
    add hl, de
    ; fall through

dbd_row:
    ; Dibuja dither en B celdas a partir de HL
dbd_cell_loop:
    push bc
    push hl
    ld de, dbd_pattern
    ld b, 8
dbd_scanline:
    ld a, (de)
    ld (hl), a
    inc de
    inc h
    djnz dbd_scanline
    pop hl
    inc hl
    pop bc
    djnz dbd_cell_loop
    ret

; Patrón de triángulo inferior-derecho (8 bytes)
; PAPER = fondo (izquierda), INK = triángulo (derecha)
dbd_pattern:
    defb 0x00               ; ........
    defb 0x01               ; .......X
    defb 0x03               ; ......XX
    defb 0x07               ; .....XXX
    defb 0x0F               ; ....XXXX
    defb 0x1F               ; ...XXXXX
    defb 0x3F               ; ..XXXXXX
    defb 0x7F               ; .XXXXXXX

; =============================================================================
; OPTIMIZED C FUNCTIONS REPLACEMENT
; High-performance replacements for spectalk.c bottlenecks
; (EXTERNs already declared at top of file)
; =============================================================================

EXTERN _ignore_count
EXTERN _ignore_list

EXTERN _show_timestamps
EXTERN _wrap_indent
EXTERN _current_attr
EXTERN _time_hour
EXTERN _time_minute
; _ATTR_MSG_TIME = _theme_attrs + 12
EXTERN _last_ts_hour
EXTERN _last_ts_minute

; -----------------------------------------------------------------------------
; void main_print_time_prefix(void)
; Prints "HH:MM| " using ATTR_MSG_TIME, then restores current_attr.
; show_timestamps: 0=off, 1=always, 2=on-change (only when HH:MM differs)
; Sets wrap_indent=6 when printed, 0 when skipped.
; -----------------------------------------------------------------------------
PUBLIC _main_print_time_prefix
_main_print_time_prefix:
    push ix
    push iy

    ld a, (_show_timestamps)
    or a
    jr z, mptp_off          ; mode 0: off

    cp 2
    jr nz, mptp_do          ; mode 1: always print

    ; mode 2: on-change — check if HH:MM matches last printed
    ld a, (_time_hour)
    ld b, a
    ld a, (_last_ts_hour)
    cp b
    jr nz, mptp_changed     ; hour differs
    ld a, (_time_minute)
    ld b, a
    ld a, (_last_ts_minute)
    cp b
    jr nz, mptp_changed     ; minute differs

    ; Same time — print spaces as indent (6 chars) instead of timestamp
    ld a, 6
    ld (_wrap_indent), a
    ld b, 6
mptp_spaces:
    push bc
    ld l, ' '
    call _main_putc
    pop bc
    djnz mptp_spaces
    jr mptp_ret

mptp_changed:
    ; Update last printed time
    ld a, (_time_hour)
    ld (_last_ts_hour), a
    ld a, (_time_minute)
    ld (_last_ts_minute), a

mptp_do:
    ; saved_attr = current_attr
    ld a, (_current_attr)
    push af

    ; current_attr = ATTR_MSG_TIME
    ld a, (_theme_attrs + 12) ; ATTR_MSG_TIME
    ld (_current_attr), a

    ; print HH
    ld a, (_time_hour)
    call mptp_put2

    ; ':'
    ld l, ':'
    call _main_putc

    ; print MM
    ld a, (_time_minute)
    call mptp_put2

    ; " " (solo espacio, sin barra vertical)
    ld l, ' '
    call _main_putc

    ; restore current_attr AND g_ps64_attr (prevent color leak to next char)
    pop af
    ld (_current_attr), a
    ld (_g_ps64_attr), a

    ; wrap_indent = 6
    ld a, 6
    ld (_wrap_indent), a
    jr mptp_ret

mptp_off:
    xor a
    ld (_wrap_indent), a

mptp_ret:
    pop iy
    pop ix
    ret

; --- local: print 2-digit decimal from A (00..99), leading zero ---
; clobbers AF, BC
mptp_put2:
    ld b, 0
mptp_div10:
    cp 10
    jr c, mptp_div_done
    sub 10
    inc b
    jr mptp_div10
mptp_div_done:
    ld c, a            ; ones

    ; tens
    ld a, b
    add a, '0'
    ld l, a
    push bc
    call _main_putc
    pop bc

    ; ones
    ld a, c
    add a, '0'
    ld l, a
    jp _main_putc           ; tail call

; -----------------------------------------------------------------------------
; void main_putc(char c) __z88dk_fastcall
; Imprime un caracter gestionando saltos de línea y coordenadas.
; L = c
; -----------------------------------------------------------------------------
PUBLIC _main_putc
_main_putc:
    ; Suppress output during help overlay
    ld a, (_help_active)
    or a
    ret nz
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
; void main_print_wrapped_ram(char *s) __z88dk_fastcall
; Wrap por palabras dentro del ancho disponible (64 - main_col).
; HL = string (RAM)  [MODIFICA temporalmente RAM insertando 0]
; -----------------------------------------------------------------------------
PUBLIC _main_print_wrapped_ram
_main_print_wrapped_ram:
    push ix
    
    ; Convertir UTF-8 a ASCII in-place antes de procesar
    call _utf8_to_ascii     ; HL se preserva (fastcall, mismo puntero)

mpwr_loop:
    ld a, (hl)
    or a
    jp z, mpwr_finalize          ; <-- era jr z

    ; Si estamos al final de línea, pasar a la siguiente SIN perder HL
    ld a, (_main_col)
    cp 64
    jr c, mpwr_have_col
    push hl
    call _main_newline
    pop hl

    ; Si la paginación fue cancelada durante el pause, main_col queda en 64.
    ; Salir para no pisar "Cancelled".
    ld a, (_main_col)
    cp 64
    jp z, mpwr_abort             ; <-- era jr z

mpwr_have_col:
    ; avail = 64 - main_col  -> B
    ld a, (_main_col)
    ld b, a
    ld a, 64
    sub b
    ld b, a

    ; DE = start
    ld d, h
    ld e, l

    ld ix, 0                     ; último espacio (0 = ninguno)

mpwr_scan:
    ld a, (hl)
    or a
    jr z, mpwr_cut_end

    cp 32                        ; ' '
    jr nz, mpwr_scan_next
    push hl
    pop ix

mpwr_scan_next:
    inc hl
    djnz mpwr_scan

    ; Alcanzados "avail" chars
    ld a, ixl
    or ixh
    jr z, mpwr_cut_hard

    ; BC = último espacio
    push ix
    pop bc

    ; Si el único espacio es el primero (BC == DE), usar hard cut
    ld a, b
    cp d
    jr nz, mpwr_use_space
    ld a, c
    cp e
    jr z, mpwr_cut_hard

mpwr_use_space:
    ; cutpoint = BC (espacio), next_start = BC+1
    ld h, b
    ld l, c
    inc bc
    jr mpwr_cut_common

mpwr_cut_hard:
    ; cutpoint = HL (start+avail), next_start = HL
    ld b, h
    ld c, l
    jr mpwr_cut_common

mpwr_cut_end:
    ; cutpoint = HL ('\0'), next_start = HL
    ld b, h
    ld c, l

mpwr_cut_common:
    ; Guardar char original y cortar temporalmente
    ld a, (hl)
    push af
    xor a
    ld (hl), a

    push hl                      ; cutpoint
    push bc                      ; next_start

    ; Imprimir segmento (start = DE)
    ; Fast path: si estamos al inicio de línea y sin indent, usar print_line64_fast
    ld a, (_main_col)
    or a
    jr nz, mpwr_print_puts
    ld a, (_wrap_indent)
    or a
    jr nz, mpwr_print_puts

    ; Args para _print_line64_fast: [y][start_lo][start_hi][attr]
    ld a, (_current_attr)
    ld b, a
    ld c, d                      ; C = start_hi
    push bc                      ; [start_hi][attr]

    ld a, (_main_line)
    ld c, a                      ; C = y
    ld b, e                      ; B = start_lo
    push bc                      ; [y][start_lo]

    call _print_line64_fast

    ; Limpiar 4 bytes de args
    pop bc
    pop bc
    jr mpwr_print_done

mpwr_print_puts:
    ld h, d
    ld l, e
    call _main_puts

mpwr_print_done:
    ; Restaurar next_start, cutpoint y char
    pop hl                       ; HL = next_start
    pop de                       ; DE = cutpoint
    pop af
    ld (de), a

    ; Si terminó cadena, salir al finalize
    ld a, (hl)
    or a
    jr z, mpwr_finalize          ; OPT: jp→jr (16 bytes)

    ; Nueva línea para continuar SIN perder HL
    push hl
    call _main_newline
    pop hl

    ; Si la paginación fue cancelada durante el pause, salir
    ld a, (_main_col)
    cp 64
    jr z, mpwr_abort             ; OPT: jp→jr (13 bytes)

    jp mpwr_loop                 ; <-- era jr mpwr_loop

mpwr_finalize:
    ; Igual que main_print: reset indent antes del newline final
    xor a
    ld (_wrap_indent), a
    call _main_newline

    pop ix
    ret

mpwr_abort:
    pop ix
    ret
    
; -----------------------------------------------------------------------------
; void main_puts(const char *s) __z88dk_fastcall
; Imprime una cadena usando la versión optimizada de putc.
; HL = string
; -----------------------------------------------------------------------------

EXTERN _help_active
PUBLIC _main_puts
_main_puts:
    ; Suppress output during help overlay
    ld a, (_help_active)
    or a
    ret nz
    ; HL = string pointer
    ; Initialize BPE return stack (empty)
    push hl
    ld hl, bpe_rstack
    ld (bpe_rsp), hl
    pop hl
    ; Optimización: cargar globals una vez y mantener en registros
    ; B = main_col, C = current_attr, D/E = string pointer
    ld d, h
    ld e, l             ; DE = puntero string
    
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_current_attr)
    ld c, a             ; C = attr
    ld (_g_ps64_attr), a
    ld a, (_main_col)
    ld b, a             ; B = col

puts_opt_loop:
    ld a, (de)
    or a
    jp z, puts_bpe_pop  ; 0x00: end of string or end of BPE dict entry

    cp 0x80
    jp nc, puts_bpe_expand ; >= 0x80: BPE token, expand from dictionary

    cp 10
    jp z, puts_opt_nl   ; '\n' -> newline

    ; Chequear wrap
    ld h, a             ; H = caracter (temporal)
    bit 6, b
    jr nz, puts_opt_wrap
    
puts_opt_emit:
    ; --- Inline space fast path (skip unpack_glyph + full render) ---
    ; ~50 T-states for space vs ~650 for full print_str64_char path
    ld a, h
    cp 32
    jr nz, puts_opt_char

    ; Verify row cache is valid (cold on first char after newline/scroll)
    ; If miss, fall through to full path which warms the cache
    ld a, (_g_ps64_y)
    ld hl, cache_row_y
    cp (hl)
    jr nz, puts_opt_char

    ; Cache hit — render space inline
    ; B = col, C = attr, DE = string ptr
    push de              ; save string pointer

    ; Screen address: cache_scr_base + col/2
    ld hl, (cache_scr_base)
    ld a, b
    srl a
    ld e, a
    ld d, 0
    add hl, de           ; HL = screen byte for this column pair

    bit 0, b
    jr nz, puts_sp_right
    ; Left nibble: AND 0x0F across 8 scanlines
    ld d, 8
puts_sp_left:
    ld a, (hl)
    and 0x0F
    ld (hl), a
    inc h
    dec d
    jr nz, puts_sp_left
    jr puts_sp_attr
puts_sp_right:
    ; Right nibble: AND 0xF0 across 8 scanlines
    ld d, 8
puts_sp_right_lp:
    ld a, (hl)
    and 0xF0
    ld (hl), a
    inc h
    dec d
    jr nz, puts_sp_right_lp
puts_sp_attr:
    ; Attribute: cache_atr_base + col/2
    ld hl, (cache_atr_base)
    ld a, b
    srl a
    ld e, a
    ld d, 0
    add hl, de
    ld a, c              ; C = current_attr
    cp (hl)
    jr z, puts_sp_done
    ld (hl), a
puts_sp_done:
    pop de               ; restore string pointer
    inc b                ; main_col++
    inc de               ; next char
    jp puts_opt_loop

puts_opt_char:
    ; Emitir caracter normal sin re-cargar globals
    ld a, b
    ld (_g_ps64_col), a
    ld l, h             ; L = caracter para print_str64_char
    push bc
    push de
    call _print_str64_char
    pop de
    pop bc
    inc b               ; main_col++
    inc de              ; siguiente caracter
    jp puts_opt_loop

puts_opt_wrap:
    ; Necesitamos wrap - sincronizar y llamar main_newline
    ld a, b
    ld (_main_col), a
    push de
    push hl             ; H tiene el caracter actual
    call _main_newline
    pop hl
    pop de
    ; Recargar globals post-newline (main_newline clobbers BC, changes main_line/col)
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_current_attr)
    ld c, a             ; FIX: restore C = attr (clobbered by main_newline)
    ld a, (_main_col)
    ld b, a
    jp puts_opt_emit

puts_opt_nl:
    ; '\n' - sincronizar col y llamar newline
    ld a, b
    ld (_main_col), a
    push de
    call _main_newline
    pop de
    ; Recargar globals
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_main_col)
    ld b, a
    inc de
    jp puts_opt_loop

; --- BPE expansion for puts_opt ---
; Token >= 0x80 found in A: push continuation (DE+1) to BPE stack,
; set DE to dict entry (3 bytes: b1, b2, 0x00). Loop continues reading
; from dict. On null, pops continuation and resumes original string.
puts_bpe_expand:
    ; A = token (0x80-0xFF), DE = current string position
    ; B = main_col, C = current_attr — MUST be preserved!
    inc de                  ; advance past the token
    push bc                 ; save B=col, C=attr
    ; Push continuation address (DE) onto BPE return stack
    ld hl, (bpe_rsp)
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (bpe_rsp), hl
    ; Calculate dict entry address: bpe_dict + (token - 0x80) * 3
    sub 0x80
    ld l, a
    ld h, 0
    ld c, l
    ld b, h                 ; BC = (token - 0x80) — temp, will restore
    add hl, hl              ; HL = * 2
    add hl, bc              ; HL = * 3
    ld bc, bpe_dict
    add hl, bc
    ex de, hl               ; DE = &bpe_dict[(token-0x80)*3]
    pop bc                  ; restore B=col, C=attr
    jp puts_opt_loop         ; continue reading from dict entry

puts_bpe_pop:
    ; Null byte hit: end of string or end of BPE dict entry
    ; Check if BPE stack has entries
    ld hl, (bpe_rsp)
    ld de, bpe_rstack
    or a                    ; clear carry
    sbc hl, de
    jr z, puts_opt_done     ; stack empty -> real end of string
    ; Pop continuation address from BPE stack
    ld hl, (bpe_rsp)
    dec hl
    ld d, (hl)
    dec hl
    ld e, (hl)
    ld (bpe_rsp), hl
    jp puts_opt_loop         ; resume original string

puts_opt_done:
    ; Sincronizar main_col de vuelta
    ld a, b
    ld (_main_col), a
    ret


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
; Mejor usamos __z88dk_callee estándar: Stack: [RetAddr] [Addr] [Count]
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
    dec hl          ; HL = Último byte origen
    
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
    ret z           ; max_len == 0 → nothing to do
    dec a           ; max_len - 1 = max chars to copy
    ld b, a         ; B = loop counter
    jr z, stcn_term ; If max_len was 1, just write \0
    
stcn_loop:
    ld a, (hl)
    or a
    jr z, stcn_term ; Source ended
    ld (de), a
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
EXTERN _S_CRLF

; Common string " " for param separator
isc_space: defb ' ', 0

; void irc_send_cmd_internal(const char *cmd, const char *p1, const char *p2)
; Sends: CMD [p1] [ :p2]\r\n
; Stack: [ret] [cmd] [p1] [p2]
PUBLIC _irc_send_cmd_internal
_irc_send_cmd_internal:
    ; Check connection state first
    ld a, (_connection_state)
    cp 2                    ; STATE_TCP_CONNECTED
    ret c                   ; Return if not connected
    
    pop bc                  ; ret
    pop hl                  ; cmd
    pop de                  ; p1
    pop ix                  ; p2 (do NOT clobber IY: sdcc_iy frame pointer)
    push ix
    push de
    push hl
    push bc
    
    ; Send cmd
    call _uart_send_string
    
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
    ld hl, _S_CRLF
    jp _uart_send_string    ; tail call



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
    pop hl                ; restore original ERR_SP
    ld (23613), hl
    pop iy
    ld l, 1               ; return 1 (detected)
    ret
esx_det_fail:
    ; ROM ERROR_1 jumped here via ERR_SP — no esxDOS
    pop hl                ; restore original ERR_SP (was pushed before handler)
    ld (23613), hl
    pop iy
    ld l, 0               ; return 0 (not detected)
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
    push iy
    push ix
    push hl
    pop ix              ; IX = HL = path (esxDOS wants both)
    ld a, '*'           ; default drive
    ld b, 0x01          ; FA_READ
    rst 8
    defb 0x9A           ; F_OPEN
    jr nc, esxfo_ok
    xor a               ; error → handle = 0
esxfo_ok:
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
    ld hl, (_esx_buf)
    push hl
    pop ix              ; esxDOS F_READ needs buffer destination in IX
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

; -----------------------------------------------------------------------------
; void esx_fcreate(const char *path) __z88dk_fastcall
; Input:  HL = path string
; Output: _esx_handle = file handle (0 on error)
; Creates file for writing (FA_WRITE | FA_CREATE_AL = 0x0C)
; -----------------------------------------------------------------------------
_esx_fcreate:
    push iy
    push ix
    push hl
    pop ix              ; IX = HL = path
    ld a, '*'           ; default drive
    ld b, 0x0C          ; FA_WRITE | FA_CREATE_AL
    rst 8
    defb 0x9A           ; F_OPEN
    jr nc, esxfc_ok
    xor a               ; error → handle = 0
esxfc_ok:
    ld (_esx_handle), a
    pop ix
    pop iy
    ret

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
    pop ix
    pop iy
    ret


; =============================================================================
; UTF-8 to ASCII - Versión corregida
; Cubre Latin-1 Supplement (C2 80-BF, C3 80-BF) = codepoints 0080-00FF
; =============================================================================
PUBLIC _utf8_to_ascii

_utf8_to_ascii:
    push hl                 ; Guardar inicio para retornar
    ld d, h
    ld e, l                 ; DE = escritura, HL = lectura

u8a_loop:
    ld a, (hl)
    or a
    jp z, u8a_done
    
    cp 0x80
    jr c, u8a_copy          ; OPT: jp→jr (00-7F: ASCII)

    cp 0xC2
    jr c, u8a_skip1         ; OPT: jp→jr (80-C1: inválido)

    cp 0xC4
    jr c, u8a_latin1        ; OPT: jp→jr (C2-C3: Latin-1)

    cp 0xE0
    jr c, u8a_skip2         ; OPT: jp→jr (C4-DF: Latin Extended)

    cp 0xF0
    jr c, u8a_3byte         ; E0-EF: 3-byte handler (smart quotes)
    
    ; F0+: 4 bytes
    inc hl
    ld a, (hl) : or a : jp z, u8a_done
    inc hl
    ld a, (hl) : or a : jp z, u8a_done
    inc hl
    ld a, (hl) : or a : jp z, u8a_done
    inc hl
    ld a, '?'
    jp u8a_store

; Smart quotes: E2 80 98/99 → apostrophe (39), E2 80 9C/9D → double quote (34)
u8a_3byte:
    cp 0xE2
    jr nz, u8a_skip3
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
    cp 0x80
    jr nz, u8a_skip1        ; not E2 80 xx → skip byte 3 → '?'
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
    inc hl
    or 0x01                  ; pair 98/99→99, 9C/9D→9D
    cp 0x99
    ld a, 39                 ; apostrophe for smart single quotes
    jp z, u8a_store
    ld a, 34                 ; double quote for smart double quotes (cp 0x99 didn't match → must be 0x9D)
    jp u8a_store
    ld a, '?'
    jp u8a_store

u8a_skip3:
    inc hl
    ld a, (hl) : or a : jp z, u8a_done
u8a_skip2:
    inc hl
    ld a, (hl) : or a : jp z, u8a_done
u8a_skip1:
    inc hl
    ld a, '?'
    jr u8a_store            ; OPT: jp→jr (~74 bytes)

u8a_copy:
    ld (de), a
    inc hl
    inc de
    jr u8a_loop             ; OPT: jp→jr (backward ~70 bytes)

u8a_latin1:
    ; A = C2 o C3
    ld b, a                 ; B = primer byte
    inc hl
    ld a, (hl)
    or a
    jr z, u8a_done          ; OPT: jp→jr
    ld c, a                 ; C = segundo byte (80-BF)
    inc hl

    ; Verificar que C es continuation (80-BF)
    ld a, c
    and 0xC0
    cp 0x80
    jr nz, u8a_invalid      ; OPT: jp→jr

    ; B=C2: codepoint = 80 + (C & 3F) = 80-BF
    ; B=C3: codepoint = C0 + (C & 3F) = C0-FF
    ld a, b
    cp 0xC3
    jr z, u8a_c3            ; OPT: jp→jr

    ; C2: codepoints 80-BF (símbolos, ¡¿ etc)
    ld a, c
    cp 0xA1                 ; ¡
    ld a, '!'
    jr z, u8a_store         ; OPT: jp→jr
    ld a, c
    cp 0xBF                 ; ¿
    ld a, '?'
    jr z, u8a_store         ; OPT: jp→jr
    ld a, c
    cp 0xAB                 ; «
    ld a, '<'
    jr z, u8a_store         ; OPT: jp→jr
    ld a, c
    cp 0xBB                 ; »
    ld a, '>'
    jr z, u8a_store         ; OPT: jp→jr
    ld a, ' '               ; resto -> espacio
    jr u8a_store            ; OPT: jp→jr

u8a_c3:
    ; C3: codepoints C0-FF (vocales acentuadas, ñ, ç, etc)
    ; Índice en tabla = C & 3F (0-3F corresponde a C0-FF)
    ld a, c
    and 0x3F
    ld c, a
    ld b, 0
    push hl             ; FIX-C2: guardar puntero de lectura del string
    ld hl, u8a_tbl_c0
    add hl, bc
    ld a, (hl)
    pop hl              ; FIX-C2: restaurar puntero de lectura
    jr u8a_store            ; OPT: jp→jr (5 bytes)

u8a_invalid:
    ld a, '?'
    
u8a_store:
    ld (de), a
    inc de
    jp u8a_loop

u8a_done:
    xor a
    ld (de), a
    pop hl
    ret

; Tabla para C3 80-BF -> codepoints C0-FF (64 bytes)
; ÀÁÂÃÄÅÆÇ ÈÉÊËÌÍÎÏ ÐÑÒÓÔÕÖ× ØÙÚÛÜÝÞ߀ àáâãäåæç èéêëìíîï ðñòóôõö÷ øùúûüýþÿ
u8a_tbl_c0:
    defb 'A','A','A','A','A','A','A','C'  ; C0-C7: ÀÁÂÃÄÅÆÇ
    defb 'E','E','E','E','I','I','I','I'  ; C8-CF: ÈÉÊËÌÍÎÏ
    defb 'D','N','O','O','O','O','O','x'  ; D0-D7: ÐÑÒÓÔÕÖ×
    defb 'O','U','U','U','U','Y','T','s'  ; D8-DF: ØÙÚÛÜÝÞß
    defb 'a','a','a','a','a','a','a','c'  ; E0-E7: àáâãäåæç
    defb 'e','e','e','e','i','i','i','i'  ; E8-EF: èéêëìíîï
    defb 'o','n','o','o','o','o','o','/'  ; F0-F7: ðñòóôõö÷
    defb 'o','u','u','u','u','y','t','y'  ; F8-FF: øùúûüýþÿ

; =============================================================================
; void sntp_process_response(const char *line) __z88dk_fastcall
; Parses AT+CIPSNTPTIME? response to extract HH:MM:SS
; HL = line pointer
; Response format: +CIPSNTPTIME:Thu Jan 01 00:00:00 1970
; =============================================================================
PUBLIC _sntp_process_response
EXTERN _sntp_waiting
EXTERN _sntp_queried
EXTERN _time_hour
EXTERN _time_minute
EXTERN _time_second
EXTERN _last_frames_lo
EXTERN _tick_accum
EXTERN _draw_status_bar

_sntp_process_response:
    ; Validate: if (!line || !*line) return
    ld a, h
    or l
    ret z
    ld a, (hl)
    or a
    ret z

    ; Count length into B, keep HL = start
    push hl
    ld b, 0
spr_len:
    ld a, (hl)
    or a
    jr z, spr_len_done
    inc hl
    inc b
    jr nz, spr_len     ; max 255
spr_len_done:
    pop hl              ; HL = line start, B = len

    ; if (len < 20) return
    ld a, b
    cp 20
    ret c

    ; if (line[0] != '+' || line[1] != 'C') return
    ld a, (hl)
    cp '+'
    ret nz
    inc hl
    ld a, (hl)
    cp 'C'
    ret nz
    dec hl              ; HL = line start again

    ; Check for "1970" at end: line[len-4..len-1]
    ; DE = line + len - 4
    push hl
    ld c, b             ; C = len (save)
    ld a, b
    sub 4
    ld e, a
    ld d, 0
    add hl, de          ; HL = &line[len-4]
    ld a, (hl)
    cp '1'
    jr nz, spr_no1970
    inc hl
    ld a, (hl)
    cp '9'
    jr nz, spr_no1970
    inc hl
    ld a, (hl)
    cp '7'
    jr nz, spr_no1970
    inc hl
    ld a, (hl)
    cp '0'
    jr nz, spr_no1970
    ; It's 1970 — ESP not synced yet
    pop hl
    xor a
    ld (_sntp_waiting), a
    ret

spr_no1970:
    pop hl              ; HL = line start
    ; C = len (still)

    ; Search for HH:MM:SS pattern starting at offset 13
    ; Pattern: line[i] in '0'..'2', line[i+2]==':',  line[i+5]==':'
    ; Loop from i=13 to i < len-7
    ld a, c
    sub 7
    ld b, a             ; B = len-7 (upper bound exclusive)

    ; Advance HL to line+13
    push hl             ; save line start on stack
    ld de, 13
    add hl, de          ; HL = &line[13]
    ld e, 13            ; E = i (current index)

spr_scan:
    ld a, e
    cp b
    jr nc, spr_notfound  ; OPT: jp→jr (i >= len-7)

    ; Check line[i] >= '0' && line[i] <= '2'
    ld a, (hl)
    cp '0'
    jr c, spr_next
    cp '3'              ; '2'+1
    jr nc, spr_next

    ; Check line[i+2] == ':'
    push hl
    inc hl
    inc hl
    ld a, (hl)
    pop hl
    cp ':'
    jr nz, spr_next

    ; Check line[i+5] == ':'
    push hl
    inc hl
    inc hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    pop hl
    cp ':'
    jr nz, spr_next

    ; === FOUND HH:MM:SS at HL ===
    ; Parse hour
    call spr_parse2
    ld (_time_hour), a
    inc hl              ; HL past second digit
    inc hl              ; HL past ':'

    ; Parse minute
    call spr_parse2
    ld (_time_minute), a
    inc hl              ; HL past second digit
    inc hl              ; HL past ':'

    ; Parse second
    call spr_parse2
    ld (_time_second), a

    ; Validate ranges: hour < 24, minute < 60, second < 60
    ld a, (_time_hour)
    cp 24
    jr nc, spr_invalid      ; invalid hour
    ld a, (_time_minute)
    cp 60
    jr nc, spr_invalid      ; invalid minute
    ld a, (_time_second)
    cp 60
    jr nc, spr_invalid      ; invalid second

    ; Sync frame ticker to current FRAMES, sntp_waiting = 0, sntp_queried = 1
    ld a, (23672)           ; FRAMES low byte
    ld (_last_frames_lo), a
    xor a
    ld (_tick_accum), a     ; Reset frame accumulator
    ld (_sntp_waiting), a
    ld a, 1
    ld (_sntp_queried), a

    pop hl              ; clean stack (line start)
    jp _draw_status_bar ; tail call

spr_next:
    inc hl
    inc e
    jr spr_scan             ; OPT: jp→jr (backward ~100 bytes)

spr_notfound:
    pop hl              ; clean stack (line start)
    xor a
    ld (_sntp_waiting), a
    ret

spr_invalid:
    pop hl              ; clean stack (line start)
    ret                 ; keep sntp_waiting=1 so it retries

; --- subroutine: parse 2-digit decimal at (HL) ---
; Input:  HL = pointer to first digit
; Output: A = parsed value (0..99), HL = pointer to second digit
; Clobbers: C
spr_parse2:
    ld a, (hl)
    sub '0'
    ld c, a
    add a, a            ; *2
    add a, a            ; *4
    add a, c            ; *5
    add a, a            ; *10
    inc hl
    ld c, a
    ld a, (hl)
    sub '0'
    add a, c
    ret

; =============================================================================
; uint8_t read_key(void)
; Keyboard handler with debounce and auto-repeat.
; Returns key code in L (0 = no key)
; Uses globals: last_k, repeat_timer, debounce_zero
; Calls: in_inkey()
; =============================================================================
PUBLIC _read_key
EXTERN _in_inkey
EXTERN _last_k
EXTERN _repeat_timer
EXTERN _debounce_zero

DEFC RK_KEY_LEFT = 8
DEFC RK_KEY_RIGHT = 9
DEFC RK_KEY_DOWN = 10
DEFC RK_KEY_UP = 11
DEFC RK_KEY_BS = 12

_read_key:
    push iy
    call _in_inkey      ; L = key (0 if none)
    pop iy
    ld a, l
    or a
    jr nz, rk_got_key

    ; k == 0: clear state
    xor a
    ld (_last_k), a
    ld (_repeat_timer), a
    ; if (debounce_zero) debounce_zero--
    ld a, (_debounce_zero)
    or a
    jr z, rk_ret_zero
    dec a
    ld (_debounce_zero), a
rk_ret_zero:
    ld l, 0
    ret

rk_got_key:
    ld b, a             ; B = k

    ; if (k == '0' && debounce_zero > 0) → suppress
    cp '0'
    jr nz, rk_check_new
    ld a, (_debounce_zero)
    or a
    jr z, rk_check_new
    dec a
    ld (_debounce_zero), a
    ld l, 0
    ret

rk_check_new:
    ; if (k != last_k) → new key
    ld a, (_last_k)
    cp b
    jr z, rk_repeat

    ; --- Case fold check: ignore Shift release (e.g. 'S' → 's') ---
    ; lk_fold = last_k | 32, k_fold = k | 32
    ; if k_fold == lk_fold && lk_fold in 'a'..'z' → suppress
    ld c, a             ; C = last_k
    or 32               ; A = last_k | 32
    ld d, a             ; D = lk_fold
    ld a, b
    or 32               ; A = k | 32
    cp d
    jr nz, rk_new_emit
    ; Both fold to same letter — check if letter
    ld a, d
    cp 'a'
    jr c, rk_new_emit
    cp 'z'+1
    jr nc, rk_new_emit
    ; Same letter, just shift change — update but don't emit
    ld a, b
    ld (_last_k), a
    ld l, 0
    ret

rk_new_emit:
    ; last_k = k
    ld a, b
    ld (_last_k), a

    ; Set repeat_timer based on key type
    cp RK_KEY_BS
    jr z, rk_new_bs
    cp RK_KEY_LEFT
    jr z, rk_new_arrow
    cp RK_KEY_RIGHT
    jr z, rk_new_arrow
    ; Default: repeat_timer = 20
    ld a, 20
    jr rk_new_set_timer

rk_new_bs:
    ld a, 12
    ld (_repeat_timer), a
    ld a, 8
    ld (_debounce_zero), a
    ld l, b
    ret

rk_new_arrow:
    ld a, 15
rk_new_set_timer:
    ld (_repeat_timer), a
    ; debounce_zero = 0 (non-backspace)
    xor a
    ld (_debounce_zero), a
    ld l, b
    ret

rk_repeat:
    ; k == last_k → auto-repeat handling

    ; if (k == KEY_BACKSPACE) debounce_zero = 8
    ld a, b
    cp RK_KEY_BS
    jr nz, rk_rep_timer
    ld a, 8
    ld (_debounce_zero), a

rk_rep_timer:
    ; if (repeat_timer > 0) { repeat_timer--; return 0; }
    ld a, (_repeat_timer)
    or a
    jr z, rk_rep_fire
    dec a
    ld (_repeat_timer), a
    ld l, 0
    ret

rk_rep_fire:
    ; Timer expired — emit based on key type
    ld a, b
    cp RK_KEY_BS
    jr nz, rk_rep_lr
    ld a, 1
    ld (_repeat_timer), a
    ld l, b
    ret

rk_rep_lr:
    cp RK_KEY_LEFT
    jr z, rk_rep_lr_emit
    cp RK_KEY_RIGHT
    jr z, rk_rep_lr_emit
    cp RK_KEY_UP
    jr z, rk_rep_ud
    cp RK_KEY_DOWN
    jr z, rk_rep_ud
    ; Other keys: no repeat
    ld l, 0
    ret

rk_rep_lr_emit:
    ld a, 2
    ld (_repeat_timer), a
    ld l, b
    ret

rk_rep_ud:
    ld a, 5
    ld (_repeat_timer), a
    ld l, b
    ret

; =============================================================================
; void print_char64(uint8_t y, uint8_t col, uint8_t c, uint8_t attr) __z88dk_callee
; Stack (byte-packed): [ret_lo][ret_hi][y][col][c][attr]
; OPT: hand-written ASM replaces SDCC version (33 -> 19 bytes)
; =============================================================================
; void nav_push(uint8_t idx) __z88dk_fastcall
; Push idx onto nav_history ring, dedup top, shift if full.
; OPT: hand-written ASM (79 -> 55 bytes, LDIR replaces byte-by-byte loop)
; =============================================================================
EXTERN _nav_history
EXTERN _nav_hist_ptr
DEFC NAV_HIST_SZ = 6

_nav_push:
    ld c, l                     ; C = idx (fastcall param)
    ld a, (_nav_hist_ptr)
    ld b, a                     ; B = ptr
    or a
    jr z, np_no_dup             ; ptr == 0, skip dup check

    ; Dup check: history[ptr-1] == idx?
    dec a
    ld e, a
    ld d, 0
    ld hl, _nav_history
    add hl, de                  ; HL = &history[ptr-1]
    ld a, (hl)
    cp c
    ret z                       ; duplicate, return

np_no_dup:
    ld a, b                     ; A = ptr
    cp NAV_HIST_SZ
    jr c, np_append             ; ptr < 6, just append

    ; Shift left: history[0..4] = history[1..5]
    push bc                     ; save B=ptr, C=idx (LDIR clobbers BC)
    ld hl, _nav_history + 1
    ld de, _nav_history
    ld bc, NAV_HIST_SZ - 1
    ldir
    pop bc
    ld hl, _nav_hist_ptr
    dec (hl)                    ; ptr--
    ld b, (hl)                  ; B = new ptr

np_append:
    ; history[ptr] = idx; ptr++
    ld e, b
    ld d, 0
    ld hl, _nav_history
    add hl, de
    ld (hl), c                  ; history[ptr] = idx
    ld hl, _nav_hist_ptr
    inc (hl)                    ; ptr++
    ret

; =============================================================================
_print_char64:
    pop de                      ; DE = return address
    pop hl                      ; L = y, H = col
    ld a, l
    ld (_g_ps64_y), a
    ld a, h
    ld (_g_ps64_col), a
    pop hl                      ; L = c, H = attr
    ld a, h
    ld (_g_ps64_attr), a
    push de                     ; restore return address for tail call
    jp _print_str64_char        ; fastcall: L = char

; =============================================================================
; int8_t find_empty_channel_slot(void)
; Returns index of first inactive slot (1..9) or -1 (0xFF) if full.
; Scans channels[1]..channels[9] checking CH_FLAG_ACTIVE.
; OPT: hand-written ASM replaces SDCC version (46 -> 23 bytes)
; =============================================================================
_find_empty_channel_slot:
    ld hl, _channels + 32 + 30  ; &channels[1].flags
    ld de, 32                    ; struct size
    ld b, 9                      ; slots 1..9
    ld c, 1                      ; current index
fecs_loop:
    bit 0, (hl)                  ; CH_FLAG_ACTIVE?
    jr z, fecs_found             ; not active -> empty slot
    add hl, de
    inc c
    djnz fecs_loop
    ld l, 0xFF                   ; return -1
    ret
fecs_found:
    ld l, c                      ; return index
    ret

; =============================================================================
; int8_t find_query(const char *nick) __z88dk_fastcall
; Search for a nick in channels[]. Returns slot index or -1.
; HL = nick pointer
; Returns: L = slot index (0..N) or 0xFF (-1)
; =============================================================================
PUBLIC _find_query
EXTERN _st_stricmp
EXTERN _channels        ; ChannelInfo[10], 32 bytes each
EXTERN _channel_count
EXTERN _irc_server
EXTERN _S_CHANSERV
EXTERN _S_NICKSERV
EXTERN _S_GLOBAL

DEFC FQ_CH_SIZE = 32
DEFC FQ_FLAGS_OFS = 30
DEFC FQ_FLAG_ACTIVE = 0x01
DEFC FQ_FLAG_QUERY = 0x02

_find_query:
    ; Validate: if (!nick || !*nick) return -1
    ld a, h
    or l
    jr z, fq_bail_neg1
    ld a, (hl)
    or a
    jr z, fq_bail_neg1
    jr fq_valid

fq_bail_neg1:
    ld l, 0xFF
    ret

fq_valid:

    push iy             ; preserve sdcc frame pointer

    ; Save nick in IY for repeated use
    push hl
    pop iy              ; IY = nick

    ; Service filter: check first char (lowercase)
    or 0x20             ; A = nick[0] | 0x20
    cp 'c'
    jr nz, fq_not_c
    ld hl, _S_CHANSERV
    call fq_check_service
    jr z, fq_ret_0
    jr fq_check_server

fq_not_c:
    cp 'n'
    jr nz, fq_not_n
    ld hl, _S_NICKSERV
    call fq_check_service
    jr z, fq_ret_0
    jr fq_check_server

fq_not_n:
    cp 'g'
    jr nz, fq_check_server
    ld hl, _S_GLOBAL
    call fq_check_service
    jr z, fq_ret_0

fq_check_server:
    ; if (irc_server[0] && st_stricmp(nick, irc_server) == 0) return 0
    ld a, (_irc_server)
    or a
    jr z, fq_loop_init
    ld hl, _irc_server
    call fq_check_service
    jr z, fq_ret_0

fq_loop_init:
    ; Loop: for i=1; i < channel_count; i++
    ; DE = &channels[1] = channels + 32
    ld de, _channels + FQ_CH_SIZE
    ld b, 1             ; B = i

fq_loop:
    ld a, (_channel_count)
    cp b
    jr z, fq_ret_neg1_iy ; i >= channel_count
    jr c, fq_ret_neg1_iy

    ; Check flags: (ch->flags & (ACTIVE|QUERY)) == (ACTIVE|QUERY)
    ; flags at offset 30 from DE
    push de             ; save ch base
    ld hl, FQ_FLAGS_OFS
    add hl, de          ; HL = &ch->flags
    ld a, (hl)
    pop de              ; restore ch base
    and FQ_FLAG_ACTIVE | FQ_FLAG_QUERY
    cp FQ_FLAG_ACTIVE | FQ_FLAG_QUERY
    jr nz, fq_next

    ; st_stricmp(ch->name, nick) — name at offset 0, so DE = &ch->name
    push de             ; save ch base
    push bc             ; save i
    push iy             ; arg1: nick
    push de             ; arg2: ch->name
    call _st_stricmp
    pop af              ; clean arg2
    pop af              ; clean arg1
    ; HL = result (0 if equal)
    ld a, l
    or h
    pop bc              ; restore i
    pop de              ; restore ch base
    jr z, fq_found      ; match!

fq_next:
    ; DE += 32 (next channel)
    ld hl, FQ_CH_SIZE
    add hl, de
    ex de, hl           ; DE = next ch
    inc b
    jr fq_loop

fq_found:
    ; Return B (= i)
    pop iy
    ld l, b
    ret

fq_ret_0:
    pop iy
    ld l, 0
    ret

fq_ret_neg1_iy:
    pop iy
    ld l, 0xFF
    ret

; --- subroutine: compare IY (nick) with HL (service string) ---
; Input:  HL = string to compare, IY = nick
; Output: Z flag set if match (stricmp == 0)
; Clobbers: AF, BC, DE, HL
fq_check_service:
    push iy             ; arg1: nick
    push hl             ; arg2: service string
    call _st_stricmp
    pop af
    pop af
    ld a, l
    or h
    ret

; =============================================================================
; Attribute setter helpers - saves 3 bytes per call vs inline assignment
; current_attr = ATTR_X requires 6 bytes inline (ld a,(nn); ld (nn),a)
; call _set_attr_X requires 3 bytes
; =============================================================================
PUBLIC _set_attr_sys
PUBLIC _set_attr_err
PUBLIC _set_attr_priv
PUBLIC _set_attr_chan
PUBLIC _set_attr_nick
PUBLIC _set_attr_join
EXTERN _current_attr
; theme_attrs offsets: 9=SERVER/SYS 15=ERROR 4=PRIV 2=CHAN 11=NICK 10=JOIN

_set_attr_sys:
    ld a, (_theme_attrs + 9)  ; ATTR_MSG_SERVER
    jr set_attr_common

_set_attr_err:
    ld a, (_theme_attrs + 15) ; ATTR_ERROR
    jr set_attr_common

_set_attr_priv:
    ld a, (_theme_attrs + 4)  ; ATTR_MSG_PRIV
    jr set_attr_common

_set_attr_chan:
    ld a, (_theme_attrs + TA_MSG_CHAN)
    jr set_attr_common

_set_attr_nick:
    ld a, (_theme_attrs + 11) ; ATTR_MSG_NICK
    jr set_attr_common

_set_attr_join:
    ld a, (_theme_attrs + 10) ; ATTR_MSG_JOIN
    ; fall through to set_attr_common

set_attr_common:
    ld (_current_attr), a
    ret

; =============================================================================
; COMPACT KEYBOARD READER (replaces z88dk in_inkey)
; Saves ~80 bytes: no rowtable (bit-scan instead), no CAPS+SYM table section,
; no ghost-key detection (unnecessary for single-user IRC client).
; Returns: HL = ASCII code (0 if no key)
; =============================================================================
_in_inkey:
asm_in_inkey:
    ld bc, 0xFEFE          ; B = row $FE, C = port $FE
    ld e, 0                ; E = table offset accumulator

    ; --- Row 0: CAPS SHIFT row (port $FEFE) ---
    in a,(c)
    or 0xE1                ; mask CS (bit0) + bits 5-7
    cp 0xFF
    jr nz, ik_found

    ; --- Rows 1-6: main keyboard rows ---
    ld b, 0xFD
ik_row_loop:
    ld a, e
    add a, 5
    ld e, a                ; E += 5 (next row base)
    in a,(c)
    or 0xE0                ; mask bits 5-7
    cp 0xFF
    jr nz, ik_found
    rlc b
    jp m, ik_row_loop      ; loop while bit 7 set ($FD..$BF)

    ; --- Row 7: SYM SHIFT row (port $7FFE, B=$7F after loop) ---
    ld a, e
    add a, 5
    ld e, a                ; E = 35 (row 7 base)
    in a,(c)
    or 0xE2                ; mask SS (bit1) + bits 5-7
    cp 0xFF
    jr nz, ik_found

    ; No key pressed
ik_nokey:
    ld hl, 0
    ret

ik_found:
    ; A = row reading (one of bits 0-4 is 0 = pressed key)
    ; Bit-scan from bit 0 to find key position
    ld d, 0
ik_bitscan:
    rra
    jr nc, ik_gotkey
    inc d
    jr ik_bitscan

ik_gotkey:
    ; D = key position (0-4), E = row base offset
    ld a, e
    add a, d
    ld e, a                ; E = table index (0-39)
    ld d, 0
    ld hl, ik_keytable
    add hl, de             ; HL -> unshifted table entry

    ; --- Shift detection ---
    ld a, 0xFE
    in a,(0xFE)            ; read CAPS SHIFT row
    ld d, a                ; D bit0: 0=CS pressed, 1=not
    ld a, 0x7F
    in a,(0xFE)            ; read SYM SHIFT row

    bit 0, d
    jr nz, ik_nocaps
    ; CAPS is pressed — check if SYM also pressed
    bit 1, a
    jr z, ik_nokey         ; both shifts = ignore (no CTRL codes needed)
    ld bc, 40
    add hl, bc             ; -> CAPS section
    jr ik_lookup

ik_nocaps:
    bit 1, a
    jr nz, ik_lookup       ; no shift
    ld bc, 80
    add hl, bc             ; -> SYM section

ik_lookup:
    ld l, (hl)
    ld h, 0
    ret

; --- Translation table: 120 bytes (unshifted + CAPS + SYM) ---
; Row order: 0=$FEFE(CS,Z,X,C,V) 1=$FDFE(A,S,D,F,G) 2=$FBFE(Q,W,E,R,T)
;            3=$F7FE(1,2,3,4,5) 4=$EFFE(0,9,8,7,6) 5=$DFFE(P,O,I,U,Y)
;            6=$BFFE(EN,L,K,J,H) 7=$7FFE(SP,SS,M,N,B)
ik_keytable:
    ; Unshifted (offset 0-39)
    defb 0xFF, 'z','x','c','v'     ; row 0
    defb 'a','s','d','f','g'       ; row 1
    defb 'q','w','e','r','t'       ; row 2
    defb '1','2','3','4','5'       ; row 3
    defb '0','9','8','7','6'       ; row 4
    defb 'p','o','i','u','y'       ; row 5
    defb  13,'l','k','j','h'       ; row 6 (13=ENTER)
    defb  32,0xFF,'m','n','b'      ; row 7 (32=SPACE, 0xFF=SS)

    ; CAPS shifted (offset 40-79)
    defb 0xFF, 'Z','X','C','V'     ; row 0
    defb 'A','S','D','F','G'       ; row 1
    defb 'Q','W','E','R','T'       ; row 2
    defb   7,  6,128,129,  8       ; row 3 (EDIT,CAPSLOCK,TRUEVID,INVVID,LEFT)
    defb  12,  8,  9, 11, 10       ; row 4 (DELETE,RIGHT,TAB,UP,DOWN)
    defb 'P','O','I','U','Y'       ; row 5
    defb  13, 'L','K','J','H'      ; row 6
    defb   3,0xFF,'M','N','B'      ; row 7 (3=BREAK=CS+SPACE)

    ; SYM shifted (offset 80-119)
    defb 0xFF, ':','`','?','/'      ; row 0
    defb '~','|','\\','{','}'      ; row 1
    defb 131,132,133, '<','>'       ; row 2 (131-133=gfx chars)
    defb '!','@','#','$','%'       ; row 3
    defb '_',')','(', 39,'&'       ; row 4 (39=apostrophe)
    defb  34, ';',130,']','['       ; row 5 (34=quote, 130=gfx)
    defb  13, '=','+','-','^'      ; row 6
    defb  32,0xFF,'.',',','*'      ; row 7

; =============================================================================
; IM2 ISR TEMPLATE - Copied to 0xFCFC during CRT init
; Increments FRAMES low byte (23672) for HALT-based timing
; =============================================================================
_im2_isr_template:
    push af
    ld a, (23672)           ; FRAMES low byte
    inc a
    ld (23672), a
    pop af
    ei
    reti
_im2_isr_end:

; =============================================================================
; SYSTEM RAM HIJACKING - Variables mapped to unused ZX system areas
; Protected by IM2: divMMC automapper no longer triggers on every interrupt.
; Printer Buffer and CHANS are safe during normal operation.
; esxDOS calls (RST 8) may still touch these areas — tested safe in practice
; because esxDOS file I/O does not use Printer Buffer/CHANS as scratch.
; Zeroed during CRT init (code_crt_init section above)
; =============================================================================

; PRINTER BUFFER (0x5B00 - 0x5BFF: 256 bytes, unused — no ZX Printer)
PUBLIC _input_cache_char
PUBLIC _input_cache_attr

defc _input_cache_char = 0x5B00  ; 128 bytes (INPUT_LINES * SCREEN_COLS)
defc _input_cache_attr = 0x5B80  ;  64 bytes (INPUT_LINES * 32)
; 0x5BC0-0x5BFF (64 bytes) unused — esxDOS corrupts Printer Buffer during RST 8
; irc_pass, nickserv_pass, network_name moved back to BSS (must survive esxDOS calls)

; CHANS WORKSPACE (0x5CB6 - 0x5DB5: 256 bytes, unused — no ROM I/O channels)
PUBLIC _line_buffer
PUBLIC _temp_input

defc _line_buffer      = 0x5CB6  ; 128 bytes (LINE_BUFFER_SIZE)
defc _temp_input       = 0x5D36  ; 128 bytes (LINE_BUFFER_SIZE)

; UDG AREA (0xFF58 - 0xFFF1: 154 bytes, unused — program uses custom 64-col font)
PUBLIC _friend_nicks
PUBLIC _away_message
PUBLIC _names_target_channel

defc _friend_nicks          = 0xFF58  ;  90 bytes (MAX_FRIENDS * IRC_NICK_SIZE)
defc _away_message          = 0xFFB2  ;  32 bytes
defc _names_target_channel  = 0xFFD2  ;  32 bytes (NAMES_TARGET_CHANNEL_SIZE)
