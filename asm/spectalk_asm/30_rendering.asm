; =============================================================================
; GRAPHICS SYSTEM - TABLES
; =============================================================================

; Compute screen row base: HL = screen address for char row A (0-23), scanline 0
; Formula: H = 0x40 | (y & 0x18), L = (y & 7) * 32
; Input: A = row (0-23). Output: HL. Clobbers: AF.
_compute_screen_base:
    ld h, a             ; save y
    and 0x07            ; y & 7
    rrca                ; *32
    rrca
    rrca
    ld l, a             ; L = (y&7) * 32
    ld a, h             ; restore y
    and 0x18            ; y & 0x18 (third select)
    or 0x40             ; | 0x40 (screen base high byte)
    ld h, a
    ret

; Compute attribute row base: HL = 0x5800 + A*32
; Input: A = row (0-23). Output: HL. Clobbers: DE.
_compute_attr_base:
    ld l, a
    call _l_mul32           ; HL = A * 32
    ld de, 0x5800
    add hl, de
    ret

; =============================================================================
; FUENTE 64-COL
; =============================================================================
; Solo 10 valores ?nicos en toda la fuente original:
; 0x00, 0x22, 0x44, 0x55, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0xFF
; Raw 6-row font data is generated from the packed source into SPECTALK.DAT.
; Renderers draw one explicit blank top scanline, 6 glyph scanlines, then one
; explicit blank bottom scanline.
; Data loaded from SPECTALK.DAT at startup:
; [576 raw glyph rows][75 theme_raw] = 651 bytes contiguous.
SECTION bss_user
PUBLIC _font_lut
_font_lut:
font_lut:
font64_raw6:
    defs 576              ; 96 chars * 6 rows, LUT-expanded bytes
PUBLIC _theme_raw
_theme_raw:
    defs 75
PUBLIC _bpe_dict
_bpe_dict:
bpe_dict:
    defs 318

SECTION code_user

; =============================================================================
; GLYPH DECOMPRESSOR
; Input: A = char (ASCII 32-127)
; Output: HL = pointer to the 6 raw source rows.
; Preserves: IY (required by z88dk)
; Destroys: AF, DE, HL.
; =============================================================================
blank_glyph:
    defb 0, 0, 0, 0, 0, 0, 0

unpack_glyph:
    ; Calculate offset: (char - 32) * 6. The raw table stores exactly the
    ; six non-padding glyph rows, already expanded to the old LUT byte values.
    sub 32
    ld l, a
    ld h, 0
    add hl, hl          ; *2
    ld d, h
    ld e, l             ; DE = *2
    add hl, hl          ; *4
    add hl, de          ; *6
    ld de, font64_raw6
    add hl, de
    ret

; =============================================================================
; GRAPHICS SYSTEM - FUNCTIONS
; =============================================================================

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
    
    ; Calcular direcci?n de screen
    call _compute_screen_base
    ld d, h
    ld e, l
    
    ; Borrar 8 scanlines
    ld b, 8
cli_px_loop:
    push de
    push bc
    ld h, d
    ld l, e
    ld (hl), 0
    inc e
    ld bc, 31
    ldir
    pop bc
    pop de
    inc d
    djnz cli_px_loop
    
    ; Atributos via lookup table (preserva A para clear_zone caller)
    pop af
    call _compute_attr_base

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
; Stack: [IX+4]=y, [IX+5]=attr
; -----------------------------------------------------------------------------
_clear_line:
    call ___sdcc_enter_ix
    
    ld a, (ix+4)
    ld c, (ix+5)
    call cli_internal
    
    pop ix
    ret

; -----------------------------------------------------------------------------
; void clear_zone(uint8_t start, uint8_t lines, uint8_t attr)
; Stack: [IX+4]=start, [IX+5]=lines, [IX+6]=attr (sdcc_iy byte-packed)
; -----------------------------------------------------------------------------
_clear_zone:
    call ___sdcc_enter_ix
    
    ld a, (ix+4)        ; start
    ld b, (ix+5)        ; lines
    ld c, (ix+6)        ; attr

    ; Si lines == 0, salir. inc/dec preserva B y refleja Z sin tocar A.
    inc b
    dec b
    jr z, cz_done

