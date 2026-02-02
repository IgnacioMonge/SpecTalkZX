/*
 * spectalk.c - Main module for SpecTalk ZX
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
 *
 * Portions of this code are derived from BitchZX, also licensed under GPLv2.
 */

#include "../include/spectalk.h"
#include "../include/font64_data.h"
#include "../include/themes.h"

const char *theme_get_name(uint8_t theme_id)
{
    if (theme_id < 1 || theme_id > 3) theme_id = 1;
    return themes[theme_id - 1].name;
}

// Forward declaration for internal function
void draw_status_bar_real(void);
void text_shift_right(char *addr, uint16_t count) __z88dk_callee; 
void text_shift_left(char *dest, uint16_t count) __z88dk_callee;

// =============================================================================
// COMMON STRINGS (save ROM by sharing across modules)
// =============================================================================
const char S_NOTCONN[] = "Not connected";
const char S_OK[] = "OK";
const char S_ERROR[] = "ERROR";
const char S_FAIL[] = "FAIL";
const char S_CLOSED[] = "CLOSED";
const char S_SERVER[] = "Server";
const char S_CHANSERV[] = "ChanServ";
const char S_NOTSET[] = "(not set)";
const char S_DISCONN[] = "Disconnected";
const char S_NICKSERV[] = "NickServ";
const char S_APPNAME[] = "SpecTalk ZX v" VERSION;
const char S_APPDESC[] = "IRC Client for ZX Spectrum";
const char S_COPYRIGHT[] = "(C) 2026 M. Ignacio Monge Garcia";
const char S_MAXWIN[] = "Max windows reached (10).";
const char S_SWITCHTO[] = "Switched to ";
const char S_PRIVMSG[] = "PRIVMSG ";
const char S_NOTICE[]  = "NOTICE ";
const char S_TIMEOUT[] = "Connection timeout";

// =============================================================================
// THEME SYSTEM - Global attributes set by apply_theme()
// =============================================================================
uint8_t current_theme = 1;  // 1=DEFAULT, 2=TERMINAL, 3=COLORFUL
uint8_t ATTR_BANNER;
uint8_t ATTR_STATUS;
uint8_t ATTR_MSG_CHAN;
uint8_t ATTR_MSG_SELF;
uint8_t ATTR_MSG_PRIV;
uint8_t ATTR_MSG_PRIV_INV;
uint8_t ATTR_MAIN_BG;
uint8_t ATTR_INPUT;
uint8_t ATTR_INPUT_BG;
uint8_t ATTR_PROMPT;
uint8_t STATUS_RED;
uint8_t STATUS_YELLOW;
uint8_t STATUS_GREEN;
uint8_t BORDER_COLOR;
uint8_t ATTR_MSG_SERVER;
uint8_t ATTR_MSG_JOIN;
uint8_t ATTR_MSG_NICK;
uint8_t ATTR_MSG_TIME;
uint8_t ATTR_MSG_TOPIC;
uint8_t ATTR_MSG_MOTD;
uint8_t ATTR_ERROR;

// =============================================================================
// UI STATE FLAGS
// =============================================================================

// Dirty flags for deferred UI updates
uint8_t status_bar_dirty = 1;
uint8_t counting_new_users = 0;  // Flag: next 353 should reset count
uint8_t show_names_list = 0;     // Flag: show 353 user list (for /names command)

// Activity indicator for inactive channels
uint8_t other_channel_activity = 0;  // Set when msg arrives on non-active channel
uint8_t irc_is_away = 0;

// NAMES reply state machine (robust user counting with timeout)
uint8_t names_pending = 0;
uint16_t names_timeout_frames = 0;
char names_target_channel[NAMES_TARGET_CHANNEL_SIZE];

// Keep-alive system: detect silent disconnections
// KEEPALIVE_SILENCE = 3 min = 9000 frames, KEEPALIVE_TIMEOUT = 30s = 1500 frames
#define KEEPALIVE_SILENCE_FRAMES 9000
#define KEEPALIVE_TIMEOUT_FRAMES 1500
uint16_t server_silence_frames = 0;  // Frames since last server activity
uint8_t  keepalive_ping_sent = 0;    // 1 = waiting for PONG
uint16_t keepalive_timeout = 0;      // Timeout counter after PING sent

// Pagination for long LIST/WHO results
uint8_t pagination_active = 0;
uint16_t pagination_count = 0;
uint8_t search_draining = 0;

// SEARCH state
uint8_t search_mode = SEARCH_NONE;
char    search_pattern[SEARCH_PATTERN_SIZE];
uint16_t search_index = 0;

// Pending search system (evita mezcla de resultados)
uint8_t pending_search_type = PEND_NONE;
char pending_search_arg[32];
uint8_t pending_search_requires_ready = 0;
uint8_t active_search_type = PEND_NONE;
char active_search_arg[32];
uint16_t search_drain_timeout = 0;  // Timeout para liberar drenaje

uint8_t ui_freeze_status = 0;
void draw_status_bar(void)
{
    status_bar_dirty = 1;
}


// Input cell cache
uint8_t input_cache_char[INPUT_LINES][SCREEN_COLS];
uint8_t input_cache_attr[INPUT_LINES][32];

static void put_char64_input_cached(uint8_t y, uint8_t col, char ch, uint8_t attr)
{
    if (y < INPUT_START || y > INPUT_END || col >= SCREEN_COLS) {
        print_char64(y, col, ch, attr);
        return;
    }
    
    uint8_t r = y - INPUT_START;
    uint8_t ax = col >> 1;
    
    // Skip if unchanged
    if (input_cache_char[r][col] == (uint8_t)ch && input_cache_attr[r][ax] == attr) {
        return;
    }
    
    input_cache_char[r][col] = (uint8_t)ch;
    input_cache_attr[r][ax] = attr;
    
    print_char64(y, col, ch, attr);
}

// Colors / Attributes - IRC Message Color Scheme
// =============================================================================
// UART DRIVER AND RING buffer
// =============================================================================

uint8_t ring_buffer[RING_BUFFER_SIZE];
uint16_t rb_head = 0;
uint16_t rb_tail = 0;

// Line parser state
char rx_line[RX_LINE_SIZE];
uint16_t rx_pos = 0;
uint16_t rx_overflow = 0;  // Public for ASM access

// TIMEOUT_* values are defined in spectalk.h (single source of truth)

uint8_t uart_drain_limit = DRAIN_NORMAL;

// Wait frames while draining UART - prevents buffer overflow during waits
static void uart_drain_cleanup(uint8_t frames, uint8_t drop_all)
{
    // Optional paced drain (frame-synced) to keep UART serviced during waits.
    while (frames--) {
        HALT();
        uart_drain_to_buffer();
    }

    if (drop_all) {
        // Aggressively drain UART into ring, then discard buffered bytes.
        uint8_t saved = uart_drain_limit;
        uart_drain_limit = 0;
        uart_drain_to_buffer();
        rb_tail = rb_head;
        uart_drain_limit = saved;
    }
}

void wait_drain(uint8_t frames) __z88dk_fastcall
{
    uart_drain_cleanup(frames, 0);
}

void uart_drain_and_drop_all(void)
{
    uart_drain_cleanup(0, 1);
}


// rb_pop() está implementada en spectalk_asm.asm

// =============================================================================
// GLOBAL bufferS
// =============================================================================
char line_buffer[LINE_BUFFER_SIZE];
uint8_t line_len = 0;
uint8_t cursor_pos = 0;

// CAPS LOCK state (from BitStream)
volatile uint8_t caps_lock_mode = 0;
volatile uint8_t caps_latch = 0;

// =============================================================================
// IRC STATE
// =============================================================================
char irc_server[IRC_SERVER_SIZE] = "";
char irc_port[IRC_PORT_SIZE] = "6667";
char irc_nick[IRC_NICK_SIZE] = "";
char irc_pass[IRC_PASS_SIZE] = "";
char user_mode[USER_MODE_SIZE] = "";
char network_name[NETWORK_NAME_SIZE] = "";
uint8_t connection_state = STATE_DISCONNECTED;

// =============================================================================
// MULTI-WINDOW SUPPORT
// =============================================================================
ChannelInfo channels[MAX_CHANNELS];
uint8_t current_channel_idx = 0;
uint8_t channel_count = 0;

uint8_t nav_history[NAV_HIST_SIZE];
uint8_t nav_hist_ptr = 0;

void nav_push(uint8_t idx) {
    if (nav_hist_ptr > 0 && nav_history[nav_hist_ptr - 1] == idx) return;
    if (nav_hist_ptr >= NAV_HIST_SIZE) {
        memmove(&nav_history[0], &nav_history[1], NAV_HIST_SIZE - 1);
        nav_hist_ptr--;
    }
    nav_history[nav_hist_ptr++] = idx;
}

