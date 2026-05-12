/*
 * overlay_api.h — Minimal ABI for overlay C code
 * Declarations match the resident binary's actual prototypes.
 * Addresses resolved at link time via overlay_defs.asm (gen_overlay_defs.py).
 *
 * DO NOT include spectalk.h — this header is self-contained.
 */

#ifndef OVERLAY_API_H
#define OVERLAY_API_H

#include <stdint.h>

#define TZ_RTC 127

/* ===== Resident wrappers ===== */

extern uint8_t overlay_header(const char *title) __z88dk_fastcall;

/* ===== Rendering primitives ===== */

extern void print_str64(uint8_t y, uint8_t col, const char *s, uint8_t attr)
    __z88dk_callee;
extern void print_char64(uint8_t y, uint8_t col, uint8_t c, uint8_t attr)
    __z88dk_callee;
extern void clear_line(uint8_t y, uint8_t attr);
extern void print_big_str(uint8_t y, uint8_t col, const char *s, uint8_t attr)
    __z88dk_callee;
extern void print_line64_fast(uint8_t y, const char *s, uint8_t attr);
extern void draw_badge_dither(uint8_t count) __z88dk_fastcall;
extern void notif_draw(uint8_t start_col, const char *str, uint8_t attr);
extern void notif_center(const char *str, uint8_t attr);
extern void input_cache_invalidate(void);
extern void main_putc(char c) __z88dk_fastcall;
extern void main_puts(const char *s) __z88dk_fastcall;
extern void main_print(const char *s) __z88dk_fastcall;
extern void main_newline(void);
extern void set_attr_sys(void);
extern void set_attr_priv(void);
extern void sys_puts_print(const char *label, const char *value) __z88dk_callee;
extern void notify2(const char *a, const char *b, uint8_t attr) __z88dk_callee;
extern void ui_err(const char *s) __z88dk_fastcall;
extern void ui_usage(const char *a) __z88dk_fastcall;
extern void reset_rx_state(void);
extern const uint8_t ikkle_packed[];

/* ===== String/number utilities ===== */

extern char *skip_spaces(char *p) __z88dk_fastcall;
extern char *split_at_space(char *p) __z88dk_fastcall;
extern uint8_t st_strlen(const char *s) __z88dk_fastcall;
extern int st_stricmp(const char *a, const char *b);
extern void st_copy_n(char *dst, const char *src, uint8_t max_len);
extern uint16_t str_to_u16(const char *s) __z88dk_fastcall;
extern char *u16_to_dec(char *dst, uint16_t v);
extern void fast_u8_to_str(char *buf, uint8_t val) __z88dk_callee;
extern void puts_u8_nolz(uint8_t v) __z88dk_fastcall;

/* ===== esxDOS file I/O ===== */

extern void esx_fopen(const char *path) __z88dk_fastcall;
extern void esx_fread(void);
extern void esx_fclose(void);
extern uint8_t esx_fseek_set(uint16_t offset) __z88dk_fastcall;

/* ===== Resident variables ===== */

/* esxDOS state */
extern uint8_t  esx_handle;
extern uint16_t esx_buf;
extern uint16_t esx_count;
extern uint16_t esx_result;

/* Overlay state */
extern uint8_t  overlay_mode;
extern uint8_t  help_page;
extern uint8_t  config_dirty;
extern uint8_t  notif_enabled;
extern uint8_t  rx_overflow;
extern uint8_t  status_bar_dirty;

/* Buffers */
#define OVERLAY_SLOT_SIZE 512     /* MUST match RX_LINE_SIZE (spectalk.h) — aliased */
extern uint8_t  overlay_slot[];   /* 512B scratch buffer (aliased to rx_line) */
extern uint8_t  ring_buffer[];    /* 2048B — overlay code lives here */
extern uint16_t rb_head;
extern uint16_t rb_tail;
extern uint16_t rx_pos;
extern uint16_t rx_last_len;

/* Theme */
extern uint8_t  theme_attrs[];
extern uint8_t  theme_raw[];      /* 75B: 3 themes x 25B, loaded from DAT */

/* Print cursor state (set by print_str64, used by cfg_item layout) */
extern volatile uint8_t g_ps64_y;
extern volatile uint8_t g_ps64_col;
extern volatile uint8_t g_ps64_attr;

