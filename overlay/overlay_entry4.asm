;; overlay_entry4.asm — Entry table for SPCTLK4.OVL
SECTION code_user
EXTERN _status_render_ovl
EXTERN _save_config_ovl
EXTERN _overlay_slot
PUBLIC _cfg_put
PUBLIC _cfg_kv
PUBLIC _cfg_put_autojoin
    dw 2                      ; entry_count = 2
    dw _status_render_ovl     ; entry 0 → status
    dw _save_config_ovl       ; entry 1 → config save

; Local SPCTLK4 config-save helpers. They keep the former resident ABI.
; char *cfg_put(char *p, const char *s) __z88dk_callee
_cfg_put:
    pop bc
    pop de
    pop hl
    push bc
    ex de, hl
ovl4_cfg_put_loop:
    ld a, (de)
    or a
    jr z, ovl4_cfg_put_done
    call ovl4_put_a_hl
    inc de
    jr ovl4_cfg_put_loop
ovl4_cfg_put_done:
    ret

; A=byte, HL=destination. Returns HL=destination+1, or overlay_slot+513
; when no room remains. Preserves DE so callers can keep source pointers live.
ovl4_put_a_hl:
    push de
    push hl
    ld de, _overlay_slot + 512
    or a
    sbc hl, de
    pop hl
    jr c, ovl4_put_room
    ld hl, _overlay_slot + 513
    pop de
    ret
ovl4_put_room:
    ld (hl), a
    inc hl
    pop de
    ret

; char *cfg_kv(char *p, const char *key, const char *val) __z88dk_callee
; Writes key, then val (or digit if val<=9), then CRLF.
_cfg_kv:
    pop hl
    pop de
    pop bc
    ex (sp), hl
    push hl
    push bc
    push de
    call _cfg_put
    pop de
    ld a, d
    or a
    jr nz, ovl4_ckv_string
    ld a, e
    cp 10
    jr nc, ovl4_ckv_string
    add a, '0'
    call ovl4_put_a_hl
    jr ovl4_ckv_crlf
ovl4_ckv_string:
    push de
    push hl
    call _cfg_put
ovl4_ckv_crlf:
    ld a, 13
    call ovl4_put_a_hl
    ld a, 10
    call ovl4_put_a_hl
    ret

; char *cfg_put_autojoin(char *p) __z88dk_fastcall
; Prefer active non-query channel slots; fall back to preloaded search_pattern
; when disconnected or before channels have been opened.
EXTERN _channels
EXTERN _search_pattern
EXTERN _autojoin_channels
_cfg_put_autojoin:
    ld (ovl4_cpa_dest), hl
    xor a
    ld (ovl4_cpa_any), a
    ld hl, _channels + 32
    ld b, 9
ovl4_cpa_loop:
    push hl
    ld a, l
    add a, 30
    ld l, a
    jr nc, ovl4_cpa_flag_addr
    inc h
ovl4_cpa_flag_addr:
    ld a, (hl)
    pop hl
    and 0x03
    cp 0x01
    jr nz, ovl4_cpa_next
    ld a, (hl)
    cp '#'
    jr z, ovl4_cpa_chan
    cp '&'
    jr nz, ovl4_cpa_next
ovl4_cpa_chan:
    ld a, (ovl4_cpa_any)
    or a
    jr nz, ovl4_cpa_comma
    inc a
    ld (ovl4_cpa_any), a
    push bc
    push hl
    ld de, ovl4_ck_chans
    call ovl4_cpa_put_de
    pop hl
    pop bc
    jr ovl4_cpa_put_chan
ovl4_cpa_comma:
    push hl
    ld a, ','
    call ovl4_cpa_put_a
    pop hl
ovl4_cpa_put_chan:
    push bc
    push hl
    ld d, h
    ld e, l
    call ovl4_cpa_put_de
    pop hl
    pop bc
ovl4_cpa_next:
    ld a, l
    add a, 32
    ld l, a
    jr nc, ovl4_cpa_next_ok
    inc h
ovl4_cpa_next_ok:
    djnz ovl4_cpa_loop

    ld a, (ovl4_cpa_any)
    or a
    jr nz, ovl4_cpa_crlf

    ld hl, _autojoin_channels
    ld a, (hl)
    cp '#'
    jr z, ovl4_cpa_fallback
    cp '&'
    jr nz, ovl4_cpa_ret_dest
ovl4_cpa_fallback:
    ld de, ovl4_ck_chans
    call ovl4_cpa_put_de
    ld de, _autojoin_channels
    call ovl4_cpa_put_de
ovl4_cpa_crlf:
    ld a, 13
    call ovl4_cpa_put_a
    ld a, 10
    call ovl4_cpa_put_a
    ld hl, (ovl4_cpa_dest)
    ret
ovl4_cpa_ret_dest:
    ld hl, (ovl4_cpa_dest)
    ret

ovl4_cpa_put_a:
    ld hl, (ovl4_cpa_dest)
    call ovl4_put_a_hl
    ld (ovl4_cpa_dest), hl
    ret

ovl4_cpa_put_de:
ovl4_cpa_put_loop:
    ld a, (de)
    or a
    jr z, ovl4_cpa_put_done
    call ovl4_cpa_put_a
    inc de
    jr ovl4_cpa_put_loop
ovl4_cpa_put_done:
    ret

ovl4_cpa_dest: defw 0
ovl4_cpa_any:  defb 0
ovl4_ck_chans: defb "channels=", 0