void nav_fix_on_delete(uint8_t deleted_idx) {
    uint8_t i, j = 0;
    for (i = 0; i < nav_hist_ptr; i++) {
        uint8_t h = nav_history[i];
        if (h == deleted_idx) continue;
        if (h > deleted_idx) h--;
        nav_history[j++] = h;
    }
    nav_hist_ptr = j;
}

// =============================================================================
// IGNORE LIST
// =============================================================================
char ignore_list[MAX_IGNORES][16];
uint8_t ignore_count = 0;

// Add nick to ignore list, returns 1 on success
uint8_t add_ignore(const char *nick) __z88dk_fastcall
{
    if (ignore_count >= MAX_IGNORES) return 0;
    if (is_ignored(nick)) return 0;  // Already ignored
    strncpy(ignore_list[ignore_count], nick, 15);
    ignore_list[ignore_count][15] = '\0';
    ignore_count++;
    return 1;
}

// Remove nick from ignore list, returns 1 on success
uint8_t remove_ignore(const char *nick) __z88dk_fastcall
{
    uint8_t i, j;
    for (i = 0; i < ignore_count; i++) {
        if (st_stricmp(ignore_list[i], nick) == 0) {
            // Shift remaining entries
            for (j = i; j < ignore_count - 1; j++) {
                strncpy(ignore_list[j], ignore_list[j + 1], sizeof(ignore_list[j]) - 1);
                ignore_list[j][sizeof(ignore_list[j]) - 1] = 0;
            }
            ignore_count--;
            return 1;
        }
    }
    return 0;
}

// Find channel by name, returns index or -1 if not found
int8_t find_channel(const char *name) __z88dk_fastcall
{
    uint8_t i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active && st_stricmp(channels[i].name, name) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

// Find query by nick (case-insensitive)
int8_t find_query(const char *nick) __z88dk_fastcall
{
    uint8_t i;
    
    // FILTRO DE SERVICIOS: Forzar Slot 0
    if (nick && *nick) {
        if (st_stricmp(nick, S_CHANSERV) == 0 || 
            st_stricmp(nick, S_NICKSERV) == 0 ||
            st_stricmp(nick, "Global") == 0 ||
            (irc_server[0] && st_stricmp(nick, irc_server) == 0)) {
            return 0; // Slot 0 reservado
        }
    }

    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active && channels[i].is_query && 
            st_stricmp(channels[i].name, nick) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t find_empty_channel_slot(void)
{
    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if (!channels[i].active) {
            return (int8_t)i;
        }
    }
    return -1;
}

// Add channel, returns index or -1 if full
int8_t add_channel(const char *name) __z88dk_fastcall
{
    int8_t idx = find_empty_channel_slot();
    if (idx < 0) return -1;
    
    strncpy(channels[idx].name, name, sizeof(channels[idx].name) - 1);
    channels[idx].name[sizeof(channels[idx].name) - 1] = '\0';
    channels[idx].mode[0] = '\0';
    channels[idx].user_count = 0;
    channels[idx].active = 1;
    channels[idx].is_query = 0;
    channels[idx].has_unread = 0;
    channel_count++;
    return idx;
}

// Add query window for private messages, returns index or -1 if full
int8_t add_query(const char *nick) __z88dk_fastcall
{
    // INTERCEPCIÓN DE SERVICIOS (ChanServ, NickServ)
    // Si es un servicio, usamos el Slot 0 y actualizamos su nombre
    if (st_stricmp(nick, S_CHANSERV) == 0 || st_stricmp(nick, S_NICKSERV) == 0) {
        // Renombramos "Server" a "ChanServ" para claridad visual
        strncpy(channels[0].name, nick, sizeof(channels[0].name) - 1);
        channels[0].name[sizeof(channels[0].name) - 1] = 0;
        return 0;
    }

    // Check if query already exists (usa el find_query modificado arriba)
    int8_t idx = find_query(nick);
    if (idx >= 0) return idx;  // Already exists
    
    idx = find_empty_channel_slot();
    if (idx < 0) return -1;
    
    strncpy(channels[idx].name, nick, sizeof(channels[idx].name) - 1);
    channels[idx].name[sizeof(channels[idx].name) - 1] = '\0';
    channels[idx].mode[0] = '\0';
    channels[idx].user_count = 0;
    channels[idx].active = 1;
    channels[idx].is_query = 1;
    channels[idx].has_unread = 0;
    channel_count++;
    return idx;
}

void remove_channel(uint8_t idx) __z88dk_fastcall
{
    uint8_t i;
    uint8_t next_idx = 0;
    uint8_t was_current = (idx == current_channel_idx);

    if (idx >= MAX_CHANNELS || !channels[idx].active) return;
    if (idx == 0) return;

    // 1. Si cerramos la ventana actual, buscar a dónde ir (usando índices ORIGINALES)
    if (was_current) {
        // Buscar en historial hacia atrás el último slot válido que no sea el que cerramos
        for (i = nav_hist_ptr; i > 0; i--) {
            uint8_t h = nav_history[i - 1];
            if (h != idx && h < MAX_CHANNELS && channels[h].active) {
                next_idx = h;
                break;
            }
        }
    }

    // 2. DEFRAGMENTACIÓN (mover slots hacia arriba)
    for (i = idx; i < MAX_CHANNELS - 1; i++) {
        channels[i] = channels[i + 1];
    }
    memset(&channels[MAX_CHANNELS - 1], 0, sizeof(ChannelInfo));
    if (channel_count > 1) channel_count--;

    // 3. Ajustar índices DESPUÉS de defragmentar
    if (was_current) {
        // Ajustar next_idx si apuntaba a un slot que se movió
        if (next_idx > idx) next_idx--;
        current_channel_idx = next_idx;
    } else if (current_channel_idx > idx) {
        current_channel_idx--;
    }

    // 4. Limpiar historial (ajustar índices y eliminar el borrado)
    nav_fix_on_delete(idx);

    draw_status_bar();
}

void switch_to_channel(uint8_t idx) __z88dk_fastcall
{
    if (idx >= MAX_CHANNELS || !channels[idx].active) return;
    if (idx == current_channel_idx) return;
    
    // GUARDAR DÓNDE ESTÁBAMOS ANTES DE CAMBIAR
    nav_push(current_channel_idx);
    
    current_channel_idx = idx;
    channels[idx].has_unread = 0;
    other_channel_activity = 0;    
    set_border(BORDER_COLOR); 
    status_bar_dirty = 1;
}

// Reset all channels (on disconnect)
void reset_all_channels(void)
{
    uint8_t i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        channels[i].active = 0;
        channels[i].name[0] = '\0';
        channels[i].mode[0] = '\0';
        channels[i].user_count = 0;
        channels[i].is_query = 0;
        channels[i].has_unread = 0;
    }
    
    // Slot 0: SIEMPRE reservado para Server / ChanServ
    channels[0].active = 1;
    strncpy(channels[0].name, S_SERVER, sizeof(channels[0].name) - 1);
    channels[0].name[sizeof(channels[0].name) - 1] = 0;
    channels[0].is_query = 1;  // No user count for this window
    
    current_channel_idx = 0;
    channel_count = 1;
}

// ============================================================
// IRC PARAMETER TOKENIZATION
// ============================================================
// Pre-tokenize IRC message params to avoid repeated strchr/skip
// in individual handlers. Based on BitchZX str_word_find pattern.
// 
// After calling tokenize_params(par):
// =============================================================================
// IRC PARAMS TOKENIZER
// =============================================================================
char *irc_params[IRC_MAX_PARAMS];
uint8_t irc_param_count = 0;

// tokenize_params está implementada en ASM (spectalk_asm.asm)
extern void tokenize_params(char *par, uint8_t max_params);

// Safe param accessor - returns empty string if out of bounds
const char* irc_param(uint8_t idx)
{
    if (idx >= irc_param_count || !irc_params[idx]) return "";
    return irc_params[idx];
}

// cursor visibility control (shared across modules)
uint8_t cursor_visible = 1;  // cursor only visible when user can type

// Connection state tracking (shared across modules)
uint8_t closed_reported = 0;

// TIME TRACKING (Optimized: No 32-bit math)
// Eliminamos uptime_frames y time_sync_frames para ahorrar librería de división
uint8_t time_hour = 0;
uint8_t time_minute = 0;
uint8_t time_second = 0;        // Nuevo: Segundos explícitos
uint8_t time_synced = 0;        // Flag: time has been synced
static uint8_t sntp_init_sent = 0;     // Flag: SNTP init commands sent
uint8_t sntp_waiting = 0;       // Flag: waiting for SNTP response

// Simple ticker for 50Hz counting
static uint8_t tick_counter = 0;

// SCREEN STATE
uint8_t main_line = MAIN_START;
uint8_t main_col = 0;
uint8_t current_attr;  // Initialized in apply_theme()
// COMMAND HISTORY
#define HISTORY_SIZE    4
#define HISTORY_LEN     64

static char history[HISTORY_SIZE][HISTORY_LEN];
static uint8_t hist_head = 0;
static uint8_t hist_count = 0;
static int8_t hist_pos = -1;
static char temp_input[LINE_BUFFER_SIZE];

