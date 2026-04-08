/*
 * spectalk_ovl2.c — About + Config overlay for SpecTalkZX
 * Loaded into ring_buffer from SPCTLK2.OVL on demand.
 *
 * Entry 0: about_render_ovl
 * Entry 1: config_render_ovl
 */

#include "overlay_api.h"
#include "globe_data.h"

static const char s_exit[] = "ANY KEY TO EXIT";

/* ================================================================
 * ENTRY 0 — About screen
 * ================================================================ */

static const char s_app[]  = "SpecTalkZX";
static const char s_desc[] = "IRC client for ZX Spectrum";
static const char s_conn[] = "Connecting ZX to the world since 2025!";
static const char s_ver[]  = "v" VERSION;
static const char s_copy[] = "(C) 2026 M. Ignacio Monge";
static const char s_repo[] = "github.com/IgnacioMonge/SpecTalkZX";
static const char s_lic[]  = "GPL2";
static const char s_bld[]  = "Build=";

uint8_t g_rot;              /* globe rotation — non-static for ASM access */
uint8_t globe_row;          /* base row — non-static for ASM access */

/* Globe renderer in ASM (globe_render.asm) */
extern void render_globe(void);
extern void globe_init_dither(void);

void about_render_ovl(void)
{
    uint8_t r = overlay_header(s_app);
    uint8_t a_chan = theme_attrs[TATTR_MSG_CHAN];
    uint8_t a_nick = theme_attrs[TATTR_MSG_NICK];
    uint8_t tr = r + 1;

    print_str64(tr, 2, s_desc, a_chan);
    print_str64(tr++, 30, s_ver, a_nick);
    print_str64(tr++, 2, s_copy, a_chan);
    print_str64(tr++, 2, s_repo, theme_attrs[TATTR_MSG_TIME]);
    print_str64(tr, 2, s_bld, a_nick);
    print_str64(tr, 9, __DATE__, a_chan);
    print_str64(tr, 24, s_lic, theme_attrs[TATTR_MSG_TIME]);
    tr += 3;
    print_big_str(tr, 2, s_conn, a_nick);

    notif_center("(N) What's New / Any key to exit", theme_attrs[TATTR_MSG_SYS]);

    /* First frame of globe (main loop ticks via overlay_call) */
    globe_row = r + 1;
    g_rot = 32;
    globe_init_dither();   /* one-time pixel dither (ASM) */
    render_globe();        /* first attr frame (ASM) */

    rb_head = 0; rb_tail = 0; rx_pos = 0;
}

/* ================================================================
 * ENTRY 2 — Globe animation tick (called from main loop each frame)
 * Overlay is already resident in ring_buffer — no disk I/O.
 * ================================================================ */

void globe_tick_ovl(void)
{
    g_rot = (g_rot + 1) & 63;
    render_globe();
}

/* ================================================================
 * ENTRY 1 — Config display
 * ================================================================ */

static const char cl_nick[]  = "nick=";
static const char cl_srv[]   = "server=";
static const char cl_port[]  = "port=";
static const char cl_pass[]  = "pass=";
static const char cl_nkp[]   = "nick pass=";
static const char cl_theme[] = "theme=";
static const char cl_beep[]  = "beep=";
static const char cl_click[] = "click=";
static const char cl_ncol[]  = "nick color=";
static const char cl_traf[]  = "traffic=";
static const char cl_ts[]    = "timestamps=";
static const char cl_acon[]  = "autoconnect=";
static const char cl_away[]  = "auto away=";
static const char cl_tz[]    = "timezone=";

static const char cv_on[]    = "on";
static const char cv_off[]   = "off";
static const char cv_set[]   = "set";
static const char cv_notset[]= "(not set)";
static const char cv_smart[] = "smart";

static uint8_t cfg_col;

static void cfg_item(const char *label, const char *val)
{
    uint8_t r = g_ps64_y;
    uint8_t col = cfg_col;
    uint8_t a_nick = theme_attrs[TATTR_MSG_NICK];

    print_char64(r, col, *label & 0xDF, a_nick);
    print_str64(r, col + 1, label + 1, a_nick);
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

    cfg_item(cl_nick, irc_nick[0] ? (const char*)irc_nick : cv_notset);
    cfg_item(cl_srv, irc_server[0] ? (const char*)irc_server : cv_notset);
    cfg_item(cl_port, irc_port);
    cfg_item(cl_pass, irc_pass[0] ? cv_set : cv_notset);
    cfg_item(cl_nkp, nickserv_pass[0] ? cv_set : cv_notset);
    buf[0] = '0' + current_theme; buf[1] = 0;
    cfg_item(cl_theme, buf);
    cfg_item(cl_beep, beep_enabled ? cv_on : cv_off);
    cfg_item(cl_click, keyclick_enabled ? cv_on : cv_off);
    cfg_item(cl_ncol, nick_color_mode ? cv_on : cv_off);
    cfg_item(cl_traf, show_traffic ? cv_on : cv_off);
    cfg_item(cl_ts, show_timestamps == 0 ? cv_off :
                    show_timestamps == 1 ? cv_on : cv_smart);
    cfg_item(cl_acon, autoconnect ? cv_on : cv_off);
    cfg_item("notif=", notif_enabled ? cv_on : cv_off);

    if (autoaway_minutes) {
        fast_u8_to_str(buf, autoaway_minutes); buf[2] = 'm'; buf[3] = 0;
        cfg_item(cl_away, buf);
    } else {
        cfg_item(cl_away, cv_notset);
    }
    if (sntp_tz < 0) { buf[0] = '-'; fast_u8_to_str(buf + 1, (uint8_t)(-sntp_tz)); }
    else { buf[0] = '+'; fast_u8_to_str(buf + 1, (uint8_t)sntp_tz); }
    buf[3] = 0;
    cfg_item(cl_tz, buf);

    if (cfg_col == 34) { g_ps64_y++; cfg_col = 2; }
    {
        uint8_t i, col = 12;
        uint8_t row = g_ps64_y;
        char *fn;
        print_str64(row, 2, "Friends:", theme_attrs[TATTR_MSG_NICK]);
        for (i = 0, fn = friend_nicks[0]; i < MAX_FRIENDS; i++, fn += IRC_NICK_SIZE) {
            if (*fn) { print_str64(row, col, fn, theme_attrs[TATTR_MSG_CHAN]); col += st_strlen(fn) + 1; }
        }
        if (col == 12) print_str64(row, col, cv_notset, theme_attrs[TATTR_MSG_TIME]);
        row++;
        col = 12;
        print_str64(row, 2, "Ignores:", theme_attrs[TATTR_MSG_NICK]);
        if (ignore_count == 0) print_str64(row, col, cv_notset, theme_attrs[TATTR_MSG_TIME]);
        else for (i = 0; i < ignore_count; i++) {
            print_str64(row, col, ignore_list[i], theme_attrs[TATTR_MSG_CHAN]);
            col += st_strlen(ignore_list[i]) + 1;
        }
    }

    notif_center(config_dirty ? "(S)AVE TO SD | ANY KEY TO EXIT" : s_exit,
                theme_attrs[TATTR_MSG_SYS]);
    rb_head = 0; rb_tail = 0; rx_pos = 0;
}

/* Banner moved to SPCTLK1.OVL entry 1 to make room for globe */