cz_loop:
    call cli_internal
    inc a
    djnz cz_loop

cz_done:
    pop ix
    ret

; =============================================================================
; IMPRESI?N 64 COLUMNAS
; =============================================================================

; -----------------------------------------------------------------------------
; Cache helper: devuelve base de fila screen usando cache y refresca attr en miss.
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
    ld (cache_row_y), a
    ld b, a
    call _compute_screen_base
    ld (cache_scr_base), hl
    ld a, b
    push hl                ; Guardar screen base
    call _compute_attr_base
    ld (cache_atr_base), hl
    pop hl                 ; Devolver screen base
    ret

; -----------------------------------------------------------------------------
; void print_str64_char(uint8_t ch) __z88dk_fastcall
; Draw a 4-pixel character usando fuente 64 columnas
; Input: L = ASCII character
; No preserva: AF, BC, DE, HL
; Preserva: IY (no lo usa)
; -----------------------------------------------------------------------------
_print_str64_char:
    ; Validate character. Space/control/high bytes use direct clear path.
    ld a, l
    cp 33
    jr c, p64_space_path
    cp 128
    jr nc, p64_space_path

p64_not_space:
    ; Descomprimir/ubicar glifo.
    call unpack_glyph       ; Input: A = char, Output: HL = glyph rows
    push hl                 ; Guardar puntero fuente descomprimida

    ; Obtener screen base (cache o lookup)
    call p64_get_scr_base   ; HL = screen row base
    call dbc_add_col        ; HL += col/2, B = col (para paridad)

    pop de                  ; DE = puntero fuente

    ; Decidir lado izquierdo o derecho
    bit 0, b
    jr nz, p64_right

    ; === LADO IZQUIERDO (columna par) ===
    ld a, (hl)
    and 0x0F                ; Conservar nibble derecho
    ld (hl), a
    inc h

    ld b, 6
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
    ld a, (hl)
    and 0x0F                ; Bottom padding: clear left nibble only
    ld (hl), a
    jr p64_set_attr

p64_right:
    ; === LADO DERECHO (columna impar) ===
    ld a, (hl)
    and 0xF0                ; Conservar nibble izquierdo
    ld (hl), a
    inc h

    ld b, 6
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
    ld a, (hl)
    and 0xF0                ; Bottom padding: clear right nibble only
    ld (hl), a

p64_set_attr:
    ; cache_atr_base queda v?lida tras p64_get_scr_base
    ld hl, (cache_atr_base)

    ld a, (_g_ps64_col)
    srl a
    add a, l
    ld l, a

    ; Escribir atributo solo si cambia (evita store redundante)
    ld a, (_g_ps64_attr)
    cp (hl)
    ret z
    ld (hl), a
    ret

p64_space_path:
    ; Obtener screen base (cache o lookup)
    call p64_get_scr_base   ; HL = screen row base
    call dbc_add_col        ; HL += col/2, B = col (para paridad)

    ; Space clear (8 scanlines): C = nibble preserve mask
    ; Even col (bit0=0) -> AND 0x0F ; odd col (bit0=1) -> AND 0xF0
    bit 0, b
    ld c, 0xF0
    jr nz, p64_space_clear
    ld c, 0x0F
p64_space_clear:
    ld b, 8
p64_space_loop:
    ld a, (hl)
    and c
    ld (hl), a
    inc h
    djnz p64_space_loop
    jr p64_set_attr

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
    push af
    call _compute_screen_base
    call dbc_add_col
    call dbc_render_top
    pop af
    inc a
    call _compute_screen_base
    call dbc_add_col
    call dbc_render_bot
    ; Attribute cells cover both 4px halves, so set the touched physical cell
    ; for both rows after the bitmap write.
    ld a, (_g_ps64_y)
    call _compute_attr_base
    call dbc_add_col
    ld a, (_g_ps64_attr)
    ld (hl), a
    ld de, 32
    add hl, de
    ld (hl), a
    ret