static void history_add(const char *cmd, uint8_t len)
{
    uint8_t i;
    if (len == 0) return;
    if (hist_count > 0) {
        // OPTIMIZADO: % 4 -> & 3
        uint8_t last = (hist_head + HISTORY_SIZE - 1) & 3;
        if (strcmp(history[last], cmd) == 0) return;
    }
    for (i = 0; i < len && i < HISTORY_LEN - 1; i++) {
        history[hist_head][i] = cmd[i];
    }
    history[hist_head][i] = 0;
    
    // OPTIMIZADO: % 4 -> & 3
    hist_head = (hist_head + 1) & 3;
    
    if (hist_count < HISTORY_SIZE) hist_count++;
    hist_pos = -1;
}

static void history_nav_up(void)
{
    uint8_t idx;
    if (hist_count == 0) return;
    if (hist_pos == -1) memcpy(temp_input, line_buffer, line_len + 1);
    if (hist_pos < (int8_t)(hist_count - 1)) hist_pos++;
    
    // OPTIMIZADO: % 4 -> & 3
    idx = (hist_head + HISTORY_SIZE - 1 - hist_pos) & 3;
    
    strncpy(line_buffer, history[idx], sizeof(line_buffer) - 1);
    line_buffer[sizeof(line_buffer) - 1] = 0;
    line_len = strlen(line_buffer);
    cursor_pos = line_len;
}

static void history_nav_down(void)
{
    uint8_t idx;
    if (hist_pos < 0) return;
    hist_pos--;
    if (hist_pos < 0) {
        memcpy(line_buffer, temp_input, LINE_BUFFER_SIZE);
        line_len = strlen(line_buffer);
    } else {
        // OPTIMIZADO: % 4 -> & 3
        idx = (hist_head + HISTORY_SIZE - 1 - hist_pos) & 3;
        strncpy(line_buffer, history[idx], sizeof(line_buffer) - 1);
        line_buffer[sizeof(line_buffer) - 1] = 0;
        line_len = strlen(line_buffer);
    }
    cursor_pos = line_len;
}

// Draw 1-pixel horizontal line - faster and more elegant than '-' chars
static void draw_hline(uint8_t y, uint8_t x_start, uint8_t width, uint8_t scanline, uint8_t attr)
{
    uint8_t x;
    uint8_t *screen_ptr = (uint8_t*)(screen_row_base[y] + ((uint16_t)scanline << 8) + x_start);
    uint8_t *attr_ptr = (uint8_t*)(0x5800 + (uint16_t)y * 32 + x_start);
    for (x = 0; x < width; x++) {
        *screen_ptr++ = 0xFF;
        *attr_ptr++ = attr;
    }
}

// ============================================================
// CONTEXTO GLOBAL PARA RUTINAS DE IMPRESIÓN 64 COL
// Estas variables son accedidas por el código ASM en spectalk_asm.asm
// ============================================================
uint8_t g_ps64_y;
uint8_t g_ps64_col;
uint8_t g_ps64_attr;
const char *g_ps64_str;


void print_char64(uint8_t y, uint8_t col, uint8_t c, uint8_t attr) __z88dk_callee
{
    g_ps64_y = y;
    g_ps64_col = col;
    g_ps64_attr = attr;
    print_str64_char(c);
}

// ============================================================
// ASM-OPTIMIZED PRINT STRING (64-column mode)
// ============================================================

void print_str64(uint8_t y, uint8_t col, const char *s, uint8_t attr) __z88dk_callee
{
    g_ps64_y = y;
    g_ps64_col = col;
    g_ps64_attr = attr;
    g_ps64_str = s;     // Usamos una variable global temporal para el puntero

    __asm
    push ix
    
    ld hl, (_g_ps64_str) ; HL = Puntero al string
    
_pstr_loop:
    ld a, (hl)          ; Leer carácter
    or a                ; ¿Es 0 (fin de string)?
    jr z, _pstr_end     ; Si es 0, salir
    
    push hl             ; Guardar puntero string (print_char lo podría alterar)
    
    ; Chequear límite de screen (col < 64)
    ld a, (_g_ps64_col)
    cp #64
    jr nc, _pstr_skip   ; Si col >= 64, no dibujar, pero seguir avanzando string
    
    ; Llamar a print_str64_char (Fastcall: argumento en L)
    ld l, (hl)          ; Cargar carácter en L
    call _print_str64_char
    
    ; Incrementar columna global
    ld hl, #_g_ps64_col
    inc (hl)
    
_pstr_skip:
    pop hl              ; Recuperar puntero string
    inc hl              ; Siguiente carácter del string
    jr _pstr_loop

_pstr_end:
    pop ix
    __endasm;
}

// Scroll seguro: Solo mueve lines de la 3 a la 19. No toca banners.
// scroll_main_zone está implementada en ASM (spectalk_asm.asm)
extern void scroll_main_zone(void);

// MAIN AREA OUTPUT

void main_newline(void)
{
    main_col = 0;
    if (main_line < MAIN_END) {
        main_line++;
    } else {
        scroll_main_zone();
    }
}

// ============================================================
// Versión optimizada de main_run: Calcula punteros de fila una vez por segmento
// ============================================================

// OPTIMIZACIÓN: Helper para evitar duplicar lógica de alineación de atributos
static void ensure_attr_alignment(uint8_t target_attr)
{
    if (current_attr != target_attr) {
        if (main_col & 1) {
            if (main_col >= SCREEN_COLS) {
                main_newline();
            } else {
                print_char64(main_line, main_col, ' ', current_attr);
                main_col++;
            }
        }
        current_attr = target_attr;
    }
}

uint8_t main_run(const char *s, uint8_t attr, uint8_t min_width) __z88dk_callee
{
    uint8_t printed = 0;
    const char *p = s;
    
    ensure_attr_alignment(attr);
    
    g_ps64_y = main_line;
    g_ps64_col = main_col;
    g_ps64_attr = attr;
    
    while (*p) {
        if (g_ps64_col >= SCREEN_COLS) {
            main_col = g_ps64_col;
            main_newline();
            g_ps64_y = main_line;
            g_ps64_col = main_col;
        }
        print_str64_char((uint8_t)*p);
        g_ps64_col++;
        p++;
        printed++;
    }
    
    while (printed < min_width) {
        if (g_ps64_col >= SCREEN_COLS) {
            main_col = g_ps64_col;
            main_newline();
            g_ps64_y = main_line;
            g_ps64_col = main_col;
        }
        print_str64_char(' ');
        g_ps64_col++;
        printed++;
    }
    
    main_col = g_ps64_col;
    return printed;
}

void main_run_char(char c, uint8_t attr) __z88dk_callee
{
    ensure_attr_alignment(attr);
    
    if (main_col >= SCREEN_COLS) {
        main_newline();
    }
    print_char64(main_line, main_col, c, attr);
    main_col++;
}

void main_run_u16(uint16_t val, uint8_t attr, uint8_t min_width) __z88dk_callee
{
    char buf[8];
    char *end = u16_to_dec(buf, val);
    *end = '\0';
    
    uint8_t len = (uint8_t)(end - buf);
    
    ensure_attr_alignment(attr);
    
    while (len < min_width) {
        if (main_col >= SCREEN_COLS) {
            main_newline();
        }
        print_char64(main_line, main_col, ' ', attr);
        main_col++;
        min_width--;
    }
    
    char *q = buf;
    while (*q) {
        if (main_col >= SCREEN_COLS) {
            main_newline();
        }
        print_char64(main_line, main_col, *q, attr);
        main_col++;
        q++;
    }
}


void main_print(const char *s) __z88dk_fastcall
{
    // Fast path: Solo si estamos al inicio de la línea y el texto cabe en una línea
    // Para textos largos, usamos slow path que hace wrap automático
    
    if (main_col == 0) {
        // Verificar si el texto es corto (cabe en pantalla)
        const char *p = s;
        uint8_t len = 0;
        while (*p && len < SCREEN_COLS) { p++; len++; }
        
        if (*p == '\0') {
            // Texto corto - usar fast path
            print_line64_fast(main_line, s, current_attr);
            main_newline();
            return;
        }
        // Texto largo - usar slow path para wrap
    }
    
    // Slow path: Carácter a carácter (maneja wrap automático)
    main_puts(s);
    main_newline();
}

// Draw 1-pixel separator line in main area
void main_hline(void)
{
    if (main_col > 0) main_newline();
    draw_hline(main_line, 0, 32, 3, current_attr);  // scanline 3 = centered
    main_newline();
}

// =============================================================================
// PENDING SEARCH SYSTEM (evita mezcla de resultados de búsquedas encadenadas)
// =============================================================================

// Limpia estado de búsqueda/paginación
void cancel_search_state(uint8_t drain) __z88dk_fastcall
{
    pagination_active = 0;
    pagination_count = 0;
    search_mode = SEARCH_NONE;
    search_index = 0;
    search_draining = drain ? 1 : 0;
    search_drain_timeout = 0;
    active_search_type = PEND_NONE;
    active_search_arg[0] = '\0';
}

