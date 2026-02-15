/*
 * spectalk.h - Common header for SpecTalk ZX modular build
 * SpecTalk ZX - IRC Client for ZX Spectrum
 * Copyright (C) 2026 M. Ignacio Monge Garcia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SPECTALK_H
#define SPECTALK_H

#include <string.h>
#include <stdint.h>
#include <input.h>
#include <arch/zx.h>

// =============================================================================
// SDCC COMPATIBILITY
// =============================================================================
#ifdef __SDCC
#define ST_NAKED __naked
#pragma disable_warning 110
#else
#define ST_NAKED
#endif

// Theme helper (defined in spectalk.c)
const char *theme_get_name(uint8_t theme_id);

// =============================================================================
// VERSION
// =============================================================================
#define VERSION "1.2.2"

// =============================================================================
// SCREEN LAYOUT CONSTANTS
// =============================================================================
#define SCREEN_COLS     64
#define SCREEN_PHYS     32

#define TOP_BANNER_LINE   0
#define MAIN_START        2
#define MAIN_LINES        18
#define MAIN_END          (MAIN_START + MAIN_LINES - 1)
#define INFO_LINE         21
#define INPUT_START       22
#define INPUT_LINES       2
#define INPUT_END         (INPUT_START + INPUT_LINES - 1)

// =============================================================================
// KEY CODES
// =============================================================================
#define KEY_UP        11
#define KEY_DOWN      10
#define KEY_LEFT      8
#define KEY_RIGHT     9
#define KEY_BACKSPACE 12
#define KEY_ENTER     13

// =============================================================================
// CONNECTION STATES
// =============================================================================
#define STATE_DISCONNECTED  0
#define STATE_WIFI_OK       1
#define STATE_TCP_CONNECTED 2
#define STATE_IRC_READY     3
#define CH_FLAG_ACTIVE     0x01
#define CH_FLAG_QUERY      0x02
#define CH_FLAG_UNREAD     0x04
#define CH_FLAG_MENTION    0x08

// =============================================================================
// buffer SIZES
// =============================================================================
#define RING_BUFFER_SIZE 2048
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#define LINE_BUFFER_SIZE 128

// IRC/user-visible string sizes (must match definitions in spectalk.c)
#define IRC_SERVER_SIZE   32
#define IRC_PORT_SIZE      6
#define IRC_NICK_SIZE     18
#define IRC_PASS_SIZE     24
#define USER_MODE_SIZE     6
#define NETWORK_NAME_SIZE 16

// Cross-module buffers (must match definitions in spectalk.c)
#define NAMES_TARGET_CHANNEL_SIZE 32
#define SEARCH_PATTERN_SIZE       32
// RX_LINE_SIZE: Also defined in spectalk_asm.asm line 420 (ld de, RX_LINE_SIZE-2)
// If changed, update ASM constant accordingly
#define RX_LINE_SIZE              512

// =============================================================================
// CHANNEL/WINDOW MANAGEMENT
// =============================================================================
#define MAX_CHANNELS 10
#define NAV_HIST_SIZE 6
#define MAX_IGNORES 8

// Channel flags already defined above (lines 80-83)

typedef struct {
    char name[22];           // Channel name "#retro" or query nick or "Server"
    char mode[6];            // Channel mode (e.g. "+nt") - only for channels
    uint16_t user_count;     // Number of users - only for channels
    uint8_t flags;           // CH_FLAG_ACTIVE | CH_FLAG_QUERY | CH_FLAG_UNREAD
    uint8_t _pad;            // Padding para alinear a 32 bytes (i*32 = shift)
} ChannelInfo;               // TOTAL: 32 bytes (potencia de 2)

// =============================================================================
// IRC PARSING
// =============================================================================
#define IRC_MAX_PARAMS 10

// Search modes
#define SEARCH_NONE   0
#define SEARCH_CHAN   1
#define SEARCH_USER   2

// Pending search types (para evitar mezcla de resultados)
#define PEND_NONE         0
#define PEND_LIST         1
#define PEND_WHO          2
#define PEND_SEARCH_CHAN  3
#define PEND_SEARCH_USER  4
#define PEND_NAMES        5

// =============================================================================
// TIMEOUTS (frames, HALT-based)
// =============================================================================
// Exported because user_cmds.c uses them when driving the ESP AT transport.
#define TIMEOUT_DNS     400
#define TIMEOUT_SSL    1200
#define TIMEOUT_PROMPT  250

// Pagination
#define PAGINATION_THRESHOLD 40
#define PAGINATION_MAX_COUNT 60000  // Límite de seguridad para evitar overflow

// Names timeout
#define NAMES_TIMEOUT_FRAMES 500

// =============================================================================
// DRAIN LIMITS
// =============================================================================
#define DRAIN_NORMAL    32
#define DRAIN_FAST      192
#define RX_TICK_DRAIN_MAX        255
#define RX_TICK_PARSE_BYTE_BUDGET 1024  // Reduced from 4096 to prevent UI lag
#define RX_TICK_LINE_BUDGET_MAX   32

// Buffer pressure thresholds
#define BUFFER_PRESSURE_THRESHOLD (RING_BUFFER_SIZE * 3 / 4)  // 1536 bytes = 75%
#define BUFFER_CRITICAL_MARGIN 128  // Reserve 128 bytes before full

// =============================================================================
// FRAME MACRO
// =============================================================================
#define HALT() do { __asm__("ei"); __asm__("halt"); } while(0)

// =============================================================================
// EXTERNAL ASM FUNCTIONS (spectalk_asm.asm)
// =============================================================================
extern void set_border(uint8_t color) __z88dk_fastcall;
extern void notification_beep(void);
extern void mention_beep(void);
extern void check_caps_toggle(void);
extern uint8_t key_shift_held(void);
extern void input_cache_invalidate(void);
extern void print_str64_char(uint8_t ch) __z88dk_fastcall;
extern void print_line64_fast(uint8_t y, const char *s, uint8_t attr);
extern void redraw_input_asm(void);
extern void draw_indicator(uint8_t y, uint8_t phys_x, uint8_t attr);
extern void clear_line(uint8_t y, uint8_t attr);
extern void clear_zone(uint8_t start, uint8_t lines, uint8_t attr);
extern uint16_t screen_row_base[];
extern uint8_t* screen_line_addr(uint8_t y, uint8_t phys_x, uint8_t scanline);
extern uint8_t* attr_addr(uint8_t y, uint8_t phys_x);
extern int st_stricmp(const char *a, const char *b);
extern const char* st_stristr(const char *hay, const char *needle);
extern uint8_t st_strlen(const char *s) __z88dk_fastcall;
extern char* u16_to_dec(char *dst, uint16_t v);
extern uint16_t str_to_u16(const char *s) __z88dk_fastcall;
extern char* skip_to(char *s, char c);
extern void st_copy_n(char *dst, const char *src, uint8_t max_len);
extern void uart_send_string(const char *s) __z88dk_fastcall;
extern void strip_irc_codes(char *s);
extern int16_t rb_pop(void);
extern uint8_t rb_push(uint8_t b) __z88dk_fastcall;
extern uint8_t try_read_line_nodrain(void);

// =============================================================================
// EXTERNAL UART DRIVER (ay_uart.asm or divmmc_uart.asm)
// =============================================================================
extern void     ay_uart_init(void);
extern void     ay_uart_send(uint8_t byte) __z88dk_fastcall;
extern uint8_t  ay_uart_read(void);
extern uint8_t  ay_uart_ready(void);

// =============================================================================
// GLOBAL variables (defined in spectalk.c)
// =============================================================================

// Ring buffer
extern uint8_t ring_buffer[];
extern uint16_t rb_head;
extern uint16_t rb_tail;

// Connection state
extern char irc_server[IRC_SERVER_SIZE];
extern char irc_port[IRC_PORT_SIZE];
extern char irc_nick[IRC_NICK_SIZE];
extern uint8_t irc_is_away;
extern char away_message[64];
extern uint8_t away_reply_cd;
extern uint8_t autoaway_minutes;
extern uint16_t autoaway_counter;
extern uint8_t autoaway_active;
extern uint8_t beep_enabled;
extern uint8_t show_quits;
extern int8_t sntp_tz;
extern char irc_pass[IRC_PASS_SIZE];
extern char user_mode[USER_MODE_SIZE];
extern char network_name[NETWORK_NAME_SIZE];
extern uint8_t connection_state;

// Shanetwork UI/time state (defined in spectalk.c)
extern uint8_t cursor_visible;
extern uint8_t time_synced;
extern uint8_t time_hour;
extern uint8_t time_minute;
extern uint8_t time_second;
extern uint8_t sntp_waiting;
extern uint8_t closed_reported;

// Main text cursor position (shared with command handlers)
extern uint8_t main_line;
extern uint8_t main_col;
extern uint8_t wrap_indent;  // Indentación para líneas que continúan

// Channels
extern ChannelInfo channels[];
extern uint8_t current_channel_idx;
extern uint8_t channel_count;

// Navigation history
extern uint8_t nav_history[];
extern uint8_t nav_hist_ptr;

// Ignore list
extern char ignore_list[][16];
extern uint8_t ignore_count;

// Theme attributes (set by apply_theme)
extern uint8_t ATTR_BANNER;
extern uint8_t ATTR_STATUS;
extern uint8_t ATTR_MSG_CHAN;
extern uint8_t ATTR_MSG_SELF;
extern uint8_t ATTR_MSG_PRIV;
extern uint8_t ATTR_MAIN_BG;
extern uint8_t ATTR_INPUT;
extern uint8_t ATTR_INPUT_BG;
extern uint8_t ATTR_PROMPT;
extern uint8_t ATTR_MSG_SERVER;
extern uint8_t ATTR_MSG_JOIN;
extern uint8_t ATTR_MSG_NICK;
extern uint8_t ATTR_MSG_TIME;
extern uint8_t ATTR_MSG_TOPIC;
extern uint8_t ATTR_MSG_MOTD;

// =============================================================================
// CROSS-MODULE FUNCTIONS (defined in spectalk.c)
// =============================================================================
extern void main_print_time_prefix(void);
extern void main_run_u16(uint16_t val, uint8_t attr) __z88dk_callee;
extern void reset_all_channels(void);
void sntp_process_response(const char *line) __z88dk_fastcall;
extern uint8_t ATTR_ERROR;
extern uint8_t STATUS_RED;
extern uint8_t STATUS_YELLOW;
extern uint8_t STATUS_GREEN;
extern uint8_t BORDER_COLOR;
extern uint8_t current_theme;

#define ATTR_MSG_SYS ATTR_MSG_SERVER

// Current attribute for main_print
extern uint8_t current_attr;

// UI state
extern uint8_t status_bar_dirty;
extern uint8_t force_status_redraw;
extern uint8_t ui_freeze_status;
extern uint8_t other_channel_activity;

// Names tracking
extern uint8_t names_pending;
extern uint16_t names_timeout_frames;
extern char names_target_channel[NAMES_TARGET_CHANNEL_SIZE];
extern uint8_t counting_new_users;
extern uint8_t show_names_list;


// Keep-alive system
extern uint16_t server_silence_frames;
extern uint8_t  keepalive_ping_sent;
extern uint16_t keepalive_timeout;

// Pagination
extern uint8_t pagination_active;
extern uint8_t search_data_lost;
extern uint8_t buffer_pressure;
extern uint16_t pagination_count;
extern uint8_t pagination_timeout;

// Search
extern uint8_t search_mode;
extern uint8_t search_flush_state;    // 0=idle, 1=draining, 2=command sent
extern uint8_t search_header_rcvd;    // Flag: server sent 321/352 (not rate-limited)
extern char search_pattern[SEARCH_PATTERN_SIZE];
extern uint16_t search_index;

// Search functions
void cancel_search_state(void);
void start_pagination(void);
void start_search_command(uint8_t type, const char *arg);

// IRC parsing
extern char *irc_params[IRC_MAX_PARAMS];
extern uint8_t irc_param_count;

// Input buffers
extern char line_buffer[LINE_BUFFER_SIZE];
extern uint8_t line_len;
extern uint8_t cursor_pos;
// tx_buffer eliminado - se usa uart_send_string directo

// RX line buffer
extern char rx_line[RX_LINE_SIZE];
extern uint16_t rx_pos;
extern uint16_t rx_last_len;
extern uint8_t rx_overflow;  // Flag: overflow detected (0 or 1)

// UART drain
extern uint8_t uart_drain_limit;
extern uint8_t uart_aggressive_drain;

// Print context (used by ASM)
extern uint8_t g_ps64_y;
extern uint8_t g_ps64_col;
extern uint8_t g_ps64_attr;
// g_ps64_str solo se usa internamente en spectalk.c, no necesita extern

// Caps lock
extern uint8_t caps_lock_mode;
extern uint8_t caps_latch;

// Input cache
extern uint8_t input_cache_char[][SCREEN_COLS];
extern uint8_t input_cache_attr[][32];

// =============================================================================
// COMMON STRINGS (save ROM by sharing)
// =============================================================================
extern const char S_NOTCONN[];
extern const char S_OK[];
extern const char S_ERROR[];
extern const char S_FAIL[];
extern const char S_CLOSED[];
extern const char S_SERVER[];
extern const char S_CHANSERV[];
extern const char S_NOTSET[];
extern const char S_DISCONN[];
extern const char S_NICKSERV[];
extern const char S_APPNAME[];
extern const char S_APPDESC[];
extern const char S_COPYRIGHT[];
extern const char S_LICENSE[];
extern const char S_MAXWIN[];
extern const char S_SWITCHTO[];
extern const char S_PRIVMSG[];
extern const char S_NOTICE[];
extern const char S_TIMEOUT[];
extern const char S_NOWIN[];
extern const char S_CANCELLED[];
extern const char S_RANGE_MINUTES[];
extern const char S_MUST_CHAN[];
extern const char S_ARROW_IN[];
extern const char S_ARROW_OUT[];
extern const char S_EMPTY_PAT[];
extern const char S_ALREADY_IN[];
extern const char S_CRLF[];
extern const char S_ASTERISK[];
extern const char S_COLON_SP[];
extern const char S_SP_COLON[];
extern const char S_SP_PAREN[];
extern const char S_AT_CIPCLOSE[];
extern const char S_AT_CIPMODE0[];
extern const char S_AT_CIPMUX0[];
extern const char S_AT_CIPSERVER0[];
extern const char S_PROMPT[];
extern const char S_CAP_END[];
extern const char S_GLOBAL[];
extern const char S_TOPIC_PFX[];
extern const char S_YOU_LEFT[];
extern const char S_CONN_REFUSED[];
extern const char S_INIT_DOTS[];
extern const char S_ACTION[];
extern const char S_PONG[];

// =============================================================================
// UI MACROS
// =============================================================================
void ui_msg(uint8_t attr, const char *s) __z88dk_callee;
void ui_err(const char *s) __z88dk_fastcall;
void ui_sys(const char *s) __z88dk_fastcall;
void ui_usage(const char *a) __z88dk_fastcall;

#define ERR_NOTCONN() ui_err(S_NOTCONN)
#define ERR_MSG(s)    ui_err(s)
#define SYS_MSG(s)    ui_sys(s)
#define SYS_PUTS(s)   do { current_attr = ATTR_MSG_SYS; main_puts(s); } while(0)

// Compatibility macros
#define irc_channel (channels[current_channel_idx].name)
#define chan_mode (channels[current_channel_idx].mode)
#define chan_user_count (channels[current_channel_idx].user_count)

// =============================================================================
// FUNCTION DECLARATIONS - UI (spectalk.c)
// =============================================================================
void main_print(const char *s) __z88dk_fastcall;
void main_print_wrapped_ram(char *s) __z88dk_fastcall;
void main_puts(const char *s) __z88dk_fastcall;
void main_puts2(const char *a, const char *b) __z88dk_callee;
void main_puts3(const char *a, const char *b, const char *c) __z88dk_callee;
void main_putc(char c) __z88dk_fastcall;
void main_newline(void);
void main_hline(void);
void print_char64(uint8_t y, uint8_t col, uint8_t c, uint8_t attr) __z88dk_callee;
void print_str64(uint8_t y, uint8_t col, const char *s, uint8_t attr) __z88dk_callee;
void main_run2(const char *s, uint8_t attr) __z88dk_callee;
void main_run_char(char c, uint8_t attr) __z88dk_callee;
void draw_status_bar(void);
void redraw_input_full(void);
void reapply_screen_attributes(void);
void cls_fast(void);
void draw_status_bar_real(void);
void fast_u8_to_str(char *buf, uint8_t val);

// =============================================================================
// FUNCTION DECLARATIONS - CHANNEL MANAGEMENT (spectalk.c)
// =============================================================================
int8_t find_channel(const char *name) __z88dk_fastcall;
int8_t find_query(const char *nick) __z88dk_fastcall;
int8_t find_empty_channel_slot(void);
int8_t add_channel(const char *name) __z88dk_fastcall;
int8_t add_query(const char *nick) __z88dk_fastcall;
void remove_channel(uint8_t idx) __z88dk_fastcall;
void switch_to_channel(uint8_t idx) __z88dk_fastcall;
void nav_push(uint8_t idx);
void nav_fix_on_delete(uint8_t deleted_idx);

// =============================================================================
// FUNCTION DECLARATIONS - IGNORE LIST (spectalk.c)
// =============================================================================
uint8_t is_ignored(const char *nick) __z88dk_fastcall;
uint8_t add_ignore(const char *nick) __z88dk_fastcall;
uint8_t remove_ignore(const char *nick) __z88dk_fastcall;

// =============================================================================
// FUNCTION DECLARATIONS - IRC PARAMS (spectalk.c)
// =============================================================================
void tokenize_params(char *par, uint8_t max_params);
const char* irc_param(uint8_t idx) __z88dk_fastcall;

// =============================================================================
// FUNCTION DECLARATIONS - UART/TRANSPORT (spectalk.c)
// =============================================================================
void uart_drain_to_buffer(void);
void wait_drain(uint8_t frames) __z88dk_fastcall;
void flush_all_rx_buffers(void);
void uart_send_crlf(void) __z88dk_fastcall;
void uart_send_line(const char *s) __z88dk_fastcall;

// AT response helpers (spectalk.c)
uint8_t wait_for_response(const char *expected, uint16_t max_frames);
uint8_t wait_for_prompt_char(uint8_t prompt_ch, uint16_t max_frames);
uint8_t esp_at_cmd(const char *cmd) __z88dk_fastcall;

// =============================================================================
// FUNCTION DECLARATIONS - IRC SEND (spectalk.c)
// =============================================================================
// irc_send_raw eliminado - se usa uart_send_string directo
void irc_send_cmd1(const char *cmd, const char *p1) __z88dk_callee;
void irc_send_cmd2(const char *cmd, const char *p1, const char *p2) __z88dk_callee;
void irc_send_privmsg(const char *target, const char *msg) __z88dk_callee;

// =============================================================================
// FUNCTION DECLARATIONS - IRC HANDLERS (irc_handlers.c)
// =============================================================================
void parse_irc_message(char *line) __z88dk_fastcall;
void process_irc_data(void);

// =============================================================================
// FUNCTION DECLARATIONS - USER COMMANDS (user_cmds.c)
// =============================================================================
void parse_user_input(char *line) __z88dk_fastcall;

// =============================================================================
// FUNCTION DECLARATIONS - MISC (spectalk.c)
// =============================================================================
void force_disconnect(void);
void force_disconnect_wifi(void);
void apply_theme(void);
void init_screen(void);
uint8_t esp_init(void);
void draw_banner(void);

// Configuration file (esxDOS)
uint8_t config_load(void);

#endif // SPECTALK_H
