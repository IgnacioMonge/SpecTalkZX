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
    ; CPIR bounded scan: C becomes the inverse of the scanned count.
    ld bc, 0x0100         ; max 256 bytes scanned (finds strings 0-255)
    xor a                 ; A = 0 (search for NUL)
    cpir                  ; scan until match or BC=0
    ld a, c
    cpl                   ; length, capped at 255 if no NUL in first 256
    ld l, a
    ld h, b               ; B is always 0 after bounded CPIR
    ret

; -----------------------------------------------------------------------------
; int st_stricmp(const char *a, const char *b)
; Comparaci?n case-insensitive
; cdecl stack is restored after extracting a/b.
; Retorna: 0 si iguales, <0 si a<b, >0 si a>b
; -----------------------------------------------------------------------------
_st_stricmp:
    pop bc                  ; return
    pop hl                  ; a
    pop de                  ; b
    push de
    push hl
    push bc                 ; restore cdecl stack

stricmp_loop:
    ld a, (hl)
    call irc_tolower        ; Convertir A seg?n RFC 1459
    ld b, a                 ; B = a normalizado
    
    ld a, (de)
    call irc_tolower        ; Convertir seg?n RFC 1459
    
    ; Comparar (NOTE-M12: retorna b-a, signo invertido vs strcmp.
    ; OK porque TODOS los callers solo chequean ==0)
    sub b                   ; A = b_norm - a_norm
    jr nz, stricmp_diff
    
    ; Iguales - ?fin de string?
    or b
    jr z, stricmp_equal
    
    inc hl
    inc de
    jr stricmp_loop

stricmp_diff:
    ; Extender signo a 16 bits
    ld l, a
    add a, a
    sbc a, a
    ld h, a
    ret

stricmp_equal:
    ld hl, 0
    ret

; -----------------------------------------------------------------------------
; irc_tolower: Convierte car?cter a min?scula seg?n RFC 1459
; Input: A = car?cter
; Output: A = lowercase character (incluyendo []\^ -> {}|~)
; Preserva: BC, DE, HL
; -----------------------------------------------------------------------------
irc_tolower:
    ; RFC 1459: A-Z (65-90) y [\]^ (91-94) son contiguos ? rango 65-94
    cp 'A'
    ret c
    cp '^' + 1
    ret nc
    add a, 32
    ret

; -----------------------------------------------------------------------------
; const char* st_stristr(const char *hay, const char *needle)
; B?squeda case-insensitive de substring
; Retorna: puntero a primera ocurrencia, o NULL
; cdecl stack is restored after extracting hay/needle.
; -----------------------------------------------------------------------------
_st_stristr:
    pop bc                  ; return
    pop hl                  ; hay
    pop de                  ; needle
    push de
    push hl
    push bc                 ; restore cdecl stack
    
    ; Validar needle
    ld a, d
    or e
    ret z                   ; needle NULL -> return hay
    ld a, (de)
    or a
    ret z                   ; needle vac?o -> return hay
    
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

    ; Normalizar via RFC 1459 (audit W08: was ASCII-only)
    call irc_tolower
    ld b, a

    ld a, c
    call irc_tolower
    
    cp b
    jr nz, sstr_next_pos    ; No coinciden
    
    ; Coinciden, avanzar
    inc hl
    inc de
    jr sstr_inner
    
sstr_next_pos:
    pop de                  ; Restaurar needle
    pop hl                  ; Restaurar hay
    inc hl                  ; Siguiente posici?n
    jr sstr_outer
    
sstr_found:
    pop de                  ; Limpiar stack
    pop hl                  ; HL = posici?n del match
    ret

sstr_fail:
    ld hl, 0
    ret

; =============================================================================
; CONVERSIONES NUM?RICAS
; =============================================================================

_u16_to_dec:
    pop bc                  ; Retorno
    pop de                  ; dst
    pop hl                  ; v
    push hl
    push de
    push bc
    
    ex af, af'
    xor a                   ; AF' = flag "ya imprimimos algo"
    ex af, af'
    
    ld bc, -10000
    call u16_digit

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
    ret

; Subrutina: extrae un d?gito restando BC repetidamente
; input: HL = valor, BC = -potencia, DE = buffer
; output: HL = residuo, DE avanzado si se imprimi?
u16_digit:
    ld a, '0' - 1