// Helper: copia argumento (máx 31 chars)
static void copy_cmd_arg32(char *dst, const char *src)
{
    uint8_t i = 0;
    if (!src) { dst[0] = 0; return; }
    while (src[i] && src[i] != ' ' && i < 31) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

// Inicia un comando de búsqueda inmediatamente
void start_search_command(uint8_t type, const char *arg)
{
    char tmp[32];
    copy_cmd_arg32(tmp, arg);

    active_search_type = type;
    memcpy(active_search_arg, tmp, sizeof(active_search_arg));
    active_search_arg[31] = 0;

    pagination_active = 1;
    pagination_count = 0;
    search_index = 0;
    search_draining = 0;
    search_drain_timeout = 0;

    if (type == PEND_LIST) {
        search_mode = SEARCH_CHAN;
        search_pattern[0] = 0;
        irc_send_cmd1("LIST", tmp);
        return;
    }

    if (type == PEND_WHO) {
        search_mode = SEARCH_USER;
        search_pattern[0] = 0;
        irc_send_cmd1("WHO", tmp);
        return;
    }

    if (type == PEND_SEARCH_CHAN) {
        search_mode = SEARCH_CHAN;
        strncpy(search_pattern, tmp, 30);
        search_pattern[30] = 0;
        uart_send_string("LIST *");
        uart_send_string(search_pattern);
        uart_send_string("*\r\n");
        return;
    }

    if (type == PEND_SEARCH_USER) {
        search_mode = SEARCH_USER;
        strncpy(search_pattern, tmp, 30);
        search_pattern[30] = 0;
        irc_send_cmd1("WHO", search_pattern);
        return;
    }

    // Tipo desconocido - limpiar
    cancel_search_state(0);
}

// Encola búsqueda para cuando el buffer esté limpio
void queue_search_command(uint8_t type, const char *arg)
{
    copy_cmd_arg32(pending_search_arg, arg);
    pending_search_type = type;
    pending_search_requires_ready = 0;
    cancel_search_state(1);  // Activar drenaje para ignorar restos
}

// Intenta iniciar búsqueda pendiente si las condiciones son favorables
void pending_search_try_start(void)
{
    uint8_t t;
    char arg[32];

    if (pending_search_type == PEND_NONE) return;

    // Verificar estado de conexión
    if (pending_search_requires_ready) {
        if (connection_state < STATE_IRC_READY) return;
    } else {
        if (connection_state < STATE_TCP_CONNECTED) return;
    }

    // No iniciar si aún estamos drenando o hay búsqueda activa
    if (search_draining) return;
    if (pagination_active || search_mode != SEARCH_NONE) return;
    
    // No iniciar si hay datos pendientes en el buffer
    if (rb_head != rb_tail) return;

    // Copiar y limpiar pending
    t = pending_search_type;
    memcpy(arg, pending_search_arg, sizeof(arg));

    pending_search_type = PEND_NONE;
    pending_search_requires_ready = 0;
    pending_search_arg[0] = 0;

    // Iniciar el comando
    start_search_command(t, arg);
}

// PAGINATION FOR LONG RESULTS (LIST/WHO)
// Returns 1 if user cancelled, 0 otherwise
uint8_t pagination_check(void)
{
    uint8_t key;
    
    if (!pagination_active) return 0;
    
    // Auto-limpieza visual al final del buffer
    if (pagination_count == 0 && main_line >= MAIN_END - 3) {
        clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
        main_line = MAIN_START;
        main_col = 0;
        return 0;
    }
    
    if (main_line >= MAIN_END) {
        current_attr = ATTR_MSG_SYS;
        main_print("-- Any key: more | EDIT: cancel --");
        
        while (in_inkey() != 0) { HALT(); }
        
        // Esperar tecla (drenando UART para no desbordar hardware)
        while ((key = in_inkey()) == 0) { 
            HALT();
            uart_drain_to_buffer(); 
        }
        
        while (in_inkey() != 0) { HALT(); }
        
        // EDIT = key 7 -> CANCELAR
        if (key == 7) {
            cancel_search_state(1);  // Activar drenaje
            
            // NO borrar pantalla - mantener resultados visibles
            // Solo añadir mensaje de cancelación
            main_newline();
            ui_err("Cancelled");
            return 1;
            
        }
        
        clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
        main_line = MAIN_START;
        main_col = 0;
    }
    return 0;
}

// STATUS BAR

static void draw_clock(void)
{
    char time_part[10];
    char *p = time_part;
    
    // Usar ATTR_STATUS para que coincida con el theme actual
    uint8_t clock_attr = ATTR_STATUS; 
    
    *p++ = '[';
    // OPTIMIZADO: Eliminadas divisiones / y %
    fast_u8_to_str(p, time_hour); p += 2;
    *p++ = ':';
    fast_u8_to_str(p, time_minute); p += 2;
    *p++ = ']';
    *p = 0;
    
    // Imprimir en columna 54 sin borrar la line entera
    print_str64(INFO_LINE, 54, time_part, clock_attr);
}

// Return current HH:MM (simple getters)
static void get_current_hhmm(uint8_t *hh, uint8_t *mm)
{
    *hh = time_hour;
    *mm = time_minute;
}

// Print timestamp prefix: [HH:MM]
// Note: do not override PAPER/INK here; let the active theme/message
// attribute decide the final colors.
void main_print_time_prefix(void)
{
    uint8_t hh, mm;
    char tmp[2];
    uint8_t saved_attr = current_attr;

    get_current_hhmm(&hh, &mm);

    current_attr = ATTR_MSG_TIME;

    main_putc('[');
    fast_u8_to_str(tmp, hh);
    main_putc(tmp[0]);
    main_putc(tmp[1]);
    main_putc(':');
    fast_u8_to_str(tmp, mm);
    main_putc(tmp[0]);
    main_putc(tmp[1]);
    main_putc(']');
    main_putc(' ');

    current_attr = saved_attr;
}


// sb_append está implementada en ASM (spectalk_asm.asm)
extern char *sb_append(char *dst, const char *src, const char *limit);

// Variables estáticas para caché de repintado (estas SÍ deben ser estáticas para persistir)
static char sb_last_status[64] = ""; 
static char sb_left_part[64];  // Buffer estático (ahorra stack frame)
uint8_t force_status_redraw = 1;

static void draw_connection_indicator(void)
{
    uint8_t ind_attr;

    if (connection_state >= STATE_TCP_CONNECTED) {
        // Conectado (TCP o IRC) -> SIEMPRE VERDE
        // El estado AWAY solo cambiará la forma del icono (Círculo vs C), no el color.
        ind_attr = STATUS_GREEN; 
    } else {
        // Desconectado -> ROJO
        ind_attr = STATUS_RED; 
    }

    // Llamar a rutina gráfica ASM 
    // Coordenadas: INFO_LINE (21), Columna Física 31 (Extremo derecho)
    draw_indicator(INFO_LINE, 31, ind_attr);
}

extern char *u16_to_dec3(char *dst, uint16_t v) __z88dk_callee;
void draw_status_bar_real(void)
{
    char *p = sb_left_part; 
    uint8_t ind_attr;
    
    // Calcular límites absolutos para recortes seguros
    char *limit_global = sb_left_part + 63;
    
    // --- 1. CONSTRUCCIÓN DE LA CADENA ---
    *p++ = '[';
    if (irc_nick[0]) {
        p = sb_append(p, irc_nick, sb_left_part + 11); // Max 10 chars
        if (user_mode[0]) {
            *p++ = '(';
            p = sb_append(p, user_mode, limit_global);
            *p++ = ')';
        }
    } else {
        p = sb_append(p, "no-nick", limit_global);
    }
    *p++ = ']'; *p++ = ' '; *p++ = '[';

    // Lógica contaje canales
    if (current_channel_idx > 0) {
        if (other_channel_activity) *p++ = '*';
        
        if (channel_count > 1) { 
            uint8_t cur = current_channel_idx; 
            uint8_t tot = channel_count - 1;
            
            // Optimización manual de itoa simple
            if (cur >= 10) { *p++ = '1'; *p++ = '0' + (cur - 10); } 
            else *p++ = '0' + cur;
            
            *p++ = '/';
            
            if (tot >= 10) { *p++ = '1'; *p++ = '0' + (tot - 10); } 
            else *p++ = '0' + tot;
            
            *p++ = ':';
        }
    } else if (other_channel_activity) {
        *p++ = '*';
    }

    // --- NOMBRE DE VENTANA / ESTADO / RED ---
    if (current_channel_idx == 0 && channel_count == 1) {
        if (connection_state >= STATE_TCP_CONNECTED) {
            if (network_name[0]) { 
                p = sb_append(p, network_name, sb_left_part + 40);
            } else if (irc_server[0]) {
                p = sb_append(p, irc_server, sb_left_part + 40);
            } else { 
                p = sb_append(p, "connected", limit_global);
            }
        } else if (connection_state == STATE_WIFI_OK) {
             p = sb_append(p, "wifi-ready", limit_global);
        } else {
             p = sb_append(p, "offline", limit_global);
        }
    } else if (irc_channel[0]) {
        if (channels[current_channel_idx].is_query && current_channel_idx != 0) *p++ = '@';
        
        // A. Nombre del canal (Límite 35)
        if (current_channel_idx == 0 && network_name[0]) {
             p = sb_append(p, network_name, sb_left_part + 35);
        } else {
             p = sb_append(p, irc_channel, sb_left_part + 35);
        }
        
        // B. Modos (Límite 45)
        if (!channels[current_channel_idx].is_query && chan_mode[0]) {
            *p++ = '(';
            p = sb_append(p, chan_mode, sb_left_part + 45);
            *p++ = ')';
        }

        // C. Red
        if (current_channel_idx != 0 && (network_name[0] || irc_server[0]) && (p < sb_left_part + 48)) {
            *p++ = '@'; 
            char *net = network_name[0] ? network_name : irc_server;
            if (net == irc_server && net[0]=='i' && net[1]=='r' && net[2]=='c' && net[3]=='.') net += 4;
            p = sb_append(p, net, sb_left_part + 52);
        }

    } else if (connection_state >= STATE_TCP_CONNECTED) {
         if (network_name[0]) p = sb_append(p, network_name, limit_global);
         else if (irc_server[0]) p = sb_append(p, irc_server, sb_left_part + 40);
         else p = sb_append(p, "connected", limit_global);
    } else if (connection_state == STATE_WIFI_OK) {
         p = sb_append(p, "wifi-ready", limit_global);
    } else {
         p = sb_append(p, "offline", limit_global);
    }
    
    if (p < limit_global) *p++ = ']';
    else *(limit_global - 1) = ']';
    
    // Usuarios (solo canales) - OPTIMIZADO con u16_to_dec3 (hasta 4 dígitos)
    if (irc_channel[0] && !channels[current_channel_idx].is_query && chan_user_count > 0) {
        if (p < limit_global - 6) {
            extern char *u16_to_dec3(char *dst, uint16_t v) __z88dk_callee;

            char buf[8];
            *p++ = ' '; *p++ = '[';

            u16_to_dec3(buf, chan_user_count);

            char *n = buf;
            while (*n) *p++ = *n++;

            if (p < limit_global) *p++ = ']';
            else *(limit_global - 1) = ']';
        }
    } 
    *p = 0; 
    
    // --- 2. RENDERIZADO RÁPIDO ---
    if (force_status_redraw || strcmp(sb_left_part, sb_last_status) != 0) {
        print_line64_fast(INFO_LINE, sb_left_part, ATTR_STATUS);
        strncpy(sb_last_status, sb_left_part, sizeof(sb_last_status) - 1);
        sb_last_status[sizeof(sb_last_status) - 1] = 0;
        force_status_redraw = 0;
    }
    
    draw_clock();
    
    if (connection_state < STATE_WIFI_OK) ind_attr = STATUS_RED;
    else if (connection_state < STATE_TCP_CONNECTED) ind_attr = STATUS_YELLOW;
    else ind_attr = STATUS_GREEN;
    
    draw_connection_indicator();
}

// INPUT AREA

static void draw_cursor_underline(uint8_t y, uint8_t col)
{
    uint8_t phys_x = col >> 1;
    uint8_t half = col & 1;
    uint8_t *ptr0, *ptr7;
    
    // Máscara: 0xF0 para izquierda, 0x0F para derecha
    uint8_t mask = (half == 0) ? 0xF0 : 0x0F;
    uint8_t inv_mask = ~mask;
    
    // 1. Forzar atributo
    *attr_addr(y, phys_x) = ATTR_INPUT;
    
    // 2. LIMPIEZA PREVIA (borrar cursor viejo en ambas lines)
    ptr0 = screen_line_addr(y, phys_x, 0);  // Scanline superior (sobreline)
    ptr7 = screen_line_addr(y, phys_x, 7);  // Scanline inferior (subrayado)
    
    *ptr0 &= inv_mask; 
    *ptr7 &= inv_mask; 
    
    // 3. LÓGICA EFECTIVA (Caps Lock XOR Shift)
    // Si Caps=1 y Shift=0 -> Efectivo=1 (Mayús -> Arriba)
    // Si Caps=1 y Shift=1 -> Efectivo=0 (Minús -> Abajo)
    uint8_t shift_pressed = key_shift_held();
    uint8_t effective_caps = (caps_lock_mode ^ shift_pressed);

    // 4. DIBUJAR NUEVO cursor
    if (effective_caps) {
        // Modo Mayúsculas efectivas: Sobreline (Scanline 0)
        *ptr0 |= mask;
    } else {
        // Modo Minúsculas efectivas: Subrayado (Scanline 7)
        *ptr7 |= mask;
    }
}

static void refresh_cursor_char(uint8_t idx, uint8_t show_cursor)
{
    uint16_t abs_pos = (uint16_t)idx + 2;
    uint8_t row = INPUT_START + (abs_pos >> 6);
    uint8_t col = abs_pos & 63;

    if (row > INPUT_END) return;

    char c = (idx < line_len) ? line_buffer[idx] : ' ';
    
    // Only show cursor if both show_cursor AND cursor_visible are true
    if (show_cursor && cursor_visible) {
        // Draw character with cache, then add cursor
        put_char64_input_cached(row, col, c, ATTR_INPUT);
        draw_cursor_underline(row, col);
    } else {
        // redraw character without cache to ensure it's complete (no cursor artifacts)
        print_char64(row, col, c, ATTR_INPUT);
        // Update cache to match
        uint8_t r = row - INPUT_START;
        uint8_t ax = col >> 1;
        input_cache_char[r][col] = (uint8_t)c;
        input_cache_attr[r][ax] = ATTR_INPUT;
    }
}

static void input_draw_prompt(void)
{
    print_char64(INPUT_START, 0, '>', ATTR_PROMPT);
    print_char64(INPUT_START, 1, ' ', ATTR_PROMPT);
}

static void input_put_char_at(uint16_t abs_pos, char c)
{
    // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
    uint8_t row = INPUT_START + (abs_pos >> 6);
    uint8_t col = abs_pos & 63;
    
    if (row > INPUT_END) return;
    put_char64_input_cached(row, col, c, ATTR_INPUT);
}

static void redraw_input_from(uint8_t start_pos)
{
    uint8_t row, col, i;
    uint16_t abs_pos;

    if (start_pos == 0) {
        put_char64_input_cached(INPUT_START, 0, '>', ATTR_PROMPT);
        put_char64_input_cached(INPUT_START, 1, ' ', ATTR_PROMPT);
    }

    for (i = start_pos; i < line_len; i++) {
        abs_pos = (uint16_t)i + 2;
        // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
        row = INPUT_START + (abs_pos >> 6);
        col = abs_pos & 63;
        
        if (row > INPUT_END) break;
        put_char64_input_cached(row, col, line_buffer[i], ATTR_INPUT);
    }

    // Clear a small tail after EOL to remove stale characters without full redraw
    abs_pos = (uint16_t)line_len + 2;
    // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
    row = INPUT_START + (abs_pos >> 6);
    col = abs_pos & 63;

    uint8_t clear_count = 0;
    while (row <= INPUT_END && clear_count < 8) {
        put_char64_input_cached(row, col, ' ', ATTR_INPUT);
        col++;
        if (col >= SCREEN_COLS) { col = 0; row++; }
        clear_count++;
    }

    // cursor refresh
    refresh_cursor_char(cursor_pos, 1);
}

void redraw_input_full(void)
{
    uint16_t i;
    // Coordenadas iniciales: line 22, Columna 2 (después del "> ")
    uint8_t row = INPUT_START;
    uint8_t col = 2; 

    // 1. Resetear la caché lógica (ASM)
    input_cache_invalidate();
    
    // 2. Limpieza Visual Masiva
    // Borramos las lines visualmente de golpe.
    for (i = INPUT_START; i <= INPUT_END; i++) {
        clear_line((uint8_t)i, ATTR_INPUT_BG);
    }

    // 3. Dibujar Prompt
    input_draw_prompt();

    // 4. Dibujar Texto (Bucle optimizado)
    // Solo iteramos hasta line_len (lo que hay escrito).
    for (i = 0; i < line_len; i++) {
        // Llamada directa a la rutina de pintado
        put_char64_input_cached(row, col, line_buffer[i], ATTR_INPUT);

        // Cálculo de posición incremental (sin divisiones ni módulos lentos)
        col++;
        if (col >= SCREEN_COLS) {
            col = 0;
            row++;
            if (row > INPUT_END) break; // Protección
        }
    }

    // 5. Restaurar cursor
    refresh_cursor_char(cursor_pos, 1);
}

static void input_clear(void)
{
    // 1. Resetear variables de state
    line_len = 0;
    cursor_pos = 0;
    line_buffer[0] = 0;
    hist_pos = -1;  // Resetear puntero del historial
    
    // 2. Delegar el trabajo visual (Limpieza + Prompt + cursor)
    redraw_input_full();
}

void fast_u8_to_str(char *buf, uint8_t val)
{
    uint8_t tens = 0;
    // Restas sucesivas: más rápido y pequeño que l_div para N < 100
    while (val >= 10) {
        val -= 10;
        tens++;
    }
    buf[0] = '0' + tens;
    buf[1] = '0' + val;
}

static char g_input_safe;
static void input_add_char(char c) __z88dk_fastcall
{
    // 1. Borrar cursor visual
    refresh_cursor_char(cursor_pos, 0);

    if (c >= 32 && c < 127 && line_len < (INPUT_LINES * SCREEN_COLS - 2)) {
        
        // A. CASO INSERTAR: Usamos memmove (estándar) en lugar de ASM manual
        // memmove gestiona el solapamiento automáticamente
        if (cursor_pos < line_len) {
           text_shift_right(line_buffer + cursor_pos, line_len - cursor_pos);
        }
        
        // B. Escribir caracter
        line_buffer[cursor_pos] = c;
        line_len++;
        cursor_pos++;
        line_buffer[line_len] = 0;
        
        // Redibujar desde el cambiof
        if (cursor_pos == line_len) {
            // Append rápido
            input_put_char_at((uint16_t)(cursor_pos - 1) + 2, c);
            refresh_cursor_char(cursor_pos, 1);
        } else {
            redraw_input_from((uint8_t)(cursor_pos - 1));
        }
    }
}

static void input_backspace(void)
{
    if (cursor_pos > 0) {
        uint8_t was_at_end = (cursor_pos == line_len);
        refresh_cursor_char(cursor_pos, 0);

        cursor_pos--;
        
        // Desplazamiento a la izquierda con memmove (Reemplaza bloque ASM ldir)
        if (cursor_pos < line_len - 1) {
            text_shift_left(line_buffer + cursor_pos, line_len - cursor_pos - 1);
        }
        
        line_len--;
        line_buffer[line_len] = 0;

        if (was_at_end) {
            input_put_char_at((uint16_t)(cursor_pos + 1) + 2, ' ');
            refresh_cursor_char(cursor_pos, 1);
        } else {
            redraw_input_from((uint8_t)cursor_pos);
        }
    }
}


static void input_left(void)
{
    if (cursor_pos > 0) {
        refresh_cursor_char(cursor_pos, 0);
        cursor_pos--;
        refresh_cursor_char(cursor_pos, 1);
    }
}

static void input_right(void)
{
    if (cursor_pos < line_len) {
        refresh_cursor_char(cursor_pos, 0);
        cursor_pos++;
        refresh_cursor_char(cursor_pos, 1);
    }
}


// Hide cursor during command execution (visual feedback)
static void set_input_busy(uint8_t busy)
{
    if (busy) {
        // Hide cursor by redrawing character without underline
        uint16_t cur_abs = cursor_pos + 2;
        // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
        uint8_t row = INPUT_START + (cur_abs >> 6);
        uint8_t col = cur_abs & 63;
        
        if (row <= INPUT_END) {
            char c = (cursor_pos < line_len) ? line_buffer[cursor_pos] : ' ';
            input_put_char_at(cur_abs, c);
        }
    } else {
        // Restore cursor
        refresh_cursor_char(cursor_pos, 1);
    }
}

// KEYBOARD HANDLING
// Keyboard handling copied from BitStream (proven low-latency behaviour on HW).
static uint8_t last_k = 0;
static uint16_t repeat_timer = 0;
static uint8_t debounce_zero = 0;

static uint8_t read_key(void)
{
    uint8_t k = in_inkey();

    if (k == 0) {
        last_k = 0;
        repeat_timer = 0;
        if (debounce_zero > 0) debounce_zero--;
        return 0;
    }

    // Some firmwares/ROM paths can briefly report '0' during key transitions.
    if (k == '0' && debounce_zero > 0) {
        debounce_zero--;
        return 0;
    }

    // New key - return immediately
    if (k != last_k) {
        last_k = k;

        if (k == KEY_BACKSPACE) {
            repeat_timer = 12;
        } else if (k == KEY_LEFT || k == KEY_RIGHT) {
            repeat_timer = 15;
        } else {
            repeat_timer = 20;
        }

        if (k == KEY_BACKSPACE) debounce_zero = 8;
        else debounce_zero = 0;

        return k;
    }

    // Holding key - auto-repeat
    if (k == KEY_BACKSPACE) debounce_zero = 8;

    if (repeat_timer > 0) {
        repeat_timer--;
        return 0;
    }

    if (k == KEY_BACKSPACE) {
        repeat_timer = 1;
        return k;
    }
    if (k == KEY_LEFT || k == KEY_RIGHT) {
        repeat_timer = 2;
        return k;
    }
    if (k == KEY_UP || k == KEY_DOWN) {
        repeat_timer = 5;
        return k;
    }

    return 0;
}

void uart_send_crlf(void) __z88dk_fastcall
{
    ay_uart_send('\r');
    ay_uart_send('\n');
}

void uart_send_line(const char *s) __z88dk_fastcall
{
    uart_send_string(s);
    uart_send_crlf();
}

// SNTP TIME SYNC (non-blocking)
static void sntp_init(void)
{
    if (sntp_init_sent) return;
    if (connection_state < STATE_WIFI_OK) return;
    
    // Configure SNTP (timezone 1 = CET for Spain/Europe)
    uart_send_line("AT+CIPSNTPCFG=1,1,\"pool.ntp.org\"");
    sntp_init_sent = 1;
    sntp_waiting = 0;
}

static void sntp_query_time(void)
{
    if (!sntp_init_sent) return;
    if (sntp_waiting) return;
    if (connection_state < STATE_WIFI_OK) return;
    
    uart_send_line("AT+CIPSNTPTIME?");
    sntp_waiting = 1;
}

void sntp_process_response(const char *line) __z88dk_fastcall
{
    // Format: +CIPSNTPTIME:Fri Dec 27 21:45:30 2024
    // or:     +CIPSNTPTIME:Thu Jan 01 00:00:00 1970 (not synced)
    
    uint8_t i;
    
    // Check for +CIPSNTPTIME: prefix
    if (rx_pos < 20) return;
    if (line[0] != '+' || line[1] != 'C') return; // Chequeo rápido
    
    // Check if it's 1970 (not synced)
    if (rx_pos < 4) { sntp_waiting = 0; return; }
    for (i = 0; i <= (uint8_t)(rx_pos - 4); i++) {
        if (line[i] == '1' && line[i+1] == '9' &&
            line[i+2] == '7' && line[i+3] == '0') {
            sntp_waiting = 0;
            return;
        }
    }

    // Find time pattern HH:MM:SS
    for (i = 13; i < rx_pos - 7; i++) {
        if (line[i] >= '0' && line[i] <= '2' &&
            line[i+2] == ':' && line[i+5] == ':') {
            
            // Found HH:MM:SS - Update global clock directly
            time_hour = (line[i] - '0') * 10 + (line[i+1] - '0');
            time_minute = (line[i+3] - '0') * 10 + (line[i+4] - '0');
            time_second = (line[i+6] - '0') * 10 + (line[i+7] - '0');
            
            tick_counter = 0; // Reset sub-second counter for accuracy
            
            time_synced = 1;
            sntp_waiting = 0;
            draw_status_bar();
            return;
        }
    }
    
    sntp_waiting = 0;
}

// AT COMMAND HELPERS
// try_read_line_nodrain() está implementada en spectalk_asm.asm para mejor rendimiento

uint8_t wait_for_response(const char *expected, uint16_t max_frames)
{
    uint16_t frames = 0;
    rx_pos = 0;
    
    while (frames < max_frames) {
        HALT();
        
        if (in_inkey() == 7) return 0;  // EDIT = cancel
        
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            // Primero checar errors
            if (strstr(rx_line, S_ERROR) != NULL) return 0;
            if (strstr(rx_line, S_FAIL) != NULL) return 0;
            // Luego la respuesta waitda
            if (expected && strstr(rx_line, expected) != NULL) return 1;
            // Si no hay expected específico, OK es suficiente
            if (!expected && strstr(rx_line, S_OK) != NULL) return 1;
            rx_pos = 0;
        }
        
        frames++;
    }
    
    return 0;
}

