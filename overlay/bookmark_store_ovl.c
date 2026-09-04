/*
 * bookmark_store_ovl.c -- cold bookmark load/save/autojoin entries.
 * Linked into SPCTLK3 on Classic and SPCTLK4 on Spectranext.
 */

#include "overlay_api.h"

#define BM_LINE_MAX 160
#define BM_AUTOLOGIN 0x80

extern uint8_t bookmark_sel;
extern uint8_t bookmark_active_slot;

#ifdef SPECTALK_SPECTRANEXT
#define BM_PATH "/CFG/SPTBM1.CFG"
#define BM_PATH_SLOT 10
#else
#define BM_PATH "/SYS/CONFIG/SPTBM1.CFG"
#define BM_PATH_SLOT 17
#define BM_PATH_ALT "/SYS/SPTBM1.CFG"
#define BM_PATH_ALT_SLOT 10
#ifndef SPECTALK_NEXT
static char bm_path_buf[] = BM_PATH;
#endif
#endif
static const char bm_error[] = "Error";

static const char *bm_path(uint8_t slot) __z88dk_fastcall
{
#ifdef SPECTALK_SPECTRANEXT
    st_copy_n((char *)ring_buffer, BM_PATH, sizeof(BM_PATH));
    ring_buffer[BM_PATH_SLOT] = (uint8_t)('1' + slot);
    return (const char *)ring_buffer;
#elif defined(SPECTALK_NEXT)
    char *path = (char *)overlay_slot + BM_LINE_MAX;
    st_copy_n(path, BM_PATH, sizeof(BM_PATH));
    path[BM_PATH_SLOT] = (uint8_t)('1' + slot);
    return path;
#else
    bm_path_buf[BM_PATH_SLOT] = (uint8_t)('1' + slot);
    return bm_path_buf;
#endif
}

#ifndef SPECTALK_SPECTRANEXT
static const char *bm_path_alt(uint8_t slot) __z88dk_fastcall
{
#ifdef SPECTALK_NEXT
    char *path = (char *)overlay_slot + BM_LINE_MAX;
#else
    char *path = bm_path_buf;
#endif
    st_copy_n(path, BM_PATH_ALT, sizeof(BM_PATH_ALT));
    path[BM_PATH_ALT_SLOT] = (uint8_t)('1' + slot);
    return path;
}
#endif

static const char *bm_line(uint8_t slot) __z88dk_fastcall
{
    uint16_t n;

    esx_fopen(bm_path(slot));
#ifndef SPECTALK_SPECTRANEXT
    if (!esx_handle) esx_fopen(bm_path_alt(slot));
#endif
    if (!esx_handle) {
        input_cache_invalidate();
        return 0;
    }

    esx_buf = (uint16_t)overlay_slot;
    esx_count = BM_LINE_MAX - 1;
    esx_fread();
    n = esx_result;
    esx_fclose();
    input_cache_invalidate();
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
#ifdef SPECTALK_SPECTRANEXT
    esx_buf = (uint16_t)overlay_slot;
    esx_count = expected;
    if (!esx_replace_write(bm_path(bookmark_sel))) {
        input_cache_invalidate();
        goto err;
    }
#else
    esx_fcreate(bm_path(bookmark_sel));
    if (!esx_handle) esx_fcreate(bm_path_alt(bookmark_sel));
    if (!esx_handle) {
        input_cache_invalidate();
        goto err;
    }
    esx_buf = (uint16_t)overlay_slot;
    esx_count = expected;
    esx_fwrite();
    esx_fclose();
#endif
    input_cache_invalidate();

#ifdef SPECTALK_SPECTRANEXT
    overlay_slot[0] = 1;
#else
    overlay_slot[0] = (esx_result == expected);
    if (!overlay_slot[0]) goto err;
#endif
    reset_rx_state();
    return;
err:
    overlay_slot[0] = 0;
    ui_err(bm_error);
    reset_rx_state();
}
