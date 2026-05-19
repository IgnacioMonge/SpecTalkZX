/*
 * bookmarks_ovl.c -- IRC session bookmark storage for SPCTLK8.OVL.
 */

#include "overlay_api.h"

#define BM_USER_SLOTS 5
#define BM_LINE_MAX 160
#define BM_FIRST_ROW 6
#define BM_LAST_ROW 19
#define BM_INDENT 4
#define BM_ACTIVE_NONE 0xff
#define BM_AUTOLOGIN 0x80
#define BM_OCCUPIED 0x80
#define BM_ROW_MASK 0x7f

extern uint8_t bookmark_sel;
extern uint8_t bookmark_active_slot;
extern uint8_t bookmark_rows[];

static char bm_path_buf[] = "/SYS/CONFIG/SPTBM1.CFG";
static const char bm_title[] = "BOOKMARKS";
static const char bm_footer[] = "ENTER:CONNECT  S:STORE  A:AUTO  D:DELETE  BREAK:SAVE/EXIT";
static const char bm_empty[] = "empty";
static const char bm_autocon[] = " (autocon";
static const char bm_autojoin[] = "/autojoin";

void bookmarks_list_ovl(void);
void bookmarks_cursor_ovl(void);

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

static uint8_t bm_server_eq(const char *p) __z88dk_fastcall
{
    const char *s = irc_server;
    if (!p) return 0;
    while (*s && *p == *s) {
        p++;
        s++;
    }
    return (!*s && *p == '|');
}

static uint8_t bm_current_active(void)
{
    uint8_t slot = bookmark_active_slot & 0x7F;
    if (slot) return (uint8_t)(slot - 1);
    if (autoconnect && irc_server[0]) {
        for (slot = 0; slot < BM_USER_SLOTS; slot++) {
            if (bm_server_eq(bm_line(slot))) {
                bookmark_active_slot = (uint8_t)(slot + 1);
                if (autojoin) bookmark_active_slot |= BM_AUTOLOGIN;
                return slot;
            }
        }
    }
    return BM_ACTIVE_NONE;
}

static const char *bm_skip_field(const char *p)
{
    while ((uint8_t)*p >= 32 && *p != '|') p++;
    if (*p == '|') p++;
    return p;
}

static void bm_server_row(uint8_t slot, uint8_t row, const char *p)
{
    char line[64];
    char *q = line;
    char *end = line + 62;
    const char *s;
    uint8_t attr = p ? theme_attrs[10] : theme_attrs[TATTR_MSG_TIME];

    clear_line(row, theme_attrs[TATTR_MAIN_BG]);

    *q++ = (slot == bookmark_sel) ? '>' : ' ';
    *q++ = (uint8_t)('1' + slot);
    *q++ = '.';
    *q++ = ' ';

    if (!p) {
        p = bm_empty;
        while (*p) *q++ = *p++;
    } else {
        while ((uint8_t)*p >= 32 && *p != '|' && q < end) *q++ = *p++;
        if (*p && *p != '|') q[-1] = '>';
        if (*p == '|') p++;
        if ((uint8_t)*p >= 32 && *p != '|' && q < end) {
            *q++ = ':';
            while ((uint8_t)*p >= 32 && *p != '|' && q < end) *q++ = *p++;
        }
        while ((uint8_t)*p >= 32 && *p != '|') p++;
        if (*p == '|') p++;
        if ((uint8_t)*p >= 32 && *p != '|' && q < end - 1) {
            *q++ = ' ';
            *q++ = 'P';
        }
        if ((bookmark_active_slot & 0x7F) == slot + 1) {
            s = bm_autocon;
            while (*s && q < end) *q++ = *s++;
            if (bookmark_active_slot & BM_AUTOLOGIN) {
                s = bm_autojoin;
                while (*s && q < end) *q++ = *s++;
            }
            if (q < end) *q++ = ')';
        }
    }
    *q = 0;
    print_str64(row, 0, line, attr);
}