// Wait for any of up to three expected substrings in a line, with standard error/FAIL/CLOSED handling.
uint8_t wait_for_prompt_char(uint8_t prompt_ch, uint16_t max_frames)
{
    uint16_t frames = 0;
    int16_t c;

    while (frames < max_frames) {
        HALT();

        if (in_inkey() == 7) return 0;  // EDIT = cancel

        uart_drain_to_buffer();

        while ((c = rb_pop()) != -1) {
            if ((uint8_t)c == prompt_ch) return 1;
        }

        frames++;
    }

    return 0;
}

// Helper para comandos AT de configuración (ahorra código repetido)
uint8_t esp_at_cmd(const char *cmd) __z88dk_fastcall
{
    uart_send_line(cmd);
    return wait_for_response(S_OK, 30);
}


// ESP/WIFI INITIALIZATION
// Drop any bytes currently buffered in the RX ring (used by flush routines)
static void rx_drop_buffered(void)
{
    rb_tail = rb_head;
}

static void uart_flush_rx(void)
{
    // Drain UART into ring and discard buffered bytes until quiescent.
    // Keeps UART reads centralized in uart_drain_to_buffer().
    uint16_t max_wait = 500;
    uint16_t max_bytes = 500;

    while (max_bytes > 0) {
        if (ay_uart_ready()) {
            uart_drain_to_buffer();
            // Drop everything we just buffered; this is a flush.
            rx_drop_buffered();
            max_bytes--;
            max_wait = 100;
        } else {
            if (max_wait == 0) break;
            max_wait--;
        }
    }
}

