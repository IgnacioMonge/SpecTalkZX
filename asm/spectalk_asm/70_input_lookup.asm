; =============================================================================
; UTF-8 to ASCII - Versi?n corregida
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
    jr c, u8a_copy          ; OPT: jp?jr (00-7F: ASCII)

    cp 0xC2
    jr c, u8a_skip1         ; OPT: jp?jr (80-C1: inv?lido)

    cp 0xC4
    jr c, u8a_latin1        ; OPT: jp?jr (C2-C3: Latin-1)

    cp 0xE0
    jr c, u8a_skip2         ; OPT: jp?jr (C4-DF: Latin Extended)

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
    jp u8a_store             ; jp: jr out of range after smart-quote fix

; Smart quotes: E2 80 98/99 ? apostrophe (39), E2 80 9C/9D ? double quote (34)
u8a_3byte:
    cp 0xE2
    jr nz, u8a_skip3
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
    cp 0x80
    jr nz, u8a_skip1        ; not E2 80 xx ? skip byte 3 ? '?'
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
    inc hl
    or 0x01                  ; pair 98/99?99, 9C/9D?9D
    ld c, a                  ; save OR'd byte3 (fix: A clobbered by ld)
    cp 0x99
    ld a, 39                 ; apostrophe for smart single quotes
    jr z, u8a_store
    ld a, c                  ; restore OR'd byte3
    cp 0x9D
    ld a, 34                 ; double quote for smart double quotes
    jr z, u8a_store
    ld a, '?'                ; audit L11: unknown E2 80 XX ? '?' not '"'
    jr u8a_store

u8a_skip3:
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
u8a_skip2:
    inc hl
    ld a, (hl) : or a : jr z, u8a_done
u8a_skip1:
    inc hl
    ld a, '?'
    jr u8a_store            ; OPT: jp?jr (~74 bytes)

u8a_copy:
    ld (de), a
    inc hl
    inc de
    jr u8a_loop             ; OPT: jp?jr (backward ~70 bytes)

u8a_latin1:
    ; A = C2 o C3
    ld b, a                 ; B = primer byte
    inc hl
    ld a, (hl)
    or a
    jr z, u8a_done          ; OPT: jp?jr
    ld c, a                 ; C = segundo byte (80-BF)
    inc hl

    ; Verificar que C es continuation (80-BF)
    ld a, c
    and 0xC0
    cp 0x80
    jr nz, u8a_invalid      ; OPT: jp?jr

    ; B=C2: codepoint = 80 + (C & 3F) = 80-BF
    ; B=C3: codepoint = C0 + (C & 3F) = C0-FF
    ld a, b
    cp 0xC3
    jr z, u8a_c3            ; OPT: jp?jr

    ; C2: codepoints 80-BF (s?mbolos, ?? etc)
    ld a, c
    cp 0xA1                 ; ?
    ld a, '!'
    jr z, u8a_store         ; OPT: jp?jr
    ld a, c
    cp 0xBF                 ; ?
    ld a, '?'
    jr z, u8a_store         ; OPT: jp?jr
    ld a, c
    cp 0xAB                 ; ?
    ld a, '<'
    jr z, u8a_store         ; OPT: jp?jr
    ld a, c
    cp 0xBB                 ; ?
    ld a, '>'
    jr z, u8a_store         ; OPT: jp?jr
    ld a, ' '               ; resto -> espacio
    jr u8a_store            ; OPT: jp?jr

u8a_c3:
    ; C3: codepoints C0-FF (vocales acentuadas, ?, ?, etc)
    ; ?ndice en tabla = C & 3F (0-3F corresponde a C0-FF)
    ld a, c
    and 0x3F
    ld c, a
    ld b, 0
    push hl             ; FIX-C2: guardar puntero de lectura del string
    ld hl, u8a_tbl_c0
    add hl, bc
    ld a, (hl)
    pop hl              ; FIX-C2: restaurar puntero de lectura
    jr u8a_store            ; OPT: jp?jr (5 bytes)

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
; ???????? ???????? ???????? ???????? ???????? ???????? ???????? ????????
u8a_tbl_c0:
    defb 'A','A','A','A','A','A','A','C'  ; C0-C7: ????????
    defb 'E','E','E','E','I','I','I','I'  ; C8-CF: ????????
    defb 'D','N','O','O','O','O','O','x'  ; D0-D7: ????????
    defb 'O','U','U','U','U','Y','T','s'  ; D8-DF: ????????
    defb 'a','a','a','a','a','a','a','c'  ; E0-E7: ????????
    defb 'e','e','e','e','i','i','i','i'  ; E8-EF: ????????
    defb 'o','n','o','o','o','o','o','/'  ; F0-F7: ????????
    defb 'o','u','u','u','u','y','t','y'  ; F8-FF: ????????

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
    ; It's 1970 ? ESP not synced yet
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
    jr nc, spr_notfound  ; OPT: jp?jr (i >= len-7)

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
    jr spr_scan             ; OPT: jp?jr (backward ~100 bytes)

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

    ; if (k == '0' && debounce_zero > 0) ? suppress
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
    ; if (k != last_k) ? new key
    ld a, (_last_k)
    cp b
    jr z, rk_repeat

    ; --- Case fold check: ignore Shift release (e.g. 'S' ? 's') ---
    ; lk_fold = last_k | 32, k_fold = k | 32
    ; if k_fold == lk_fold && lk_fold in 'a'..'z' ? suppress
    ld c, a             ; C = last_k
    or 32               ; A = last_k | 32
    ld d, a             ; D = lk_fold
    ld a, b
    or 32               ; A = k | 32
    cp d
    jr nz, rk_new_emit
    ; Both fold to same letter ? check if letter
    ld a, d
    cp 'a'
    jr c, rk_new_emit
    cp 'z'+1
    jr nc, rk_new_emit
    ; Same letter, just shift change ? update but don't emit
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
    ; k == last_k ? auto-repeat handling

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
    ; Timer expired ? emit based on key type
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
    ; Other keys: standard repeat
    ld a, 3
    ld (_repeat_timer), a
    ld l, b
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

    ; st_stricmp(ch->name, nick) ? name at offset 0, so DE = &ch->name
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

