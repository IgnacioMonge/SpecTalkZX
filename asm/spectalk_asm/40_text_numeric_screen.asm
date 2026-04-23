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
    ld bc, 0x0100         ; max 256 bytes scanned (finds strings 0-255)
    xor a                 ; A = 0 (search for NUL)
    cpir                  ; scan until match or BC=0
    ; HL = byte after NUL, DE = start
    or a
    sbc hl, de            ; HL = length + 1
    dec hl                ; HL = length
    ret

; -----------------------------------------------------------------------------
; int st_stricmp(const char *a, const char *b)
; Comparaci?n case-insensitive
; Stack: [IX+4,5]=a, [IX+6,7]=b
; Retorna: 0 si iguales, <0 si a<b, >0 si a>b
; -----------------------------------------------------------------------------
_st_stricmp:
    call ___sdcc_enter_ix

    call _ld_hl_ix6
    ex de, hl               ; DE = b
    call _ld_hl_ix4         ; HL = a

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
    ld a, b
    or a
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
    jr stricmp_done

stricmp_equal:
    ld hl, 0
stricmp_done:
    pop ix
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
; Stack: [IX+4,5]=hay, [IX+6,7]=needle
; -----------------------------------------------------------------------------
_st_stristr:
    call ___sdcc_enter_ix

    call _ld_hl_ix6
    ex de, hl               ; DE = needle
    call _ld_hl_ix4         ; HL = hay
    
    ; Validar needle
    ld a, d
    or e
    jr z, sstr_ret_hay      ; needle NULL -> return hay
    ld a, (de)
    or a
    jr z, sstr_ret_hay      ; needle vac?o -> return hay
    
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
; CONVERSIONES NUM?RICAS
; =============================================================================

_u16_to_dec:
    pop bc                  ; Retorno
    pop de                  ; dst
    pop hl                  ; v
    push hl
    push de
    push bc
    
    push ix
    ld ixl, 0               ; IXL = flag "ya imprimimos algo"
    
    ld bc, -10000
    call u16_digit
    jr u16_common_1000      ; Saltar a c?digo com?n

; =============================================================================
; char* u16_to_dec3(char *dst, uint16_t v)
; Versi?n reducida: imprime hasta 4 d?gitos (1000,100,10,1). Ideal para contadores.
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
    ld ixl, 0               ; IXL = flag "ya imprimimos algo"

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
    ld a, ixl
    or a
    ret z                   ; Suprimir cero inicial
    ld a, '0'
u16_do_print:
    ld (de), a
    inc de
    inc ixl                 ; Cualquier valor no cero sirve como flag
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
    ld c, a
    ld b, 0
    
    ; DE * 10 = (DE * 2 * 2 + DE) * 2 = DE * 5 * 2
    ld l, e
    ld h, d                 ; HL = DE (original)
    add hl, hl              ; *2
    add hl, hl              ; *4
    add hl, de              ; *5
    add hl, hl              ; *10
    
    add hl, bc
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
; Convenci?n: cdecl
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
    ld (hl), 0              ; Terminar string aqu?
    inc hl                  ; Return next position
    
skpto_end:
    ret

; =============================================================================
; C?DIGOS IRC
; =============================================================================

; -----------------------------------------------------------------------------
; void strip_irc_codes(char *s)
; Elimina c?digos de control IRC in-situ (^B, ^C, ^O, ^R, ^_)
; Stack: [IX+6,7]=s
; -----------------------------------------------------------------------------
_strip_irc_codes:
    call ___sdcc_enter_ix

    call _ld_hl_ix4         ; HL = lectura
    ld d, h
    ld e, l                 ; DE = escritura

strip_loop:
    ld a, (hl)
    or a
    jr z, strip_done
    
    ; Filtrar c?digos de un byte
    cp 0x02                 ; Bold
    jr z, strip_skip
    cp 0x0F                 ; Reset
    jr z, strip_skip
    cp 0x16                 ; Reverse
    jr z, strip_skip
    cp 0x1F                 ; Underline
    jr z, strip_skip
    
    ; Color (^C + d?gitos opcionales)
    cp 0x03
    jr z, strip_color
    
    ; Car?cter normal - copiar
    ld (de), a
    inc de
