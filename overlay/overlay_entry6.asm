;; overlay_entry6.asm -- Raw UDP NTP fallback for old ESP AT firmwares.
;; Entry 0: raw UDP NTP fallback. Entry 1: channel switcher render.

SECTION code_user

PUBLIC _sntp_udp_ovl
EXTERN _switcher_render_ovl

EXTERN _uart_send_string
EXTERN _uart_send_line
EXTERN _ay_uart_send
EXTERN _frame_wait
EXTERN _draw_status_bar
EXTERN uartRead
EXTERN _reset_rx_state

EXTERN _sntp_tz
EXTERN _sntp_waiting
EXTERN _sntp_queried
EXTERN _time_hour
EXTERN _time_minute
EXTERN _time_second
EXTERN _last_frames_lo
EXTERN _tick_accum
EXTERN _status_bar_dirty

DEFC TZ_RTC = 127

    dw 2
    dw _sntp_udp_ovl
    dw _switcher_render_ovl

_sntp_udp_ovl:
    ld a, (_sntp_tz)
    cp TZ_RTC
    jp z, udp_done

    ld hl, cmd_cipdomain
    call _uart_send_line
    call wait_domain
    jp c, udp_fail_dns
    call read_domain_ip
    jp c, udp_fail_dns
    call wait_ok
    jp c, udp_fail_dns

    ld hl, cmd_cipstart_pfx
    call _uart_send_string
    ld hl, ip_buf
    call _uart_send_string
    ld hl, cmd_cipstart_tail
    call _uart_send_line
    call wait_ok
    jp c, udp_fail_open

    ld hl, cmd_cipsend
    call _uart_send_line
    ld a, '>'
    call wait_char
    jp c, udp_fail_send

    ld l, $0B                  ; Match working .ntpdate request (client, v1)
    call _ay_uart_send
    ld d, 59
udp_send_zero:
    push de
    ld l, 0
    call _ay_uart_send
    pop de
    dec d
    jr nz, udp_send_zero

    call wait_ipd
    jp c, udp_fail_ipd
    call skip_ipd_len
    jp c, udp_fail_ipd

    ld b, 40                   ; transmit timestamp starts at byte 40
udp_skip_ts:
    push bc
    call read_byte_timeout
    pop bc
    jp nc, udp_fail_ipd
    djnz udp_skip_ts

    ld hl, ntp_secs
    ld b, 4
udp_store_ts:
    push bc
    push hl
    call read_byte_timeout
    pop hl
    pop bc
    jp nc, udp_fail_ipd
    ld (hl), a
    inc hl
    djnz udp_store_ts

    call convert_ntp_time
    jp c, udp_fail_time
    call commit_ntp_time
    jr udp_close

udp_fail_open:
udp_fail_send:
udp_fail_ipd:
udp_fail_time:
udp_close:
    ld hl, cmd_cipclose
    call _uart_send_line

udp_fail_dns:
udp_fail:
    xor a
    ld (_sntp_waiting), a

udp_done:
    jp _reset_rx_state

wait_domain:
    call wait_plus
    ret c
    ld hl, tail_domain
    call match_tail
    ret c
    jr nz, wait_domain
    ret

wait_ipd:
    call wait_plus
    ret c
    ld hl, tail_ipd
    call match_tail
    ret c
    jr nz, wait_ipd
    ret

wait_plus:
    call read_byte_timeout
    ccf
    ret c
    cp '+'
    jr nz, wait_plus
    ret

match_tail:
    ld a, (hl)
    or a
    ret z
    ld e, a
    push hl
    call read_byte_timeout
    pop hl
    ccf
    ret c
    cp e
    jr nz, match_mismatch
    inc hl
    jr match_tail
match_mismatch:
    ld a, 1
    or a
    ret

read_domain_ip:
    ld hl, ip_buf
    ld c, 15
read_domain_ip_loop:
    push hl
    push bc
    call read_byte_timeout
    pop bc
    pop hl
    ccf
    ret c
    cp ' '
    jr c, read_domain_ip_done
    ld (hl), a
    inc hl
    dec c
    jr nz, read_domain_ip_loop
    push hl
    call read_byte_timeout
    pop hl
    ccf
    ret c
    cp ' '
    ccf
    ret c
read_domain_ip_done:
    ld (hl), 0
    or a
    ret

wait_ok:
    call read_byte_timeout
    ccf
    ret c
    cp 'O'
    jr nz, wait_ok
    call read_byte_timeout
    ccf
    ret c
    cp 'K'
    jr nz, wait_ok
    ret

wait_char:
    ld e, a
wait_char_loop:
    call read_byte_timeout
    ccf
    ret c
    cp e
    jr nz, wait_char_loop
    ret

skip_ipd_len:
    call read_byte_timeout
    ccf
    ret c
    cp ':'
    jr nz, skip_ipd_len
    ret

read_byte_timeout:
    ld b, 250
rbt_frame:
    ld c, 16
rbt_poll:
    push bc
    call uartRead
    pop bc
    ret c
    dec c
    jr nz, rbt_poll
    call _frame_wait
    djnz rbt_frame
    or a
    ret

convert_ntp_time:
    ld de, ntp_secs + 3
    ld hl, epoch_2018 + 3
    call cmp4
    jp m, conv_fail
    call sub4

    call apply_timezone

    ld c, 2                    ; 2018 % 4