u16_sub_loop:
    inc a
    add hl, bc
    jr c, u16_sub_loop
    
    ; Deshacer ?ltima resta
    sbc hl, bc
    
    ; ?Imprimir?
    cp '0'
    jr nz, u16_do_print
    
    ; Es cero - ?ya imprimimos algo?
    ex af, af'
    or a
    jr z, u16_skip          ; Suprimir cero inicial
    ex af, af'
u16_do_print:
    ld (de), a
    inc de
    ex af, af'
    inc a                   ; Cualquier valor no cero sirve como flag
    ex af, af'
    ret
u16_skip:
    ex af, af'
    ret

; -----------------------------------------------------------------------------
; uint16_t str_to_u16(const char *s) __z88dk_fastcall
; Convierte string decimal a uint16
; input: HL = puntero al string
; Retorna: HL = valor
; -----------------------------------------------------------------------------
_str_to_u16:
    ex de, hl               ; DE = string pointer
    ld hl, 0                ; Acumulador
stu16_loop:
    ld a, (de)
    sub '0'
    cp 10
    ret nc                  ; non-digit (including < '0' after underflow)
    
    ; HL = HL * 10 + A, keeping DE as the string pointer
    add hl, hl              ; *2
    ld c, l
    ld b, h                 ; BC = original HL * 2
    add hl, hl              ; *4
    add hl, hl              ; *8
    add hl, bc              ; *10
    ld c, a
    ld b, 0
    add hl, bc              ; + digit
    inc de
    jr stu16_loop

; =============================================================================
; PARSING IRC
; =============================================================================

; =============================================================================
; UART
; =============================================================================

; -----------------------------------------------------------------------------
; void uart_send_string(const char *s) __z88dk_fastcall
; Env?a string por UART
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
    ; 1. Borde
    ld a, (_theme_attrs + TA_BORDER)
    out (0xFE), a

    ; 2. Banner row 0 (0x5800 - 32 bytes) BRIGHT 1
    ; P2: HL reuse: helper returns next byte (0x5820)
    ld a, (_theme_attrs + TA_BANNER)
    or 0x40
    ld hl, 0x5800
    ld bc, 32
    call _fast_fill_attr
    ; 3. Banner row 1: HL already at 0x5820 after helper
    xor 0x40
    ld bc, 32
    call _fast_fill_attr

    ; 4. Rows 2-20: sep_top + chat + sep_bot ? contiguous, same attr
    ; P1: 0x5840..0x5AA0 = 608 bytes, all MAIN_BG
    ld bc, 608
    ld a, (_theme_attrs + TA_MAIN_BG)
    call _fast_fill_attr

    ; 5. Barra Estado (0x5AA0)
    ld bc, 32
    ld a, (_theme_attrs + TA_STATUS)
    call _fast_fill_attr

    ; 6. Input (0x5AC0)
    ld bc, 64
    ld a, (_theme_attrs + TA_INPUT_BG)
    call _fast_fill_attr

    ; 8. AVISAR A TODOS LOS SISTEMAS DE REPINTADO
    ld hl, _force_status_redraw   ; Avisa al renderizador interno
    ld (hl), 1
    jp _set_sbd                  ; Avisa al bucle Main de C <--- CR?TICO

; -----------------------------------------------------------------------------
; Rutina auxiliar: _fast_fill_attr
; Rellena BC bytes en (HL) con el valor A.
; P3: Simplified ? all callers pass BC >= 32, so BC==0 checks are dead code.
; Returns: HL = byte after the filled range.
; -----------------------------------------------------------------------------
_fast_fill_attr:
    ld (hl), a              ; Write first byte
    ld d, h
    ld e, l
    inc de                  ; DE = HL+1
    dec bc                  ; BC = count-1
    ldir
    ex de, hl               ; HL = byte after the filled range
    ret

; =============================================================================
; void cls_fast(void)
; Borrado completo de pantalla con estrategia "Chunked LDIR".
; 1. Borra el bitmap (6144 bytes) en bloques de 128 bytes para no bloquear UART.
; 2. Repinta los atributos usando el sistema de temas.
; =============================================================================

