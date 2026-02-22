/*
 * spectalk.c - Main module for SpecTalk ZX
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
 *
 * Portions of this code are derived from BitchZX, also licensed under GPLv2.
 */

#include "../include/spectalk.h"
// font64_data.h ya no se necesita - fuente comprimida integrada en spectalk_asm.asm
#include "../include/themes.h"

// =============================================================================
// COMPILE-TIME SAFETY: Verificar que constantes C coinciden con ASM
// Si algún assert falla, actualizar AMBOS: spectalk.h Y spectalk_asm.asm
// =============================================================================
#if RING_BUFFER_SIZE != 2048
#error "RING_BUFFER_SIZE must be 2048 (sync with spectalk_asm.asm:31,122-126)"
#endif
#if RX_LINE_SIZE != 512
#error "RX_LINE_SIZE must be 512 (sync with spectalk_asm.asm:421)"
#endif

// OPT-01: Definición de themes[] movida aquí desde themes.h para evitar duplicación
const Theme themes[THEME_COUNT] = {
    // THEME 1: DEFAULT (Blue/Cyan)
    {
        "Default",
        (PAPER_BLUE  | INK_WHITE | BRIGHT),    // banner
        (PAPER_WHITE | INK_BLUE),              // status
        (PAPER_BLACK | INK_WHITE),             // msg_chan (blanco sin brillo)
        (PAPER_BLACK | INK_YELLOW | BRIGHT),   // msg_self
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_priv
        (PAPER_BLACK | INK_WHITE),             // main_bg
        (PAPER_CYAN  | INK_BLUE),              // input
        (PAPER_CYAN  | INK_BLACK),             // input_bg
        (PAPER_CYAN  | INK_BLUE),              // prompt
        (PAPER_BLACK | INK_WHITE),             // msg_server
        (PAPER_BLACK | INK_MAGENTA),           // msg_join
        (PAPER_BLACK | INK_MAGENTA | BRIGHT),  // msg_nick
        (PAPER_BLACK | INK_CYAN | BRIGHT),     // msg_time
        (PAPER_BLACK | INK_YELLOW | BRIGHT),   // msg_topic
        (PAPER_BLACK | INK_CYAN),              // msg_motd
        (PAPER_BLACK | INK_RED | BRIGHT),      // error
        (PAPER_WHITE | INK_RED),               // ind_red
        (PAPER_WHITE | INK_YELLOW),            // ind_yellow
        (PAPER_WHITE | INK_GREEN),             // ind_green
        INK_BLACK                              // border
    },
    // THEME 2: TERMINAL (Green Monochrome)
    {
        "The Terminal",
        (PAPER_GREEN | INK_BLACK | BRIGHT),    // banner
        (PAPER_GREEN | INK_BLACK),             // status
        (PAPER_BLACK | INK_GREEN),             // msg_chan
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_self
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_priv
        (PAPER_BLACK | INK_GREEN),             // main_bg
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // input
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // input_bg
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // prompt
        (PAPER_BLACK | INK_GREEN),             // msg_server
        (PAPER_BLACK | INK_GREEN),             // msg_join
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_nick
        (PAPER_BLACK | INK_GREEN),             // msg_time
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_topic
        (PAPER_BLACK | INK_GREEN),             // msg_motd
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // error
        (PAPER_GREEN | INK_RED),               // ind_red
        (PAPER_GREEN | INK_YELLOW),            // ind_yellow
        (PAPER_GREEN | INK_BLACK),             // ind_green
        INK_BLACK                              // border
    },
    // THEME 3: COLORFUL (The Commander - Blue)
    {
        "The Commander",
        (PAPER_RED   | INK_WHITE | BRIGHT),    // banner
        (PAPER_CYAN  | INK_BLACK),             // status
        (PAPER_BLUE  | INK_CYAN),              // msg_chan
        (PAPER_BLUE  | INK_YELLOW),            // msg_self
        (PAPER_BLUE  | INK_GREEN),             // msg_priv
        (PAPER_BLUE  | INK_WHITE),             // main_bg
        (PAPER_WHITE | INK_BLUE),              // input
        (PAPER_WHITE | INK_BLUE),              // input_bg
        (PAPER_WHITE | INK_BLUE),              // prompt
        (PAPER_BLUE  | INK_WHITE),             // msg_server
        (PAPER_BLUE  | INK_MAGENTA),           // msg_join
        (PAPER_BLUE  | INK_YELLOW),            // msg_nick
        (PAPER_BLUE  | INK_WHITE),             // msg_time
        (PAPER_BLUE  | INK_YELLOW),            // msg_topic
        (PAPER_BLUE  | INK_CYAN),              // msg_motd
        (PAPER_BLUE  | INK_RED),               // error
        (PAPER_CYAN  | INK_RED),               // ind_red
        (PAPER_CYAN  | INK_YELLOW),            // ind_yellow
        (PAPER_CYAN  | INK_GREEN),             // ind_green
        INK_BLUE                               // border
    }
};

const char *theme_get_name(uint8_t theme_id)
{
    if ((uint8_t)(theme_id - 1) > 2)
        theme_id = 1;
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
const char S_FAIL[] = "FAIL";
const char S_SERVER[] = "Server";
const char S_CHANSERV[] = "ChanServ";
const char S_NOTSET[] = "(not set)";
const char S_DISCONN[] = "Disconnected";
const char S_NICKSERV[] = "NickServ";
const char S_APPNAME[] = "SpecTalk ZX v" VERSION;
const char S_APPDESC[] = "IRC Client for ZX Spectrum";
const char S_COPYRIGHT[] = "(C) 2026 M. Ignacio Monge Garcia";
const char S_LICENSE[] = "Licensed under GNU GPL v2.0";
const char S_MAXWIN[] = "Max windows reached (10).";
const char S_SWITCHTO[] = "Switched to ";
const char S_PRIVMSG[] = "PRIVMSG ";
const char S_NOTICE[]  = "NOTICE ";
const char S_TIMEOUT[] = "Connection timeout";
const char S_NOWIN[]   = "Not in Server window";
const char S_CANCELLED[] = "Cancelled";
const char S_RANGE_MINUTES[] = "Range: 1-60 minutes (0=off)";
const char S_MUST_CHAN[] = "Must be in a channel or specify one";
const char S_ARROW_IN[] = "<< ";
const char S_ARROW_OUT[] = ">> ";
const char S_EMPTY_PAT[] = "Empty pattern";
const char S_ALREADY_IN[] = "Already in ";
const char S_CRLF[] = "\r\n";
const char S_ASTERISK[] = "* ";
const char S_COLON_SP[] = ": ";
const char S_SP_COLON[] = " :";
const char S_SP_PAREN[] = " (";
const char S_AT_CIPCLOSE[] = "AT+CIPCLOSE";
const char S_AT_CIPMODE0[] = "AT+CIPMODE=0";
const char S_AT_CIPMUX0[] = "AT+CIPMUX=0";
const char S_AT_CIPSERVER0[] = "AT+CIPSERVER=0";
const char S_PROMPT[] = "> ";
const char S_CAP_END[] = "CAP END";
const char S_GLOBAL[] = "Global";
const char S_TOPIC_PFX[] = "Topic: ";
const char S_YOU_LEFT[] = "<< You left ";
const char S_CONN_REFUSED[] = "Connection refused";
const char S_INIT_DOTS[] = "Initializing...";
const char S_ACTION[] = "ACTION ";
const char S_PONG[] = "PONG ";
// OPT H3: Constantes compartidas para strings duplicados
const char S_ON[] = "on";
const char S_OFF[] = "off";
const char S_DEFAULT_PORT[] = "6667";
const char S_CONNECTED[] = "connected";  // D5
// OPT-AGG: Nuevas constantes compartidas (cross-module dedup)
const char S_DOTS3[] = "...";
const char S_NICK_INUSE[] = "Nick in use, trying: ";
const char S_NICK_SP[] = "NICK ";
const char S_AS_SP[] = " as ";
const char S_MIN[] = " min";
const char S_SET[] = "(set)";
const char S_DOT_SP[] = ". ";           // D9: dedup from search results
const char S_USAGE_MSG[] = "msg nick message"; // D9: dedup from cmd_msg
const char S_COMMA_SP[] = ", ";              // D9: dedup (3 uses)

// =============================================================================
// THEME SYSTEM - Global attributes set by apply_theme()
// =============================================================================
uint8_t current_theme = 1;  // 1=DEFAULT, 2=TERMINAL, 3=COLORFUL
uint8_t ATTR_BANNER;
uint8_t ATTR_STATUS;
uint8_t ATTR_MSG_CHAN;
uint8_t ATTR_MSG_SELF;
uint8_t ATTR_MSG_PRIV;
uint8_t ATTR_MAIN_BG;
uint8_t ATTR_INPUT;
uint8_t ATTR_INPUT_BG;
uint8_t ATTR_PROMPT;
uint8_t ATTR_MSG_SERVER;
uint8_t ATTR_MSG_JOIN;
uint8_t ATTR_MSG_NICK;
uint8_t ATTR_MSG_TIME;
uint8_t ATTR_MSG_TOPIC;
uint8_t ATTR_MSG_MOTD;
uint8_t ATTR_ERROR;
uint8_t STATUS_RED;
uint8_t STATUS_YELLOW;
uint8_t STATUS_GREEN;
uint8_t BORDER_COLOR;

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
char away_message[32] = "";  // OPT-04: Reducido de 64 a 32 bytes
uint8_t away_reply_cd = 0;   // Global cooldown (seconds) for away auto-replies

// Auto-away system
uint8_t autoaway_minutes = 0;    // 0 = disabled, 1-60 = minutes until auto-away
uint16_t autoaway_counter = 0;   // Seconds of inactivity
uint8_t autoaway_active = 0;     // 1 = current away is auto-away (not manual)

// NAMES reply state machine (robust user counting with timeout)
uint8_t names_pending = 0;
uint16_t names_timeout_frames = 0;
char names_target_channel[NAMES_TARGET_CHANNEL_SIZE];

// User preferences (toggle with commands)
uint8_t beep_enabled = 1;   // 0 = silent mode
uint8_t show_quits = 1;     // 0 = hide QUIT messages
uint8_t show_timestamps = 1; // 1 = show [HH:MM] prefix on messages
int8_t sntp_tz = 1;         // SNTP timezone offset (-12..+12), default CET

// Keep-alive system: detect silent disconnections
// KEEPALIVE_SILENCE = 3 min = 9000 frames, KEEPALIVE_TIMEOUT = 30s = 1500 frames
#define KEEPALIVE_SILENCE_FRAMES 9000
#define KEEPALIVE_TIMEOUT_FRAMES 1500
uint16_t server_silence_frames = 0;  // Frames since last server activity
uint8_t  keepalive_ping_sent = 0;    // 1 = waiting for PONG
uint16_t keepalive_timeout = 0;      // Timeout counter after PING sent
uint8_t ping_latency = 0;            // 0=good, 1=medium (>500ms), 2=high (>1000ms)

// Pagination for long LIST/WHO results
uint8_t pagination_active = 0;
uint16_t pagination_count = 0;
uint8_t pagination_lines = 0;  // Líneas impresas desde última pausa
uint8_t pagination_timeout = 0;  // Frames sin datos válidos → safety timeout
uint8_t search_data_lost = 0;    // Flag: se perdieron datos durante listado
uint8_t buffer_pressure = 0;   // 1 = buffer >75% (indicator shows empty circle)

// Flush state for search commands (simple drain-based approach)
// States: 0=idle, 1=draining, 2=command sent
uint8_t search_flush_state = 0;
static uint8_t search_flush_stable = 0;  // Frames con buffer vacío consecutivos
static uint8_t search_pending_type = 0;  // Tipo de comando pendiente
uint8_t search_header_rcvd = 0;          // Flag: recibimos 321/352 header (no rate-limited)
// NOTA: Usamos search_pattern[] para almacenar el argumento pendiente (ahorra 32 bytes)

// SEARCH state
uint8_t search_mode = SEARCH_NONE;
char    search_pattern[SEARCH_PATTERN_SIZE];  // También usado como argumento pendiente durante drenaje
uint16_t search_index = 0;

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
    
    // Skip if char unchanged AND attr matches VRAM (not just cache)
    // FIX: VRAM can be modified by draw_cursor_underline/clear_zone bypassing cache
    if (input_cache_char[r][col] == (uint8_t)ch && 
        *attr_addr(y, ax) == attr) {
        input_cache_attr[r][ax] = attr;  // Sync cache with VRAM
        return;
    }
    
    input_cache_char[r][col] = (uint8_t)ch;
    input_cache_attr[r][ax] = attr;
    
    print_char64(y, col, ch, attr);
}