; Shared: HL += col/2, and B = col (for parity test by caller).
; Caller draw_big_char ignores B post-call (dbc_render_top/bot reload `ld b, 3`).
; Caller _print_str64_char needs B = col for `bit 0, b`.
dbc_add_col:
    ld a, (_g_ps64_col)
    ld b, a
    srl a
    ; screen row base L is a multiple of 32; col/2 0..31 never carries into H
    add a, l
    ld l, a
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

; Shared: mask 2 scanlines with C (preserve nibble), advance HL
; Trailing inc h is harmless for bot callers (HL not used after ret).
dbc_mask_2sc:
    ld a, (hl)
    and c
    ld (hl), a
    inc h
    ld a, (hl)
    and c
    ld (hl), a
    inc h
    ret

dbc_render_top:
    ld a, (_g_ps64_col)
    rra
    jr c, dbc_rt_right
    ld c, 0x0F
    call dbc_mask_2sc
    ld b, 3
    jr dbc_left_core
dbc_rt_right:
    ld c, 0xF0
    call dbc_mask_2sc
    ld b, 3
    jr dbc_right_core

dbc_render_bot:
    ld a, (_g_ps64_col)
    rra
    jr c, dbc_rb_right
    ld b, 3
    call dbc_left_core
    ld c, 0x0F
    jp dbc_mask_2sc
dbc_rb_right:
    ld b, 3
    call dbc_right_core
    ld c, 0xF0
    jp dbc_mask_2sc

; =============================================================================
; IKKLE-4 FONT DATA (nibble-packed, 128 bytes ? uppercase only)
; =============================================================================
; Jack Oatley's "Ikkle-4" font (public domain), 4px wide x 4px tall.
; Only chars 32-95 stored (64 entries). Chars >= 96 folded by renderer.
; Each byte packs 2 scanlines: high nibble = scanline N, low = scanline N+1.
; -----------------------------------------------------------------------------
_ikkle_packed:
ikkle_packed:
    DEFB 0x00,0x00,0x88,0x08,0x08,0x00,0xF9,0xF0,0xF9,0xF0,0xF9,0xF0,0xF9,0xF0,0x08,0x00
    DEFB 0x48,0x84,0x84,0x48,0x08,0x00,0x04,0xE4,0x00,0x0C,0x00,0xC0,0x00,0x08,0x24,0x48
    DEFB 0xEA,0xAE,0xC4,0x4E,0xE2,0x4E,0xE6,0x2E,0x8A,0xE2,0xEC,0x2C,0xEC,0xAE,0xE2,0x48
    DEFB 0xEE,0xAE,0xEA,0x6E,0x08,0x08,0x04,0x0C,0x06,0x86,0x0C,0x0C,0x0C,0x2C,0xE6,0x04
    DEFB 0xF9,0xF0,0xEA,0xEA,0xEE,0xAE,0xC8,0x8C,0xCA,0xAE,0xEC,0x8E,0xE8,0xC8,0xE8,0xAE
    DEFB 0xAE,0xAA,0x44,0x44,0x44,0x4C,0xAC,0xAA,0x88,0x8C,0xAE,0xEA,0xCA,0xAA,0xEA,0xAE
    DEFB 0xEA,0xE8,0xEA,0xE2,0xEA,0xCA,0xEC,0x2E,0xE4,0x44,0xAA,0xAE,0xAA,0xA4,0xAA,0xEE
    DEFB 0xA4,0x4A,0xAE,0x44,0xE6,0x8E,0xC8,0x8C,0x84,0x42,0xC4,0x4C,0x4A,0x00,0x00,0x0C

; =============================================================================
; NOTIFICATION BAR RENDERER ? Ikkle-4 font at row 20 (scanlines 2-5)
; =============================================================================
DEFC NOTIF_ROW_BASE = 0x5080
DEFC NOTIF_ATTR     = 0x5A80

; void notif_clear(void)
_notif_clear:
    ld hl, NOTIF_ROW_BASE
    xor a
    ld b, 8
nc_clear_loop:
    ld c, 32
nc_clear_inner:
    ld (hl), a
    inc l               ; L goes 0x80..0x9F, no wrap
    dec c
    jr nz, nc_clear_inner
    ld l, 0x80
    inc h
    djnz nc_clear_loop
    ld hl, NOTIF_ATTR
    ld a, (_theme_attrs + TA_MAIN_BG)
    ld bc, 32
    jp _fast_fill_attr

