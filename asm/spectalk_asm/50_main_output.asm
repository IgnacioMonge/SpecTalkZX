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

; 2B transient scratch in the free printer-buffer tail ($5BE7-$5BFF).
; Only live inside main_print_wrapped_ram()'s scan, before render/BPE calls.
defc mpwr_last_space = 0x5BE7

; -----------------------------------------------------------------------------
; void main_print_time_prefix(void)
; Prints "HH:MM| " using ATTR_MSG_TIME, then restores current_attr.
; show_timestamps: 0=off, 1=always, 2=on-change (only when HH:MM differs)
; Sets wrap_indent=6 when printed, 0 when skipped.
; -----------------------------------------------------------------------------
PUBLIC _main_print_time_prefix
_main_print_time_prefix:
    ld a, (_show_timestamps)
    or a
    jr z, mptp_off          ; mode 0: off

    cp 2
    jr nz, mptp_do          ; mode 1: always print

    ; mode 2: on-change ? check if HH:MM matches last printed
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

    ; Same time ? print spaces as indent (6 chars) instead of timestamp
    ld a, 6
    ld (_wrap_indent), a
    ld b, 6
mptp_spaces:
    push bc
    ld l, ' '
    call _main_putc
    pop bc
    djnz mptp_spaces
    ret

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
    ret

mptp_off:
    xor a
    ld (_wrap_indent), a

mptp_ret:
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
    jr _main_putc           ; tail call (in range)

; -----------------------------------------------------------------------------
; void main_putc(char c) __z88dk_fastcall
; Imprime un caracter gestionando saltos de l?nea y coordenadas.
; L = c
; -----------------------------------------------------------------------------
PUBLIC _main_putc
_main_putc:
    ; Suppress output during help overlay
    ld a, (_overlay_mode)
    or a
    ret nz
    ld a, l
    cp 10               ; ?Es '\n'?
    jp z, _main_newline
    
    ld b, a             ; Guardar caracter en B
    
    ; Chequear l?mite de columna (SCREEN_COLS = 64)
    ld a, (_main_col)
    cp 64
    jr c, mputc_no_wrap
    
    ; Necesitamos wrap - guardar B antes de llamar a main_newline
    push bc
    call _main_newline
    pop bc
    ld a, (_main_col)
    cp 64
    ret z
    
mputc_no_wrap:
    ; Configurar variables globales para el driver de video (bypasea print_char64 de C)
    ld (_g_ps64_col), a

    ld a, (_main_line)
    ld (_g_ps64_y), a
    
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
    ; Convertir UTF-8 a ASCII in-place antes de procesar
    call _utf8_to_ascii     ; HL se preserva (fastcall, mismo puntero)

mpwr_loop:
    ld a, (hl)
    or a
    jr z, mpwr_finalize

    ; Si estamos al final de l?nea, pasar a la siguiente SIN perder HL
    ld a, (_main_col)
    cp 64
    jr c, mpwr_have_col
    push hl
    call _main_newline
    pop hl

    ; Si la paginaci?n fue cancelada durante el pause, main_col queda en 64.
    ; Salir para no pisar "Cancelled".
    ld a, (_main_col)
    cp 64
    jr z, mpwr_abort

mpwr_have_col:
    ; avail = 64 - main_col  -> B
    cpl
    add a, 65
    ld b, a

    ; DE = start
    ld d, h
    ld e, l

    ld (mpwr_last_space), hl     ; start sentinel: none/first-space => hard cut

mpwr_scan:
    ld a, (hl)
    or a
    jr z, mpwr_cut_end

    cp 32                        ; ' '
    jr nz, mpwr_scan_next
    ld (mpwr_last_space), hl

mpwr_scan_next:
    inc hl
    djnz mpwr_scan

    ; Alcanzados "avail" chars
    ld b, h
    ld c, l                      ; hard cutpoint / next_start
    ld hl, (mpwr_last_space)     ; last usable space, or start sentinel

    ; Si no hubo espacio usable (o el único espacio es el primero), hard cut
    or a
    sbc hl, de
    jr z, mpwr_cut_hard_from_bc
    add hl, de