// Helper para comandos AT simples durante inicialización
// Envía comando y espera OK o timeout corto
static void esp_hard_cmd(const char *cmd) {
    uart_send_string(cmd);
    uart_send_string("\r\n");
    // Reutilizamos wait_for_response para ahorrar bytes
    wait_for_response(NULL, 30);
    rx_pos = 0;
}

uint8_t esp_init(void)
{
    uint8_t i;
    uint16_t frames;

    ay_uart_init();

    // waitr a que la UART se estabilice
    for (i = 0; i < 10; i++) HALT();
    uart_flush_rx();
    
    // 1. Intentar salir del modo transparente (+++)
    for (i = 0; i < 55; i++) HALT();  // 1.1s silencio
    uart_send_string("+++");
    for (i = 0; i < 55; i++) HALT();  // 1.1s silencio
    uart_flush_rx();
    
    // 2. Lista de commands de Initialization (Esto ahorra mucho código)
    static const char * const init_cmds[] = {
        "AT+CIPMODE=0",
        "AT+CIPCLOSE",
        "ATE0",
        "AT+CIPSERVER=0",
        "AT+CIPMUX=0",
        NULL // Terminador
    };
    
    const char * const *p = init_cmds; // Ajustar puntero
    while (*p) {
        esp_hard_cmd(*p++);
    }
    
    // 3. Test final AT
    uart_send_string("AT\r\n");
    
    rx_pos = 0;
    
    // Timeout ~3 segundos
    for (frames = 0; frames < 150; frames++) {
        HALT();
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            if (rx_line[0] == 'O' && rx_line[1] == 'K') {
                connection_state = STATE_WIFI_OK;
                closed_reported = 0;
                return 1;
            }
            rx_pos = 0;
        }
    }
    
    connection_state = STATE_DISCONNECTED;
    return 0;
}