; void notif_draw(uint8_t start_col, const char *str, uint8_t attr)
; Stack: [IX+4]=start_col, [IX+5,6]=str, [IX+7]=attr
_notif_draw:
    call ___sdcc_enter_ix

    call _notif_clear

    ld l, (ix+5)
    ld h, (ix+6)                ; HL = str
    ld c, (ix+4)                ; C = current column
    ; notify() only feeds strings already clamped to the 64-col bar.
    ; Stop on NUL and avoid a second in-loop end check.

nd_char_loop:
    ld a, (hl)
    or a
    jr z, nd_done

    push hl

    ld a, c
    srl a
    ld b, a                     ; B = phys_x

    ld a, (hl)

    cp 33
    jr c, nd_set_attr_space     ; space
    cp 128
    jr nc, nd_set_attr_space
    cp 96
    jr c, nd_no_fold
    sub 32                      ; lowercase -> uppercase
nd_no_fold:

    sub 32
    add a, a
    ld e, a
    ld d, 0
    push hl
    ld hl, ikkle_packed
    add hl, de
    ld d, (hl)                  ; D = packed byte 0
    inc hl
    ld e, (hl)                  ; E = packed byte 1
    pop hl

    ld l, b
    set 7, l
    ld h, 0x52

    bit 0, c
    jr nz, nd_odd_col

    ; === EVEN COLUMN ===
    ld a, d
    and 0xF0
    ld (hl), a
    ld a, d
    and 0x0F
    rlca \ rlca \ rlca \ rlca
    inc h
    ld (hl), a
    ld a, e
    and 0xF0
    inc h
    ld (hl), a
    ld a, e
    and 0x0F
    rlca \ rlca \ rlca \ rlca
    inc h
    ld (hl), a
    jr nd_set_attr_drawn

    ; === ODD COLUMN ===
nd_odd_col:
    ld a, d
    rrca \ rrca \ rrca \ rrca
    and 0x0F
    or (hl)
    ld (hl), a
    ld a, d
    and 0x0F
    inc h
    or (hl)
    ld (hl), a
    ld a, e
    rrca \ rrca \ rrca \ rrca
    and 0x0F
    inc h
    or (hl)
    ld (hl), a
    ld a, e
    and 0x0F
    inc h
    or (hl)
    ld (hl), a

nd_set_attr_space:
    ld l, b
    set 7, l
nd_set_attr_drawn:
    ld h, 0x5A
    ld a, (ix+7)
    ld (hl), a

nd_skip:
    pop hl
    inc hl
    inc c
    jr nd_char_loop

nd_done:
    pop ix
    ret

; -----------------------------------------------------------------------------
; void put_char64_input_cached(uint8_t y, uint8_t col, char ch, uint8_t attr)
; __z88dk_callee. Preserves the C helper contract, but specializes the hot
; input rows and fixed Printer Buffer cache addresses.
; Stack: [ret] [y] [col] [ch] [attr]
; -----------------------------------------------------------------------------
PUBLIC _put_char64_input_cached
_put_char64_input_cached:
    pop de                  ; return address
    pop bc                  ; C = y, B = col
    pop hl                  ; L = ch, H = attr
    push de
    ld d, l                 ; D = ch
    ld e, h                 ; E = attr

    ld a, c
    cp 22
    jr c, pci_draw
    cp 24
    jr nc, pci_draw
    ld a, b
    cp 64
    jr nc, pci_draw

    call pci_char_addr
    ld a, (hl)
    cp d
    jr nz, pci_update

    ; VRAM attr for input rows: row 22=$5AC0, row 23=$5AE0.
    ld a, b
    srl a
    add a, 0xC0
    ld l, a
    ld h, 0x5A
    ld a, c
    cp 23
    jr nz, pci_vattr_ready
    set 5, l
pci_vattr_ready:
    ld a, (hl)
    cp e
    jr nz, pci_update
    ret

pci_update:
    call pci_char_addr
    ld (hl), d

pci_draw:
    ld a, c
    ld (_g_ps64_y), a
    ld a, b
    ld (_g_ps64_col), a
    ld a, e
    ld (_g_ps64_attr), a
    ld l, d
    jp _print_str64_char

