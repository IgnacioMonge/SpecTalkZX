#include <stdint.h>

#include "spxn_rom.h"

#define RX_LINE_SIZE 512u
#define TZ_RTC 127
#define NTP_PORT 123u
#define NTP_PACKET_SIZE 48u
#define NTP_TX_SECONDS 40u
#define POLLIN 4u
#define POLLNVAL 128u

struct endpoint {
    uint8_t ip4be[4];
    uint16_t port;
    uint16_t local_port;
};

extern uint8_t overlay_slot[];
extern int8_t sntp_tz;
extern int8_t sntp_tz_last;
extern uint8_t sntp_waiting;
extern uint8_t sntp_queried;
extern uint8_t time_hour;
extern uint8_t time_minute;
extern uint8_t time_second;
extern uint8_t last_frames_lo;
extern uint16_t tick_accum;
extern int16_t spxn_resolve(const char *name, uint8_t *ip4be) __z88dk_callee;
extern void frame_wait(void);
extern void draw_status_bar(void);

static void close_socket(uint8_t fd)
{
    spxn_regs.a = fd;
    (void)spxn_rom_hlcall(ROM_CLOSE);
}

static uint8_t same_host(const struct endpoint *a, const struct endpoint *b)
{
    uint8_t i;

    if (a->port != b->port) return 0u;
    for (i = 0u; i != 4u; ++i)
        if (a->ip4be[i] != b->ip4be[i]) return 0u;
    return 1u;
}

static uint8_t ge24(const uint8_t *value, uint8_t hi, uint8_t mid, uint8_t lo)
{
    if (value[2] != hi) return value[2] > hi;
    if (value[1] != mid) return value[1] > mid;
    return value[0] >= lo;
}

static void sub24(uint8_t *value, uint8_t hi, uint8_t mid, uint8_t lo)
{
    uint8_t borrow = value[0] < lo;
    uint8_t sub = mid + borrow;

    value[0] -= lo;
    borrow = value[1] < sub;
    value[1] -= sub;
    value[2] -= hi + borrow;
}

static void decode_time(const uint8_t *stamp, uint8_t *hour,
                        uint8_t *minute, uint8_t *second)
{
    uint8_t remainder[3] = { 0u, 0u, 0u };
    uint8_t byte;
    uint8_t bit;

    for (byte = 0u; byte != 4u; ++byte) {
        for (bit = 0x80u; bit; bit >>= 1) {
            remainder[2] = (remainder[2] << 1) | (remainder[1] >> 7);
            remainder[1] = (remainder[1] << 1) | (remainder[0] >> 7);
            remainder[0] = (remainder[0] << 1) | !!(stamp[byte] & bit);
            if (ge24(remainder, 0x01u, 0x51u, 0x80u))
                sub24(remainder, 0x01u, 0x51u, 0x80u); /* 86400 */
        }
    }
    *hour = 0u;
    while (ge24(remainder, 0u, 0x0Eu, 0x10u)) {
        sub24(remainder, 0u, 0x0Eu, 0x10u); /* 3600 */
        ++*hour;
    }
    *minute = 0u;
    while (ge24(remainder, 0u, 0u, 60u)) {
        sub24(remainder, 0u, 0u, 60u);
        ++*minute;
    }
    *second = remainder[0];
}

static uint8_t fetch_utc(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    struct endpoint destination;
    struct endpoint source;
    uint16_t ticks = 250u;
    uint16_t length;
    uint8_t fd;
    uint8_t flags;
    uint8_t i;

    if (spxn_resolve("pool.ntp.org", destination.ip4be) != SPXN_OK) return 0u;
    destination.port = NTP_PORT;
    destination.local_port = 0u;
    for (i = 0u; i != NTP_PACKET_SIZE; ++i) overlay_slot[i] = 0u;
    overlay_slot[0] = 0x23u;

    spxn_regs.bc = SOCK_DGRAM;
    if (spxn_rom_hlcall(ROM_SOCKET) & ROM_CARRY) return 0u;
    fd = spxn_regs.a;
    spxn_regs.a = fd;
    spxn_regs.de = (uint16_t)(uintptr_t)overlay_slot;
    spxn_regs.bc = NTP_PACKET_SIZE;
    spxn_regs.hl = (uint16_t)(uintptr_t)&destination;
    if ((spxn_rom_ixcall(ROM_SENDTO) & ROM_CARRY) ||
        spxn_regs.bc != NTP_PACKET_SIZE) {
        close_socket(fd);
        return 0u;
    }

    while (ticks--) {
        spxn_regs.a = fd;
        flags = spxn_rom_hlcall(ROM_POLLFD);
        if (flags & ROM_CARRY) break;
        if (!(flags & ROM_ZERO)) {
            flags = (uint8_t)spxn_regs.bc;
            if (flags & POLLNVAL) break;
            if (flags & POLLIN) {
                spxn_regs.a = fd;
                spxn_regs.de = (uint16_t)(uintptr_t)overlay_slot;
                spxn_regs.bc = RX_LINE_SIZE;
                spxn_regs.hl = (uint16_t)(uintptr_t)&source;
                if (spxn_rom_ixcall(ROM_RECVFROM) & ROM_CARRY) break;
                length = spxn_regs.bc;
                close_socket(fd);
                if (length < NTP_PACKET_SIZE || !same_host(&source, &destination) ||
                    (overlay_slot[0] & 0xC0u) == 0xC0u ||
                    ((overlay_slot[0] & 7u) != 4u &&
                     (overlay_slot[0] & 7u) != 5u) ||
                    !overlay_slot[1] || overlay_slot[1] > 15u ||
                    !(overlay_slot[NTP_TX_SECONDS] |
                      overlay_slot[NTP_TX_SECONDS + 1u] |
                      overlay_slot[NTP_TX_SECONDS + 2u] |
                      overlay_slot[NTP_TX_SECONDS + 3u])) return 0u;

                decode_time(overlay_slot + NTP_TX_SECONDS, hour, minute, second);
                return 1u;
            }
        }
        frame_wait();
    }
    close_socket(fd);
    return 0u;
}

void spectranext_clock_ovl(void)
{
    int16_t hour;
    int8_t timezone;
    uint8_t utc_hour;
    uint8_t utc_minute;
    uint8_t utc_second;

    sntp_waiting = 1u;
    if (!fetch_utc(&utc_hour, &utc_minute, &utc_second)) {
        sntp_waiting = 0u;
        return;
    }
    timezone = sntp_tz == TZ_RTC ? sntp_tz_last : sntp_tz;
    hour = (int16_t)utc_hour + timezone;
    if (hour < 0) hour += 24;
    else if (hour >= 24) hour -= 24;
    time_hour = (uint8_t)hour;
    time_minute = utc_minute;
    time_second = utc_second;
    last_frames_lo = *(volatile uint8_t *)23672;
    tick_accum = 0u;
    sntp_waiting = 0u;
    sntp_queried = 1u;
    draw_status_bar();
}
