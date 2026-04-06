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
extern void draw_badge_dither(uint8_t count) __z88dk_fastcall;
extern void notif_draw(uint8_t start_col, const char *str, uint8_t attr);
extern void notif_center(const char *str, uint8_t attr);
extern void input_cache_invalidate(void);

/* ===== String/number utilities ===== */

extern uint8_t st_strlen(const char *s) __z88dk_fastcall;
extern void fast_u8_to_str(char *buf, uint8_t val) __z88dk_callee;

/* ===== esxDOS file I/O ===== */

extern void esx_fopen(const char *path) __z88dk_fastcall;
extern void esx_fread(void);
extern void esx_fclose(void);

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

/* Buffers */
extern uint8_t  overlay_slot[];   /* 512B scratch buffer (aliased to rx_line) */
extern uint8_t  ring_buffer[];    /* 2048B — overlay code lives here */
extern uint16_t rb_head;
extern uint16_t rb_tail;
extern uint16_t rx_pos;

/* Theme */
extern uint8_t  theme_attrs[];
extern uint8_t  theme_raw[];      /* 75B: 3 themes x 25B, loaded from DAT */

/* Print cursor state (set by print_str64, used by cfg_item layout) */
extern volatile uint8_t g_ps64_y;
extern volatile uint8_t g_ps64_col;

/* Shared string constants from resident */
extern const char K_DAT[];
extern const char S_APPDESC[];

/* === Config variables (read-only from overlay) === */
extern char     irc_nick[];
extern char     irc_server[];
extern char     irc_port[];
extern char     irc_pass[];
extern char     nickserv_pass[];
extern char     nickserv_nick[];
extern uint8_t  current_theme;
extern uint8_t  beep_enabled;
extern uint8_t  keyclick_enabled;
extern uint8_t  nick_color_mode;
extern uint8_t  show_traffic;
extern uint8_t  show_timestamps;
extern uint8_t  autoconnect;
extern uint8_t  autoaway_minutes;
extern int8_t   sntp_tz;
extern char     friend_nicks[][18];   /* MAX_FRIENDS(5) x IRC_NICK_SIZE(18) */
extern char     ignore_list[][16];
extern uint8_t  ignore_count;

/* ===== Theme attribute indices (must match spectalk.h) ===== */
#define TATTR_MSG_CHAN   2
#define TATTR_MSG_SYS    9      /* = ATTR_MSG_SERVER */
#define TATTR_MSG_NICK  11
#define TATTR_MSG_TIME  12
#define TATTR_MSG_TOPIC 13

/* ===== Version (single source of truth: spectalk.h) ===== */
#define VERSION "1.3.7"

/* ===== Layout constants ===== */
#define LINES_PER_PAGE  12
#define BPE_HELP_OFFSET 691
#define MAX_FRIENDS     5
#define IRC_NICK_SIZE   18

#endif /* OVERLAY_API_H */