mpwr_use_space:
    ; cutpoint = HL (espacio), next_start = HL+1
    ld b, h
    ld c, l
    inc bc
    jr mpwr_cut_common

mpwr_cut_hard_from_bc:
    ld h, b
    ld l, c
mpwr_cut_end:
mpwr_cut_hard:
    ; cutpoint = HL (NUL o start+avail), next_start = HL
    ld b, h
    ld c, l
    ; fall through

mpwr_cut_common:
    ; Guardar char original y cortar temporalmente
    ld a, (hl)
    push af
    xor a
    ld (hl), a

    push hl                      ; cutpoint
    push bc                      ; next_start

    ; Imprimir segmento (start = DE)
    ; Fast path: si estamos al inicio de l?nea, usar print_line64_fast
    ld a, (_main_col)
    or a
    jr nz, mpwr_print_puts

    ; Set start byte offset from wrap_indent (0 if no indent)
    ld a, (_wrap_indent)
    rrca                         ; indent_bytes = wrap_indent / 2 (0 or 6 only)
    ld (_plf_start_byte), a

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
    ex de, hl
    call _main_puts

mpwr_print_done:
    ; Restaurar next_start, cutpoint y char
    pop hl                       ; HL = next_start
    pop de                       ; DE = cutpoint
    pop af
    ld (de), a

    ; Si termin? cadena, salir al finalize
    ld a, (hl)
    or a
    jr z, mpwr_finalize          ; OPT: jp?jr (16 bytes)

    ; Nueva l?nea para continuar SIN perder HL
    push hl
    call _main_newline
    pop hl

    ; Si la paginaci?n fue cancelada durante el pause, salir
    ld a, (_main_col)
    cp 64
    jr z, mpwr_abort             ; OPT: jp?jr (13 bytes)

    jp mpwr_loop                 ; <-- era jr mpwr_loop

mpwr_finalize:
    ; Igual que main_print: reset indent antes del newline final
    xor a
    ld (_wrap_indent), a
    jp _main_newline

mpwr_abort:
    ret
    
; -----------------------------------------------------------------------------
; void main_puts(const char *s) __z88dk_fastcall
; Imprime una cadena usando la versi?n optimizada de putc.
; HL = string
; -----------------------------------------------------------------------------

EXTERN _overlay_mode
PUBLIC _main_puts
_main_puts:
    ; Suppress output during help overlay
    ld a, (_overlay_mode)
    or a
    ret nz
    ; HL = string pointer
    ; Initialize BPE return stack (empty)
    ; Optimizaci?n: cargar globals una vez y mantener en registros
    ; B = main_col, C = current_attr, D/E = string pointer
    ld d, h
    ld e, l             ; DE = puntero string
    ld hl, bpe_rstack
    ld (bpe_rsp), hl

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
    jr z, puts_opt_nl   ; '\n' -> newline

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

    ; Cache hit ? render space inline
    ; B = col, C = attr, DE = string ptr
    push de              ; save string pointer

    ; Screen address: cache_scr_base + col/2 (L múltiplo de 32, col/2 0..31 → no carry)
    ld hl, (cache_scr_base)
    ld a, b
    srl a
    add a, l
    ld l, a              ; HL = screen byte for this column pair

    ; Clear the selected nibble across 8 scanlines.
    ld e, 0x0F
    bit 0, b
    jr z, puts_sp_mask_set
    ld e, 0xF0
puts_sp_mask_set:
    ld d, 8
puts_sp_loop:
    ld a, (hl)
    and e
    ld (hl), a
    inc h
    dec d
    jr nz, puts_sp_loop
puts_sp_attr:
    ; Attribute: cache_atr_base + col/2 (L múltiplo de 32, col/2 0..31 → no carry)
    ld hl, (cache_atr_base)
    ld a, b
    srl a
    add a, l
    ld l, a
    ld (hl), c           ; C = current_attr