conv_year_loop:
    ld hl, year_normal + 3
    ld a, c
    and 3
    jr nz, conv_year_cmp
    ld hl, year_leap + 3
conv_year_cmp:
    ld de, ntp_secs + 3
    call cmp4
    jp m, conv_months
    call sub4
    inc c
    jr conv_year_loop

conv_months:
    ld hl, month_normal
    ld a, c
    and 3
    jr nz, conv_month_loop
    ld hl, month_leap
conv_month_loop:
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    push hl
    ex de, hl
    ld de, ntp_secs + 3
    call cmp4
    jp m, conv_days_pop
    call sub4
    pop hl
    jr conv_month_loop
conv_days_pop:
    pop hl

conv_days:
    ld de, ntp_secs + 3
    ld hl, day_sec + 3
    call cmp4
    jp m, conv_hours
    call sub4
    jr conv_days

conv_hours:
    ld c, 0
conv_hour_loop:
    ld de, ntp_secs + 3
    ld hl, hour_sec + 3
    call cmp4
    jp m, conv_hour_done
    call sub4
    inc c
    jr conv_hour_loop
conv_hour_done:
    ld a, c
    cp 24
    jr nc, conv_fail
    ld (_time_hour), a

    ld c, 0
conv_min_loop:
    ld de, ntp_secs + 3
    ld hl, min_sec + 3
    call cmp4
    jp m, conv_min_done
    call sub4
    inc c
    jr conv_min_loop
conv_min_done:
    ld a, c
    cp 60
    jr nc, conv_fail
    ld (_time_minute), a

    ld a, (ntp_secs + 3)
    cp 60
    jr nc, conv_fail
    ld (_time_second), a
    or a
    ret
conv_fail:
    scf
    ret

apply_timezone:
    ld a, (_sntp_tz)
    or a
    ret z
    jp p, tz_add
    neg
    ld b, a
tz_sub_loop:
    push bc
    ld de, ntp_secs + 3
    ld hl, hour_sec + 3
    call sub4
    pop bc
    djnz tz_sub_loop
    ret
tz_add:
    ld b, a
tz_add_loop:
    push bc
    ld de, ntp_secs + 3
    ld hl, hour_sec + 3
    call add4
    pop bc
    djnz tz_add_loop
    ret

commit_ntp_time:
    ld a, (23672)
    ld (_last_frames_lo), a
    ld hl, 0
    ld (_tick_accum), hl
    xor a
    ld (_sntp_waiting), a
    inc a
    ld (_sntp_queried), a
    ld (_status_bar_dirty), a
    jp _draw_status_bar

cmp4:
    push de
    push hl
    ld b, 4
    or a
cmp4_loop:
    ld a, (de)
    sbc a, (hl)
    dec hl
    dec de
    djnz cmp4_loop
    pop hl
    pop de
    ret

add4:
    ld b, 4
    or a
add4_loop:
    ld a, (de)
    adc a, (hl)
    ld (de), a
    dec hl
    dec de
    djnz add4_loop
    ret

sub4:
    push de
    push hl
    ld b, 4
    or a
sub4_loop:
    ld a, (de)
    sbc a, (hl)
    ld (de), a
    dec hl
    dec de
    djnz sub4_loop
    pop hl
    pop de
    ret

cmd_cipdomain:
    DEFM "AT+CIPDOMAIN=", 34, "time.google.com", 34
    DEFB 0
cmd_cipstart_pfx:
    DEFM "AT+CIPSTART=", 34, "UDP", 34, ",", 34
    DEFB 0
cmd_cipstart_tail:
    DEFM 34, ",123"
    DEFB 0
cmd_cipsend:
    DEFM "AT+CIPSEND=60"
    DEFB 0
cmd_cipclose:
    DEFM "AT+CIPCLOSE"
    DEFB 0
tail_domain:
    DEFM "CIPDOMAIN:"
    DEFB 0
tail_ipd:
    DEFM "IPD,"
    DEFB 0
epoch_2018:
    DEFB $DD, $F3, $F8, $80
year_normal:
    DEFB $01, $E1, $33, $80
year_leap:
    DEFB $01, $E2, $85, $00
month_31:
    DEFB $00, $28, $DE, $80
month_28:
    DEFB $00, $24, $EA, $00
month_29:
    DEFB $00, $26, $3B, $80
month_30:
    DEFB $00, $27, $8D, $00
day_sec:
    DEFB $00, $01, $51, $80
hour_sec:
    DEFB $00, $00, $0E, $10
min_sec:
    DEFB $00, $00, $00, $3C

month_normal:
    DEFW month_31 + 3, month_28 + 3, month_31 + 3, month_30 + 3
    DEFW month_31 + 3, month_30 + 3, month_31 + 3, month_31 + 3
    DEFW month_30 + 3, month_31 + 3, month_30 + 3, month_31 + 3
month_leap:
    DEFW month_31 + 3, month_29 + 3, month_31 + 3, month_30 + 3
    DEFW month_31 + 3, month_30 + 3, month_31 + 3, month_31 + 3
    DEFW month_30 + 3, month_31 + 3, month_30 + 3, month_31 + 3

ntp_secs:
    DEFS 4
ip_buf:
    DEFS 16