// Forward declaration
void force_disconnect(void);

// Time synchronization function
static void sync_time(void)
{
    if (connection_state < STATE_WIFI_OK) return;
    
    // Initialize SNTP (sends config command, non-blocking)
    sntp_init();
    
    // Note: Time query will be sent later in main loop after sync delay
}

// Force-close any active TCP connection.
// In transparent mode, must exit with +++ first
void force_disconnect(void)
{
    uint8_t i;
    for (i = 0; i < 55; i++) {
        HALT();
        uart_drain_and_drop_all(); 
    }
    
    ay_uart_send('+');
    ay_uart_send('+');
    ay_uart_send('+');
    
    for (i = 0; i < 55; i++) {
        HALT();
        uart_drain_and_drop_all();
    }
    
    // 2. Cerrar conexión TCP explícitamente
    uart_send_line("AT+CIPCLOSE");
    // Esperamos respuesta brevemente, ignorando si da ERROR (ya cerrado)
    (void)wait_for_response(S_OK, 50);
    
    // 3. Desactivar Modo Transparente (Volver a modo normal)
    uart_send_line("AT+CIPMODE=0");
    (void)wait_for_response(S_OK, 50);

    // 4. Resetear estado lógico del cliente
    connection_state = STATE_WIFI_OK;
    closed_reported = 0;
    server_silence_frames = 0;
    keepalive_ping_sent = 0;
    keepalive_timeout = 0;
    reset_all_channels();
}

uint8_t esp_send_line_crlf_nowait(const char *line) __z88dk_fastcall
{
    const char *p;
    
    if (connection_state < STATE_TCP_CONNECTED) return 0;

    // Send line directly byte by byte (transparent mode)
    p = line;
    while (*p) {
        ay_uart_send((uint8_t)*p);
        p++;
    }
    // Send CRLF
    ay_uart_send('\r');
    ay_uart_send('\n');
    
    return 1;
}

// =============================================================================
// OPTIMIZED SENDING HELPERS (STREAMING)
// =============================================================================

// Envía "CMD param\r\n" - ahorra ~30 bytes por uso vs uart_send_string x3
void irc_send_cmd1(const char *cmd, const char *p1) __z88dk_callee
{
    if (connection_state < STATE_TCP_CONNECTED) return;
    uart_send_string(cmd);
    if (p1 && *p1) { uart_send_string(" "); uart_send_string(p1); }
    uart_send_string("\r\n");
}

// Envía "CMD p1 :p2\r\n" - para comandos con texto final
void irc_send_cmd2(const char *cmd, const char *p1, const char *p2) __z88dk_callee
{
    if (connection_state < STATE_TCP_CONNECTED) return;
    uart_send_string(cmd);
    if (p1 && *p1) { uart_send_string(" "); uart_send_string(p1); }
    if (p2 && *p2) { uart_send_string(" :"); uart_send_string(p2); }
    uart_send_string("\r\n");
}

void irc_send_privmsg(const char *target, const char *msg) __z88dk_callee
{
    // 1. ENVÍO DIRECTO
    if (connection_state >= STATE_TCP_CONNECTED) {
        uart_send_string(S_PRIVMSG);
        uart_send_string(target);
        uart_send_string(" :");
        uart_send_string(msg);
        uart_send_string("\r\n");
    }

    // 2. MOSTRAR EN screen
    if (target[0] == '#') {
        // Mensaje en canal: hora, nick dedicado, mensaje del canal
        main_print_time_prefix();

        // En 64 cols: imprimir "<nick> " entero con ATTR_MSG_NICK
        current_attr = ATTR_MSG_NICK;
        main_putc('<');
        main_puts(irc_nick);
        main_puts("> ");

        current_attr = ATTR_MSG_CHAN;
        main_print(msg);
    } else {
        int8_t query_idx = add_query(target);
        if (query_idx >= 0 && !channels[query_idx].active) status_bar_dirty = 1;

        main_print_time_prefix();

        if (query_idx >= 0 && (uint8_t)query_idx == current_channel_idx) {
            // En la ventana del privado: mantener "<YO> msg"
            current_attr = ATTR_MSG_NICK;
            main_putc('<');
            main_puts(irc_nick);
            main_puts("> ");

            current_attr = ATTR_MSG_PRIV;
            main_print(msg);
        } else {
            // Fuera de la ventana: >> NICKNAME (receptor) : MSG
            // Requisito: ">> NICKNAME (amarillo): MSG (verde)"
            current_attr = ATTR_MSG_PRIV;
            main_puts(">> ");
            main_puts(target);
            main_puts(": ");

            current_attr = ATTR_MSG_SELF;
            main_print(msg);
        }
    }
}


// ============================================================
// REST OF MAIN MODULE (apply_theme, draw_banner, init_screen, main)
// ============================================================

void apply_theme(void)
{
    const Theme *t;
    
    // Protección de rango
    if (current_theme < 1 || current_theme > 3) current_theme = 1;
    t = &themes[current_theme - 1];
    
    // 1. Cargar variables de color
    ATTR_BANNER       = t->banner;
    ATTR_STATUS       = t->status;
    ATTR_MSG_CHAN     = t->msg_chan;
    ATTR_MSG_SELF     = t->msg_self;
    ATTR_MSG_PRIV     = t->msg_priv;
    ATTR_MAIN_BG      = t->main_bg;
    ATTR_INPUT        = t->input;
    ATTR_INPUT_BG     = t->input_bg;
    ATTR_PROMPT       = t->prompt;
    ATTR_MSG_SERVER   = t->msg_server;
    ATTR_MSG_JOIN     = t->msg_join;
    ATTR_MSG_NICK     = t->msg_nick;
    ATTR_MSG_TIME     = t->msg_time;
    ATTR_MSG_TOPIC    = t->msg_topic;
    ATTR_MSG_MOTD     = t->msg_motd;
    ATTR_ERROR        = t->error;
    
    STATUS_RED        = t->ind_red;
    STATUS_YELLOW     = t->ind_yellow;
    STATUS_GREEN      = t->ind_green;
    BORDER_COLOR      = t->border;

    // 2. Aplicar cambios físicos a la pantalla
    set_border(BORDER_COLOR); 
    
    // "Barrer" la pantalla con los nuevos colores base (esto borraba el indicador)
    reapply_screen_attributes(); 
    
    // 3. RESTAURAR UI INMEDIATAMENTE (La solución)
    draw_banner();
    
    // Forzamos el flag para que draw_status_bar_real repinte el texto
    force_status_redraw = 1; 
    
    // Pintamos la barra Y EL INDICADOR ahora mismo, encima del barrido
    draw_status_bar_real(); 
    
    redraw_input_full();
}