puts_sp_done:
    pop de               ; restore string pointer
    inc b                ; main_col++
    inc de               ; next char
    jr puts_opt_loop

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
    jr puts_opt_loop

puts_opt_wrap:
    ; Necesitamos wrap - sincronizar y llamar main_newline
    ld a, b
    ld (_main_col), a
    push de
    push hl             ; H tiene el caracter actual
    call _main_newline
    pop hl
    pop de
    call puts_reload_nl
    bit 6, b
    ret nz
    jr puts_opt_emit

puts_opt_nl:
    ; '\n' - sincronizar col y llamar newline
    ld a, b
    ld (_main_col), a
    push de
    call _main_newline
    pop de
    call puts_reload_nl
    bit 6, b
    ret nz
    inc de
    jr puts_opt_loop

puts_reload_nl:
    ; Shared reload after newline (main_newline clobbers BC)
    ld a, (_main_line)
    ld (_g_ps64_y), a
    ld a, (_current_attr)
    ld c, a
    ld a, (_main_col)
    ld b, a
    ret

; --- BPE expansion for puts_opt ---
; Token >= 0x80 found in A: push continuation (DE+1) to BPE stack,
; set DE to dict entry (3 bytes: b1, b2, 0x00). Loop continues reading
; from dict. On null, pops continuation and resumes original string.
puts_bpe_expand:
    ; A = token (0x80-0xFF), DE = current string position
    ; B = main_col, C = current_attr ? MUST be preserved!
    inc de                  ; advance past the token
    push bc                 ; save B=col, C=attr
    ; W8 fix: check rstack overflow before push (8 levels = 16 bytes)
    ld c, a                 ; save token while comparing fixed-page rsp low byte
    ld hl, (bpe_rsp)
    ld a, l
    cp 0xE5                 ; low byte of bpe_rstack + 16
    jr nc, puts_bpe_overflow
    ld a, c                 ; restore token
    ; Push continuation address (DE) onto BPE return stack
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
    ld b, h                 ; BC = (token - 0x80) ? temp, will restore
    add hl, hl              ; HL = * 2
    add hl, bc              ; HL = * 3
    ld bc, bpe_dict
    add hl, bc
    ex de, hl               ; DE = &bpe_dict[(token-0x80)*3]
    pop bc                  ; restore B=col, C=attr
    jp puts_opt_loop         ; continue reading from dict entry

puts_bpe_overflow:
    ; W8: rstack full ? treat token as literal '?', skip it
    pop bc                  ; restore B=col, C=attr
    jp puts_opt_loop         ; continue with DE (already advanced past token)

puts_bpe_pop:
    ; Null byte hit: end of string or end of BPE dict entry
    ; Check if BPE stack has entries
    ld hl, (bpe_rsp)
    ld a, l
    cp 0xD5                 ; low byte of bpe_rstack
    jr z, puts_opt_done     ; stack empty -> real end of string
    ; Pop continuation address from BPE stack
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
; Verifica si un nick est? en la lista de ignorados (Array de 16 bytes stride).
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
    ; Convenci?n C (stack): push right-to-left
    push hl             ; Arg2: nick
    push de             ; Arg1: list_item
    call _st_stricmp_cleanup
    
    ; Resultado en HL. Si es 0, hay coincidencia.
    ld a, h
    or l
    ; Restaurar contexto
    pop hl              ; nick
    pop de              ; list_ptr actual
    pop bc              ; contador
    jr z, ign_match
    
    ; Avanzar DE + 16 (tama?o fijo de cada entrada en ignore_list)
    ld a, e
    add a, 16
    ld e, a
    jr nc, ign_next
    inc d
    
ign_next:
    djnz ign_loop       ; Siguiente iteraci?n
    
ign_fail:
    ld l, 0
    ret
    
ign_match:
    ld l, 1
    ret