pci_char_addr:
    ld l, b
    ld h, 0x5B
    ld a, c
    cp 23
    ret nz
    set 6, l
    ret

; -----------------------------------------------------------------------------
; void print_line64_fast(uint8_t y, const char *s, uint8_t attr)
; Stack layout (con push ix):
;   [IX+4]=y, [IX+5,6]=s (pointer), [IX+7]=attr
;
; Optimized: procesa pares de columnas, calcula screen addr 1 vez,
; escribe bytes completos (sin AND/OR de preservaci?n), attr fill al final.
; -----------------------------------------------------------------------------
_print_line64_fast:
    call ___sdcc_enter_ix
    push iy

    ; --- Calcular screen_row_base[y] una sola vez ---
    ld a, (ix+4)           ; y
    call _compute_screen_base

    ; Guardar attr y string pointer
    ld a, (ix+7)
    ld (plf_attr_val), a   ; attr para despu?s
    ld e, (ix+5)
    ld d, (ix+6)           ; DE = string pointer

    ; Tambi?n guardar y para attr fill al final
    ld a, (ix+4)
    ld (plf_y_val), a

    ; Check start offset (set by caller for wrap_indent support).
    ; Unificado: si start=0, add a,l es no-op y sub c deja B=32.
    ; screen row base L is a multiple of 32; start_byte 0..31 never carries into H.
    ld a, (_plf_start_byte)
    ld c, a
    add a, l
    ld l, a
    ld a, 32
    sub c
    ld b, a
    ld a, (plf_pair_count)
    or a
    jr z, plf_use_max_count
    cp b
    jr c, plf_count_ready
plf_use_max_count:
    ld a, b
plf_count_ready:
    ld (plf_pair_count), a
    ld iyl, a

plf_pair_loop:
    push hl                ; Guardar screen addr del byte actual

    ; Fast path: if both chars in this byte are blank after normalization
    ; (space, NUL padding, or invalid control/high byte), skip glyph unpacking.
    ld h, d
    ld l, e                ; HL = candidate string pointer
    ld a, (hl)
    or a
    jr z, plf_blank_pair   ; left NUL: right is also padding, DE unchanged
    cp 33
    jr c, plf_left_blank
    cp 128
    jr c, plf_left_normal
plf_left_blank:
    inc hl                 ; left consumed
    ld a, (hl)
    or a
    jr z, plf_blank_pair_set_de
    cp 33
    jr c, plf_right_blank
    cp 128
    jr c, plf_left_blank_right_normal
plf_right_blank:
    inc hl                 ; right consumed
plf_blank_pair_set_de:
    ld d, h
    ld e, l
plf_blank_pair:
    pop hl                 ; screen addr
    xor a
    ld b, 8
plf_blank_loop:
    ld (hl), a
    inc h
    djnz plf_blank_loop
    jr plf_pair_advance

plf_left_blank_right_normal:
    inc hl                 ; right consumed; A still holds right char
    ld (plf_str_ptr), hl
    call unpack_glyph
    ex de, hl              ; DE = right glyph
    ld ix, blank_glyph
    jr plf_write_pair

plf_left_normal:
    ; A = left char (33..127), HL = string pointer from the lookahead pass.
    inc hl                 ; consume left char
    ld (plf_str_ptr), hl
    call unpack_glyph      ; A still has char; HL/DE don't matter (destroyed)
    push hl                ; save left glyph pointer above saved screen addr

    ; --- Leer char derecho antes de copiar el izquierdo ---
    ; If the right side is blank, the write loop can use the left source directly.
    ld hl, (plf_str_ptr)
    ld a, (hl)
    or a
    jr z, plf_right_blank_left_direct ; NUL: no avanzar puntero, usar espacio
    inc hl                 ; Avanzar puntero
    cp 33
    jr c, plf_right_blank_left_direct ; control/space: blank, pointer consumed
    cp 128
    jr c, plf_right_ok     ; char 33-127: OK
plf_right_blank_left_direct:
    ld (plf_str_ptr), hl
    ld de, blank_glyph
    pop ix                 ; IX = left glyph; screen addr remains stacked
    jr plf_write_pair
