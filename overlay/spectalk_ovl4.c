/*
 * spectalk_ovl4.c — Status + Config Save overlay for SpecTalkZX
 * Loaded into ring_buffer from SPCTLK4.OVL on demand.
 *
 * Entry 0: status_render_ovl
 * Entry 1: save_config_ovl
 */

#include "overlay_api.h"

static const char s_exit[] = "ANY KEY TO EXIT";

/* ================================================================
 * ENTRY 0 — Status display
 * ================================================================ */

extern uint8_t connection_state;
extern uint8_t channels[];
extern char    network_name[];
extern uint8_t ping_latency;
extern uint16_t uptime_minutes;
#define MAX_CHANNELS    10
#define CH_SIZE         32
#define CH_FLAG_ACTIVE  0x01
#define CH_FLAG_QUERY   0x02
#define CH_FLAGS_OFF    30
#define STATE_DISCONNECTED  0
#define STATE_WIFI_OK       1
#define STATE_TCP_CONNECTED 2
#define STATE_IRC_READY     3

static const char ss_nick[]  = "Nick:";
static const char ss_srv[]   = "Server:";
static const char ss_net[]   = "Network:";
static const char ss_state[] = "State:";
static const char ss_lag[]   = "Latency:";
static const char ss_up[]    = "Uptime:";
static const char ss_chans[] = "Channels:";

void status_render_ovl(void)
{
    uint8_t r = overlay_header("Status");
    uint8_t a_nick = theme_attrs[TATTR_MSG_NICK];
    uint8_t a_chan  = theme_attrs[TATTR_MSG_CHAN];
    uint8_t i;

    print_str64(r, 2, ss_nick, a_nick);
    print_str64(r++, 11, irc_nick[0] ? (const char *)irc_nick : "(none)", a_chan);

    print_str64(r, 2, ss_srv, a_nick);
    print_str64(r++, 11, irc_server[0] ? (const char *)irc_server : "(none)", a_chan);

    print_str64(r, 2, ss_net, a_nick);
    print_str64(r++, 11, network_name[0] ? (const char *)network_name : "-", a_chan);

    print_str64(r, 2, ss_state, a_nick);
    { const char *st;
      switch (connection_state) {
          case STATE_IRC_READY:     st = "IRC ready"; break;
          case STATE_TCP_CONNECTED: st = "TCP"; break;
          case STATE_WIFI_OK:       st = "WiFi OK"; break;
          default:                  st = "Offline"; break;
      }
      print_str64(r++, 11, st, a_chan);
    }

    /* Latency */
    print_str64(r, 2, ss_lag, a_nick);
    { const char *lag;
      switch (ping_latency) {
          case 0:  lag = "Good"; break;
          case 1:  lag = "Medium"; break;
          default: lag = "High"; break;
      }
      print_str64(r++, 11, lag, a_chan);
    }

    /* Uptime */
    print_str64(r, 2, ss_up, a_nick);
    { char ubuf[8];
      uint16_t m = uptime_minutes;
      uint8_t h = 0;
      while (m >= 60) { m -= 60; h++; }
      fast_u8_to_str(ubuf, h);
      ubuf[2] = 'h'; ubuf[3] = ' ';
      fast_u8_to_str(ubuf + 4, (uint8_t)m);
      ubuf[6] = 'm'; ubuf[7] = 0;
      print_str64(r++, 11, ubuf, a_chan);
    }

    r++; /* blank line before channels */
    print_str64(r++, 2, ss_chans, a_nick);
    { uint8_t rl = r, rr = r;  /* two-column row counters */
      for (i = 0; i < MAX_CHANNELS; i++) {
        uint8_t *ch = (uint8_t *)channels + i * CH_SIZE;
        uint8_t flags = ch[CH_FLAGS_OFF];
        if (flags & CH_FLAG_ACTIVE) {
            char idx[4];
            uint8_t attr = (flags & CH_FLAG_QUERY) ? theme_attrs[TATTR_MSG_TIME] : a_chan;
            idx[0] = ' '; idx[1] = '0' + i; idx[2] = '.'; idx[3] = 0;
            if (i < 5) {
                print_str64(rl, 2, idx, a_nick);
                print_str64(rl++, 6, (const char *)ch, attr);
            } else {
                print_str64(rr, 33, idx, a_nick);
                print_str64(rr++, 37, (const char *)ch, attr);
            }
        }
      }
    }

    notif_center(s_exit, theme_attrs[TATTR_MSG_SYS]);
    rb_head = 0; rb_tail = 0; rx_pos = 0;
}

/* ================================================================
 * ENTRY 1 — Config save to SD
 * Uses overlay_slot (512B) as write buffer.
 * ================================================================ */

extern char *cfg_put(char *p, const char *s) __z88dk_callee;
extern char *cfg_kv(char *p, const char *key, const char *val) __z88dk_callee;
extern void esx_fcreate(const char *path) __z88dk_fastcall;
extern void esx_fwrite(void);
extern char *u16_to_dec(char *dst, uint16_t v);
extern void main_puts(const char *s) __z88dk_fastcall;
extern void main_print(const char *s) __z88dk_fastcall;
extern void set_attr_sys(void);
extern void ui_err(const char *s) __z88dk_fastcall;