_cls_fast:
    ; --- BORRADO DE BITMAP (0x4000..0x57FF) = 6144 bytes ---
    ; Stack clear: 6 * 256 iterations * 2 PUSHes = 6144 bytes.
    ; Startup-only clear: the sole caller is init_screen(), reached after ROM
    ; startup with interrupts enabled.  Use fixed DI/EI instead of saving IFF
    ; with `ld a,i`, whose P/V result is affected by a known Z80 interrupt
    ; acceptance edge case.
    di
    ld (cls_restore_sp + 1), sp
    ld sp, 0x5800
    ld hl, 0
    ld bc, 0x0006
cls_inner:
    push hl
    push hl
    djnz cls_inner
    dec c
    jr nz, cls_inner
cls_restore_sp:
    ld sp, 0
    ei

    ; --- ATRIBUTOS ---
    jr _reapply_screen_attributes

; =============================================================================
; uint8_t overlay_header(const char *title) __z88dk_fastcall
; Shared overlay header renderer. Mirrors the former C implementation.
; =============================================================================
_overlay_header:
    push hl                  ; title

    call _clear_main

    ld a, (_theme_attrs + TA_BANNER)
    or 0x40
    ld h, a
    ld l, 3                 ; MAIN_START
    push hl
    call _clear_line
    pop bc

    ld a, (_theme_attrs + TA_BANNER)
    and 0xBF
    ld h, a
    ld l, 4                 ; MAIN_START + 1
    push hl
    call _clear_line
    pop bc

    pop hl                  ; title
    ld a, (_theme_attrs + TA_BANNER)
    push af
    inc sp                  ; attr is a 1-byte callee arg
    push hl
    ld bc, 0x0103           ; y=3, col=1
    push bc
    call _print_big_str

    ld hl, 0x40A0            ; SCREEN_ROW_ADDR(5)
    ld a, 0xFF
    ld bc, 32
    call _fast_fill_attr

    ld hl, 0x58A0            ; ATTR row 5
    ld a, (_theme_attrs + TA_MSG_TOPIC)
    ld bc, 32
    call _fast_fill_attr

    ld hl, 6                 ; MAIN_START + 3
    ret

; =============================================================================
; void main_hline(void)
; Draws the 1-pixel main-area separator used by startup/info screens.
; Keeps the C contract: newline first if main_col > 0, draw scanline 3 of
; main_line plus 32 attrs with current_attr, then newline again.
; =============================================================================
_main_hline:
    ld a, (_main_col)
    or a
    call nz, _main_newline

    ld a, (_main_line)
    call _compute_screen_base
    inc h
    inc h
    inc h                       ; scanline 3
    ld a, 0xFF
    ld bc, 32
    call _fast_fill_attr

    ld a, (_main_line)
    call _compute_attr_base
    ld a, (_current_attr)
    ld bc, 32
    call _fast_fill_attr

    jp _main_newline


; =============================================================================
; void uart_drain_to_buffer(void)
; Lee bytes del UART y los mete en el Ring Buffer lo m?s r?pido posible.
; CR?TICO: Minimiza la latencia entre bytes para evitar p?rdida de datos en AY.
; OPTIMIZED: loop counter in B, protected across call chain with PUSH/POP BC.
; P11D2: P5-style inline read plus 2-poll inter-byte dwell after non-empty drains.
; =============================================================================

DRAIN_ZXUNO_ADDR        EQU 0xFC3B
DRAIN_UART_DATA_REG     EQU 0xC6
DRAIN_UART_STAT_REG     EQU 0xC7
DRAIN_UART_BYTE_RECIVED EQU 0x80

_uart_drain_to_buffer:
    ld a, (_uart_drain_limit)
    or a
    jr nz, drain_set_limit

    ; Caso l?mite=0 -> Usar seguridad de 255 iteraciones (suficiente)
    dec a

drain_set_limit:
    ; Caso con l?mite (ej: 32 bytes)
    ld b, a
    ld c, 0                 ; C=1 once this call has pushed at least one byte

drain_loop_start:
    push bc                 ; preserve loop counter across calls

    ; 2. Leer byte si hay datos