/* Shared string constants from resident */
extern const char K_DAT[];
extern const char S_APPDESC[];
extern const char S_AUTOAWAY[];

/* === Config variables (read-only from overlay) === */
extern char     irc_nick[];
extern char     irc_server[];
extern char     irc_port[];
extern char     irc_pass[];
extern char     nickserv_pass[];
extern char     nickserv_nick[];
extern uint8_t  current_theme;
extern uint8_t  current_channel_idx;
extern uint8_t  channels[];
extern uint8_t  beep_enabled;
extern uint8_t  keyclick_enabled;
extern uint8_t  nick_color_mode;
extern uint8_t  show_traffic;
extern uint8_t  show_timestamps;
extern uint8_t  autoconnect;
extern uint8_t  autojoin;
extern uint8_t  autoaway_minutes;
extern uint16_t autoaway_counter;
extern uint8_t  autoaway_active;
extern uint8_t  connection_state;
extern int8_t   sntp_tz;
extern int8_t   sntp_tz_last;
extern uint8_t  sntp_init_sent;
extern uint8_t  sntp_waiting;
extern uint8_t  sntp_queried;
extern uint8_t  time_hour;
extern uint8_t  time_minute;
extern uint8_t  time_second;
extern uint8_t  last_frames_lo;
extern uint16_t tick_accum;
extern char     search_pattern[];
extern char     autojoin_channels[];
extern char     friend_nicks[][18];   /* MAX_FRIENDS(5) x IRC_NICK_SIZE(18) */
extern uint8_t  friend_count;
extern char     ignore_list[][16];
extern uint8_t  ignore_count;
extern uint8_t  add_ignore(const char *nick) __z88dk_fastcall;
extern uint8_t  remove_ignore(const char *nick) __z88dk_fastcall;
extern uint8_t  is_ignored(const char *nick) __z88dk_fastcall;

/* Shared config-key strings from resident core */
extern const char K_NICK[];
extern const char K_SERVER[];
extern const char K_PORT[];
extern const char K_PASS[];
extern const char K_NKPASS[];
extern const char K_AUTOCONN[];
extern const char K_AUTOJOIN[];
extern const char K_THEME[];
extern const char K_AUTOAWAY[];
extern const char K_BEEP[];
extern const char K_CLICK[];
extern const char K_NCOLOR[];
extern const char K_TRAFFIC[];
extern const char K_TS[];
extern const char K_CHANNELS[];
extern const char K_CFG_PRI[];
extern const char K_CFG_ALT[];
extern const char K_TZ[];
extern const char K_NOTIF[];
extern const char S_ANYKEY[];

/* ===== Theme attribute indices (must match spectalk.h) ===== */
#define TATTR_MSG_CHAN   2
#define TATTR_MAIN_BG    5
#define TATTR_MSG_SYS    9      /* = ATTR_MSG_SERVER */
#define TATTR_MSG_NICK  11
#define TATTR_MSG_TIME  12
#define TATTR_MSG_TOPIC 13

/* ===== Version (single source of truth: spectalk.h) ===== */
#define VERSION "1.3.8"

/* ===== Layout constants ===== */
#define LINES_PER_PAGE  12
#define BPE_HELP_OFFSET 13672
#define EARTH_FRAME0_OFFSET 969
#define EARTH_ATTR0_OFFSET 1556
#define EARTH_LOGO_OFFSET 1600
#define EARTH_DELTA_OFFSET 2128
#define EARTH_DELTA_SIZE 11544
#define EARTH_PACKET_SIZE 481
#define EARTH_FRAME_COUNT 24
#define EARTH_FRAME0_SIZE 587
#define EARTH_ATTR0_SIZE 44
#define EARTH_LOGO_SIZE 528
#define EARTH_LOGO_W_BYTES 22
#define EARTH_LOGO_H 24
#define EARTH_LOGO_ATTR_H 3
#define MAX_FRIENDS     5
#define MAX_IGNORES     5
#define IRC_NICK_SIZE   18
#define ATTR_MSG_SYS    theme_attrs[TATTR_MSG_SYS]

#endif /* OVERLAY_API_H */
