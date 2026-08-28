/* Classic RTC + ESP SNTP clock acquisition backend. */

#include "../include/spectalk.h"

const char S_AT_SNTPTIME[] = "AT+CIPSNTPTIME?";
static const char S_SNTP_CFG[] = "AT+CIPSNTPCFG=1,";
static const char S_NTP_POOL[] = ",\"pool.ntp.org\"";

void classic_clock_init(void) ST_NAKED
{
    __asm
    ld      a, (_sntp_tz)
    cp      TZ_RTC
    ret     z
    ld      a, (_sntp_init_sent)
    or      a
    ret     nz
    ld      a, (_connection_state)
    cp      STATE_WIFI_OK
    ret     nz

    ld      hl, _S_SNTP_CFG
    call    _uart_send_string
    ld      a, (_sntp_tz)
    or      a
    jp      p, classic_clock_init_abs
    ld      l, '-'
    call    _ay_uart_send
    ld      a, (_sntp_tz)
    neg

classic_clock_init_abs:
    cp      10
    jr      c, classic_clock_init_one_digit
    sub     10
    ld      e, a
    ld      l, '1'
    call    _ay_uart_send
    ld      a, e

classic_clock_init_one_digit:
    add     a, '0'
    ld      l, a
    call    _ay_uart_send
    ld      hl, _S_NTP_POOL
    call    _uart_send_line
    ld      a, 1
    ld      (_sntp_init_sent), a
    xor     a
    ld      (_sntp_waiting), a
    ret
    __endasm;
}

void classic_clock_query(void) ST_NAKED
{
    __asm
    ld      a, (_sntp_init_sent)
    dec     a
    ret     nz
    ld      a, (_sntp_waiting)
    or      a
    ret     nz
    ld      a, (_connection_state)
    cp      STATE_WIFI_OK
    ret     nz
    ld      hl, _S_AT_SNTPTIME
    call    _uart_send_line
    ld      a, 1
    ld      (_sntp_waiting), a
    ret
    __endasm;
}

uint8_t classic_clock_poll_rx(void)
{
    if (connection_state != STATE_WIFI_OK || !sntp_waiting) return 0;

    net_pump_rx();
    while (try_read_line_nodrain()) {
        if (rx_last_len >= 1 && rx_line[0] == '+') {
            sntp_process_response(rx_line);
        }
        if (rx_line[0] == 'E') sntp_init_sent = 2;
        if (rx_line[0] == 'O' || rx_line[0] == 'E') sntp_waiting = 0;
    }
    return 1;
}