plf_right_ok:
    ld (plf_str_ptr), hl
    ; Copy the left glyph only when the right glyph must also be resolved.
    pop hl                 ; HL = left glyph; screen addr remains stacked
    ld de, plf_left_buf
    ld bc, 6
    ldir
    call unpack_glyph      ; A still has char; HL/DE don't matter (destroyed)
    ex de, hl              ; DE = pointer to right glyph
    ld ix, plf_left_buf

plf_write_pair:
    ; --- Combinar y escribir 8 scanlines ---
    ; Keep vertical alignment identical to _print_str64_char():
    ; blank top scanline, 6 glyph rows, then blank bottom scanline.
    pop hl                 ; HL = screen addr

    xor a
    ld (hl), a             ; scanline 0 stays blank like per-char rendering
    inc h

    ld b, 6
plf_write_loop:
    ld a, (ix+0)           ; Glyph byte izquierdo completo
    and 0xF0               ; Mask diferida (antes pre-masked en save loop)
    ld c, a
    ld a, (de)             ; Byte del char derecho
    and 0x0F               ; Solo nibble bajo (lado derecho)
    or c                   ; Combinar: left_high | right_low
    ld (hl), a             ; Escribir byte completo a pantalla
    inc ix
    inc de
    inc h                  ; Siguiente scanline
    djnz plf_write_loop
    xor a
    ld (hl), a             ; scanline 7 bottom padding
    inc h                  ; Keep pair-advance contract: HL is base + 8 scanlines

    ld de, (plf_str_ptr)   ; DE = string pointer para siguiente iteraci?n
plf_pair_advance:
    ; HL is one screen character cell below the byte just written (H += 8).
    ; Restore the row base without another stack round-trip.
    ld a, h
    sub 8
    ld h, a
    inc hl                 ; Siguiente byte (siguiente par de columnas)

    dec iyl
    jp nz, plf_pair_loop

    ; --- Attr fill: write attributes from plf_start_byte onwards ---
    ld a, (plf_y_val)
    call _compute_attr_base   ; HL = attr base
    ld a, (_plf_start_byte)
    ld c, a
    add hl, bc             ; Skip indent attrs
    ld a, (plf_attr_val)
    ld (hl), a             ; Write first byte
    ld d, h
    ld e, l
    inc de
    ld a, (plf_pair_count)
    dec a                  ; remaining after first byte
    jr z, plf_no_ldir      ; start_byte=31 → BC would be 0 → LDIR = 65536 bytes
    ld c, a
    ldir                   ; Fill remaining attrs
plf_no_ldir:
    ; Reset optional caller state
    xor a
    ld (_plf_start_byte), a
    ld (plf_pair_count), a

    ; (S01) Trailing update de _g_ps64_col/y/attr eliminado. Verificado:
    ; todos los readers (main_puts, main_newline, print_char64, print_big_str,
    ; print_str64) setean los globals antes
    ; de usarlos. Ningún consumidor depende del valor post-retorno.

    pop iy
    pop ix
    ret

; -----------------------------------------------------------------------------
; void print_status_left54_fast(const char *s) __z88dk_fastcall
; Render only logical columns 0..53 of the status bar (27 physical cells).
; Columns 54..63 are owned by draw_clock() and draw_indicator(); keeping this
; left render bounded avoids right-side flicker when user counts update.
; -----------------------------------------------------------------------------
_print_status_left54_fast:
    ld a, 27
    ld (plf_pair_count), a
    ld c, h                     ; C = str_hi
    ld h, l                     ; H = str_lo
    ld l, 21                    ; INFO_LINE
    ld a, (_theme_attrs + TA_STATUS)
    ld b, a                     ; B = ATTR_STATUS
    push bc                     ; [str_hi][attr]
    push hl                     ; [y][str_lo]
    call _print_line64_fast
    pop bc
    pop bc
    ret