// =============================================================================
// UART DRIVER AND RING buffer
// =============================================================================

// Ring buffer: single-producer/single-consumer, thread-safe en Z80 sin IRQ
uint8_t ring_buffer[RING_BUFFER_SIZE];
uint16_t rb_head = 0;
uint16_t rb_tail = 0;

// Line parser state
char rx_line[RX_LINE_SIZE];
uint16_t rx_pos = 0;
uint16_t rx_last_len = 0;
uint8_t rx_overflow = 0;  // Flag for ASM access (0 or 1)

// TIMEOUT_* values are defined in spectalk.h (single source of truth)

uint8_t uart_drain_limit = DRAIN_NORMAL;
#ifndef ST_UART_AY
#define ST_UART_AY 0
#endif
uint8_t uart_aggressive_drain = (ST_UART_AY ? 1 : 0);

// Wait frames while draining UART - prevents buffer overflow during waits
void wait_drain(uint8_t frames) __z88dk_fastcall
{
    while (frames--) {
        HALT();
        uart_drain_to_buffer();
    }
}

// Limpia completamente todos los buffers de recepción
// Usar antes de enviar comandos de búsqueda para evitar residuos
void flush_all_rx_buffers(void)
{
    // 1. Drenar UART al ring buffer (sin límite)
    uint8_t saved = uart_drain_limit;
    uart_drain_limit = 0;
    uart_drain_to_buffer();
    uart_drain_limit = saved;
    
    // 2. Vaciar ring buffer
    rb_tail = rb_head;
    
    // 3. Limpiar buffer de línea parcial
    rx_line[0] = 0;
    rx_pos = 0;
    rx_overflow = 1;  // Descartar hasta próximo \n
}


// rb_pop() está implementada en spectalk_asm.asm

// =============================================================================
// GLOBAL bufferS
// =============================================================================
char line_buffer[LINE_BUFFER_SIZE];
uint8_t line_len = 0;
uint8_t cursor_pos = 0;

// CAPS LOCK state (from BitStream)
uint8_t caps_lock_mode = 0;
uint8_t caps_latch = 0;

// =============================================================================
// IRC STATE
// =============================================================================
char irc_server[IRC_SERVER_SIZE] = "";
char irc_port[IRC_PORT_SIZE] = "6667";
char irc_nick[IRC_NICK_SIZE] = "";
char irc_pass[IRC_PASS_SIZE] = "";
char nickserv_pass[IRC_PASS_SIZE] = "";
uint8_t autoconnect = 0;
char friend_nicks[3][IRC_NICK_SIZE] = { {0}, {0}, {0} };
uint8_t friends_ison_sent = 0;
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

