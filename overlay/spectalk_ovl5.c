/*
 * spectalk_ovl5.c — Config overlay for SpecTalkZX.
 * Loaded into ring_buffer from SPCTLK5.OVL on demand.
 *
 * Entry 0: config_render_ovl
 */

#include "overlay_api.h"

static const char cl_nkp[]   = "nick pass=";
static const char cl_ncol[]  = "nick color=";
static const char cl_away[]  = "auto away=";
static const char cl_tz[]    = "timezone=";

static const char cv_on[]     = "on";
static const char cv_off[]    = "off";
static const char cv_set[]    = "set";
static const char cv_notset[] = "(not set)";
static const char cv_smart[]  = "smart";

static uint8_t cfg_col;

extern void rtc_seed_ovl(void);
extern void reset_rx_state(void);

void rtc_enable_ovl(void)
{
    int8_t old_tz = (sntp_tz == TZ_RTC) ? sntp_tz_last : sntp_tz;
    sntp_tz_last = old_tz;
    sntp_tz = TZ_RTC;
    rtc_seed_ovl();
    if (sntp_tz == TZ_RTC) {
        sntp_init_sent = 0;
        sntp_waiting = 0;
        sntp_queried = 0;
        sys_puts_print(K_TZ, "RTC");
        status_bar_dirty = 1;
        config_dirty = 1;
    } else {
        sntp_tz = old_tz;
        ui_err("No RTC");
    }
    reset_rx_state();
}

static void cfg_item(const char *label, const char *val)
{
    uint8_t r = g_ps64_y;
    uint8_t col = cfg_col;
    uint8_t a_nick = theme_attrs[TATTR_MSG_NICK];

    print_str64(r, col, label, a_nick);
    if (g_ps64_col & 1) ++g_ps64_col;
    {
        uint8_t a_val = (val == cv_notset) ? theme_attrs[TATTR_MSG_TIME] : theme_attrs[TATTR_MSG_CHAN];
        print_char64(r, g_ps64_col, ' ', a_val);
        print_str64(r, g_ps64_col + 1, val, a_val);
    }
    if (cfg_col == 34) { g_ps64_y++; cfg_col = 2; }
    else cfg_col = 34;
}

void config_render_ovl(void)
{
    char buf[4];

    g_ps64_y = overlay_header("Config");
    cfg_col = 2;

    cfg_item(K_NICK, irc_nick[0] ? (const char*)irc_nick : cv_notset);
    cfg_item(K_SERVER, irc_server[0] ? (const char*)irc_server : cv_notset);
    cfg_item(K_PORT, irc_port);
    cfg_item(K_PASS, irc_pass[0] ? cv_set : cv_notset);
    cfg_item(cl_nkp, nickserv_pass[0] ? cv_set : cv_notset);
    buf[0] = '0' + current_theme; buf[1] = 0;
    cfg_item(K_THEME, buf);
    cfg_item(K_BEEP, beep_enabled ? cv_on : cv_off);
    cfg_item(K_CLICK, keyclick_enabled ? cv_on : cv_off);
    cfg_item(cl_ncol, nick_color_mode ? cv_on : cv_off);
    cfg_item(K_TRAFFIC, show_traffic ? cv_on : cv_off);
    cfg_item(K_DIVIDER, show_channel_separators ? cv_on : cv_off);
    cfg_item(K_TS, show_timestamps == 0 ? cv_off :
                    show_timestamps == 1 ? cv_on : cv_smart);
    cfg_item(K_AUTOCONN, autoconnect ? cv_on : cv_off);
    cfg_item(K_AUTOJOIN, autojoin ? cv_on : cv_off);
    cfg_item(K_NOTIF, notif_enabled ? cv_on : cv_off);
    cfg_item(K_COUNTSYNC, count_sync_enabled ? cv_on : cv_off);

    if (autoaway_minutes) {
        fast_u8_to_str(buf, autoaway_minutes); buf[2] = 'm'; buf[3] = 0;
        cfg_item(cl_away, buf);
    } else {
        cfg_item(cl_away, cv_notset);
    }
    if (sntp_tz == TZ_RTC) {
        buf[0] = 'R'; buf[1] = 'T'; buf[2] = 'C'; buf[3] = 0;
    } else if (sntp_tz < 0) {
        buf[0] = '-'; fast_u8_to_str(buf + 1, (uint8_t)(-sntp_tz)); buf[3] = 0;
    } else {
        buf[0] = '+'; fast_u8_to_str(buf + 1, (uint8_t)sntp_tz); buf[3] = 0;
    }
    cfg_item(cl_tz, buf);

    if (cfg_col == 34) { g_ps64_y++; cfg_col = 2; }
    cfg_item(K_CHANNELS, (search_pattern[0] == '#' || search_pattern[0] == '&') ? (const char *)search_pattern : cv_notset);

    if (cfg_col == 34) { g_ps64_y++; cfg_col = 2; }
    {
        uint8_t i, col = 12;
        uint8_t row = g_ps64_y;
        const char *fn;
        print_str64(row, 2, "Friends:", theme_attrs[TATTR_MSG_NICK]);
        for (i = MAX_FRIENDS, fn = friend_nicks[0]; i != 0; i--, fn += IRC_NICK_SIZE) {
            if (*fn) { print_str64(row, col, fn, theme_attrs[TATTR_MSG_CHAN]); col += st_strlen(fn) + 1; }
        }
        if (col == 12) print_str64(row, col, cv_notset, theme_attrs[TATTR_MSG_TIME]);
        row++;
        col = 12;
        print_str64(row, 2, "Ignores:", theme_attrs[TATTR_MSG_NICK]);
        if (ignore_count == 0) print_str64(row, col, cv_notset, theme_attrs[TATTR_MSG_TIME]);
        else {
            const char *ign = ignore_list[0];
            for (i = ignore_count; i != 0; i--, ign += 16) {
                print_str64(row, col, ign, theme_attrs[TATTR_MSG_CHAN]);
                col += st_strlen(ign) + 1;
            }
        }
    }

    notif_center(config_dirty ? "(S)AVE TO SD | ANY KEY TO EXIT" : S_ANYKEY,
                theme_attrs[TATTR_MSG_SYS]);
    reset_rx_state();
}