void draw_banner(void)
{
    clear_line(TOP_BANNER_LINE, ATTR_BANNER);
    print_str64(TOP_BANNER_LINE, 1, "SPECTALK ZX - IRC CLIENT FOR ZX SPECTRUM", ATTR_BANNER);
    print_str64(TOP_BANNER_LINE, 58, "v" VERSION, ATTR_BANNER);
}

void init_screen(void)
{
    // 1. Limpieza Física (ASM): Borra pixels y aplica atributos base del tema
    cls_fast();
    
    // 2. Posición inicial del texto (línea 2 = MAIN_START)
    main_line = MAIN_START; 
    main_col = 0;
    
    // 3. Restaurar elementos fijos de la interfaz
    draw_banner();
    
    // 4. Pintar barra de estado inmediatamente (cls_fast la borró)
    force_status_redraw = 1;
    draw_status_bar_real();
    
    // 5. Resetear atributos de texto al valor por defecto
    current_attr = ATTR_MSG_CHAN;
}


// =============================================================================
// OPTIMIZED UI HELPERS (Saves ROM space vs Macros)
// =============================================================================

void ui_msg(uint8_t attr, const char *s) __z88dk_callee
{
    current_attr = attr;
    main_print(s);
}

void ui_err(const char *s) __z88dk_fastcall
{
    ui_msg(ATTR_ERROR, s);
}

void ui_sys(const char *s) __z88dk_fastcall
{
    ui_msg(ATTR_MSG_SYS, s);
}

void ui_usage(const char *a) __z88dk_fastcall
{
    current_attr = ATTR_ERROR;
    main_puts("Usage: /");
    main_print(a);
}

// MAIN
void main(void)
{
    uint8_t c;
    
    apply_theme();
    init_screen();
    
    current_attr = ATTR_MSG_SYS;
    main_puts(S_APPNAME);
    main_puts(" - ");
    main_print(S_APPDESC);
    main_print(S_COPYRIGHT);
    main_hline();
    
    // --- Initialization ---
    current_attr = ATTR_MSG_PRIV;
    main_puts("Initializing...");
    
    if (!esp_init()) {
        main_putc(' '); current_attr = ATTR_ERROR; main_puts(S_FAIL); main_newline();
        ui_sys("Press any key to retry...");
        while (!in_inkey()) HALT();
        while (in_inkey()) HALT();
        
        current_attr = ATTR_MSG_PRIV; main_puts("Initializing...");
        if (!esp_init()) {
            main_putc(' '); current_attr = ATTR_ERROR; main_puts(S_FAIL); main_newline();
        } else { main_newline(); }
    } else { main_newline(); }
    
    // Check WiFi
    current_attr = ATTR_MSG_PRIV;
    main_puts("Checking connection...");
    
    if (connection_state < STATE_WIFI_OK) {
        main_putc(' '); current_attr = ATTR_ERROR; main_puts("NO WIFI"); main_newline();
        ui_sys("Join WiFi network first");
    } else {
        main_newline();
        sync_time();  
    }
    // -------------------------------------------

    current_attr = ATTR_MSG_SYS;
    main_hline();
    main_print("Type !help or !about for more info.");
    main_print("Tip: /nick Name then /server host");
    main_newline();
    
    current_attr = ATTR_MAIN_BG;
    
    draw_status_bar();
    redraw_input_full();
    
    {
        uint8_t prev_caps_mode = caps_lock_mode;
        uint8_t prev_shift_held = 0;
        uint16_t sntp_timer = 0;
        uint8_t sntp_queried = 0;
        
        while (1) {
            HALT(); // 50 Hz Sync
            
            // --- RELOJ INCREMENTAL (Sin divisiones de 32 bits) ---
            tick_counter++;
            if (tick_counter >= 50) {
                tick_counter = 0;
                time_second++;
                if (time_second >= 60) {
                    time_second = 0;
                    time_minute++;
                    // Actualizar reloj en pantalla cada minuto
                    draw_clock(); 
                    
                    if (time_minute >= 60) {
                        time_minute = 0;
                        time_hour++;
                        if (time_hour >= 24) time_hour = 0;
                    }
                }
            }
            
            // 1. TAREAS DE BAJA FRECUENCIA
            if (sntp_init_sent | names_pending) {
                if (sntp_init_sent && !sntp_queried) {
                    if (++sntp_timer >= 150) { sntp_query_time(); sntp_queried = 1; }
                }
                if (names_pending) {
                    if (++names_timeout_frames >= NAMES_TIMEOUT_FRAMES) {
                        names_pending = 0;
                        names_timeout_frames = 0;
                        names_target_channel[0] = '\0';
                        counting_new_users = 0;
                    }
                }
            }
            
            // 1.5. KEEP-ALIVE: Detect silent disconnections
            if (connection_state == STATE_IRC_READY) {
                server_silence_frames++;
                
                if (keepalive_ping_sent) {
                    // Waiting for PONG - check timeout
                    if (++keepalive_timeout >= KEEPALIVE_TIMEOUT_FRAMES) {
                        // No response to PING - connection is dead
                        ui_err("Connection timeout (no response)");
                        force_disconnect();
                        connection_state = STATE_WIFI_OK;
                        reset_all_channels();
                        draw_status_bar();
                        keepalive_ping_sent = 0;
                        keepalive_timeout = 0;
                        server_silence_frames = 0;
                    }
                } else if (server_silence_frames >= KEEPALIVE_SILENCE_FRAMES) {
                    // No server activity for too long - send PING to check
                    uart_send_string("PING :keepalive\r\n");
                    keepalive_ping_sent = 1;
                    keepalive_timeout = 0;
                }
            } else {
                // Not connected - reset counters
                server_silence_frames = 0;
                keepalive_ping_sent = 0;
            }
            
            // 1.6. PENDING SEARCH SYSTEM
            // Timeout para search_draining: si no hay datos en ~1 segundo, liberar
            if (search_draining) {
                if (rb_head != rb_tail) {
                    search_drain_timeout = 0;  // Hay datos, resetear timeout
                } else if (++search_drain_timeout > 50) {
                    // Sin datos durante ~1 segundo, liberar drenaje
                    search_draining = 0;
                    search_drain_timeout = 0;
                }
            }
            // Intentar iniciar búsqueda pendiente
            pending_search_try_start();
            
            // 2. FLUSH STATUS BAR
            if (status_bar_dirty && !ui_freeze_status) {
                status_bar_dirty = 0;
                draw_status_bar_real();
            }
            
            // 3. INPUT Y teclado
            check_caps_toggle();
            {
                uint8_t shift_held = key_shift_held();
                if ((prev_caps_mode != caps_lock_mode) || (prev_shift_held != shift_held)) {
                    prev_caps_mode = caps_lock_mode; 
                    prev_shift_held = shift_held;
                    refresh_cursor_char(cursor_pos, 1);
                }
            }
            
            c = read_key();
            if (c >= '5' && c <= '8' && key_shift_held()) {
                if (c == '5') c = KEY_LEFT;
                else if (c == '6') c = KEY_DOWN;
                else if (c == '7') c = KEY_UP;
                else if (c == '8') c = KEY_RIGHT;
            }
            if (c != 0 && !pagination_active && search_mode == SEARCH_NONE) {
                if (c >= 32 && c <= 126) {
                    uint8_t shift = key_shift_held();
                    if (c >= 'a' && c <= 'z') { if (caps_lock_mode ^ shift) c -= 32; } 
                    else if (c >= 'A' && c <= 'Z') { if (!(caps_lock_mode ^ shift)) c += 32; }
                    input_add_char(c); 
                }
                else if (c == KEY_ENTER) {
                     if (line_len > 0) {
                        char cmd_copy[LINE_BUFFER_SIZE]; 
                        memcpy(cmd_copy, line_buffer, line_len + 1);
                        history_add(cmd_copy, line_len); 
                        input_clear();
                        set_input_busy(1); 
                        parse_user_input(cmd_copy); 
                        set_input_busy(0);
                        refresh_cursor_char(cursor_pos, 1);
                     }
                }
                else if (c == KEY_BACKSPACE) input_backspace();
                else if (c == KEY_LEFT) input_left();
                else if (c == KEY_RIGHT) input_right();
                else if (c == KEY_UP) { history_nav_up(); redraw_input_full(); }
                else if (c == KEY_DOWN) { history_nav_down(); redraw_input_full(); }
            }
            
            process_irc_data();
        }
    }
}