strip_skip:
    inc hl
    jr strip_loop

strip_color:
    inc hl                  ; Saltar ^C
    
    ; Saltar hasta 2 d?gitos de foreground
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
    
    ; Saltar hasta 2 d?gitos de background
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

; Subrutina: ?es A un d?gito?
; Retorna: Carry set si es d?gito
strip_is_digit:
    cp '0'
    jr c, sid_false         ; < '0' -> no es d?gito
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
    ; P2: HL reuse ? after LDIR, HL points to next byte (0x5820)
    ld a, (_theme_attrs + TA_BANNER)
    or 0x40
    ld hl, 0x5800
    ld bc, 32
    call _fast_fill_attr
    ; 3. Banner row 1 ? HL already at 0x5820 after LDIR
    ld a, (_theme_attrs + TA_BANNER)
    and 0xBF
    ld bc, 32
    call _fast_fill_attr

    ; 4. Rows 2-20: sep_top + chat + sep_bot ? contiguous, same attr
    ; P1: 0x5840..0x5AA0 = 608 bytes, all MAIN_BG
    ld hl, 0x5840
    ld bc, 608
    ld a, (_theme_attrs + TA_MAIN_BG)
    call _fast_fill_attr

    ; 5. Barra Estado (0x5AA0)
    ld hl, 0x5AA0
    ld bc, 32
    ld a, (_theme_attrs + TA_STATUS)
    call _fast_fill_attr

    ; 6. Input (0x5AC0)
    ld hl, 0x5AC0
    ld bc, 64
    ld a, (_theme_attrs + TA_INPUT_BG)
    call _fast_fill_attr

    ; 8. AVISAR A TODOS LOS SISTEMAS DE REPINTADO
    ld hl, _force_status_redraw   ; Avisa al renderizador interno
    ld (hl), 1
    call _set_sbd                ; Avisa al bucle Main de C <--- CR?TICO

    ret

; -----------------------------------------------------------------------------
; Rutina auxiliar: _fast_fill_attr
; Rellena BC bytes en (HL) con el valor A.
; P3: Simplified ? all callers pass BC >= 32, so BC==0 checks are dead code.
; After LDIR: HL = start + BC (points past last byte written).
; -----------------------------------------------------------------------------
_fast_fill_attr:
    ld (hl), a              ; Write first byte
    ld d, h
    ld e, l
    inc de                  ; DE = HL+1
    dec bc                  ; BC = count-1
    ldir
    ret



; =============================================================================
; void cls_fast(void)
; Borrado completo de pantalla con estrategia "Chunked LDIR".
; 1. Borra el bitmap (6144 bytes) en bloques de 128 bytes para no bloquear UART.
; 2. Repinta los atributos usando el sistema de temas.
; =============================================================================

_cls_fast:
    ; --- BORRADO DE BITMAP (0x4000..0x57FF) = 6144 bytes ---
    ld hl, 0x4000
    xor a
    ld (hl), a
    ld de, 0x4001
    ld bc, 6143
    ldir

    ; --- ATRIBUTOS ---
    jr _reapply_screen_attributes


; =============================================================================
; void uart_drain_to_buffer(void)
; Lee bytes del UART y los mete en el Ring Buffer lo m?s r?pido posible.
; CR?TICO: Minimiza la latencia entre bytes para evitar p?rdida de datos en AY.
; OPTIMIZED: No usa EXX, contador en IYL
; =============================================================================

_uart_drain_to_buffer:
    push iy                 ; Preservar IY (est?ndar z88dk)

    ld a, (_uart_drain_limit)
    or a
    jr nz, drain_set_limit

    ; Caso l?mite=0 -> Usar seguridad de 255 iteraciones (suficiente)
    dec a

drain_set_limit:
    ; Caso con l?mite (ej: 32 bytes) - usar IYL como contador
    ld iyl, a