static const char CK_HDR[]  = "; SpecTalkZX config\r\n";
static const char CK_SRV[]  = "server=";
static const char CK_PORT[] = "port=";
static const char CK_NICK[] = "nick=";
static const char CK_PASS[] = "pass=";
static const char CK_NKP[]  = "nickpass=";
static const char CK_NKS[]  = "nickserv=";
static const char CK_THM[]  = "theme=";
static const char CK_BEEP[] = "beep=";
static const char CK_NCOL[] = "nickcolor=";
static const char CK_TRAF[] = "traffic=";
static const char CK_TS[]   = "timestamps=";
static const char CK_ACON[] = "autoconnect=";
static const char CK_NTIF[] = "notif=";
static const char CK_AWAY[] = "autoaway=";
static const char CK_TZ[]   = "tz=";
static const char CK_PRI[]  = "/SYS/CONFIG/SPECTALK.CFG";
static const char CK_ALT[]  = "/SYS/SPECTALK.CFG";

static char *cfg_put_csv(char *p, const char *key,
                         const char *list, uint8_t elem_size, uint8_t count) __z88dk_callee
{
    uint8_t i, any = 0;
    const char *s = list;
    for (i = 0; i < count; i++, s += elem_size) {
        if (!*s) continue;
        if (!any) { p = cfg_put(p, key); *p++ = '='; any = 1; }
        else *p++ = ',';
        p = cfg_put(p, s);
    }
    if (any) { *p++ = '\r'; *p++ = '\n'; }
    return p;
}

void save_config_ovl(void)
{
    char *p = (char *)overlay_slot;
    char tmp[4];

    p = cfg_put(p, CK_HDR);

    if (irc_server[0])    p = cfg_kv(p, CK_SRV, irc_server);
    if (irc_port[0])      p = cfg_kv(p, CK_PORT, irc_port);
    if (irc_nick[0])      p = cfg_kv(p, CK_NICK, irc_nick);
    if (irc_pass[0])      p = cfg_kv(p, CK_PASS, irc_pass);
    if (nickserv_pass[0]) p = cfg_kv(p, CK_NKP, nickserv_pass);
    if (nickserv_nick[0]) p = cfg_kv(p, CK_NKS, nickserv_nick);

    /* W15: cfg_kv small-int trick — values 0-9 passed as (const char*)(uint16_t)N.
     * cfg_kv ASM detects D==0 && E<10 and writes single ASCII digit.
     * CONSTRAINT: all values below MUST be 0-9. */
    p = cfg_kv(p, CK_THM, (const char *)(uint16_t)current_theme);
    p = cfg_kv(p, CK_BEEP, (const char *)(uint16_t)beep_enabled);
    p = cfg_kv(p, "click=", (const char *)(uint16_t)keyclick_enabled);
    p = cfg_kv(p, CK_NCOL, (const char *)(uint16_t)nick_color_mode);
    p = cfg_kv(p, CK_TRAF, (const char *)(uint16_t)show_traffic);
    p = cfg_kv(p, CK_TS, (const char *)(uint16_t)show_timestamps);
    p = cfg_kv(p, CK_ACON, (const char *)(uint16_t)autoconnect);
    p = cfg_kv(p, CK_NTIF, (const char *)(uint16_t)notif_enabled);

    if (autoaway_minutes) {
        fast_u8_to_str(tmp, autoaway_minutes); tmp[2] = 0;
        p = cfg_kv(p, CK_AWAY, tmp);
    }

    p = cfg_put(p, CK_TZ);
    if (sntp_tz < 0) {
        *p++ = '-';
        fast_u8_to_str(tmp, (uint8_t)(-sntp_tz));
    } else {
        fast_u8_to_str(tmp, (uint8_t)sntp_tz);
    }
    tmp[2] = 0;
    p = cfg_put(p, tmp); *p++ = '\r'; *p++ = '\n';

    if (p < (char *)overlay_slot + 500) /* W1 guard: leave room for CSV */
        p = cfg_put_csv(p, "friends", (const char *)friend_nicks, IRC_NICK_SIZE, MAX_FRIENDS);
    if (p < (char *)overlay_slot + 500)
        p = cfg_put_csv(p, "ignores", (const char *)ignore_list, 16, ignore_count);

    set_attr_sys();
    main_puts("Saving config... ");

    esx_fcreate(CK_PRI);
    if (!esx_handle) esx_fcreate(CK_ALT);
    if (!esx_handle) { ui_err("Cannot write config"); goto done; }

    esx_buf = (uint16_t)overlay_slot;
    esx_count = (uint16_t)(p - (char *)overlay_slot);
    {
        uint16_t expected = esx_count;
        uint8_t write_ok;
        esx_fwrite();
        write_ok = (esx_result == expected);
        esx_fclose();
        input_cache_invalidate();

        if (!write_ok) {
            ui_err("Write error");
        } else {
            char nbuf[6];
            main_puts("OK (");
            u16_to_dec(nbuf, expected);
            main_puts(nbuf);
            main_print(" bytes)");
            config_dirty = 0;
        }
    }

done:
    rb_head = 0; rb_tail = 0; rx_pos = 0;
}