void nav_push(uint8_t idx)
{
    if (nav_hist_ptr > 0 && nav_history[nav_hist_ptr - 1] == idx)
        return;

    if (nav_hist_ptr >= NAV_HIST_SIZE) {
        uint8_t i;
        for (i = 0; i < (uint8_t)(NAV_HIST_SIZE - 1); i++) {
            nav_history[i] = nav_history[i + 1];
        }
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
    st_copy_n(ignore_list[ignore_count], nick, sizeof(ignore_list[0]));
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
                st_copy_n(ignore_list[j], ignore_list[j + 1], sizeof(ignore_list[0]));
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
    // OPT-08: channel_count nunca excede MAX_CHANNELS por diseño (guarded en add_slot_internal)
    ChannelInfo *ch = channels;

    for (i = 0; i < channel_count; i++, ch++) {
        if ((ch->flags & CH_FLAG_ACTIVE) && st_stricmp(ch->name, name) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

// Find query by nick (case-insensitive)
int8_t find_query(const char *nick) __z88dk_fastcall
{
    uint8_t i;

    if (!nick || !*nick) return -1;

    // FILTRO DE SERVICIOS: Forzar Slot 0 (if-else evita llamadas innecesarias)
    char c = nick[0] | 0x20;  // lowercase
    if (c == 'c') { if (st_stricmp(nick, S_CHANSERV) == 0) return 0; }
    else if (c == 'n') { if (st_stricmp(nick, S_NICKSERV) == 0) return 0; }
    else if (c == 'g') { if (st_stricmp(nick, S_GLOBAL) == 0) return 0; }

    if (irc_server[0] && st_stricmp(nick, irc_server) == 0) return 0;

    // OPT-08: channel_count nunca excede MAX_CHANNELS por diseño
    ChannelInfo *ch = &channels[1];

    for (i = 1; i < channel_count; i++, ch++) {
        if ((ch->flags & (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) == (CH_FLAG_ACTIVE | CH_FLAG_QUERY) &&
            st_stricmp(ch->name, nick) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t find_empty_channel_slot(void)
{
    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if (!(channels[i].flags & CH_FLAG_ACTIVE)) {
            return (int8_t)i;
        }
    }
    return -1;
}

// Internal: add slot with given flags
static int8_t add_slot_internal(const char *name, uint8_t flags)
{
    int8_t idx = find_empty_channel_slot();
    if (idx < 0) return -1;
    
    st_copy_n(channels[idx].name, name, sizeof(channels[0].name));
    channels[idx].mode[0] = '\0';
    channels[idx].user_count = 0;
    channels[idx].flags = flags;
    channel_count++;
    return idx;
}

// Add channel, returns index or -1 if full
int8_t add_channel(const char *name) __z88dk_fastcall
{
    return add_slot_internal(name, CH_FLAG_ACTIVE);
}

// Add query window for private messages, returns index or -1 if full
int8_t add_query(const char *nick) __z88dk_fastcall
{
    // INTERCEPCIÓN DE SERVICIOS (ChanServ, NickServ)
    if (st_stricmp(nick, S_CHANSERV) == 0 || st_stricmp(nick, S_NICKSERV) == 0) {
        st_copy_n(channels[0].name, nick, sizeof(channels[0].name));
        return 0;
    }

    // Check if query already exists
    int8_t idx = find_query(nick);
    if (idx >= 0) return idx;
    
    // CRÍTICO: Limitar queries privados para reservar slots para canales
    uint8_t query_count = 0;
    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if ((channels[i].flags & (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) == (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) {
            query_count++;
        }
    }
    
    #define MAX_QUERIES 5  // Reservar 5 slots para canales
    if (query_count >= MAX_QUERIES) {
        // Rechazar silenciosamente nuevas queries para evitar spam de error
        return -1;
    }
    
    return add_slot_internal(nick, CH_FLAG_ACTIVE | CH_FLAG_QUERY);
}

void remove_channel(uint8_t idx) __z88dk_fastcall
{
    uint8_t i;
    uint8_t next_idx = 0;
    uint8_t was_current = (idx == current_channel_idx);

    if (idx >= MAX_CHANNELS || !(channels[idx].flags & CH_FLAG_ACTIVE)) return;
    if (idx == 0) return;

    // 1. Si cerramos la ventana actual, buscar a dónde ir (usando índices ORIGINALES)
    if (was_current) {
        // Buscar en historial hacia atrás el último slot válido que no sea el que cerramos
        for (i = nav_hist_ptr; i > 0; i--) {
            uint8_t h = nav_history[i - 1];
            if (h != idx && h < MAX_CHANNELS && (channels[h].flags & CH_FLAG_ACTIVE)) {
                next_idx = h;
                break;
            }
        }
    }

    // 2. DEFRAGMENTACIÓN (mover slots hacia arriba)
    for (i = idx; i < MAX_CHANNELS - 1; i++) {
        channels[i] = channels[i + 1];
    }
    // Optimización: Invalidar slot solo con flags, evitando memset costoso
    channels[MAX_CHANNELS - 1].flags = 0;
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
    if (idx >= MAX_CHANNELS || !(channels[idx].flags & CH_FLAG_ACTIVE)) return;
    if (idx == current_channel_idx) return;

    nav_push(current_channel_idx);

    current_channel_idx = idx;

    // Clear unread + mention when user switches in
    channels[idx].flags &= (uint8_t)~(CH_FLAG_UNREAD | CH_FLAG_MENTION);

    other_channel_activity = 0;
    set_border(BORDER_COLOR);
    status_bar_dirty = 1;
}


// Reset all channels (on disconnect)
void reset_all_channels(void)
{
    uint8_t i;
    // OPT: Solo resetear slots que estaban activos
    uint8_t limit = (channel_count < MAX_CHANNELS) ? channel_count : MAX_CHANNELS;
    ChannelInfo *ch = channels;

    for (i = 0; i < limit; i++, ch++) {
        ch->flags = 0;
        ch->name[0] = '\0';
        ch->mode[0] = '\0';
        ch->user_count = 0;
    }

    // Slot 0: SIEMPRE reservado para Server / ChanServ
    channels[0].flags = CH_FLAG_ACTIVE | CH_FLAG_QUERY;  // Active + query (no user count)
    st_copy_n(channels[0].name, S_SERVER, sizeof(channels[0].name));

    current_channel_idx = 0;
    channel_count = 1;
    other_channel_activity = 0;  // FIX: limpiar indicador de actividad
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
const char* irc_param(uint8_t idx) __z88dk_fastcall
{
    if (idx >= irc_param_count || !irc_params[idx]) return "";
    return irc_params[idx];
}

// cursor visibility control (shared across modules)
uint8_t cursor_visible = 1;  // cursor only visible when user can type

// Connection state tracking (shared across modules)
uint8_t closed_reported = 0;
uint8_t disconnecting_in_progress = 0;  // FIX: Prevent reentrant disconnection

// TIME TRACKING (Optimized: No 32-bit math)
// Eliminamos uptime_frames y time_sync_frames para ahorrar librería de división
uint8_t time_hour = 0;
uint8_t time_minute = 0;
uint8_t time_second = 0;        // Nuevo: Segundos explícitos
uint8_t sntp_init_sent = 0;     // Flag: SNTP init commands sent (not static - needs reset on disconnect)
uint8_t sntp_waiting = 0;       // Flag: waiting for SNTP response

// Simple ticker for 50Hz counting
static uint8_t tick_counter = 0;

// SCREEN STATE
uint8_t main_line = MAIN_START;
uint8_t main_col = 0;
uint8_t wrap_indent = 0;  // Indentación para líneas que continúan (wrap)
uint8_t current_attr;  // Initialized in apply_theme()
// COMMAND HISTORY
#define HISTORY_SIZE    4
// E2: Reduced from 128 to 96 - saves 128 bytes BSS
#define HISTORY_LEN     96

static char history[HISTORY_SIZE][HISTORY_LEN];
static uint8_t hist_head = 0;
static uint8_t hist_count = 0;
static int8_t hist_pos = -1;
static char temp_input[LINE_BUFFER_SIZE];

static void history_add(const char *cmd, uint8_t len)
{
    uint8_t i;
    if (len == 0) return;
    
    // Ignorar si solo hay espacios
    for (i = 0; i < len && cmd[i] == ' '; i++);
    if (i == len) return;
    
    if (hist_count > 0) {
        // OPTIMIZADO: % 4 -> & 3
        uint8_t last = (hist_head + HISTORY_SIZE - 1) & 3;
        // OPT M5: st_stricmp ya linkeado, strcmp no
        if (st_stricmp(history[last], cmd) == 0) return;
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
    
    st_copy_n(line_buffer, history[idx], sizeof(line_buffer));
    line_len = st_strlen(line_buffer);
    cursor_pos = line_len;
}

static void history_nav_down(void)
{
    uint8_t idx;
    if (hist_pos < 0) return;
    hist_pos--;
    if (hist_pos < 0) {
        uint8_t tlen = st_strlen(temp_input);
        memcpy(line_buffer, temp_input, tlen + 1);
        line_len = tlen;
    } else {
        // OPTIMIZADO: % 4 -> & 3
        idx = (hist_head + HISTORY_SIZE - 1 - hist_pos) & 3;
        st_copy_n(line_buffer, history[idx], sizeof(line_buffer));
        line_len = st_strlen(line_buffer);
    }
    cursor_pos = line_len;
}

// Draw 1-pixel horizontal line - faster and more elegant than '-' chars
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

// Helper interno para pausa de paginación (retorna 1 si cancelado)
uint8_t pagination_pause(void)
{
    uint8_t key;
    uint16_t backlog;
    uint8_t prev_pressure;
    uint8_t ui_throttle = 1;

    // Mostrar prompt en la última línea visible
    print_line64_fast(MAIN_END, "-- Any key: more | EDIT: cancel --", ATTR_MSG_SYS);

    while (in_inkey() != 0) { HALT(); }
    while ((key = in_inkey()) == 0) {
        HALT();
        uart_drain_to_buffer();

        prev_pressure = buffer_pressure;
        backlog = (rb_head - rb_tail) & RING_BUFFER_MASK;
        buffer_pressure = (backlog > BUFFER_PRESSURE_THRESHOLD) ? 1 : 0;

        if (backlog > (RING_BUFFER_SIZE - BUFFER_CRITICAL_MARGIN)) {
            search_data_lost = 1;
            rb_tail = rb_head;   // descarte total O(1)
            rx_pos = 0;          // FIX: Descartar línea parcial en curso
            rx_overflow = 1;     // FIX: Descartar bytes hasta próximo \n
            buffer_pressure = 0;
        }

        // Redibujar indicador si cambió, con throttling barato
        // FIX: Transición a 0 (presión aliviada) siempre se redibuja inmediatamente
        if (buffer_pressure != prev_pressure) {
            if (buffer_pressure == 0 || --ui_throttle == 0) {
                ui_throttle = 8;
                draw_status_bar_real();
            }
        } else {
            ui_throttle = 1;
        }
    }
    while (in_inkey() != 0) { HALT(); }

    if (buffer_pressure) {
        buffer_pressure = 0;
        draw_status_bar_real();
    }

    if (key == 7) {  // EDIT = cancelar
        print_line64_fast(MAIN_END, S_CANCELLED, ATTR_ERROR);
        main_line = MAIN_END;  // FIX: Posicionar cursor en fin de zona para que
        main_col = 64;         //      el próximo main_putc fuerce scroll vía main_newline.
                               //      Así "Cancelled" se preserva y el nuevo texto va debajo.
        flush_all_rx_buffers();  // Limpiar UART, ring buffer y línea parcial
        cancel_search_state();
        return 1;
    }

    clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
    main_line = MAIN_START;
    main_col = 0;
    pagination_lines = 0;
    return 0;
}


/* main_newline moved to ASM (see asm/spectalk_asm.asm) */
extern void main_newline(void);

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


void main_run_u16(uint16_t val, uint8_t attr) __z88dk_callee
{
    static char buf[8];  // OPTIMIZACIÓN: Alojado en BSS
    char *q = u16_to_dec(buf, val);
    (void)q;  
    
    ensure_attr_alignment(attr);
    
    g_ps64_y = main_line;
    g_ps64_attr = attr;
    
    q = buf;
    while (*q) {
        if (main_col >= SCREEN_COLS) {
            main_newline();
            g_ps64_y = main_line;
        }
        g_ps64_col = main_col;
        print_str64_char((uint8_t)*q++);
        main_col++;
    }
}


void main_print(const char *s) __z88dk_fastcall
{
    // Fast path: Solo si estamos al inicio de la línea y el texto cabe en una línea
    // Para textos largos, usamos slow path que hace wrap automático
    
    if (main_col == 0 && wrap_indent == 0) {
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
    wrap_indent = 0;  // Resetear indentación antes del newline final
    main_newline();
}

// Draw 1-pixel separator line in main area
void main_hline(void)
{
    uint8_t x;
    uint8_t *screen_ptr;
    uint8_t *attr_ptr;
    
    if (main_col > 0) main_newline();
    
    // Inline: draw_hline(main_line, 0, 32, 3, current_attr)
    screen_ptr = (uint8_t*)(screen_row_base[main_line] + (3 << 8));  // scanline 3
    attr_ptr = (uint8_t*)(0x5800 + (uint16_t)main_line * 32);
    for (x = 0; x < 32; x++) {
        *screen_ptr++ = 0xFF;
        *attr_ptr++ = current_attr;
    }
    
    main_newline();
}

// =============================================================================
// SEARCH SYSTEM — Lógica simplificada
// =============================================================================

void cancel_search_state(void)
{
    pagination_active = 0;
    pagination_count = 0;
    pagination_lines = 0;
    search_mode = SEARCH_NONE;
    search_index = 0;
    pagination_timeout = 0;
    search_data_lost = 0;
    
    // Reset flush state
    search_flush_state = 0;
    search_flush_stable = 0;
    search_pending_type = 0;
    search_header_rcvd = 0;

    cursor_visible = 1;
    redraw_input_full();

    show_names_list = 0;
    buffer_pressure = 0;
    status_bar_dirty = 1;
}

// Inicia modo paginación preparando la pantalla
void start_pagination(void)
{
    clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
    main_line = MAIN_START;
    main_col = 0;
    pagination_active = 1;
    pagination_count = 0;
    pagination_lines = 0;
    pagination_timeout = 0;
    search_data_lost = 0;
    
    cursor_visible = 0;
    redraw_input_full();
}

// Envía el comando IRC real (llamada cuando el drenaje ha terminado)
// NOTA: search_pattern ya contiene el argumento (guardado en start_search_command)
// OPT M8: Lógica simplificada
static void send_pending_search_command(void)
{
    uint8_t is_chan = (search_pending_type == PEND_LIST || search_pending_type == PEND_SEARCH_CHAN);
    search_mode = is_chan ? SEARCH_CHAN : SEARCH_USER;
    
    rx_overflow = 0;  // FIX: No perder respuesta del servidor
    
    if (search_pending_type == PEND_SEARCH_CHAN) {
        uart_send_string("LIST *");
        uart_send_string(search_pattern);
        uart_send_string("*\r\n");
    } else {
        irc_send_cmd1(is_chan ? "LIST" : "WHO", search_pattern);
        if (search_pending_type == PEND_LIST || search_pending_type == PEND_WHO) 
            search_pattern[0] = 0;
    }
}

// Inicia un comando de búsqueda con drenaje progresivo.
// Fase 1: Vacía el buffer activamente hasta que quede estable (vacío X frames).
// Fase 2: Envía el comando y procesa respuestas normalmente.
void start_search_command(uint8_t type, const char *arg)
{
    // 1. Cancelar búsqueda previa si la había
    cancel_search_state();
    
    // 2. Guardar argumento en search_pattern (arg ya viene truncado por caller)
    st_copy_n(search_pattern, arg ? arg : "", sizeof(search_pattern));
    search_pending_type = type;

    // 3. Preparar pantalla
    start_pagination();
    search_index = 0;
    
    // 4. Mostrar feedback inmediato
    current_attr = ATTR_MSG_SYS;
    main_print("Searching...");
    
    // 5. Vaciar TODOS los buffers y comenzar fase de drenaje
    flush_all_rx_buffers();
    
    search_flush_state = 1;   // Estado: drenando
    search_flush_stable = 0;  // Contador de frames estables
    pagination_timeout = 0;
}

// STATUS BAR

static void draw_clock(void)
{
    static char time_part[8];
    char *p = time_part;
    
    *p++ = '[';
    fast_u8_to_str(p, time_hour); p += 2;
    *p++ = ':';
    fast_u8_to_str(p, time_minute); p += 2;
    *p++ = ']';
    *p = 0;
    
    print_str64(INFO_LINE, 54, time_part, ATTR_STATUS);
}

// Print timestamp prefix: [HH:MM] - implemented in ASM (spectalk_asm.asm)
extern void main_print_time_prefix(void);


// sb_append está implementada en ASM (spectalk_asm.asm)
extern char *sb_append(char *dst, const char *src, const char *limit);

// Extraer nombre de red del hostname: chat.freenode.net → freenode
static char *extract_network_short(char *hostname)
{
    static char net_short[12];
    char *first_dot = strchr(hostname, '.');
    if (!first_dot) return hostname;
    
    char *src = first_dot + 1;
    char *second_dot = strchr(src, '.');
    if (!second_dot) return hostname;
    
    uint8_t len = (uint8_t)(second_dot - src);
    if (len > 11) len = 11;
    memcpy(net_short, src, len);
    net_short[len] = '\0';
    return net_short;
}

// Variables estáticas para caché de repintado (estas SÍ deben ser estáticas para persistir)
// OPT H6: Reducido de 64 a 57 (máximo usado: índice 56 + null)
static char sb_last_status[57] = ""; 
static char sb_left_part[57];  // Buffer estático (ahorra stack frame)
uint8_t force_status_redraw = 1;

static void draw_connection_indicator(void)
{
    uint8_t ind_attr;

    if (connection_state >= STATE_TCP_CONNECTED) {
        ind_attr = STATUS_GREEN;  // Conectado (TCP o IRC)
    } else if (connection_state >= STATE_WIFI_OK) {
        ind_attr = STATUS_YELLOW; // WiFi OK pero sin conexión TCP
    } else {
        ind_attr = STATUS_RED;    // Sin WiFi
    }

    // Llamar a rutina gráfica ASM 
    // Coordenadas: INFO_LINE (21), Columna Física 31 (Extremo derecho)
    draw_indicator(INFO_LINE, 31, ind_attr);
}

static char* sb_put_u8_2d(char *p, uint8_t v)
{
    // Mantiene exactamente la semántica actual: 0..19 (>=10 usa '1' y (v-10))
    if (v >= 10) {
        *p++ = '1';
        *p++ = (char)('0' + (v - 10));
    } else {
        *p++ = (char)('0' + v);
    }
    return p;
}

static const char* sb_pick_status(uint8_t prefer_server_full)
{
    // prefer_server_full=1: prioridad a irc_server (hostname completo)
    // prefer_server_full=0: prioridad a network_name, o short(irc_server)
    if (connection_state >= STATE_TCP_CONNECTED) {
        if (prefer_server_full) {
            if (irc_server[0])  return irc_server;
            if (network_name[0]) return network_name;
            return S_CONNECTED;
        } else {
            if (network_name[0]) return network_name;
            if (irc_server[0])   return extract_network_short(irc_server);
            return S_CONNECTED;
        }
    }
    if (connection_state == STATE_WIFI_OK) return "wifi-ready";
    return "offline";
}

extern char *u16_to_dec3(char *dst, uint16_t v);
// D10: Extracted channel display logic — reduces SDCC register pressure
// Returns updated pointer p. Writes channel name, modes, and network
// info into the status bar buffer, truncating as needed to fit.
static char *sb_format_channel(char *p, char *central_limit, uint8_t cur_flags)
{
    uint8_t space;
    char *chan, *modes, *net;
    uint8_t chan_len, mode_len, net_len;

    if (p >= central_limit) p = central_limit - 1;
    space = (uint8_t)(central_limit - p);
    if (space) space--;  // reservar 1 char para el ']'
    if (!space) return p;

    chan = (current_channel_idx == 0 && network_name[0]) ? network_name : irc_channel;
    modes = (!(cur_flags & CH_FLAG_QUERY) && chan_mode[0]) ? chan_mode : NULL;
    net = NULL;

    chan_len = (uint8_t)st_strlen(chan);
    mode_len = modes ? (uint8_t)st_strlen(modes) + 2 : 0;  // "(modes)"
    net_len = 0;

    if (current_channel_idx != 0) {
        net = network_name[0] ? network_name : (irc_server[0] ? extract_network_short(irc_server) : NULL);
        if (net) net_len = (uint8_t)st_strlen(net) + 1;  // "@net"
    }

    // Greedy fit: try chan+modes+net, drop decorations that don't fit
    // Instead of 4-way branch, subtract greedily
    if (chan_len + mode_len + net_len > space) {
        // Can't fit all — drop modes first (net more useful)
        if (chan_len + net_len > space) {
            net = NULL; net_len = 0;
            if (chan_len + mode_len > space) {
                modes = NULL; mode_len = 0;
            }
        } else {
            modes = NULL; mode_len = 0;
        }
    }

    // Render channel name (truncated if needed)
    {
        char *chan_limit = p + (space - mode_len - net_len);
        if (chan_limit >= central_limit) chan_limit = central_limit - 1;
        p = sb_append(p, chan, chan_limit);
    }

    if (modes) {
        *p++ = '(';
        p = sb_append(p, modes, central_limit - (net_len ? net_len + 1 : 1));
        *p++ = ')';
    }
    if (net) {
        *p++ = '@';
        p = sb_append(p, net, central_limit - 1);
    }
    return p;
}

void draw_status_bar_real(void)
{
    char *p = sb_left_part;
    char *const limit_end = sb_left_part + 56;

    uint8_t cur_flags = channels[current_channel_idx].flags;
    uint8_t is_query = (cur_flags & CH_FLAG_QUERY) && current_channel_idx != 0;

    uint8_t has_mention = 0;
    {
        uint8_t i;
        for (i = 0; i < MAX_CHANNELS; i++) {
            uint8_t f = channels[i].flags;
            if ((f & (CH_FLAG_ACTIVE | CH_FLAG_MENTION)) == (CH_FLAG_ACTIVE | CH_FLAG_MENTION) &&
                i != current_channel_idx) {
                has_mention = 1;
                break;
            }
        }
    }

    *p++ = '[';
    if (irc_nick[0]) {
        p = sb_append(p, irc_nick, sb_left_part + 11);
        if (user_mode[0]) {
            *p++ = '('; p = sb_append(p, user_mode, sb_left_part + 15); *p++ = ')';
        }
    } else {
        p = sb_append(p, "no-nick", sb_left_part + 10);
    }
    *p++ = ']'; *p++ = ' '; *p++ = '[';

    static char user_buf[8];
    uint8_t user_len = 0;
    user_buf[0] = 0;

    if (irc_channel[0] && !(cur_flags & CH_FLAG_QUERY) && chan_user_count > 0) {
        char *u_end = u16_to_dec3(user_buf, chan_user_count);
        user_len = (uint8_t)(u_end - user_buf) + 3;  // " " + "[]"
    }

    {
        char *central_limit = limit_end - user_len;

        if (has_mention) *p++ = (time_second & 1) ? ' ' : '!';
        else if (other_channel_activity) *p++ = '*';

        if (channel_count > 1) {
            p = sb_put_u8_2d(p, current_channel_idx);
            *p++ = '/';
            p = sb_put_u8_2d(p, channel_count - 1);
            *p++ = ':';
        }

        // D10: Inline is_server_tab check instead of storing in variable
        if (current_channel_idx == 0 && st_stricmp(channels[0].name, S_SERVER) == 0) {
            p = sb_append(p, (char*)sb_pick_status(1), central_limit - 1);
        } else if (irc_channel[0]) {
            if (is_query) *p++ = '@';
            p = sb_format_channel(p, central_limit, cur_flags);
        } else {
            p = sb_append(p, (char*)sb_pick_status(0), central_limit - 1);
        }

        *p++ = ']';
    }

    if (user_buf[0]) {
        *p++ = ' ';
        *p++ = '[';
        p = sb_append(p, user_buf, limit_end - 1);
        *p++ = ']';
    }

    while (p < limit_end) *p++ = ' ';
    *p = 0;

    {
        uint8_t first = 255, last = 0;

        if (force_status_redraw) {
            first = 0;
            last = 55;
        } else {
            char *p_new = sb_left_part;
            char *p_old = sb_last_status;
            uint8_t i = 0;
            do {
                if (*p_new++ != *p_old++) {
                    if (first == 255) first = i;
                    last = i;
                }
            } while (++i < 56);
        }

        if (first != 255) {
            char saved = sb_left_part[last + 1];
            sb_left_part[last + 1] = 0;
            print_str64(INFO_LINE, first, sb_left_part + first, ATTR_STATUS);
            sb_left_part[last + 1] = saved;
            memcpy(sb_last_status, sb_left_part, 57);
            force_status_redraw = 0;
        }
    }

    draw_clock();
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

void refresh_cursor_char(uint8_t idx, uint8_t show_cursor)
{
    uint8_t abs_pos = idx + 2;
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
    print_str64(INPUT_START, 0, S_PROMPT, ATTR_PROMPT);
}

static void input_put_char_at(uint8_t abs_pos, char c)
{
    // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
    uint8_t row = INPUT_START + (abs_pos >> 6);
    uint8_t col = abs_pos & 63;
    
    if (row > INPUT_END) return;
    put_char64_input_cached(row, col, c, ATTR_INPUT);
}

static void redraw_input_from(uint8_t start_pos)
{
    uint8_t i;
    uint8_t abs_pos;
    uint8_t row, col;

    // 1. Si redibujamos desde el inicio, pintar prompt
    if (start_pos == 0) {
        input_draw_prompt();
    }

    // 2. Dibujar el texto actual (usando caché normal)
    for (i = start_pos; i < line_len; i++) {
        abs_pos = i + 2;
        // Cálculo seguro de coordenadas
        row = INPUT_START + (abs_pos >> 6);
        col = abs_pos & 63;
        
        if (row > INPUT_END) break;
        put_char64_input_cached(row, col, line_buffer[i], ATTR_INPUT);
    }

    // 3. Limpieza Segura: Borrar SOLO el carácter siguiente al final del texto.
    // IMPORTANTE: en 64 cols, 1 atributo cubre 2 chars (col>>1).
    // Si usamos ATTR_INPUT_BG aquí, puede cambiar el atributo del último char real (alternancia).
    abs_pos = line_len + 2;
    row = INPUT_START + (abs_pos >> 6);
    col = abs_pos & 63;
    
    if (row <= INPUT_END) {
        // Mantener el mismo atributo que el input para no contaminar la celda de atributos compartida
        put_char64_input_cached(row, col, ' ', ATTR_INPUT);
    }

    // 4. Restaurar cursor
    refresh_cursor_char(cursor_pos, 1);
}


void redraw_input_full(void)
{
    input_cache_invalidate();
    redraw_input_asm();
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
    char tens = '0';
    // Bucle simple: Ocupa menos bytes que los if/else de rangos
    while (val >= 10) {
        val -= 10;
        tens++;
    }
    buf[0] = tens;
    buf[1] = '0' + val;
}

static void input_add_char(char c) __z88dk_fastcall
{
    // 1. Borrar cursor visual
    refresh_cursor_char(cursor_pos, 0);

    // Límite: LINE_BUFFER_SIZE - 2 para dejar espacio para el terminador '\0'
    // y evitar que line_buffer[line_len] = 0 escriba fuera del buffer
    // Con LINE_BUFFER_SIZE=128, el máximo line_len es 126, y escribimos en [127] el '\0'
    if (c >= 32 && c < 127 && line_len < (LINE_BUFFER_SIZE - 2)) {
        
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
            input_put_char_at((uint8_t)(cursor_pos - 1) + 2, c);
            refresh_cursor_char(cursor_pos, 1);
        } else {
            redraw_input_from((uint8_t)(cursor_pos - 1));
        }
    }
}

static void input_backspace(void)
{
    if (cursor_pos > 0) {
        uint8_t old_len;

        // OPTIMIZACIÓN: Eliminada la llamada redundante a refresh_cursor_char.
        // Redraw_input_from ya asume la limpieza del final del string.
        refresh_cursor_char(cursor_pos, 0);
        old_len = line_len;
        cursor_pos--;

        text_shift_left(&line_buffer[cursor_pos], (uint16_t)(old_len - cursor_pos));

        line_len = (uint8_t)(old_len - 1);

        redraw_input_from(cursor_pos);
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
        uint8_t cur_abs = cursor_pos + 2;
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
static uint8_t last_k = 0;
static uint8_t repeat_timer = 0;
static uint8_t debounce_zero = 0;

static uint8_t read_key(void)
{
    uint8_t k = in_inkey();

    if (k == 0) {
        last_k = 0;
        repeat_timer = 0;
        if (debounce_zero) debounce_zero--;
        return 0;
    }

    // Some firmwares/ROM paths can briefly report '0' during key transitions.
    if (k == '0' && debounce_zero > 0) {
        debounce_zero--;
        return 0;
    }

    // New key - return immediately
    if (k != last_k) {
        // Ignorar cambio de mayúscula a minúscula (soltar Shift)
        // Ej: 'S' seguido de 's' = mismo carácter, solo cambió el shift
        {
            uint8_t lk = last_k;
            uint8_t lk_fold = (uint8_t)(lk | 32);

            if ((uint8_t)(k | 32) == lk_fold &&
                (lk_fold >= 'a' && lk_fold <= 'z')) {
                last_k = k;  // Actualizar pero no emitir
                return 0;
            }
        }

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
    
    // Configure SNTP with configurable timezone
    uart_send_string("AT+CIPSNTPCFG=1,");
    {
        int8_t tz = sntp_tz;
        char buf[5];
        char *p = buf;
        if (tz < 0) { *p++ = '-'; tz = -tz; }
        fast_u8_to_str(p, (uint8_t)tz);
        // fast_u8_to_str writes 2 chars: tens, units (no null)
        if (p[0] == '0') { p[0] = p[1]; p[1] = '\0'; }  // strip leading 0
        else { p[2] = '\0'; }
        uart_send_string(buf);
    }
    uart_send_line(",\"pool.ntp.org\"");
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
    uint8_t i;
    uint8_t len;

    if (!line || !*line) return;

    len = 0;
    while (line[len]) len++;

    if (len < 20) return;
    if (line[0] != '+' || line[1] != 'C') return;

    if (len >= 4 && line[len-4]=='1' && line[len-3]=='9' && 
        line[len-2]=='7' && line[len-1]=='0') {
        sntp_waiting = 0;
        return;
    }

    for (i = 13; i < (uint8_t)(len - 7); i++) {
        if (line[i] >= '0' && line[i] <= '2' &&
            line[i + 2] == ':' && line[i + 5] == ':') {

            {
                const char *p = &line[i];
                time_hour   = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
                time_minute = (uint8_t)((p[3] - '0') * 10 + (p[4] - '0'));
                time_second = (uint8_t)((p[6] - '0') * 10 + (p[7] - '0'));
            }

            tick_counter = 0; 
            // OPTIMIZACIÓN: Variable fantasma time_synced aniquilada
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
            // Prefix checks para ERROR/FAIL/OK (siempre al inicio de línea)
            if (rx_line[0] == 'E' && rx_line[1] == 'R') return 0;  // ERROR
            if (rx_line[0] == 'F' && rx_line[1] == 'A') return 0;  // FAIL
            // OPT-02: Usar st_stristr (ASM) en lugar de strstr (stdlib)
            if (expected && st_stristr(rx_line, expected) != NULL) return 1;
            // Si no hay expected específico, OK es suficiente
            if (!expected && rx_line[0] == 'O' && rx_line[1] == 'K') return 1;
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
// OPT L1: rx_drop_buffered eliminada (no usada)

// Helper para comandos AT simples durante inicialización
// Envía comando y espera OK o timeout corto
static void esp_hard_cmd(const char *cmd) {
    uart_send_string(cmd);
    uart_send_crlf();
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
    flush_all_rx_buffers();
    
    // 1. Intentar salir del modo transparente (+++)
    for (i = 0; i < 55; i++) HALT();  // 1.1s silencio
    uart_send_string("+++");
    for (i = 0; i < 55; i++) HALT();  // 1.1s silencio
    flush_all_rx_buffers();
    
    // 2. Lista de commands de Initialization (Esto ahorra mucho código)
    static const char * const init_cmds[] = {
        S_AT_CIPMODE0,
        S_AT_CIPCLOSE,
        "ATE0",
        S_AT_CIPSERVER0,
        S_AT_CIPMUX0,
        NULL // Terminador
    };
    
    const char * const *p = init_cmds; // Ajustar puntero
    while (*p) {
        esp_hard_cmd(*p++);
    }
    
    // 3. Test final AT - OPT M7
    uart_send_line("AT");
    
    rx_pos = 0;
    
    // Timeout ~3 segundos
    for (frames = 0; frames < 150; frames++) {
        HALT();
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            if (rx_line[0] == 'O' && rx_line[1] == 'K') {
                // ESP responds — check if WiFi has an IP
                uart_send_line("AT+CIFSR");
                rx_pos = 0;
                {
                    uint8_t has_ip = 0;
                    uint8_t w;
                    for (w = 0; w < 100; w++) {
                        HALT(); uart_drain_to_buffer();
                        if (try_read_line_nodrain()) {
                            if (rx_line[0] == '+' && st_stristr(rx_line, "STAIP")) {
                                if (!st_stristr(rx_line, "0.0.0.0")) has_ip = 1;
                            }
                            if (rx_line[0] == 'O' && rx_line[1] == 'K') break;
                            rx_pos = 0;
                        }
                    }
                    closed_reported = 0;
                    if (has_ip) {
                        connection_state = STATE_WIFI_OK;
                    } else {
                        connection_state = STATE_DISCONNECTED;
                    }
                }
                return 1;  // ESP init OK (WiFi status in connection_state)
            }
            rx_pos = 0;
        }
    }
    
    connection_state = STATE_DISCONNECTED;
    return 0;  // ESP not responding
}

// Forward declaration
void force_disconnect(void);
void force_disconnect_wifi(void);  // disconnect + reset to WiFi state

// Time synchronization function
// OPT L2: sync_time() eliminada - inlined en call site

// Helper: Force disconnect and reset state to WiFi OK
void force_disconnect_wifi(void)
{
    force_disconnect();
    connection_state = STATE_WIFI_OK;
}

// Force-close any active TCP connection.
// FIX ChatGPT audit: CENTRALIZED session reset - ALL disconnection paths MUST use this
// to avoid forgotten flags. Do NOT reset state manually elsewhere.
// In transparent mode, must exit with +++ first
void force_disconnect(void)
{
    uint8_t i;
    
    if (disconnecting_in_progress) return;
    disconnecting_in_progress = 1;
    
    if (connection_state >= STATE_TCP_CONNECTED) {
        for (i = 0; i < 65; i++) { HALT(); flush_all_rx_buffers(); }
        
        ay_uart_send('+'); ay_uart_send('+'); ay_uart_send('+');
        
        for (i = 0; i < 55; i++) { HALT(); flush_all_rx_buffers(); }
    }
    
    uart_send_line(S_AT_CIPCLOSE);
    (void)wait_for_response(S_OK, 50);
    
    uart_send_line(S_AT_CIPMODE0);
    (void)wait_for_response(S_OK, 50);

    connection_state = STATE_WIFI_OK;
    closed_reported = 0;
    
    server_silence_frames = 0;
    keepalive_ping_sent = 0;
    keepalive_timeout = 0;

    friends_ison_sent = 0;
    
    irc_is_away = 0;
    away_message[0] = '\0';
    away_reply_cd = 0;
    autoaway_counter = 0;
    autoaway_active = 0;
    
    // sntp_init_sent NOT reset — AT+CIPSNTPCFG persists in ESP8266
    sntp_waiting = 0;
    
    network_name[0] = '\0';
    user_mode[0] = '\0';
    
    names_pending = 0;
    names_timeout_frames = 0;
    names_target_channel[0] = '\0';
    counting_new_users = 0;
    
    cancel_search_state();
    
    rx_pos = 0;
    rx_overflow = 0;
    
    reset_all_channels();
    
    disconnecting_in_progress = 0;
}
// =============================================================================
// OPTIMIZED SENDING HELPERS (STREAMING)
// =============================================================================

// Internal: envía comando IRC con 0, 1 o 2 parámetros (implementada en ASM)
extern void irc_send_cmd_internal(const char *cmd, const char *p1, const char *p2);

// Envía "CMD param\r\n"
void irc_send_cmd1(const char *cmd, const char *p1) __z88dk_callee
{
    irc_send_cmd_internal(cmd, p1, NULL);
}

// Envía "CMD p1 :p2\r\n" - para comandos con texto final
void irc_send_cmd2(const char *cmd, const char *p1, const char *p2) __z88dk_callee
{
    irc_send_cmd_internal(cmd, p1, p2);
}

// Enviar ISON con hasta 3 nicks de amigos (una sola vez por sesión IRC).
void irc_check_friends_online(void)
{
    static char ison_buf[(IRC_NICK_SIZE * 3u) + 2u];
    uint8_t i, any = 0;
    char *d = ison_buf;

    if (friends_ison_sent) return;
    if (connection_state < STATE_TCP_CONNECTED) return;

    *d = '\0';

    for (i = 0; i < 3; i++) {
        const char *s = friend_nicks[i];
        if (!s[0]) continue;
        if (any) *d++ = ' ';
        st_copy_n(d, s, IRC_NICK_SIZE);
        while (*d) d++;
        any = 1;
    }

    if (!any || !ison_buf[0]) return;
    friends_ison_sent = 1;
    irc_send_cmd1("ISON", ison_buf);
}

// OPT-P2-B: Shared nick-in-use retry logic (dedup h_numeric_433 + cmd_connect)
void nick_try_alternate(void)
{
    uint8_t len = 0;
    while (irc_nick[len] && len < IRC_NICK_SIZE - 2) len++;
    irc_nick[len] = '_';
    irc_nick[len + 1] = '\0';

    current_attr = ATTR_MSG_SYS;
    main_puts(S_NICK_INUSE);
    main_print(irc_nick);

    uart_send_string(S_NICK_SP);
    uart_send_line(irc_nick);
}

void irc_send_privmsg(const char *target, const char *msg) __z88dk_callee
{
    // Auto-away: reset counter on activity, clear if auto-away active
    autoaway_counter = 0;
    if (autoaway_active) {
        uart_send_line("AWAY");
        autoaway_active = 0;
    }
    
    // 1. ENVÍO DIRECTO
    if (connection_state >= STATE_TCP_CONNECTED) {
        uart_send_string(S_PRIVMSG);
        uart_send_string(target);
        uart_send_string(S_SP_COLON);
        uart_send_string(msg);
        uart_send_crlf();
    }

    // 2. MOSTRAR EN screen
    if (target[0] == '#') {
        // Mensaje en canal: hora, nick dedicado, mensaje del canal
        main_print_time_prefix();

        // En 64 cols: imprimir "<nick> " entero con ATTR_MSG_NICK
        current_attr = ATTR_MSG_NICK;
        main_putc('<');
        main_puts2(irc_nick, S_PROMPT);

        current_attr = ATTR_MSG_CHAN;
        main_print_wrapped_ram((char*)msg);
    } else {
        // FIX: detectar creación de query para forzar refresh de status bar
        int8_t query_idx = find_query(target);
        if (query_idx < 0) {
            query_idx = add_query(target);
            if (query_idx >= 0) status_bar_dirty = 1;  // slot nuevo (o rename de slot 0 en servicios)
        }

        main_print_time_prefix();

        if (query_idx >= 0 && (uint8_t)query_idx == current_channel_idx) {
            // En la ventana del privado: mantener "<YO> msg"
            current_attr = ATTR_MSG_NICK;
            main_putc('<');
            main_puts2(irc_nick, S_PROMPT);

            current_attr = ATTR_MSG_PRIV;
            main_print_wrapped_ram((char*)msg);
        } else {
            // Fuera de la ventana: >> NICKNAME (receptor) : MSG
            // Requisito: ">> NICKNAME (amarillo): MSG (verde)"
            current_attr = ATTR_MSG_PRIV;
            main_puts2(S_ARROW_OUT, target);
            main_puts(S_COLON_SP);

            current_attr = ATTR_MSG_SELF;
            main_print_wrapped_ram((char*)msg);
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
    
    // 1. Cargar variables de color (20 bytes contiguos, mismo layout que Theme)
    memcpy(&ATTR_BANNER, &t->banner, 20);

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

void ui_err(const char *s) __z88dk_fastcall
{
    current_attr = ATTR_ERROR;
    main_print(s);
}

void ui_sys(const char *s) __z88dk_fastcall
{
    current_attr = ATTR_MSG_SYS;
    main_print(s);
}

void ui_usage(const char *a) __z88dk_fastcall
{
    current_attr = ATTR_ERROR;
    main_puts("Usage: /");
    main_print(a);
}


// =============================================================================
// CONFIGURATION FILE LOADER (esxDOS)
// Reads /SYS/CONFIG/SPECTALK.CFG or /SYS/SPECTALK.CFG
// Uses ring_buffer[] as temporary read buffer (2048 bytes, unused at startup)
// =============================================================================

// esxDOS wrappers - parameter passing via globals (no ABI risk)
// These are defined in spectalk_asm.asm
extern uint8_t esx_handle;     // file handle (set by fopen, used by fread/fclose)
extern uint16_t esx_buf;       // buffer pointer (set before fread)
extern uint16_t esx_count;     // byte count (set before fread)
extern uint16_t esx_result;    // bytes read (set by fread)

extern void esx_fopen(const char *path) __z88dk_fastcall;  // sets esx_handle
extern void esx_fread(void);                                // uses globals
extern void esx_fclose(void);                               // uses esx_handle

// Try to open and read a config file into ring_buffer[]
// ring_buffer is 2048 bytes and unused at startup (before UART activity)
static uint16_t cfg_try_read(const char *path) __z88dk_fastcall {
    uint16_t n;
    
    esx_fopen(path);
    if (!esx_handle) return 0;
    
    esx_buf = (uint16_t)(char *)ring_buffer;
    esx_count = RING_BUFFER_SIZE - 2;
    esx_fread();
    n = esx_result;
    
    esx_fclose();
    
    ring_buffer[n] = '\0';
    return n;
}

// Parse a decimal string into uint8_t
// OPT H5: cfg_parse_num eliminada - usar (uint8_t)str_to_u16() directamente

// Apply a key=value pair
static void cfg_apply(char *key, char *val) {
    // 2-char dispatch: fast and tiny
    uint8_t k0 = key[0], k1 = key[1];
    
    if (k0 == 'n' && k1 == 'i') {           // nick / nickpass
        if (key[4] == 'p') {                // nickpass
            st_copy_n(nickserv_pass, val, IRC_PASS_SIZE);
        } else {                            // nick
            st_copy_n(irc_nick, val, IRC_NICK_SIZE);
        }
    } else if (k0 == 's' && k1 == 'e') {    // server
        st_copy_n(irc_server, val, IRC_SERVER_SIZE);
    } else if (k0 == 'p' && k1 == 'o') {    // port
        st_copy_n(irc_port, val, IRC_PORT_SIZE);
    } else if (k0 == 'p' && k1 == 'a') {    // pass
        st_copy_n(irc_pass, val, IRC_PASS_SIZE);
    } else if (k0 == 't' && k1 == 'h') {    // theme
        uint8_t v = (uint8_t)str_to_u16(val);
        if (v >= 1 && v <= 3) current_theme = v;
    } else if (k0 == 'a' && k1 == 'u') {    // autoaway / autoconnect
        uint8_t v = (uint8_t)str_to_u16(val);
        if (key[4] == 'c') {                // autoCon...
            autoconnect = v & 1;
        } else {                            // autoAwa...
            if (v <= 60) autoaway_minutes = v;
        }
    } else if (k0 == 'f' && k1 == 'r') {    // friend1/2/3
        uint8_t idx = (uint8_t)(key[6] - '1');
        if (key[2] == 'i' && key[3] == 'e' && key[4] == 'n' && key[5] == 'd' && idx < 3 && key[7] == '\0') {
            st_copy_n(friend_nicks[idx], val, IRC_NICK_SIZE);
        }
    } else if (k0 == 'b' && k1 == 'e') {    // beep
        beep_enabled = (uint8_t)str_to_u16(val) & 1;
    } else if (k0 == 'q' && k1 == 'u') {    // quits
        show_quits = (uint8_t)str_to_u16(val) & 1;
    } else if (k0 == 't' && k1 == 'i') {    // timestamps
        show_timestamps = (uint8_t)str_to_u16(val) & 1;
    } else if (k0 == 't' && k1 == 'z') {    // tz
        int8_t tz;
        if (*val == '-') {
            tz = -(int8_t)str_to_u16(val + 1);
        } else {
            tz = (int8_t)str_to_u16(val);
        }
        if (tz >= -12 && tz <= 12) sntp_tz = tz;
    }
}

// Parse the config buffer (ring_buffer[]) line by line
static void cfg_parse_buf(void) {
    char *p = (char *)ring_buffer;
    char *key, *val, *eol;
    
    while (*p) {
        // Skip whitespace and blank lines
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        // Skip comments
        if (*p == ';' || *p == '#') {
            while (*p && *p != '\n' && *p != '\r') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            continue;
        }
        
        // Skip empty lines  
        if (*p == '\n' || *p == '\r') {
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            continue;
        }
        
        // Found start of key
        key = p;
        
        // Find '=' or ':'
        while (*p && *p != '=' && *p != ':' && *p != '\n' && *p != '\r') p++;
        if (*p != '=' && *p != ':') {
            // No separator found, skip rest of line
            while (*p && *p != '\n' && *p != '\r') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            continue;
        }
        
        *p = '\0';  // Terminate key
        // Trim trailing whitespace in key
        {
            char *t = p;
            while (t > key && (t[-1] == ' ' || t[-1] == '\t')) *--t = '\0';
        }
        p++;
        // Skip leading whitespace before value
        while (*p == ' ' || *p == '\t') p++;
        val = p;
        
        // Find end of value
        while (*p && *p != '\n' && *p != '\r') p++;
        eol = p;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        *eol = '\0';  // Terminate value
        // Trim trailing whitespace in value
        {
            char *t = eol;
            while (t > val && (t[-1] == ' ' || t[-1] == '\t')) *--t = '\0';
        }
        
        // Apply the key=value pair
        cfg_apply(key, val);
    }
}

uint8_t config_load(void) {
    uint16_t n;
    
    n = cfg_try_read("/SYS/CONFIG/SPECTALK.CFG");
    if (!n) n = cfg_try_read("/SYS/SPECTALK.CFG");
    if (!n) return 0;
    
    cfg_parse_buf();
    return 1;
}


// MAIN FUNCTION

void main(void)
{
    uint8_t c;
    uint8_t cfg_ok;
    
    cfg_ok = config_load();  // Load settings before theme/screen init
    apply_theme();
    init_screen();
    
    current_attr = ATTR_MSG_SYS;
    main_puts2(S_APPNAME, " - ");
    main_print(S_APPDESC);
    main_print(S_COPYRIGHT);
    main_hline();
    
    // --- Initialization ---
    {
        uint8_t retries = 2;
        while (retries--) {
            current_attr = ATTR_MSG_PRIV;
            main_puts(S_INIT_DOTS);
            if (esp_init()) { main_newline(); break; }
            main_putc(' '); current_attr = ATTR_ERROR; main_puts(S_FAIL); main_newline();
            if (retries) {
                ui_sys("Press any key to retry...");
                // FIX: Drenar la UART mientras esperamos la tecla
                while (!in_inkey()) { HALT(); uart_drain_to_buffer(); }
                while (in_inkey()) { HALT(); uart_drain_to_buffer(); }
            }
        }
    }
    
    // Check WiFi
    current_attr = ATTR_MSG_PRIV;
    main_puts("Checking connection...");
    
    if (connection_state < STATE_WIFI_OK) {
        main_putc(' '); current_attr = ATTR_ERROR; main_puts("NO WIFI"); main_newline();
        ui_sys("Join WiFi network first");
    } else {
        main_newline();
        sntp_init();  // OPT L2: inlined sync_time()
    }
    // -------------------------------------------

    current_attr = ATTR_MSG_SYS;
    main_hline();
    main_print("Type !help or !about for more info.");
    if (cfg_ok) {
        main_puts("Config loaded.");
        if (irc_server[0]) {
            if (autoconnect) {
                main_puts(" Autoconnecting");
                if (irc_nick[0]) { main_puts2(S_AS_SP, irc_nick); }
                main_puts(S_DOTS3);
            } else {
                main_puts(" /server to connect");
                if (irc_nick[0]) { main_puts2(S_AS_SP, irc_nick); }
            }
        }
        main_newline();
    } else {
        main_print("Tip: /nick Name then /server host");
    }
    main_newline();
    
    current_attr = ATTR_MAIN_BG;
    
    draw_status_bar();
    redraw_input_full();
    
    {
        uint8_t prev_caps_mode = caps_lock_mode;
        uint8_t prev_shift_held = 0;
        uint8_t sntp_timer = 0;     // <-- ÚNICO CAMBIO: antes era uint16_t
        uint8_t sntp_queried = 0;
        uint8_t autoconnect_delay = autoconnect ? 200 : 0;  // ~4 sec delay

        // FIX: Ocultar cursor durante autoconnect countdown
        if (autoconnect_delay) { cursor_visible = 0; redraw_input_full(); }

        // FIX: muestrear SHIFT 1 vez por frame y reutilizarlo
        uint8_t shift_held = 0;
        
        while (1) {
            HALT(); // 50 Hz Sync
            
            tick_counter++;
            if (tick_counter >= 50) {
                tick_counter = 0;
                time_second++;

                // Away auto-reply global cooldown (1 tick per second)
                if (away_reply_cd) away_reply_cd--;
                
                {
                    uint8_t i;
                    for (i = 0; i < MAX_CHANNELS; i++) {
                        // Optimización: Comprobar ACTIVE y MENTION en una sola operación
                        uint8_t f = channels[i].flags;
                        if ((f & (CH_FLAG_ACTIVE | CH_FLAG_MENTION)) == (CH_FLAG_ACTIVE | CH_FLAG_MENTION) &&
                            i != current_channel_idx) {
                            status_bar_dirty = 1;
                            break;
                        }
                    }
                }
                
                // Auto-away check (cada segundo, si configurado y conectado)
                if (autoaway_minutes && connection_state == STATE_IRC_READY && !irc_is_away) {
                    if (autoaway_counter < 65000) autoaway_counter++;  // Prevenir overflow
                    if (autoaway_counter >= (uint16_t)autoaway_minutes * 60) {
                        uart_send_line("AWAY :Auto-away");
                        st_copy_n(away_message, "Auto-away", sizeof(away_message));
                        autoaway_active = 1;
                        irc_is_away = 1;  // Prevenir envío duplicado antes de recibir 306
                        autoaway_counter = 0;  // Reset para evitar re-trigger
                    }
                }
                
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
            
            // 1.1. AUTOCONNECT: countdown then connect
            if (autoconnect_delay) {
                if (--autoconnect_delay == 0) {
                    if (irc_server[0] && connection_state == STATE_WIFI_OK) {
                        parse_user_input("/server");
                    } else {
                        // Conditions not met — restore cursor
                        cursor_visible = 1;
                        redraw_input_full();
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
                        // connection_state y reset_all_channels ya se hacen en force_disconnect()
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

            // 1.6. SEARCH FLUSH & TIMEOUT
            if (pagination_active && search_pending_type != PEND_NONE) {
                // Fase de drenaje activo
                if (search_flush_state == 1) {
                    // Drenar UART al ring buffer
                    uart_drain_to_buffer();
                    
                    // pending == 0  <=> ring vacío y sin línea parcial
                    if (rx_pos == 0 && rb_head == rb_tail) {
                        // Buffer vacío - contar frames estables
                        if (++search_flush_stable >= 10) {
                            // Drenaje completo - enviar comando
                            search_flush_state = 2;
                            flush_all_rx_buffers();
                            send_pending_search_command();
                            pagination_timeout = 0;
                        }
                    } else {
                        // Hay datos - descartar y reiniciar contador
                        search_flush_stable = 0;
                        flush_all_rx_buffers();
                        
                        // Timeout solo cuando hay datos persistentes (~3s)
                        if (++pagination_timeout > 150) {
                            search_flush_state = 2;
                            send_pending_search_command();
                            pagination_timeout = 0;
                        }
                    }
                }
                // Fase de espera de resultados
                else if (search_flush_state == 2) {
                    if (++pagination_timeout > 250) {
                        search_data_lost = 1;
                        current_attr = ATTR_ERROR;
                        main_print("-- Timeout (incomplete) --");
                        cancel_search_state();
                    }
                }
            }
            
            // 2. FLUSH STATUS BAR
            if (status_bar_dirty) {
                status_bar_dirty = 0;
                draw_status_bar_real();
            }
            
            // 3. INPUT Y teclado
            check_caps_toggle();

            // Sample SHIFT once per frame and reuse it
            shift_held = key_shift_held();
            if ((prev_caps_mode != caps_lock_mode) || (prev_shift_held != shift_held)) {
                prev_caps_mode = caps_lock_mode; 
                prev_shift_held = shift_held;
                refresh_cursor_char(cursor_pos, 1);
            }
            
            c = read_key();

            // D2: SHIFT+5/6/7/8 => cursor keys via lookup table
            if (c >= '5' && c <= '8' && (shift_held || prev_shift_held)) {
                static const uint8_t shift_keys[] = {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT};
                c = shift_keys[c - '5'];
            }

            if (c != 0 && !pagination_active && search_mode == SEARCH_NONE && !autoconnect_delay) {
                if (c >= 32 && c <= 126) {
                    uint8_t c_lower = c | 32;

                    if (c_lower >= 'a' && c_lower <= 'z') {
                        if (caps_lock_mode ^ shift_held) {
                            c = c_lower & 0xDF;   // Uppercase
                        } else {
                            c = c_lower;          // Lowercase
                        }
                    }

                    input_add_char(c);
                }
                else if (c == KEY_ENTER) {
                     if (line_len > 0) {
                        // OPT H2: reutilizar temp_input[] (estático) en lugar de cmd_copy local
                        memcpy(temp_input, line_buffer, line_len + 1);
                        history_add(temp_input, line_len); 
                        input_clear();
                        set_input_busy(1); 
                        parse_user_input(temp_input); 
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