static uint8_t bm_channel_rows(const char *p, uint8_t row, uint8_t last_row)
{
    char line[64];
    uint8_t n;

    if ((uint8_t)*p < 32) return row;
    while ((uint8_t)*p >= 32 && row <= last_row) {
        p = skip_spaces((char *)p);
        line[0] = ' ';
        line[1] = ' ';
        line[2] = ' ';
        line[3] = ' ';
        n = BM_INDENT;
        while ((uint8_t)*p >= 32 && n < 62) {
            line[n++] = *p++;
            if (p[-1] == ',' && n > 45) break;
        }
        line[n] = 0;
        print_str64(row++, 0, line, theme_attrs[TATTR_MSG_CHAN]);
    }
    return row;
}

static uint8_t bm_item(uint8_t slot, uint8_t row)
{
    const char *p = bm_line(slot);

    bookmark_rows[slot] = p ? (uint8_t)(row | BM_OCCUPIED) : row;
    bm_server_row(slot, row++, p);
    if (!p || row > BM_LAST_ROW) return row;
    p = bm_skip_field(p);
    p = bm_skip_field(p);
    p = bm_skip_field(p);
    return bm_channel_rows(p, row, row);
}

void bookmarks_render_ovl(void)
{
    overlay_header(bm_title);
    bookmarks_list_ovl();
    notif_center(bm_footer, theme_attrs[TATTR_MSG_SYS]);
    reset_rx_state();
}

void bookmarks_list_ovl(void)
{
    uint8_t i;
    uint8_t row = BM_FIRST_ROW;

    bm_current_active();
    clear_zone(BM_FIRST_ROW, BM_LAST_ROW - BM_FIRST_ROW + 1, theme_attrs[TATTR_MAIN_BG]);
    for (i = 0; i < BM_USER_SLOTS && row <= BM_LAST_ROW; i++)
        row = bm_item(i, row);
    reset_rx_state();
}

void bookmarks_rows_ovl(void)
{
    uint8_t prev_slot = overlay_slot[0];
    bm_current_active();
    if (prev_slot < BM_USER_SLOTS)
        bm_server_row(prev_slot, bookmark_rows[prev_slot] & BM_ROW_MASK, bm_line(prev_slot));
    if (prev_slot != bookmark_sel)
        bm_server_row(bookmark_sel, bookmark_rows[bookmark_sel] & BM_ROW_MASK, bm_line(bookmark_sel));
    reset_rx_state();
}

static void bm_cursor_char(uint8_t slot, uint8_t c)
{
    uint8_t row = bookmark_rows[slot];
    print_char64(row & BM_ROW_MASK, 0, c,
                 (row & BM_OCCUPIED) ? theme_attrs[10] : theme_attrs[TATTR_MSG_TIME]);
}

void bookmarks_cursor_ovl(void)
{
    uint8_t prev_slot = overlay_slot[0];
    if (prev_slot < BM_USER_SLOTS) bm_cursor_char(prev_slot, ' ');
    bm_cursor_char(bookmark_sel, '>');
    reset_rx_state();
}

void bookmarks_delete_ovl(void)
{
    if (!(bookmark_rows[bookmark_sel] & BM_OCCUPIED)) {
        overlay_slot[0] = 0;
        reset_rx_state();
        return;
    }

    esx_fcreate(bm_path(bookmark_sel));
    if (!esx_handle) {
        overlay_slot[0] = 0;
        ui_err("Delete error");
        reset_rx_state();
        return;
    }
    esx_fclose();
    if ((bookmark_active_slot & 0x7F) == bookmark_sel + 1) {
        bookmark_active_slot = 0;
        autoconnect = 0;
        autojoin = 0;
        autojoin_channels[0] = 0;
        search_pattern[0] = 0;
        config_dirty = 1;
    }
    overlay_slot[0] = 1;
    bookmarks_list_ovl();
}