drain_poll_status:
    ld bc, DRAIN_ZXUNO_ADDR
    ld a, DRAIN_UART_STAT_REG
    out (c), a
    inc b
    in a, (c)
    and DRAIN_UART_BYTE_RECIVED
    jr z, drain_maybe_wait  ; no hay m?s datos -> maybe wait after a byte

drain_read_ready:
    ; B is $FD after a ready status read; restore address port $FC3B.
    dec b
    ld a, DRAIN_UART_DATA_REG
    out (c), a
    inc b
    in a, (c)
    ld l, a

    ; 4. Meter en Ring Buffer
    call _rb_push           ; Retorna L=1 (?xito) o 0 (Fallo/Lleno)
    dec l
    jr nz, drain_exit_full   ; Si buffer lleno (L era 0 -> 255), PARAR

    ; Decrementar contador
    pop bc
    ld c, 1
    djnz drain_loop_start
    ret

drain_maybe_wait:
    pop bc
    ld a, c
    or a
    jr z, drain_exit_empty

    ; We already moved at least one byte. The inline path can re-poll before the
    ; next serial byte becomes visible, so wait about one 115200-baud byte time.
    push bc
    ld bc, DRAIN_ZXUNO_ADDR
    ld e, 2
drain_wait_next:
    ld a, DRAIN_UART_STAT_REG
    out (c), a
    inc b
    in a, (c)
    and DRAIN_UART_BYTE_RECIVED
    jr nz, drain_read_ready
    dec b
    dec e
    jr nz, drain_wait_next
    pop bc
    ret

drain_exit_empty:
    ret

drain_exit_full:
    pop bc
    ret

; =============================================================================
; void scroll_main_zone(void)
; Optimized scroll of chat zone (lines 2-19 -> 2-18).
; UNROLLED version: eliminates table and inner loop maintaining exactly
; the same 5 blocks and lengths as original version.
; 
; Timing note: bitmap LDIR alone copies 4096 bytes, so the old ~12k estimate
; was not credible. Treat this as a >100k T-state contended-screen operation
; until measured on the target emulator/hardware.
; =============================================================================
_scroll_main_zone:
    di

    ld iyl, 7              ; IYL = scanline offset (7..0)

smz_scanline_loop:
    ; BLOQUE 1: Filas 4-7 -> 3-6 (Src: 0x4080, Dest: 0x4060, Len: 128)
    ld a, 0x40
    add a, iyl
    ld h, a
    ld d, a
    ld l, 0x80
    ld e, 0x60
    ld b, 8
    call _scroll_stack_blit_chunks

    ; BLOQUE 2: Fila 8 -> 7 (Src: 0x4800, Dest: 0x40E0, Len: 32)
    ld a, 0x48
    call smz_cross_block

    ; BLOQUE 3: Filas 9-15 -> 8-14 (Src: 0x4820, Dest: 0x4800, Len: 224)
    ld a, 0x48
    add a, iyl
    ld h, a
    ld d, a
    ld l, 0x20
    ld e, 0x00
    ld b, 14
    call _scroll_stack_blit_chunks

    ; BLOQUE 4: Fila 16 -> 15 (Src: 0x5000, Dest: 0x48E0, Len: 32)
    ld a, 0x50
    call smz_cross_block

    ; BLOQUE 5: Filas 17-19 -> 16-18 (Src: 0x5020, Dest: 0x5000, Len: 96)
    ld a, 0x50
    add a, iyl
    ld h, a
    ld d, a
    ld l, 0x20
    ld e, 0x00
    ld b, 6
    call _scroll_stack_blit_chunks

    ; Siguiente scanline (7..0, termina al pasar a 0xFF)
    dec iyl
    jp p, smz_scanline_loop

    ; Scroll atributos (16 filas: 4->3 ... 19->18).
    ; The stack blitter copies forward in 16-byte chunks. With dst = src - 32,
    ; each write lands two chunks behind the next unread source, so overlap is
    ; safe while avoiding LDIR/LDI byte-loop overhead.
    ld de, 0x5860
    ld hl, 0x5880
    ld b, 32
    call _scroll_stack_blit_chunks

    ; Clear last chat row (19) directly; avoids general clear_line setup.
    ld (smz_clear_save_sp + 1), sp
    ld hl, 0x5080          ; SCREEN_ROW_ADDR(19) + 32
    ld de, 0
    ld b, 8