drain_loop_start:
    ; 2. ?Hay datos disponibles?
    call _ay_uart_ready     ; Retorna L=1 (S?) o 0 (No)
    ld a, l
    or a
    jr z, drain_exit        ; Si L=0, no hay m?s datos -> Salir

    ; 3. Leer byte
    call _ay_uart_read      ; Retorna byte en L

    ; 4. Meter en Ring Buffer
    call _rb_push           ; Retorna L=1 (?xito) o 0 (Fallo/Lleno)
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
    push bc                ; save offset
    ld bc, 128
    ldir
    pop bc                 ; restore C = offset

    ; BLOQUE 2: Fila 8 -> 7 (Src: 0x4800, Dest: 0x40E0, Len: 32)
    ld a, 0x48
    call smz_cross_block

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
    call smz_cross_block

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

    ; Limpiar ?ltima l?nea (19) p?xeles + atributos
    ld a, (_current_attr)
    ld c, a
    ld a, 19
    call cli_internal

    pop iy
    ret

; Cross-page scroll helper: copies 32 bytes from page A to page A-8
; A = src_page, C = scanline offset (preserved via push/pop)
; Used by blocks 2 and 4 (page boundary crossings: row 8?7, row 16?15)
smz_cross_block:
    add a, c            ; src_page + scanline offset
    ld h, a
    sub 8               ; dst = src_page - 8 + offset (previous third)
    ld d, a
    ld l, 0x00
    ld e, 0xE0
    push bc
    ld bc, 32
    ldir
    pop bc
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

    ; Guardar wrap_indent para despu?s
    ld c, a             ; C = wrap_indent (num cols)

    ; --- Calcular screen addr de la fila ---
    ld a, (_main_line)
    call _compute_screen_base

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
    call _compute_attr_base  ; HL = attr base de la fila

    ld a, (_current_attr)
mn_indent_attr:
    ld (hl), a
    inc hl
    djnz mn_indent_attr
mn_indent_attr_done:

    ; Actualizar main_col y g_ps64_col/y/attr para consistencia
    ld a, c
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
    call ___sdcc_enter_ix

    ; Inicializar contador a 0
    xor a
    ld (_irc_param_count), a

    ; HL = par (string a trocear)
    call _ld_hl_ix6

    ; Comprobar string nulo
    ld a, h
    or l
    jr z, tp_exit               ; OPT: jp?jr
    ld a, (hl)
    or a
    jr z, tp_exit               ; OPT: jp?jr

    ; B = max_params (0 => 10 por defecto, resto clamp a 1..10)
    ld a, (ix+8)
    dec a
    cp 10
    jr c, tp_max_ok
    ld a, 9
tp_max_ok:
    inc a
    ld b, a                 ; B = slots restantes (1..10)

    ; IY = puntero al array de punteros (_irc_params)
    ld iy, _irc_params

    ; --- Saltar espacios/colons iniciales ---
tp_skip_leading:
    ld a, (hl)
    or a
    jr z, tp_exit           ; OPT: jp?jr
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
    pop ix
    pop iy
    ret


; =============================================================================
; char *sb_append(char *dst, const char *src, const char *limit)
; Concatena src en dst asegurando no pasar de limit. Retorna nuevo dst en HL.
; Stack: [IX+4,5]=dst, [IX+6,7]=src, [IX+8,9]=limit
; =============================================================================
_sb_append:
    call ___sdcc_enter_ix
    
    ; Cargar argumentos
    call _ld_hl_ix6
    ex de, hl               ; DE = src
    call _ld_hl_ix4         ; HL = dst
    ld c, (ix+8)
    ld b, (ix+9)            ; BC = limit
    
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
    ; Callee cleanup: 6 bytes of params (dst+src+limit)
    pop ix              ; restore IX
    pop de              ; DE = return address
    pop af              ; skip dst (2B)
    pop af              ; skip src (2B)
    pop af              ; skip limit (2B)
    push de             ; push return address back
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