; -----------------------------------------------------------------------------
; void redraw_input_asm(void)
; Redibuja las 2 l?neas de INPUT usando line_buffer y line_len
; Incluye prompt "> " y rellena con espacios
; Usa variables globales: _line_buffer, _line_len, theme_attrs[6]=INPUT, [8]=PROMPT
; Mucho m?s r?pido que el loop C equivalente
; -----------------------------------------------------------------------------
PUBLIC _redraw_input_asm
EXTERN _line_len
; _ATTR_INPUT = _theme_attrs + 6, _ATTR_PROMPT = _theme_attrs + 8

DEFC INPUT_ROW_1 = 22
DEFC INPUT_ROW_2 = 23

_redraw_input_asm:
    ; === L?nea 1: "> " + primeros 62 chars ===
    ; Prompt char: '@' if query AND idx>0, '>' otherwise
    ld c, '>'
    ld a, (_current_channel_idx)
    or a
    jr z, ria_prompt_ready     ; idx==0 is Server, always '>'
    ld hl, (_cur_chan_ptr)     ; HL = pointer to current ChannelInfo
    ld de, 30                  ; offset of flags in ChannelInfo (22+6+2)
    add hl, de
    bit 1, (hl)                ; CH_FLAG_QUERY
    jr z, ria_prompt_ready
    ld c, '@'
ria_prompt_ready:
    ld a, c
    ; Draw byte 0 directly: prompt glyph on the left, blank on the right.
    ; Vertical layout matches print_str64_char(): blank top scanline, glyph rows.
    call unpack_glyph
    ex de, hl                  ; DE = glyph source
    ld hl, 0x50C0              ; INPUT_ROW_1 bitmap base
    xor a
    ld (hl), a
    inc h
    ld b, 6
ria_prompt_loop:
    ld a, (de)
    and 0xF0
    ld (hl), a
    inc de
    inc h
    djnz ria_prompt_loop
    xor a
    ld (hl), a

    ld hl, 0x5AC0              ; INPUT_ROW_1 attr base
    ld a, (_theme_attrs + 8)   ; ATTR_PROMPT
    ld (hl), a
    
    ; L?nea 1: byte 0 reservado para prompt, texto desde byte 1 (62 cols)
    ld a, 1
    ld (_plf_start_byte), a
    ld hl, _line_buffer
    ld a, INPUT_ROW_1
    call ria_call_plf

    ; L?nea 2: desde min(line_len, 62), o desde el NUL si no hay segunda l?nea
    ld a, (_line_len)
    cp 62
    jr c, ria_l2_ofs_ok
    ld a, 62
ria_l2_ofs_ok:
    ld hl, _line_buffer
    add a, l                    ; _line_buffer=$5CB6, +62 max: no carry
    ld l, a
    ld a, INPUT_ROW_2
    ; fall through

; Helper: call print_line64_fast(y, hl, ATTR_INPUT)
; Input: A = y, HL = string ptr
ria_call_plf:
    ld c, h                     ; C = str_hi
    ld h, l                     ; H = str_lo
    ld l, a                     ; L = y
    ld a, (_theme_attrs + 6)    ; ATTR_INPUT
    ld b, a                     ; B = attr
    push bc                     ; [str_hi][attr]
    push hl                     ; [y][str_lo]
    call _print_line64_fast
    pop bc
    pop bc
    ret


; -----------------------------------------------------------------------------
; void draw_indicator(uint8_t y, uint8_t phys_x, uint8_t attr)
; Dibuja un c?rculo de estado 8x8
; - away: medio c?rculo (prioridad)
; - buffer_pressure: c?rculo vac?o
; - normal: c?rculo s?lido
; Stack: [IX+4]=y, [IX+5]=phys_x, [IX+6]=attr
; TABLE-DRIVEN: saves ~20 bytes vs inline patterns
; -----------------------------------------------------------------------------

; Indicator patterns: 8 bytes each (shared first/last with 0x00)
; Overlapped indicator patterns: each reads 8 bytes from label.
; Shared 0x00 bytes at row 0/7 boundaries collapse 5x8=40B into 35B (-5B).
ind_pattern_solid:
    defb 0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00
    defb 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00
    defb 0x3C, 0x4E, 0x4E, 0x4E, 0x4E, 0x3C, 0x00
    defb 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00
    defb 0x00, 0x18, 0x18, 0x00, 0x00, 0x00
