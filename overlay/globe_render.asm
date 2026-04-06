;; globe_render.asm — Globe renderer for SPCTLK2.OVL
;; Cell loop adapted directly from tools/globe.asm:138-194.
;; Outer loop added for row-based globe_data format.

SECTION code_user

PUBLIC _render_globe
PUBLIC _globe_init_dither

EXTERN _globe_data
EXTERN _g_rot
EXTERN _globe_row
EXTERN _current_theme

DEFC GLOBE_COL_BASE = 22
DEFC GLOBE_NROWS    = 11
DEFC GLOBE_CELL_OFF = 88
; Attr constants removed — now in globe_palette (theme-dependent)

; ─────────────────────────────────────────────────────────────
; globe_init_dither — write 0x55 to pixel RAM for all globe
; cells. Called ONCE from about_render_ovl.
; ─────────────────────────────────────────────────────────────
_globe_init_dither:
    ld hl, _globe_data + GLOBE_CELL_OFF
    ld a, (_globe_row)
    ld d, a
    ld e, GLOBE_NROWS

gid_row:
    ld c, (hl)
    inc hl
    ld b, (hl)
    inc hl
    ; Skip lon_offsets to find next row
    ld a, b
    push de
    ld e, a
    ld d, 0
    add hl, de
    pop de
    push hl                     ; next-row ptr

    ; Pixel base scanline 0
    ld a, d
    and 0x18
    or 0x40
    ld h, a
    ld a, d
    and 0x07
    rrca
    rrca
    rrca
    add a, GLOBE_COL_BASE
    add a, c
    ld l, a

gid_cell:
    push hl
    ld a, 0x55
    ld c, 8
gid_scan:
    ld (hl), a
    inc h
    dec c
    jr nz, gid_scan
    pop hl
    inc l
    djnz gid_cell

    pop hl
    inc d
    dec e
    jr nz, gid_row
    ret

; ─────────────────────────────────────────────────────────────
; render_globe — attr-only renderer
; Inner cell loop = globe.asm:151-194 adapted for bit-packed map.
; ─────────────────────────────────────────────────────────────
_render_globe:
    ld a, (_g_rot)
    ld (rot_smc + 1), a

    ; Select palette by theme index (1-3 → 0-2 → ×4)
    ld a, (_current_theme)
    dec a
    add a, a
    add a, a
    ld c, a
    ld b, 0
    ld hl, globe_palette
    add hl, bc
    ld (smc_pal + 1), hl        ; SMC: palette base for cell loop

    ld hl, _globe_data + GLOBE_CELL_OFF
    ld a, (_globe_row)
    ld (rg_row_cur), a
    ld a, GLOBE_NROWS
    ld (rg_row_cnt), a

rg_row:
    ; Read row header
    ld c, (hl)                  ; start_col
    inc hl
    ld a, (hl)                  ; cell count
    inc hl
    ld (rg_cnt + 1), a         ; SMC cell counter
    push hl                     ; save data ptr

    ; DE = 0x5800 + row*32 + GLOBE_COL_BASE + start_col
    ld a, (rg_row_cur)
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5800 + GLOBE_COL_BASE
    add hl, de
    ld a, c
    add a, l
    ld e, a
    ld d, h

    ; BC = globe_data + row_index*8 → SMC
    ld a, (rg_row_cur)
    ld hl, _globe_row
    sub (hl)
    add a, a
    add a, a
    add a, a
    ld c, a
    ld b, 0
    ld hl, _globe_data
    add hl, bc
    ld (smc_map + 1), hl

    pop hl                      ; restore data ptr

    ; ── Cell loop (from globe.asm, adapted for bit-packed map) ──
cell_loop:
    ld a, (hl)                  ; lon_offset (bit 7 = shadow)
    inc hl
    push hl                     ; save data ptr
    push de                     ; save attr addr

    ld d, a                     ; D = lon_offset with shadow bit

    ; Map lookup: strip shadow, add rotation, mod 64
    and 63
rot_smc:
    add a, 0                    ; + rotation (SMC)
    and 63                      ; map_col

    ; Bit-packed extraction: byte = map[map_col>>3], bit = map_col & 7
    push af                     ; save map_col
    srl a
    srl a
    srl a                       ; byte_index
    ld l, a
    ld h, 0
smc_map:
    ld bc, 0                    ; SMC: map_row_base
    add hl, bc                  ; HL = map_byte_addr
    ld a, (hl)                  ; map byte
    ld l, a                     ; save map byte in L
    pop af                      ; restore map_col
    and 7                       ; bit_index
    ld b, a
    inc b                       ; loop count (1-8)
    ld a, l                     ; map byte
rg_rot:
    rlca                        ; bit 7 → carry each iteration
    djnz rg_rot                 ; carry = land after (bit_index+1) rotations

    ; Attr selection — palette[ shadow*2 + is_land ]
    ; carry = land (from rlca), D = lon_offset (bit 7 = shadow)
    ld a, 0
    rla                         ; A = is_land (0 or 1)
    bit 7, d                    ; night? (test D BEFORE pop de trashes it)
    jr z, rg_day
    or 2                        ; A = is_land + 2
rg_day:
    pop de                      ; DE = attr addr
    ; HL is free here (data ptr still on stack)
smc_pal:
    ld hl, 0                    ; SMC: palette base
    ld c, a
    add hl, bc                  ; 16-bit add — B=0 guaranteed by djnz exit
    ld a, (hl)                  ; A = attr from palette
    ld (de), a                  ; write attribute
    pop hl                      ; restore data ptr
    inc e                       ; next attr column
rg_cnt:
    ld a, 0                     ; SMC: cells remaining
    dec a
    ld (rg_cnt + 1), a
    jr nz, cell_loop

    ; Next row
    ld a, (rg_row_cur)
    inc a
    ld (rg_row_cur), a
    ld a, (rg_row_cnt)
    dec a
    ld (rg_row_cnt), a
    jp nz, rg_row
    ret

rg_row_cur: db 0
rg_row_cnt: db 0

; Globe palette: [water_day, land_day, water_night, land_night] × 3 themes
globe_palette:
    db 0x09, 0x66, 0x01, 0x04      ; Theme 1 — Default
    db 0x44, 0x64, 0x00, 0x04      ; Theme 2 — Terminal (green phosphor)
    db 0x4D, 0x66, 0x01, 0x04      ; Theme 3 — Commander (day water = BRIGHT cyan/blue)
