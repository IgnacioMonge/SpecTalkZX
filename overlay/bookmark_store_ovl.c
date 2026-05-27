/*
 * bookmark_store_ovl.c -- cold bookmark load/save/autojoin entries.
 * Linked into SPCTLK3.OVL to keep SPCTLK8 render code under 2K.
 */

#include "overlay_api.h"

#define BM_LINE_MAX 160
#define BM_AUTOLOGIN 0x80

extern uint8_t bookmark_sel;
extern uint8_t bookmark_active_slot;

static char bm_path_buf[] = "/SYS/CONFIG/SPTBM1.CFG";
static const char bm_error[] = "Error";

static const char *bm_path(uint8_t slot) __z88dk_fastcall
{
    bm_path_buf[17] = (uint8_t)('1' + slot);
    return bm_path_buf;
}

static const char *bm_line(uint8_t slot) __z88dk_fastcall
{
    uint16_t n;

    esx_fopen(bm_path(slot));
    if (!esx_handle) return 0;

    esx_buf = (uint16_t)overlay_slot;
    esx_count = BM_LINE_MAX - 1;
    esx_fread();
    n = esx_result;
    esx_fclose();
    if (!n || n >= BM_LINE_MAX) return 0;
    overlay_slot[n] = 0;
    return (const char *)overlay_slot;
}

static const char *bm_next_field(const char *p, char *dst, uint8_t max)
{
    uint8_t n = 0;
    char c;

    max--;
    while ((c = *p) >= 32 && c != '|') {
        if (n < max) dst[n++] = c;
        p++;
    }
    dst[n] = 0;
    return (c == '|') ? p + 1 : p;
}

static uint8_t bm_apply_line(const char *p, uint8_t mode)
{
    p = bm_next_field(p, irc_server, IRC_SERVER_SIZE);
    p = bm_next_field(p, irc_port, IRC_PORT_SIZE);
    p = bm_next_field(p, irc_pass, IRC_PASS_SIZE);
    p = bm_next_field(p, autojoin_channels, SEARCH_PATTERN_SIZE);
    st_copy_n(search_pattern, autojoin_channels, SEARCH_PATTERN_SIZE);

    if (!irc_server[0]) {
        ui_err(bm_error);
        return 0;
    }
    if (mode) {
        autoconnect = 1;
        autojoin = (mode == 2);
        config_dirty = 1;
    } else {
        autojoin = (autojoin_channels[0] ? 1 : 0);
    }
    return 1;
}

static char *bm_put_field(char *p, const char *s)
{
    char *end = (char *)overlay_slot + BM_LINE_MAX - 2;
    while (*s && p < end) *p++ = *s++;
    *p++ = '|';
    return p;
}

void bookmarks_apply_ovl(void)
{
    uint8_t mode = overlay_slot[0];
    const char *p = bm_line(bookmark_sel);
    if (!p) {
        overlay_slot[0] = 0;
        ui_err(bm_error);
        reset_rx_state();
        return;
    }

    if (bm_apply_line(p, mode)) {
        if (mode) {
            bookmark_active_slot = (uint8_t)(bookmark_sel + 1);
            if (mode == 2) bookmark_active_slot |= BM_AUTOLOGIN;
        }
        overlay_slot[0] = 1;
    } else {
        overlay_slot[0] = 0;
    }
    reset_rx_state();
}

void bookmarks_save_ovl(void)
{
    char *p = (char *)overlay_slot;
    uint16_t expected;

    if (!irc_server[0]) goto err;

    p = bm_put_field(p, irc_server);
    p = bm_put_field(p, irc_port);
    p = bm_put_field(p, irc_pass);
    p = bm_put_field(p, search_pattern);
    p[-1] = '\n';

    expected = (uint16_t)(p - (char *)overlay_slot);
    esx_fcreate(bm_path(bookmark_sel));
    if (!esx_handle) goto err;
    esx_buf = (uint16_t)overlay_slot;
    esx_count = expected;
    esx_fwrite();
    esx_fclose();

    overlay_slot[0] = (esx_result == expected);
    if (!overlay_slot[0]) goto err;
    reset_rx_state();
    return;
err:
    overlay_slot[0] = 0;
    ui_err(bm_error);
    reset_rx_state();
}