ind_pattern_empty   equ ind_pattern_solid + 7
ind_pattern_away    equ ind_pattern_solid + 14
ind_pattern_medium  equ ind_pattern_solid + 21
ind_pattern_small   equ ind_pattern_solid + 27

; Shared: select indicator pattern into DE based on connection state.
; Preload + early-return: DE se sobreescribe cuanto antes y cada check
; decide ret con el mismo A que usó `or a` (ld no afecta flags).
; Preserves: BC, HL, IX, IY
ind_select_pattern:
    ld de, ind_pattern_away
    ld a, (_irc_is_away)
    or a
    ret nz
    ld de, ind_pattern_empty
    ld a, (_buffer_pressure)
    or a
    ret nz
    ld de, ind_pattern_solid
    ld a, (_ping_latency)
    or a
    ret z                        ; 0 = good, use solid
    ld de, ind_pattern_medium
    cp 2
    ret c                        ; latency==1 → medium
    ld de, ind_pattern_small
    ret

_draw_indicator:
    call ___sdcc_enter_ix

    ld a, (ix+4)            ; y
    push af
    ld c, (ix+6)            ; attr

    ; Calcular direcci?n screen
    call _compute_screen_base

    ; screen row base L is a multiple of 32; phys_x 0..31 never carries into H
    ld a, (ix+5)            ; phys_x
    add a, l
    ld l, a                 ; HL = direcci?n exacta pixel
    
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

    ; Atributos via lookup table
    pop af
    call _compute_attr_base ; HL = attr base

    ; attr row base L is a multiple of 32; phys_x 0..31 never carries into H
    ld a, (ix+5)            ; phys_x
    add a, l
    ld l, a

    ld (hl), c              ; attr

    pop ix
    ret

; -----------------------------------------------------------------------------
; void draw_cursor_underline(uint8_t y, uint8_t col) __z88dk_callee
; Input cursor overlay for the 64-col input rows. Same behavior as the former C
; helper: force ATTR_INPUT, clear both scanline 0/7 cursor bits, then draw the
; active caps indicator on scanline 0 or scanline 7.
; Stack: [ret] [y] [col]
; -----------------------------------------------------------------------------
_draw_cursor_underline:
    pop de                  ; DE = return address
    pop bc                  ; C = y, B = col
    push de                 ; restore return address

    ld a, b
    srl a                   ; A = phys_x = col / 2
    ld e, a                 ; E = phys_x
    ld a, b
    and 1
    ld d, 0xF0              ; D = cursor mask
    ld b, 0x0F              ; B = inverse mask
    jr z, dcu_masks
    ld d, b
    ld b, 0xF0
dcu_masks:
    ; Attribute: attr_addr(y, phys_x) = ATTR_INPUT.
    push de                 ; save mask/phys_x
    push bc                 ; save inv_mask/y
    ld a, c
    call _compute_attr_base
    pop bc
    pop de
    ld a, e
    add a, l                ; attr rows are 32-byte aligned; no carry
    ld l, a
    ld a, (_theme_attrs + 6) ; ATTR_INPUT
    ld (hl), a

    ; Bitmap: compute scanline 0 cell address once.
    push de
    push bc
    ld a, c
    call _compute_screen_base
    pop bc
    pop de
    ld a, e
    add a, l                ; screen rows are 32-byte aligned; no carry
    ld l, a                 ; HL = scanline 0 cell

    ; Clear cursor nibble on scanline 0 and scanline 7.
    ld a, (hl)
    and b
    ld (hl), a
    ld a, h
    add a, 7
    ld h, a                 ; HL = scanline 7 cell
    ld a, (hl)
    and b
    ld (hl), a

    ; Effective caps = caps_lock_mode XOR key_shift_held().
    push de                 ; save cursor mask
    push hl                 ; save scanline 7 cell
    call _key_shift_held
    ld a, (_caps_lock_mode)
    xor l
    pop hl                  ; HL = scanline 7 cell
    pop de                  ; D = cursor mask
    or a
    jr z, dcu_draw
    ld a, h
    sub 7
    ld h, a                 ; effective caps: draw on scanline 0
dcu_draw:
    ld a, (hl)
    or d
    ld (hl), a
    ret


