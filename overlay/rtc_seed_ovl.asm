;; rtc_seed_ovl.asm -- cold startup RTC detection.
;; Entry 1 in SPCTLK5.OVL. Tries the documented esxDOS/Next driver RTC API,
;; then M_GETDATE, then a last-resort direct DivTIESUS PCF8563 read.

SECTION code_user

PUBLIC _rtc_seed_ovl

EXTERN _overlay_slot
EXTERN _sntp_tz
EXTERN _sntp_tz_last
EXTERN _time_hour
EXTERN _time_minute
EXTERN _time_second
EXTERN _last_frames_lo
EXTERN _tick_accum

DEFC ZXUNOADDR  = $FC3B
DEFC M_GETDATE  = $8E
DEFC M_DRVAPI   = $92
DEFC I2CREG     = $F8
DEFC I2CADDR_W  = $A2
DEFC I2CADDR_R  = $A3
DEFC SCL0SDA0   = 0
DEFC SCL0SDA1   = 1
DEFC SCL1SDA0   = 2
DEFC SCL1SDA1   = 3
DEFC TZ_RTC     = 127

_rtc_seed_ovl:
    call rtc_try_drvapi
    jr nc, rtc_seed_ok
rtc_try_getdate_seed:
    call rtc_try_getdate
    jr nc, rtc_seed_ok
rtc_try_pcf_seed:
    call rtc_try_pcf8563
    jr c, rtc_seed_fail

rtc_seed_ok:
    ld a, (_sntp_tz)
    cp TZ_RTC
    ret nz
    ld hl, _overlay_slot + 2
    ld a, (hl)
    ld (_time_hour), a
    dec hl
    ld a, (hl)
    ld (_time_minute), a
    dec hl
    ld a, (hl)
    ld (_time_second), a
    ld a, (23672)
    ld (_last_frames_lo), a
    ld hl, 0
    ld (_tick_accum), hl
    ret

rtc_seed_fail:
    ld a, (_sntp_tz_last)
    ld (_sntp_tz), a
    ret

; --- esxDOS / NextZXOS RTC paths --------------------------------------------

rtc_try_drvapi:
    push iy
    push ix
    ld iy, $5C3A
    xor a
    ld b, a                   ; B=0: query RTC
    ld c, a                   ; C=0: driver API
    ld d, a
    ld e, a
    ld h, a
    ld l, a
    rst 8
    defb M_DRVAPI             ; M_DRVAPI C=0/B=0: RTC query
    jr rtc_esx_epilogue

rtc_try_getdate:
    push iy
    push ix
    ld iy, $5C3A
    rst 8
    defb M_GETDATE            ; BC=date, DE=time in MS-DOS format

rtc_esx_epilogue:
    pop ix
    pop iy
    ret c

; Single-pass MS-DOS date/time decode: validates and stores hour/min/sec.
; Returns CF=0 on success, CF=1 on failure (date/time out of range).
; B,C = MS-DOS date encoding. D,E = MS-DOS time encoding.
; Stores hour at _overlay_slot+2, minute at +1, seconds at +0.
rtc_try_msdos_regs:
rtc_decode_msdos:
    ld a, b
    cp 88                     ; (2024 - 1980) << 1
    jr c, rtc_fail
    cp 112                    ; (2036 - 1980) << 1
    jr nc, rtc_fail
    ld a, c
    and 31                    ; day
    jr z, rtc_fail
    ld a, b
    and 1
    add a, a
    add a, a
    add a, a
    ld b, a
    ld a, c
    and $E0
    rlca
    rlca
    rlca
    or b                      ; month
    jr z, rtc_fail
    cp 13
    jr nc, rtc_fail

    ld a, d                   ; hour = D >> 3
    and $F8
    rrca
    rrca
    rrca
    cp 24
    jr nc, rtc_fail
    ld (_overlay_slot + 2), a

    ld a, d                   ; minute = ((D & 7) << 3) | (E >> 5)
    and 7
    add a, a
    add a, a
    add a, a
    ld b, a
    ld a, e
    and $E0
    rlca
    rlca
    rlca
    or b
    cp 60
    jr nc, rtc_fail
    ld (_overlay_slot + 1), a

    ld a, e                   ; seconds/2 must be 0..29
    and 31
    cp 30
    jr nc, rtc_fail
    add a, a                  ; seconds = (seconds/2) * 2; CF stays clear
    ld (_overlay_slot), a
    ret

rtc_fail:
    scf
    ret

; --- DivTIESUS PCF8563 direct path ------------------------------------------

rtc_try_pcf8563:
    ld a, 1