smz_clear19_px:
    ld sp, hl
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    inc h
    djnz smz_clear19_px

    ld a, (_current_attr)
    ld d, a
    ld e, a
    ld sp, 0x5A80          ; ATTR row 19 end + 1
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
    push de
smz_clear_save_sp:
    ld sp, 0x0000
    ret

; Cross-page scroll helper: copies 32 bytes from page A to page A-8
; A = src_page, IYL = scanline offset
; Used by blocks 2 and 4 (page boundary crossings: row 8->7, row 16->15)
smz_cross_block:
    add a, iyl          ; src_page + scanline offset
    ld h, a
    sub 8               ; dst = src_page - 8 + offset (previous third)
    ld d, a
    ld l, 0x00
    ld e, 0xE0

    ld b, 2
    jp _scroll_stack_blit_chunks

; B*16-byte forward copy using SP as the transfer pointer.
; Input: HL = source, DE = destination, B = 16-byte chunk count.
; Destroys all main/alternate regs and IX. _main_newline preserves IX/IY.
; Destination may cross a page: next destination is recomputed from SP + 32.
; Caller must have DI active. Mainline IM1 is only enabled inside frame_wait()
; with IY set to ROM system variables.
_scroll_stack_blit_chunks:
    ld (ssb_save_sp + 1), sp

    ld (ssb_src + 1), hl

    ld a, e
    add a, 16
    ld (ssb_dst + 1), a
    ld a, d
    adc a, 0
    ld (ssb_dst + 2), a

    ld a, b
    ld ixl, a

ssb_loop:
ssb_src:
    ld sp, 0x0000
    pop bc
    pop de
    pop hl
    pop af
    exx
    ex af, af'
    pop bc
    pop de
    pop hl
    pop af
    ld (ssb_src + 1), sp

ssb_dst:
    ld sp, 0x0000
    push af
    push hl
    push de
    push bc
    exx
    ex af, af'
    push af
    push hl
    push de
    push bc

    ld hl, 32
    add hl, sp
    ld (ssb_dst + 1), hl

    dec ixl
    jr nz, ssb_loop

ssb_save_sp:
    ld sp, 0x0000
    ret

; =============================================================================
; void main_newline(void)
; Identical behavior to the C version in src/spectalk.c
; Notes:
;  - pagination_pause() is C (sdcc_iy): preserve IY around the call.
;  - _print_str64_char clobbers BC/DE/HL/AF: preserve BC (loop counter) and HL.
;  - micro-opt: keep HL -> _g_ps64_col and store (hl)=c each iteration.
; =============================================================================
PUBLIC _main_clear_indent6
_main_newline:
    ; Suppress output during help overlay
    ld a, (_overlay_mode)
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

    ; If real output advanced from the row reserved just after a channel
    ; divider, that row is no longer reusable for separator repaint/erase.
    ld hl, _channel_context_next_row
    ld a, (_main_line)
    cp (hl)
    jr nz, mn_ctx_keep
    ld (hl), 0
mn_ctx_keep:

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
    call _uart_drain_to_buffer
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
    ; if (wrap_indent > 0) clear the fixed 6-column timestamp/wrap indent.
    ld a, (_wrap_indent)
    or a
    call nz, _main_clear_indent6

mn_ret:
    pop iy
    pop ix
    ret

; Clear the fixed 6-column main-text indent (3 physical bytes) on _main_line,
; set main_col/g_ps64 state to column 6, and prewarm the row cache.
_main_clear_indent6:
    ld a, (_main_line)
    call _compute_screen_base
    xor a
    ld b, 8
    ld c, l
mci6_scan:
    ld (hl), a
    inc l
    ld (hl), a
    inc l
    ld (hl), a
    ld l, c
    inc h
    djnz mci6_scan

    ld a, (_main_line)
    call _compute_attr_base
    ld a, (_current_attr)
    ld (hl), a
    inc l
    ld (hl), a
    inc l
    ld (hl), a

    ld a, 6
    ld (_main_col), a
    ld (_g_ps64_col), a
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_current_attr)
    ld (_g_ps64_attr), a
    jp p64_get_scr_base


