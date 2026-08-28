/* Spectranext clock acquisition through the reusable UDP/SNTP service. */

#include "../include/spectalk.h"
#define CLOCK_RETRY_FRAMES 3000u

static uint16_t clock_retry_frames;

void clock_seed_local(void)
{
    if (sntp_tz == TZ_RTC) sntp_tz = sntp_tz_last;
}

static void clock_fetch(void)
{
    if (connection_state != STATE_WIFI_OK || clock_synced) return;
    if (overlay_mode != OVERLAY_NONE) return;
    overlay_exec(5, 0);
    reset_rx_state();
}

void clock_init(void)
{
    if (clock_synced) return;
    if (clock_retry_frames) {
        clock_retry_frames--;
        return;
    }
    clock_fetch();
    clock_retry_frames = CLOCK_RETRY_FRAMES;
}

void clock_query(void)
{
    clock_fetch();
}

uint8_t clock_poll_rx(void)
{
    return 0;
}

void clock_sync_fallback(void)
{
    clock_retry_frames = 0;
    clock_fetch();
}