rtc_pcf_mask_loop:
    ld (i2c_rx_mask + 1), a
    push af
    call rtc_try_pcf8563_mask
    jr nc, rtc_pcf_mask_ok
    pop af
    add a, a
    jr nc, rtc_pcf_mask_loop
    scf
    ret
rtc_pcf_mask_ok:
    pop af
    or a
    ret

rtc_try_pcf8563_mask:
    ld bc, ZXUNOADDR
    ld a, I2CREG
    out (c), a
    inc b                     ; BC = $FD3B, I2C data register
    di

    call i2c_start
    ld a, I2CADDR_W
    call i2c_send_byte
    ld a, 2                   ; PCF8563 VL_seconds register
    call i2c_send_byte

    call i2c_start            ; repeated START for read
    ld a, I2CADDR_R
    call i2c_send_byte

    ld hl, _overlay_slot
    ld e, 6
rtc_pcf_read_ack:
    call i2c_read_byte
    ld (hl), a
    inc hl
    call i2c_ack
    dec e
    jr nz, rtc_pcf_read_ack

    call i2c_read_byte
    ld (hl), a
    call i2c_nack
    call i2c_stop
    ei                        ; restore interrupts after I2C bit-bang
    jp pcf_validate_commit

i2c_start:
    ld a, SCL1SDA1
    out (c), a
    ld a, SCL1SDA0
    out (c), a
    ret

i2c_stop:
    ld a, SCL0SDA0
    out (c), a
    ld a, SCL1SDA0
    out (c), a
    ld a, SCL1SDA1
    out (c), a
    ret

i2c_send_byte:
    scf
i2c_tx_bit:
    adc a, a
    jr z, i2c_nack            ; release SDA and clock ACK, like DivTIESUS .ntpdate
    call c, i2c_nack
    call nc, i2c_ack
    and a
    jr i2c_tx_bit

i2c_nack:
    ld d, SCL0SDA1            ; D = 1 (SCL low, SDA high)
    jr i2c_clock_pulse
i2c_ack:
    ld d, SCL0SDA0            ; D = 0 (SCL low, SDA low)
i2c_clock_pulse:
    out (c), d                ; SCL0 + SDA
    set 1, d                  ; bit 1 = SCL high
    out (c), d                ; SCL1 + SDA
    res 1, d                  ; bit 1 = SCL low
    out (c), d                ; SCL0 + SDA
    ret

i2c_read_byte:
    push hl
    push de
    ld e, 8
    ld h, 0
i2c_rx_bit:
    ld d, SCL0SDA1
    out (c), d
    ld d, SCL1SDA1
    out (c), d
    in d, (c)
    ld a, d
i2c_rx_mask:
    and 1
    add a, $ff
    rl h
    ld d, SCL0SDA1
    out (c), d
    dec e
    jr nz, i2c_rx_bit
    ld a, h
    pop de
    pop hl
    ret

pcf_validate_commit:
    ld hl, _overlay_slot
    ld a, (hl)                ; seconds: VL bit + BCD seconds
    bit 7, a
    jr nz, pcf_fail
    and $7F
    call bcd_to_bin
    ret c
    cp 60
    jr nc, pcf_fail
    ld (_overlay_slot), a

    inc hl
    ld a, (hl)                ; minutes
    and $7F
    call bcd_to_bin
    ret c
    cp 60
    jr nc, pcf_fail
    ld (_overlay_slot + 1), a

    inc hl
    ld a, (hl)                ; hours
    and $3F
    call bcd_to_bin
    ret c
    cp 24
    jr nc, pcf_fail
    ld (_overlay_slot + 2), a

    inc hl
    ld a, (hl)                ; day
    and $3F
    call bcd_to_bin
    ret c
    or a
    jr z, pcf_fail
    cp 32
    jr nc, pcf_fail

    inc hl                    ; skip weekday
    inc hl
    ld a, (hl)                ; month, century bit ignored
    and $1F
    call bcd_to_bin
    ret c
    or a
    jr z, pcf_fail
    cp 13
    jr nc, pcf_fail

    inc hl
    ld a, (hl)                ; year 24..35 -> 2024..2035
    call bcd_to_bin
    ret c
    cp 24
    jr c, pcf_fail
    cp 36
    jr nc, pcf_fail

    or a
    ret

pcf_fail:
bcd_fail:
    scf
    ret

bcd_to_bin:
    ld b, a
    and $0F
    cp 10
    jr nc, bcd_fail
    ld c, a
    ld a, b
    and $F0
    rrca
    rrca
    rrca
    rrca
    ld b, a
    add a, a                  ; tens * 2
    add a, a                  ; tens * 4
    add a, b                  ; tens * 5
    add a, a                  ; tens * 10
    add a, c
    or a
    ret