; =============================================================================
; void tokenize_params(char *par) __z88dk_fastcall
; Trocea un string IRC separando por espacios y rellenando el array global irc_params
; Modifica el string 'par' in-situ (reemplaza espacios por NULLs)
; HL = par. Appends after the parser-preloaded params up to IRC_MAX_PARAMS=10.
; =============================================================================
_tokenize_params:
    ld a, (_irc_param_count)
    cp 10
    ret nc
    ld b, a                 ; B = count inicial

    ; Comprobar string vacio
    ld a, (hl)
    or a
    ret z

    ; DE = &_irc_params[irc_param_count]
    push hl
    ld h, 0
    ld l, b
    add hl, hl
    ld de, _irc_params
    add hl, de
    ex de, hl
    pop hl
    ld a, 10
    sub b
    ld b, a                 ; B = slots restantes

    ; --- BUCLE PRINCIPAL DE TOKENIZADO ---
    ; HL = puntero actual en el string
    ; DE = puntero actual en el array irc_params
    ; B  = slots restantes
tp_main_loop:
    ; Guardar inicio del token en irc_params[count]
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a
    inc de                  ; avanzar al siguiente slot (2 bytes por puntero)

    ; Decrementar cupo max
    dec b
    jr z, tp_exit           ; si llegamos al max, paramos

    ; Buscar fin del token (espacio o NULL)
tp_scan_word:
    ld a, (hl)
    or a
    jr z, tp_exit           ; OPT: jp?jr
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
    jr z, tp_exit           ; OPT: jp?jr
    cp ' '
    jr nz, tp_main_loop     ; encontrado siguiente token
    inc hl
    jr tp_skip_spaces

tp_exit:
    ld a, 10
    sub b
    ld (_irc_param_count), a
    ret


; =============================================================================
; char *sb_append(char *dst, const char *src, const char *limit)
; Concatena src en dst asegurando no pasar de limit. Retorna nuevo dst en HL.
; Stack: [IX+4,5]=dst, [IX+6,7]=src, [IX+8,9]=limit
; =============================================================================
_sb_append:
    pop af                  ; return
    pop hl                  ; dst
    pop de                  ; src
    pop bc                  ; limit
    push af                 ; callee-cleaned stack now has only return
    
sba_loop:
    ; 1. Chequear l?mite: if (dst >= limit) exit
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
    ; HL = return value (new dst pointer)
    ret

; =============================================================================
; void draw_badge_dither(uint8_t count) __z88dk_fastcall
; Dibuja patr?n de tri?ngulos en AMBAS filas del banner (0 y 1).
; count en L = n?mero de celdas (5 fijo)
; phys_x = 32 - count (autom?tico)
; =============================================================================
_draw_badge_dither:
    ld a, 32
    sub l
    ld c, a                 ; C = phys_x (preservado entre filas)
    ld b, l                 ; B = count
    push bc                 ; guardar count + phys_x para fila 1

    ; --- Fila 0: base 0x4000 ---
    ld hl, 0x4000
    ld e, c
    ld d, 0
    add hl, de
    call dbd_row

    ; --- Fila 1: base 0x4020 ---
    pop bc                  ; restaurar count (B) + phys_x (C)
    ld hl, 0x4020
    add hl, de              ; DE sigue siendo phys_x
    ; fall through

dbd_row:
    ; Dibuja dither en B celdas a partir de HL
    ; P4: pattern computed in-place via scf/rla instead of lookup table
    ; Generates: 00, 01, 03, 07, 0F, 1F, 3F, 7F (triangle ramp)
dbd_cell_loop:
    push bc
    push hl
    xor a                   ; A = 0x00 (first scanline value)
    ld b, 8
dbd_scanline:
    ld (hl), a              ; write current value
    inc h                   ; next scanline
    scf                     ; carry = 1
    rla                     ; rotate left through carry: 0?1?3?7?F?1F?3F?7F
    djnz dbd_scanline
    pop hl
    inc hl
    pop bc
    djnz dbd_cell_loop
    ret

; =============================================================================
; OPTIMIZED C FUNCTIONS REPLACEMENT
; High-performance replacements for spectalk.c bottlenecks
; (EXTERNs already declared at top of file)

