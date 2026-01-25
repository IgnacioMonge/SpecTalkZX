/*
 * spectalk.h - Common header for SpecTalk ZX modular build
 * SpecTalk ZX v1.0 - IRC Client for ZX Spectrum
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
// Theme helpers
const char *theme_get_name(uint8_t theme_id);


#ifdef __SDCC
#define ST_NAKED __naked
#pragma disable_warning 110
#else
#define ST_NAKED
// Theme helpers

#endif

// =============================================================================
// VERSION
// =============================================================================
#define VERSION "1.0"

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

// =============================================================================
// buffer SIZES
// =============================================================================
#define RING_BUFFER_SIZE 2048
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#define LINE_BUFFER_SIZE 128
#define TX_BUFFER_SIZE   256

// IRC/user-visible string sizes (must match definitions in spectalk.c)
#define IRC_SERVER_SIZE   48
#define IRC_PORT_SIZE      8
#define IRC_NICK_SIZE     20
#define USER_MODE_SIZE     8
#define NETWORK_NAME_SIZE 20

// Cross-module buffers (must match definitions in spectalk.c)
#define NAMES_TARGET_CHANNEL_SIZE 64
#define SEARCH_PATTERN_SIZE       32
#define RX_LINE_SIZE              512

// =============================================================================
// CHANNEL/WINDOW MANAGEMENT
// =============================================================================
#define MAX_CHANNELS 10
#define NAV_HIST_SIZE 6
#define MAX_IGNORES 8

typedef struct {
    char name[32];           // Channel name "#retro" or query nick or "Server"
    char mode[12];           // Channel mode (e.g. "+nt") - only for channels
    uint16_t user_count;     // Number of users - only for channels
    uint8_t active;          // 1 if slot in use, 0 if empty
    uint8_t is_query;        // 0 = channel, 1 = query/PM window
    uint8_t has_unread;      // 1 if unread messages (for activity indicator)
} ChannelInfo;

// =============================================================================
// IRC PARSING
// =============================================================================
#define IRC_MAX_PARAMS 10

// Search modes
#define SEARCH_NONE   0
#define SEARCH_CHAN   1
#define SEARCH_USER   2

// =============================================================================
// TIMEOUTS (frames, HALT-based)
// =============================================================================
// Exported because user_cmds.c uses them when driving the ESP AT transport.
#define TIMEOUT_DNS     400
#define TIMEOUT_SSL    1200
#define TIMEOUT_PROMPT  250

// Pagination
#define PAGINATION_THRESHOLD 40

// Names timeout
#define NAMES_TIMEOUT_FRAMES 500

// =============================================================================
// DRAIN LIMITS
// =============================================================================
#define DRAIN_NORMAL    32
#define DRAIN_FAST      192
#define RX_TICK_DRAIN_MAX        255
#define RX_TICK_PARSE_BYTE_BUDGET 4096
#define RX_TICK_LINE_BUDGET_MAX   32

// =============================================================================
// FRAME MACRO
// =============================================================================
#define HALT() do { __asm__("ei"); __asm__("halt"); } while(0)

// =============================================================================
// EXTERNAL ASM FUNCTIONS (spectalk_asm.asm)
// =============================================================================
extern void set_border(uint8_t color) __z88dk_fastcall;
extern void notification_beep(void);
extern void check_caps_toggle(void);
extern uint8_t key_shift_held(void);
extern void input_cache_invalidate(void);
extern void print_str64_char(uint8_t ch) __z88dk_fastcall;
extern void print_line64_fast(uint8_t y, const char *s, uint8_t attr);
extern void draw_indicator(uint8_t y, uint8_t phys_x, uint8_t attr);
extern void clear_line(uint8_t y, uint8_t attr);
extern void clear_zone(uint8_t start, uint8_t lines, uint8_t attr);
extern void ldir_copy_fwd(void *dst, const void *src, uint16_t len);
extern uint16_t screen_row_base[];
extern uint8_t* screen_line_addr(uint8_t y, uint8_t phys_x, uint8_t scanline);
extern uint8_t* attr_addr(uint8_t y, uint8_t phys_x);
extern char* str_append(char *dst, const char *src);
extern char* str_append_n(char *dst, const char *src, uint8_t max);
extern int st_stricmp(const char *a, const char *b);
extern const char* st_stristr(const char *hay, const char *needle);
extern char* u16_to_dec(char *dst, uint16_t v);
extern uint16_t str_to_u16(const char *s) __z88dk_fastcall;
extern char* skip_to(char *s, char c);
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
extern void     ay_uart_send_block(void *buf, uint16_t len) __z88dk_callee;
extern uint8_t  ay_uart_read(void);
extern uint8_t  ay_uart_ready(void);
extern uint8_t  ay_uart_ready_fast(void);

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
extern char user_mode[USER_MODE_SIZE];
extern char network_name[NETWORK_NAME_SIZE];
extern uint8_t connection_state;

// Shanetwork UI/time state (defined in spectalk.c)
extern uint8_t cursor_visible;
extern uint8_t time_synced;
extern uint8_t time_hour;
extern uint8_t time_minute;
extern uint8_t sntp_waiting;
extern uint8_t closed_reported;

// Main text cursor position (shared with command handlers)
extern uint8_t main_line;
extern uint8_t main_col;

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
extern uint8_t ATTR_MSG_PRIV_INV;

// =============================================================================
// CROSS-MODULE FUNCTIONS (defined in spectalk.c)
// =============================================================================
extern void main_print_time_prefix(void);
extern void main_run_u16(uint16_t val, uint8_t attr, uint8_t min_width);
extern void reset_all_channels(void);
extern void sntp_process_response(const char *line);
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

// Transport control flags
extern uint8_t uart_allow_cancel;

// Names tracking
extern uint8_t names_pending;
extern uint16_t names_timeout_frames;
extern char names_target_channel[NAMES_TARGET_CHANNEL_SIZE];
extern uint8_t counting_new_users;
extern uint8_t show_names_list;

// Pagination
extern uint8_t pagination_active;
extern uint8_t pagination_cancelled;
extern uint16_t pagination_count;

// Search
extern uint8_t search_mode;
extern char search_pattern[SEARCH_PATTERN_SIZE];
extern uint16_t search_index;

// IRC parsing
extern char *irc_params[IRC_MAX_PARAMS];
extern uint8_t irc_param_count;

// Input buffers
extern char line_buffer[LINE_BUFFER_SIZE];
extern uint8_t line_len;
extern uint8_t cursor_pos;
extern char tx_buffer[TX_BUFFER_SIZE];

// RX line buffer
extern char rx_line[RX_LINE_SIZE];
extern uint16_t rx_pos;
extern uint16_t rx_overflow;

// UART drain
extern uint8_t uart_drain_limit;

// Print context (used by ASM)
extern uint8_t g_ps64_y;
extern uint8_t g_ps64_col;
extern uint8_t g_ps64_attr;
extern const char *g_ps64_str;

// Caps lock
extern volatile uint8_t caps_lock_mode;
extern volatile uint8_t caps_latch;

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
extern const char S_PRIVMSG[];
extern const char S_MAXWIN[];
extern const char S_SWITCHTO[];

// =============================================================================
// UI MACROS
// =============================================================================
#define ui_msg(attr, s) do { current_attr = attr; main_print(s); } while(0)
#define ui_err(s)       do { current_attr = ATTR_ERROR; main_print(s); } while(0)
#define ui_sys(s)       do { current_attr = ATTR_MSG_SYS; main_print(s); } while(0)
#define ui_usage(a)     do { current_attr = ATTR_ERROR; main_puts("Usage: /"); main_print(a); } while(0)

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
void main_print(const char *s);
void main_puts(const char *s);
void main_putc(char c);
void main_newline(void);
void main_hline(void);
void print_char64(uint8_t y, uint8_t col, uint8_t c, uint8_t attr);
void print_str64(uint8_t y, uint8_t col, const char *s, uint8_t attr);
uint8_t main_run(const char *s, uint8_t attr, uint8_t min_width);
void main_run_char(char c, uint8_t attr);
void draw_status_bar(void);
void redraw_input_full(void);
uint8_t pagination_check(void);
void reapply_screen_attributes(void);
void cls_fast(void);
void draw_status_bar_real(void);

// =============================================================================
// FUNCTION DECLARATIONS - CHANNEL MANAGEMENT (spectalk.c)
// =============================================================================
int8_t find_channel(const char *name);
int8_t find_query(const char *nick);
int8_t find_empty_channel_slot(void);
int8_t add_channel(const char *name);
int8_t add_query(const char *nick);
void remove_channel(uint8_t idx);
void switch_to_channel(uint8_t idx);
void nav_push(uint8_t idx);
void nav_fix_on_delete(uint8_t deleted_idx);

// =============================================================================
// FUNCTION DECLARATIONS - IGNORE LIST (spectalk.c)
// =============================================================================
uint8_t is_ignored(const char *nick);
uint8_t add_ignore(const char *nick);
uint8_t remove_ignore(const char *nick);

// =============================================================================
// FUNCTION DECLARATIONS - IRC PARAMS (spectalk.c)
// =============================================================================
void tokenize_params(char *par, uint8_t max_params);
const char* irc_param(uint8_t idx);

// =============================================================================
// FUNCTION DECLARATIONS - UART/TRANSPORT (spectalk.c)
// =============================================================================
void uart_drain_to_buffer(void);
void uart_drain_and_drop_all(void);
void wait_drain(uint8_t frames);
void uart_send_line(const char *s);

// AT response helpers (spectalk.c)
uint8_t wait_for_response(const char *expected, uint16_t max_frames);
uint8_t wait_for_response_any(const char *exp1, const char *exp2, const char *exp3, uint16_t max_frames);
uint8_t wait_for_prompt_char(uint8_t prompt_ch, uint16_t max_frames);

// =============================================================================
// FUNCTION DECLARATIONS - IRC SEND (spectalk.c)
// =============================================================================
void irc_send_raw(const char *cmd);
void irc_send_privmsg(const char *target, const char *msg);
uint8_t esp_send_line_crlf_nowait(const char *line);

// =============================================================================
// FUNCTION DECLARATIONS - IRC HANDLERS (irc_handlers.c)
// =============================================================================
void parse_irc_message(char *line);
void process_irc_data(void);

// =============================================================================
// FUNCTION DECLARATIONS - USER COMMANDS (user_cmds.c)
// =============================================================================
void parse_user_input(char *line);

// =============================================================================
// FUNCTION DECLARATIONS - MISC (spectalk.c)
// =============================================================================
void force_disconnect(void);
void apply_theme(void);
void init_screen(void);
uint8_t esp_init(void);
void draw_banner(void);

// Attribute helpers (spectalk.c)
uint8_t attr_with_ink(uint8_t base_attr, uint8_t ink_only);

#endif // SPECTALK_H