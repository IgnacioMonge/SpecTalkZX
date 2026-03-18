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

// Theme data loaded from SPECTALK.DAT at startup (in ASM bss_user, contiguous with font)
extern uint8_t theme_raw[75];

// Theme names (kept in ROM — only used by cmd_theme display)
static const char theme_name_pool[] = "Default\0The Terminal\0The Commander";

const char *theme_get_name(uint8_t theme_id) __z88dk_fastcall
{
    const char *p = theme_name_pool;
    uint8_t n;
    if ((uint8_t)(theme_id - 1) > 2) theme_id = 1;
    for (n = theme_id - 1; n; n--) { while (*p++) {} }
    return p;
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
const char S_AT_SNTPTIME[] = "AT+CIPSNTPTIME?";  // D10: dedup (2 uses)
const char S_IDENTIFY_CMD[] = " :IDENTIFY ";      // D10: dedup (2 uses)
const char S_JOINED_SP[] = " joined ";            // D10: dedup (2 uses)
const char S_AWAY_CMD[] = "AWAY";                 // D10: dedup (2 uses)
const char S_NICK_CMD[] = "NICK";                 // D10: dedup (2 uses)
const char S_SMART[] = "smart";                   // D10: dedup (2 uses)
const char S_AUTOAWAY[] = "Auto-away";            // D11: dedup (5 uses)

// =============================================================================
// THEME SYSTEM - Global attributes set by apply_theme()
// =============================================================================
uint8_t current_theme = 1;  // 1=DEFAULT, 2=TERMINAL, 3=COLORFUL
// Theme attributes array — layout MUST match Theme struct fields banner..border
// Indices: 0=BANNER 1=STATUS 2=MSG_CHAN 3=MSG_SELF 4=MSG_PRIV 5=MAIN_BG
//   6=INPUT 7=INPUT_BG 8=PROMPT 9=MSG_SERVER 10=MSG_JOIN 11=MSG_NICK
//   12=MSG_TIME 13=MSG_TOPIC 14=MSG_MOTD 15=ERROR 16=IND_RED 17=IND_YELLOW
//   18=IND_GREEN 19=BORDER
uint8_t theme_attrs[20];

// =============================================================================
// UI STATE FLAGS
// =============================================================================

// Dirty flags for deferred UI updates
uint8_t status_bar_dirty = 1;
uint8_t counting_new_users;      // Flag: next 353 should reset count
uint16_t names_count_acc;        // Temp accumulator for NAMES user count
uint8_t show_names_list;         // Flag: show 353 user list (for /names command)
uint8_t names_was_manual;        // Flag: NAMES fue iniciado por /names (no por JOIN)

// Activity indicator for inactive channels
uint8_t other_channel_activity;      // Set when msg arrives on non-active channel
uint8_t irc_is_away;
// away_message mapped to UDG area 0xFFB2 via ASM defc
uint8_t away_reply_cd;       // Global cooldown (seconds) for away auto-replies

// Auto-away system
uint8_t autoaway_minutes;        // 0 = disabled, 1-60 = minutes until auto-away
uint16_t autoaway_counter;       // Seconds of inactivity
uint8_t autoaway_active;         // 1 = current away is auto-away (not manual)

// NAMES reply state machine (robust user counting with timeout)
uint8_t names_pending;
uint16_t names_timeout_frames;
// names_target_channel mapped to UDG area 0xFFD2 via ASM defc

// User preferences (toggle with commands)
uint8_t beep_enabled = 1;   // 0 = silent mode
uint8_t show_traffic = 1;     // 0 = hide JOIN/QUIT messages
uint8_t show_timestamps = 1; // 0=off, 1=always, 2=on-change
// big_status removed (big mode eliminated)
uint8_t last_ts_hour = 0xFF;   // Last printed timestamp hour (0xFF = force print)
uint8_t last_ts_minute = 0xFF; // Last printed timestamp minute
int8_t sntp_tz = 1;         // SNTP timezone offset (-12..+12), default CET

// Keep-alive system: detect silent disconnections
// KEEPALIVE_SILENCE = 3 min = 9000 frames, KEEPALIVE_TIMEOUT = 30s = 1500 frames
#define KEEPALIVE_SILENCE_FRAMES 9000
#define KEEPALIVE_TIMEOUT_FRAMES 1500
#define LAGMETER_INTERVAL_FRAMES 3000  // 60 segundos entre PINGs de medición
uint16_t server_silence_frames;      // Frames since last server activity
uint8_t  keepalive_ping_sent;        // 1 = waiting for PONG
uint16_t keepalive_timeout;          // Timeout counter after PING sent
uint16_t lagmeter_counter;           // Counter for periodic lag measurement
uint8_t ping_latency;               // 0=good, 1=medium, 2=high

// Pagination for long LIST/WHO results
uint8_t pagination_active;
uint16_t pagination_count;
uint8_t pagination_lines;        // Líneas impresas desde última pausa
uint8_t pagination_timeout;      // Frames sin datos válidos → safety timeout
uint8_t search_data_lost;        // Flag: se perdieron datos durante listado
uint8_t buffer_pressure;         // 1 = buffer >75% (indicator shows empty circle)

// Flush state for search commands (simple drain-based approach)
// States: 0=idle, 1=draining, 2=command sent
uint8_t search_flush_state;
static uint8_t search_flush_stable;      // Frames con buffer vacío consecutivos
static uint8_t search_pending_type;      // Tipo de comando pendiente
uint8_t search_header_rcvd;              // Flag: recibimos 321/352 header (no rate-limited)
// NOTA: Usamos search_pattern[] para almacenar el argumento pendiente (ahorra 32 bytes)

// SEARCH state
uint8_t search_mode;
char    search_pattern[SEARCH_PATTERN_SIZE];  // También usado como argumento pendiente durante drenaje
uint16_t search_index;

void draw_status_bar(void)
{
    status_bar_dirty = 1;
}

// Helper: clear main area and reset cursor (saves ~16 bytes per call site)
void clear_main(void)
{
    clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
    main_line = MAIN_START;
    main_col = 0;
}


// Input cell cache (mapped to Printer Buffer 0x5B00 via ASM defc)

static void put_char64_input_cached(uint8_t y, uint8_t col, char ch, uint8_t attr) __z88dk_callee
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
uint16_t rb_head;
uint16_t rb_tail;

// Line parser state
char rx_line[RX_LINE_SIZE];
uint16_t rx_pos;
uint16_t rx_last_len;
uint8_t rx_overflow;             // Flag for ASM access (0 or 1)

// TIMEOUT_* values are defined in spectalk.h (single source of truth)

uint8_t uart_drain_limit = DRAIN_NORMAL;
#ifndef ST_UART_AY
#define ST_UART_AY 0
#endif


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
    // 1. Drain UART to ring buffer (limit=0 -> 255 iterations, sufficient for ESP buffer)
    uint8_t saved = uart_drain_limit;
    uart_drain_limit = 0;
    uart_drain_to_buffer();
    // 2. Discard and repeat drain for bytes that arrived during step 1
    rb_tail = rb_head;
    uart_drain_to_buffer();
    uart_drain_limit = saved;
    rb_tail = rb_head;

    // 3. Clear line parser state
    rx_line[0] = 0;
    rx_pos = 0;
    rx_overflow = 0;
}


// rb_pop() está implementada en spectalk_asm.asm

// =============================================================================
// GLOBAL bufferS
// =============================================================================
// line_buffer mapped to CHANS workspace 0x5CB6 via ASM defc
uint8_t line_len;
uint8_t cursor_pos;

// CAPS LOCK state (from BitStream)
uint8_t caps_lock_mode;
uint8_t caps_latch;

// =============================================================================
// IRC STATE
// =============================================================================
char irc_server[IRC_SERVER_SIZE];
char irc_port[IRC_PORT_SIZE] = "6667";
char irc_nick[IRC_NICK_SIZE];
char irc_pass[IRC_PASS_SIZE];
char nickserv_pass[IRC_PASS_SIZE];
uint8_t autoconnect;
uint8_t has_esxdos;
// friend_nicks mapped to UDG area 0xFF58 via ASM defc
uint8_t friends_ison_sent;
char user_mode[USER_MODE_SIZE];
char network_name[NETWORK_NAME_SIZE];
uint8_t connection_state;

// =============================================================================
// MULTI-WINDOW SUPPORT
// =============================================================================
ChannelInfo channels[MAX_CHANNELS];
uint8_t current_channel_idx;
ChannelInfo *cur_chan_ptr = channels;
uint8_t channel_count;

uint8_t nav_history[NAV_HIST_SIZE];
uint8_t nav_hist_ptr;

// nav_push() moved to spectalk_asm.asm (OPT: 79 -> 55 bytes, LDIR shift)


void nav_fix_on_delete(uint8_t deleted_idx) __z88dk_fastcall {
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
uint8_t ignore_count;

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

// find_query is implemented in spectalk_asm.asm for size optimization

// find_empty_channel_slot() moved to spectalk_asm.asm (OPT: 46 -> 23 bytes)

// Forward declarations for switcher live-update
static uint8_t sw_active;
static uint8_t sw_dirty;

// Internal: add slot with given flags
static int8_t add_slot_internal(const char *name, uint8_t flags) __z88dk_callee
{
    int8_t idx = find_empty_channel_slot();
    if (idx < 0) return -1;
    
    st_copy_n(channels[idx].name, name, sizeof(channels[0].name));
    channels[idx].mode[0] = '\0';
    channels[idx].user_count = 0;
    channels[idx].flags = flags;
    channel_count++;
    if (sw_active) sw_dirty = 1;
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
    // Limpiar último slot completo tras defragmentación
    channels[MAX_CHANNELS - 1].flags = 0;
    channels[MAX_CHANNELS - 1].name[0] = '\0';
    channels[MAX_CHANNELS - 1].mode[0] = '\0';
    channels[MAX_CHANNELS - 1].user_count = 0;
    if (channel_count > 1) channel_count--;
    if (sw_active) sw_dirty = 1;

    // 3. Ajustar índices DESPUÉS de defragmentar
    if (was_current) {
        // Ajustar next_idx si apuntaba a un slot que se movió
        if (next_idx > idx) next_idx--;
        current_channel_idx = next_idx;
    } else if (current_channel_idx > idx) {
        current_channel_idx--;
    }
    cur_chan_ptr = &channels[current_channel_idx];

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
    cur_chan_ptr = &channels[idx];

    // Clear unread + mention when user switches in
    channels[idx].flags &= (uint8_t)~(CH_FLAG_UNREAD | CH_FLAG_MENTION);

    other_channel_activity = 0;
    set_border(BORDER_COLOR);
    force_status_redraw = 1;
    status_bar_dirty = 1;
}

// =============================================================================
// CHANNEL SWITCHER OVERLAY (state-based, non-blocking)
// =============================================================================

// Forward declaration (defined later in this file)
extern uint8_t last_frames_lo;

// Switcher state — persists across main loop frames
// (sw_active and sw_dirty declared earlier for live-update from add/remove channel)
static uint8_t sw_sel;
static uint8_t sw_first;
static uint8_t sw_map[MAX_CHANNELS];
static uint8_t sw_count;
static uint8_t sw_released;  // EDIT key released after open
static uint16_t sw_timeout;  // frames since last key press (auto-close)
static uint8_t sw_flags_snap[MAX_CHANNELS];  // flags snapshot for change detection

// Even-width tabs + 2-char separator " |" → all tab boundaries align to
// attribute cell boundaries (8px), so inverse video never bleeds into separators.
// Tab format: " N:name " with N = slot number (0-9).
// Uses print_str64 (not print_line64_fast) → font at scanlines 1-7, matching
// the status bar's vertical alignment.

static void switcher_close(void);

// Compute tab width for a given channel slot index (always even)
static uint8_t sw_tab_width(uint8_t slot) __z88dk_fastcall
{
    // " N:name " = 1 + 1 + 1 + nlen + 1 = nlen + 4, rounded up to even
    const char *name = channels[slot].name;
    uint8_t tw = st_strlen((*name == '#' || *name == '&') ? name + 1 : name) + 4;
    if (tw & 1) tw++;
    return tw;
}

static void switcher_rebuild_map(void)
{
    uint8_t old_slot = (sw_count > 0) ? sw_map[sw_sel] : current_channel_idx;
    uint8_t i;
    sw_count = 0;
    sw_sel = 0;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].flags & CH_FLAG_ACTIVE) {
            if (i == old_slot) sw_sel = sw_count;
            sw_map[sw_count++] = i;
        }
    }
    if (sw_sel >= sw_count && sw_count > 0) sw_sel = sw_count - 1;
}

static void switcher_render(void)
{
    char buf[SCREEN_COLS + 1];
    uint8_t i, j, pos;
    uint8_t last_shown;
    uint8_t tw;

    // Rebuild map to pick up any added/removed channels
    switcher_rebuild_map();
    if (sw_count < 2) { switcher_close(); return; }

    // Adjust sw_first so selected tab is visible
    if (sw_first > sw_sel) sw_first = sw_sel;
    while (sw_first < sw_sel) {
        pos = (sw_first > 0) ? 2 : 0;
        for (i = sw_first; i <= sw_sel; i++) {
            if (i > sw_first) pos += 2;
            pos += sw_tab_width(sw_map[i]);
        }
        if (pos <= SCREEN_COLS) break;
        sw_first++;
    }

    memset(buf, ' ', SCREEN_COLS);
    buf[SCREEN_COLS] = 0;

    pos = (sw_first > 0) ? 2 : 0;  // "< " takes 2 cols (even)
    last_shown = sw_first;

    for (i = sw_first; i < sw_count; i++) {
        const char *name = channels[sw_map[i]].name;
        if (*name == '#' || *name == '&') name++;
        uint8_t nlen = st_strlen(name);
        uint8_t need;
        tw = nlen + 4;
        if (tw & 1) tw++;
        need = tw + ((i > sw_first) ? 2 : 0);

        if (pos + need > SCREEN_COLS) break;

        if (i > sw_first) {
            buf[pos] = ' ';
            buf[pos + 1] = '|';
            pos += 2;               // 2-char separator (even)
        }

        // " N:name " — slot number prefix
        buf[pos] = ' ';
        buf[pos + 1] = '0' + sw_map[i];
        buf[pos + 2] = ':';
        for (j = 0; j < nlen; j++) buf[pos + 3 + j] = name[j];
        // trailing spaces already present from memset
        pos += tw;
        last_shown = i;
    }

    if (sw_first > 0) buf[0] = '<';
    if (last_shown < sw_count - 1) buf[SCREEN_COLS - 1] = '>';

    // print_str64 renders font at scanlines 1-7 (scanline 0 cleared per char),
    // matching the status bar's vertical alignment. No clear_line needed —
    // print_str64_char updates every nibble across all 8 scanlines, so no
    // old content persists, and there's no blank-then-repaint flicker.
    print_str64(2, 0, buf, ATTR_STATUS);

    // Patch attributes: inverse=selected, BRIGHT=unread, FLASH=mention
    {
        uint8_t sel_attr = (ATTR_STATUS & 0xC0)
                         | ((ATTR_STATUS & 0x07) << 3)
                         | ((ATTR_STATUS >> 3) & 0x07);
        uint8_t *attr_base = (uint8_t *)0x5840;  // row 2 attrs
        uint8_t x0, attr, px, flags;

        pos = (sw_first > 0) ? 2 : 0;
        for (i = sw_first; i <= last_shown; i++) {
            uint8_t slot = sw_map[i];
            tw = sw_tab_width(slot);

            if (i > sw_first) pos += 2;
            x0 = pos;
            pos += tw;

            flags = channels[slot].flags;

            if (i == sw_sel) {
                attr = sel_attr;
            } else if (flags & CH_FLAG_MENTION) {
                attr = ATTR_STATUS | 0x80;       // FLASH
            } else if (flags & CH_FLAG_UNREAD) {
                attr = ATTR_STATUS | 0x40;       // BRIGHT
            } else {
                continue;
            }

            // x0 and pos are both even → clean attr cell boundaries
            for (px = x0 >> 1; px < pos >> 1; px++)
                attr_base[px] = attr;
        }
    }

    // Save flags snapshot for change detection
    {
        uint8_t k;
        for (k = 0; k < sw_count; k++)
            sw_flags_snap[k] = channels[sw_map[k]].flags;
    }

    sw_dirty = 0;
}

static void switcher_open(void)
{
    uint8_t i;
    sw_count = 0;
    sw_sel = 0;
    sw_first = 0;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].flags & CH_FLAG_ACTIVE) {
            if (i == current_channel_idx) sw_sel = sw_count;
            sw_map[sw_count++] = i;
        }
    }
    if (sw_count < 2) return;
    sw_active = 1;
    sw_released = 0;
    sw_timeout = 0;
    sw_dirty = 1;
}

static void switcher_close(void)
{
    sw_active = 0;
    clear_line(2, ATTR_MAIN_BG);
    // Redraw 1px separator (row 2, scanline 0)
    {
        uint8_t *p = (uint8_t *)0x4040;
        uint8_t j;
        for (j = 0; j < 32; j++) p[j] = 0xFF;
    }
    last_frames_lo = *(volatile uint8_t *)23672;
}


// Reset all channels (on disconnect)
void reset_all_channels(void)
{
    uint8_t i;
    ChannelInfo *ch = channels;

    for (i = 0; i < MAX_CHANNELS; i++, ch++) {
        ch->flags = 0;
        ch->name[0] = '\0';
        ch->mode[0] = '\0';
        ch->user_count = 0;
    }

    // Slot 0: SIEMPRE reservado para Server / ChanServ
    channels[0].flags = CH_FLAG_ACTIVE | CH_FLAG_QUERY;  // Active + query (no user count)
    st_copy_n(channels[0].name, S_SERVER, sizeof(channels[0].name));

    current_channel_idx = 0;
    cur_chan_ptr = channels;
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
uint8_t irc_param_count;

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
uint8_t closed_reported;
uint8_t disconnecting_in_progress;       // FIX: Prevent reentrant disconnection

// TIME TRACKING (Optimized: No 32-bit math)
// Eliminamos uptime_frames y time_sync_frames para ahorrar librería de división
uint8_t time_hour;
uint8_t time_minute;
uint8_t time_second;             // Nuevo: Segundos explícitos
uint8_t sntp_init_sent;          // Flag: SNTP init commands sent (not static - needs reset on disconnect)
uint8_t sntp_waiting;            // Flag: waiting for SNTP response
uint8_t sntp_queried;            // Flag: valid time received (stop retrying)

// Frame-accurate ticker using system variable FRAMES (23672)
uint8_t last_frames_lo;          // Last read of FRAMES low byte
uint16_t tick_accum;             // Frame accumulator (0-49 -> 1 second)

// SCREEN STATE
uint8_t main_line = MAIN_START;
uint8_t main_col;
uint8_t wrap_indent;             // Indentación para líneas que continúan (wrap)
uint8_t current_attr;  // Initialized in apply_theme()
// COMMAND HISTORY
#define HISTORY_SIZE    4
// E2: Reduced from 128 to 96 - saves 128 bytes BSS
#define HISTORY_LEN     96

static char history[HISTORY_SIZE][HISTORY_LEN];
static uint8_t hist_head;
static uint8_t hist_count;
static int8_t hist_pos = -1;
// temp_input mapped to CHANS workspace 0x5D36 via ASM defc

static void history_add(const char *cmd, uint8_t len) __z88dk_callee
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
volatile uint8_t g_ps64_y;
volatile uint8_t g_ps64_col;
volatile uint8_t g_ps64_attr;


// print_char64() moved to spectalk_asm.asm (OPT: 33 -> 19 bytes)

// Pure C implementation — no inline ASM avoids __z88dk_callee + frame pointer
// conflict that caused crash without --max-allocs-per-node200000.
void print_str64(uint8_t y, uint8_t col, const char *s, uint8_t attr) __z88dk_callee
{
    g_ps64_y = y;
    g_ps64_col = col;
    g_ps64_attr = attr;
    while (*s) {
        if (g_ps64_col < 64)
            print_str64_char(*s);
        g_ps64_col++;
        s++;
    }
}

// Scroll seguro: Solo mueve lines de la 3 a la 19. No toca banners.
// scroll_main_zone está implementada en ASM (spectalk_asm.asm)
extern void scroll_main_zone(void);
extern uint8_t read_key(void);

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

    clear_main();
    pagination_lines = 0;
    return 0;
}


/* main_newline moved to ASM (see asm/spectalk_asm.asm) */
extern void main_newline(void);

// ============================================================
// Versión optimizada de main_run: Calcula punteros de fila una vez por segmento
// ============================================================

// OPTIMIZACIÓN: Helper para evitar duplicar lógica de alineación de atributos
static void ensure_attr_alignment(uint8_t target_attr) __z88dk_fastcall
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
    // During help overlay, suppress IRC message output to prevent
    // overwriting the help display. PING/PONG is handled by process_irc_data
    // since ring_buffer is free between help pages.
    if (help_active) return;

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
    // FIX: Marcar como datos perdidos para que 366 no actualice conteo
    search_data_lost = 1;
    
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
    clear_main();
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
void start_search_command(uint8_t type, const char *arg) __z88dk_callee
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
    set_attr_sys();
    main_print("Searching...");
    
    // 5. Vaciar TODOS los buffers y comenzar fase de drenaje
    flush_all_rx_buffers();
    
    search_flush_state = 1;   // Estado: drenando
    search_flush_stable = 0;  // Contador de frames estables
    pagination_timeout = 0;
}

// STATUS BAR

// Double-height string renderer (uses draw_big_char from ASM)
extern void draw_big_char(uint8_t ch) __z88dk_fastcall;

void print_big_str(uint8_t y, uint8_t col, const char *s, uint8_t attr) __z88dk_callee
{
    g_ps64_y = y;
    g_ps64_attr = attr;
    g_ps64_col = col;
    while (*s) {
        draw_big_char((uint8_t)*s++);
        g_ps64_col++;
    }
}

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

// =============================================================================
// STATUS BAR ARCHITECTURE — CONTRATOS Y LÍMITES
// =============================================================================
//
// LAYOUT (64 columnas lógicas = 32 físicas):
//   [0-53]  : Contenido dinámico (nick, canal, usuarios)
//   [54-59] : Reloj HH:MM (6 chars)
//   [60-63] : Indicador de conexión (2 chars físicos = 4 lógicos)
//
// BUFFER:
//   sb_left_part[57] — buffer de trabajo para columnas 0-56 (incluye \0)
//   sb_last_status[57] — caché del último estado para diff-redraw
//
// LÍMITES CLAVE:
//   - limit_end = sb_left_part + 54  → última posición antes del reloj
//   - central_limit = limit_end - user_len → reserva espacio para "[NNN]"
//
// ESTRUCTURA DEL CONTENIDO [0-53]:
//   [nick(+modes)] [indicador][idx/total:]canal(@net)(modes) [users]
//   └── 12-16 chars ──┘ └─────────── variable ───────────────┘ └ 5-7 ┘
//
// =============================================================================

// sb_append: Copia src a dst hasta que dst >= limit O src termina en \0
// CONTRATO:
//   - limit es "one-past-end": NO se escribe en *limit ni más allá
//   - Retorna puntero a la siguiente posición libre (puede ser == limit)
//   - Si dst >= limit al entrar, no escribe nada y retorna dst
//   - NUNCA escribe \0 al final (el caller debe hacerlo si lo necesita)
// EJEMPLO: sb_append(buf, "ABC", buf+2) escribe "AB", retorna buf+2
extern char *sb_append(char *dst, const char *src, const char *limit) __z88dk_callee;

// Extraer nombre de red del hostname: chat.freenode.net → freenode
static char *extract_network_short(char *hostname) __z88dk_fastcall
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
static char sb_left_part[57];  // Buffer estático (ahorra stack frame)
uint8_t force_status_redraw = 1;

static void draw_connection_indicator(void)
{
    uint8_t ind_attr;

    if (connection_state >= STATE_TCP_CONNECTED) {
        ind_attr = STATUS_GREEN;
    } else if (connection_state >= STATE_WIFI_OK) {
        ind_attr = STATUS_YELLOW;
    } else {
        ind_attr = STATUS_RED;
    }

    draw_indicator(INFO_LINE, 31, ind_attr);
}

// sb_put_u8_2d: Escribe un número 0-19 como 1-2 dígitos
// CONTRATO:
//   - Escribe 1 dígito si v < 10, 2 dígitos si v >= 10
//   - NO verifica límites — el caller debe garantizar espacio
//   - Retorna puntero a la siguiente posición libre
// PRECONDICIÓN: p tiene al menos 2 bytes disponibles
static char* sb_put_u8_2d(char *p, uint8_t v) __z88dk_callee
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

static const char* sb_pick_status(uint8_t prefer_server_full) __z88dk_fastcall
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

// sb_format_channel: Escribe nombre de canal con modos y red si caben
// CONTRATO:
//   - central_limit es "one-past-end" del área disponible (excluyendo ']')
//   - Reserva internamente 1 byte para ']' que el CALLER debe escribir
//   - Prioridad de truncamiento: modes > network > channel_name
//   - Retorna puntero a siguiente posición libre
// PRECONDICIÓN: p < central_limit (verificado internamente, retorna p si no)
// POSTCONDICIÓN: El caller DEBE escribir ']' después si hay espacio
static char *sb_format_channel(char *p, char *central_limit, uint8_t cur_flags) __z88dk_callee
{
    uint8_t space;
    char *chan, *modes, *net;
    uint8_t chan_len, mode_len, net_len;

    // FIX BUG-01: Si ya estamos en o más allá del límite, no hacer nada
    // (el ']' de cierre lo maneja el caller)
    if (p >= central_limit) return p;
    
    // Reservar 1 char para el ']' que escribe el caller
    space = (uint8_t)(central_limit - p);
    if (space > 1) space--;  // reservar espacio para ']'
    else return p;  // solo queda espacio para ']', no escribir nada

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
    // Priority: channel > network > modes (modes are least important)
    if (chan_len + mode_len + net_len > space) {
        // Drop modes first (least useful info)
        modes = NULL; mode_len = 0;
        // If still doesn't fit, drop network too
        if (chan_len + net_len > space) {
            net = NULL; net_len = 0;
        }
    }

    // Render channel name (truncated if needed)
    {
        char *chan_limit = p + (space - mode_len - net_len);
        if (chan_limit >= central_limit) chan_limit = central_limit - 1;
        p = sb_append(p, chan, chan_limit);
    }

    // SAFETY-M1: literal writes '(', ')', '@' are covered by mode_len (+2)
    // and net_len (+1) in the greedy fit check above (lines 1260-1267).
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

// =============================================================================
// draw_status_bar_real: Renderiza la barra de estado completa
// =============================================================================
// LAYOUT: [nick(+modes)] [indicator idx/total:channel(@net)(modes)] [users]
//         └─ SECCIÓN 1 ─┘ └────────────── SECCIÓN 2 ──────────────┘ └─ S3 ─┘
//
// LÍMITES USADOS:
//   sb_left_part + 11  : máx para nick (10 chars + '[')
//   sb_left_part + 15  : máx para nick+modes
//   limit_end (col 54) : inicio del reloj, fin del área dinámica
//   central_limit      : limit_end - user_len, fin de sección 2
//
// SECCIÓN 1: [nick(+modes)] — siempre presente, ~12-16 chars
// SECCIÓN 2: [canal+info] — variable, usa espacio restante
// SECCIÓN 3: [users] — opcional, 0 o 5-7 chars
//
// INVARIANTES:
//   - Siempre escribimos '[' al inicio de cada sección
//   - ']' se escribe AL FINAL de cada sección, con bounds check
//   - central_limit reserva espacio para sección 3 antes de escribir sección 2
// =============================================================================
void draw_status_bar_real(void)
{
    char *p = sb_left_part;
    char *const limit_end = sb_left_part + 54;  // cols 54+ reserved for clock

    uint8_t cur_flags = chan_flags;
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

    // === SECCIÓN 1: Nick y modos de usuario ===
    // Límites: nick máx 10 chars (col 1-10), modes máx 4 chars (col 12-15)
    // Formato: [nick(+modes)]
    *p++ = '[';
    if (irc_nick[0]) {
        // +11 = col 0 ('[') + 10 chars nick máximo
        p = sb_append(p, irc_nick, sb_left_part + 11);
        if (user_mode[0]) {
            // +15 = col 11 ('(') + 3 chars mode + ')' en col 15
            *p++ = '('; p = sb_append(p, user_mode, sb_left_part + 15); *p++ = ')';
        }
    } else {
        // +10 = '[' + "no-nick" (7) + margen
        p = sb_append(p, "no-nick", sb_left_part + 10);
    }
    *p++ = ']'; *p++ = ' '; *p++ = '[';

    // === SECCIÓN 3 (preparación): Contador de usuarios ===
    // Se calcula ANTES de sección 2 para reservar espacio
    // Formato: " [NNN]" = 1 + 1 + 1-3 + 1 = 4-6 chars
    static char user_buf[8];
    uint8_t user_len = 0;
    user_buf[0] = 0;

    if (irc_channel[0] && !(cur_flags & CH_FLAG_QUERY) && chan_user_count > 0) {
        char *u_end = u16_to_dec(user_buf, chan_user_count);
        user_len = (uint8_t)(u_end - user_buf) + 3;  // " [" + numero + "]"
    }

    // === SECCIÓN 2: Canal, indicadores, modos ===
    // Espacio disponible: desde p actual hasta central_limit
    // central_limit = limit_end - user_len (reserva espacio para sección 3)
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

        // FIX BUG-01: Verificar espacio ANTES de escribir contenido
        // central_limit - 1 reserva espacio para ']'
        if (p < central_limit - 1) {
            // D10: Inline is_server_tab check instead of storing in variable
            if (current_channel_idx == 0 && st_stricmp(channels[0].name, S_SERVER) == 0) {
                p = sb_append(p, (char*)sb_pick_status(1), central_limit - 1);
            } else if (irc_channel[0]) {
                if (is_query) *p++ = '@';
                p = sb_format_channel(p, central_limit, cur_flags);
            } else if (current_channel_idx == 0) {
                p = sb_append(p, (char*)sb_pick_status(0), central_limit - 1);
            } else {
                // active slot with empty name — show placeholder
                *p++ = '#';
                if (p < central_limit - 1) *p++ = '?';
            }
        }

        // FIX BUG-01: Escribir ']' solo si hay espacio
        if (p < central_limit) *p++ = ']';
    }

    // === SECCIÓN 3 (renderizado): Escribir contador de usuarios ===
    if (user_buf[0]) {
        *p++ = ' ';
        *p++ = '[';
        p = sb_append(p, user_buf, limit_end - 1);  // -1 reserva espacio para ']'
        *p++ = ']';
    }

    // Rellenar con espacios hasta limit_end y terminar string
    while (p < limit_end) *p++ = ' ';
    *p = 0;

    print_str64(INFO_LINE, 0, sb_left_part, ATTR_STATUS);
    force_status_redraw = 0;

    draw_clock();
    draw_connection_indicator();
}


// INPUT AREA

static void draw_cursor_underline(uint8_t y, uint8_t col) __z88dk_callee
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

void refresh_cursor_char(uint8_t idx, uint8_t show_cursor) __z88dk_callee
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

// Wrappers to avoid 2-param call overhead (saves ~3 bytes per call site)
static void cursor_show(void) { refresh_cursor_char(cursor_pos, 1); }
static void cursor_hide(void) { refresh_cursor_char(cursor_pos, 0); }

static void input_draw_prompt(void)
{
    print_str64(INPUT_START, 0, S_PROMPT, ATTR_PROMPT);
}

static void input_put_char_at(uint8_t abs_pos, char c) __z88dk_callee
{
    // OPTIMIZADO: Divisiones por 64 -> Shifts/Masks
    uint8_t row = INPUT_START + (abs_pos >> 6);
    uint8_t col = abs_pos & 63;
    
    if (row > INPUT_END) return;
    put_char64_input_cached(row, col, c, ATTR_INPUT);
}

static void redraw_input_from(uint8_t start_pos) __z88dk_fastcall
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
    cursor_show();
}


void redraw_input_full(void)
{
    input_cache_invalidate();
    redraw_input_asm();
    cursor_show();
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

void fast_u8_to_str(char *buf, uint8_t val) __z88dk_callee
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
    cursor_hide();

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
            cursor_show();
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
        cursor_hide();
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
        cursor_hide();
        cursor_pos--;
        cursor_show();
    }
}

static void input_right(void)
{
    if (cursor_pos < line_len) {
        cursor_hide();
        cursor_pos++;
        cursor_show();
    }
}


// Hide cursor during command execution (visual feedback)
static void set_input_busy(uint8_t busy) __z88dk_fastcall
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
        cursor_show();
    }
}

// KEYBOARD HANDLING
uint8_t last_k;
uint8_t repeat_timer;
uint8_t debounce_zero;

// read_key is implemented in spectalk_asm.asm for size optimization


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
    if (connection_state != STATE_WIFI_OK) return;
    
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
    // Only send AT commands when in WiFi-ready state (not in transparent mode)
    if (connection_state != STATE_WIFI_OK) return;
    
    uart_send_line(S_AT_SNTPTIME);
    sntp_waiting = 1;
}

// sntp_process_response is implemented in spectalk_asm.asm for size optimization

// AT COMMAND HELPERS
// try_read_line_nodrain() está implementada en spectalk_asm.asm para mejor rendimiento

uint8_t wait_for_response(const char *expected, uint16_t max_frames) __z88dk_callee
{
    uint16_t frames = 0;
    rx_pos = 0;
    
    while (frames < max_frames) {
        HALT();
        
        if (in_inkey() == 7) return 0;  // EDIT = cancel
        
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            // FIX P0-1: Verificar longitud antes de acceder a índices fijos
            // Prefix checks para ERROR/FAIL/OK (siempre al inicio de línea)
            if (rx_last_len >= 2) {
                if (rx_line[0] == 'E' && rx_line[1] == 'R') return 0;  // ERROR
                if (rx_line[0] == 'F' && rx_line[1] == 'A') return 0;  // FAIL
            }
            // OPT-02: Usar st_stristr (ASM) en lugar de strstr (stdlib)
            if (expected && st_stristr(rx_line, expected) != NULL) return 1;
            // Si no hay expected específico, OK es suficiente
            if (!expected && rx_last_len >= 2 && rx_line[0] == 'O' && rx_line[1] == 'K') return 1;
            rx_pos = 0;
        }
        
        frames++;
    }
    
    return 0;
}

// Wait for any of up to three expected substrings in a line, with standard error/FAIL/CLOSED handling.
uint8_t wait_for_prompt_char(uint8_t prompt_ch, uint16_t max_frames) __z88dk_callee
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
static void esp_hard_cmd(const char *cmd) __z88dk_fastcall {
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
            // FIX P0-1: Verificar longitud antes de acceder a índices
            if (rx_last_len >= 2 && rx_line[0] == 'O' && rx_line[1] == 'K') {
                // ESP responds — check if WiFi has an IP
                uart_send_line("AT+CIFSR");
                rx_pos = 0;
                {
                    uint8_t has_ip = 0;
                    uint8_t w;
                    for (w = 0; w < 100; w++) {
                        HALT(); uart_drain_to_buffer();
                        if (try_read_line_nodrain()) {
                            // FIX P0-1: Verificar longitud
                            if (rx_last_len >= 1 && rx_line[0] == '+' && st_stristr(rx_line, "STAIP")) {
                                if (!st_stristr(rx_line, "0.0.0.0")) has_ip = 1;
                            }
                            if (rx_last_len >= 2 && rx_line[0] == 'O' && rx_line[1] == 'K') break;
                            rx_pos = 0;
                        }
                    }
                    closed_reported = 0;
                    if (has_ip) {
                        // Verify actual AP association (ESP may cache stale IP)
                        uart_send_line("AT+CWJAP?");
                        rx_pos = 0;
                        has_ip = 0;  // Reset - must confirm AP
                        {
                            uint8_t w2;
                            for (w2 = 0; w2 < 100; w2++) {
                                HALT(); uart_drain_to_buffer();
                                if (try_read_line_nodrain()) {
                                    // FIX P0-1: Verificar longitud antes de índices fijos
                                    if (rx_last_len >= 4) {
                                        // +CWJAP:"ssid"... means connected
                                        if (rx_line[0] == '+' && rx_line[1] == 'C' &&
                                            rx_line[2] == 'W' && rx_line[3] == 'J') {
                                            has_ip = 1;
                                        }
                                    }
                                    if (rx_last_len >= 2) {
                                        // "No AP" means not connected
                                        if (rx_line[0] == 'N' && rx_line[1] == 'o') {
                                            has_ip = 0;
                                        }
                                        if (rx_line[0] == 'O' && rx_line[1] == 'K') break;
                                    }
                                    rx_pos = 0;
                                }
                            }
                        }
                        if (has_ip) {
                            connection_state = STATE_WIFI_OK;
                        } else {
                            connection_state = STATE_DISCONNECTED;
                        }
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

// Time synchronization function
// OPT L2: sync_time() eliminada - inlined en call site

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
        // No QUIT here: sending QUIT before reconnect triggers server-side
        // throttle (many servers delay 001 for 30-120s after clean QUIT+reconnect).
        // Ghost nick clears itself via server ping timeout (~120s).
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
    lagmeter_counter = 0;

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
    names_was_manual = 0;
    
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
// Unified PONG: "PONG <server> :<token>\r\n"
void irc_send_pong(const char *token) __z88dk_fastcall
{
    uart_send_string(S_PONG);
    uart_send_string(irc_server);
    uart_send_string(S_SP_COLON);
    uart_send_line(token);
}

void irc_send_cmd1(const char *cmd, const char *p1) __z88dk_callee
{
    irc_send_cmd_internal(cmd, p1, NULL);
}

// Envía "CMD p1 :p2\r\n" - para comandos con texto final
void irc_send_cmd2(const char *cmd, const char *p1, const char *p2) __z88dk_callee
{
    irc_send_cmd_internal(cmd, p1, p2);
}

// Envía "PRIVMSG NickServ :IDENTIFY <pass>\r\n"
void send_identify(const char *pass) __z88dk_fastcall
{
    uart_send_string(S_PRIVMSG);
    uart_send_string(S_NICKSERV);
    uart_send_string(S_IDENTIFY_CMD);
    uart_send_line(pass);
}

// Enviar ISON con hasta 3 nicks de amigos (una sola vez por sesión IRC).
void irc_check_friends_online(void)
{
    // Reuse rx_line as temp buffer (not receiving during send)
    uint8_t i, any = 0;
    char *d = rx_line;

    if (friends_ison_sent) return;
    if (connection_state < STATE_TCP_CONNECTED) return;

    *d = '\0';

    for (i = 0; i < MAX_FRIENDS; i++) {
        const char *s = friend_nicks[i];
        if (!s[0]) continue;
        if (any) *d++ = ' ';
        st_copy_n(d, s, IRC_NICK_SIZE);
        while (*d) d++;
        any = 1;
    }

    if (!any || !rx_line[0]) return;
    friends_ison_sent = 1;
    irc_send_cmd1("ISON", rx_line);
}

// OPT-P2-B: Shared nick-in-use retry logic (dedup h_numeric_433 + cmd_connect)
void nick_try_alternate(void)
{
    uint8_t len = 0;
    while (irc_nick[len] && len < IRC_NICK_SIZE - 2) len++;
    if (len >= IRC_NICK_SIZE - 2) {
        // Nick at max length: rotate last char to generate variants
        char c = irc_nick[len - 1];
        if (c == '_') c = '0';
        else if (c >= '0' && c < '9') c++;
        else c = '_';
        irc_nick[len - 1] = c;
    } else {
        irc_nick[len] = '_';
        irc_nick[len + 1] = '\0';
    }

    set_attr_sys();
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
        uart_send_line(S_AWAY_CMD);
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

        // En 64 cols: imprimir "nick> " entero con ATTR_MSG_NICK
        set_attr_nick();
        main_puts2(irc_nick, S_PROMPT);

        set_attr_chan();
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
            // En la ventana del privado: mantener "YO> msg"
            set_attr_nick();
            main_puts2(irc_nick, S_PROMPT);

            set_attr_priv();
            main_print_wrapped_ram((char*)msg);
        } else {
            // Fuera de la ventana: >> NICKNAME (receptor) : MSG
            // Requisito: ">> NICKNAME (amarillo): MSG (verde)"
            set_attr_priv();
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
    uint8_t *t;

    // Protección de rango
    if (current_theme < 1 || current_theme > 3) current_theme = 1;
    t = theme_raw + (current_theme - 1) * 25;

    // Copy 20 attribute bytes from theme_raw into theme_attrs[]
    memcpy(theme_attrs, t, 20);

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

// Declaración de función ASM (fastcall: count en L)
extern void draw_badge_dither(uint8_t count) __z88dk_fastcall;

static uint8_t badge_flashing;
static uint8_t flash_timer;

void badge_flash_on(void)
{
    uint8_t *t = theme_raw + (current_theme - 1) * 25;
    if (t[21] != 0x40 || ((ATTR_BANNER >> 3) & 0x07)) return;
    badge_flashing = 1;
}

void badge_flash_off(void)
{
    if (!badge_flashing) return;
    badge_flashing = 0;
    *(uint8_t *)(0x5800 + 30) = ATTR_BANNER | 0x40;
    *(uint8_t *)(0x5800 + 31) = ATTR_BANNER | 0x40;
    *(uint8_t *)(0x5820 + 30) = ATTR_BANNER & 0xBF;
    *(uint8_t *)(0x5820 + 31) = ATTR_BANNER & 0xBF;
}

void draw_banner(void)
{
    uint8_t *t = theme_raw + (current_theme - 1) * 25;
    uint8_t bp = (ATTR_BANNER >> 3) & 0x07;  // banner PAPER color

    clear_line(0, ATTR_BANNER | 0x40);
    clear_line(1, ATTR_BANNER & 0xBF);
    print_big_str(0, 0, "SPECTALK ZX v" VERSION " - ", ATTR_BANNER);
    print_big_str(0, 21, S_APPDESC, ATTR_BANNER);

    if (t[21] != 0x40) {
        // Dithered badge: attrs + dither for 5 cells (columns 27-31)
        uint8_t b1 = t[21] | 0x40, b2 = t[22] | 0x40;
        uint8_t b3 = t[23] | 0x40, b4 = t[24] | 0x40;
        uint8_t sb = 0x40 | (bp << 3) | bp;

        *(uint8_t *)(0x5800 + 27) = sb;
        *(uint8_t *)(0x5800 + 28) = b1;
        *(uint8_t *)(0x5800 + 29) = b2;
        *(uint8_t *)(0x5800 + 30) = b3;
        *(uint8_t *)(0x5800 + 31) = b4;

        *(uint8_t *)(0x5820 + 27) = b1;
        *(uint8_t *)(0x5820 + 28) = b2;
        *(uint8_t *)(0x5820 + 29) = b3;
        *(uint8_t *)(0x5820 + 30) = b4;
        *(uint8_t *)(0x5820 + 31) = 0x40 | ((t[24] & 0x07) << 3) | ((t[20] >> 3) & 0x07);

        draw_badge_dither(5);
    } else {
        // Text badge (no dither)
        print_big_str(0, bp ? 58 : 61, bp ? "_ [] X" : "<<<", ATTR_BANNER);
    }

    // 1px separator line below banner (row 2, scanline 0)
    memset((uint8_t *)0x4040, 0xFF, 32);
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
    set_attr_chan();
}


// =============================================================================
// OPTIMIZED UI HELPERS (Saves ROM space vs Macros)
// =============================================================================

void ui_err(const char *s) __z88dk_fastcall
{
    set_attr_err();
    main_print(s);
}

void ui_sys(const char *s) __z88dk_fastcall
{
    set_attr_sys();
    main_print(s);
}

void ui_usage(const char *a) __z88dk_fastcall
{
    set_attr_err();
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
    if (n > RING_BUFFER_SIZE - 2) n = RING_BUFFER_SIZE - 2;

    esx_fclose();

    ring_buffer[n] = '\0';
    return n;
}

// Parse a decimal string into uint8_t
// OPT H5: cfg_parse_num eliminada - usar (uint8_t)str_to_u16() directamente

// Return pointer to next comma-separated token (NUL-terminates it).
// Returns NULL when no more tokens. Skips leading spaces.
static char *csv_next_tok(char **pp) {
    char *tok = *pp;
    char *comma;
    if (!tok || !*tok) return NULL;
    while (*tok == ' ') tok++;
    if (!*tok) return NULL;
    comma = tok;
    while (*comma && *comma != ',') comma++;
    if (*comma) { *comma = '\0'; *pp = comma + 1; }
    else *pp = NULL;
    // Trim trailing spaces
    { char *e = comma; while (e > tok && e[-1] == ' ') e--; *e = '\0'; }
    return tok;
}

// Apply a key=value pair
static void cfg_apply(char *key, char *val) __z88dk_callee {
    // 2-char dispatch: fast and tiny
    uint8_t k0 = key[0], k1 = key[1];
    uint8_t klen = st_strlen(key);
    
    if (k0 == 'n' && k1 == 'i') {           // nick / nickpass
        if (klen >= 8 && key[4] == 'p') {   // nickpass (8 chars)
            st_copy_n(nickserv_pass, val, IRC_PASS_SIZE);
        } else if (klen == 4) {             // nick (4 chars)
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
    } else if (k0 == 'a' && k1 == 'u') {
        // FIX P2-2: Verificar claves exactas para evitar interpretación errónea
        // autoconnect = 11 chars, autoaway = 8 chars
        uint8_t v = (uint8_t)str_to_u16(val);
        if (klen == 11 && key[4] == 'c' && key[5] == 'o') {  // autoconnect
            autoconnect = v & 1;
        } else if (klen == 8 && key[4] == 'a' && key[5] == 'w') {  // autoaway
            if (v <= 60) autoaway_minutes = v;
        }
        // Claves no reconocidas (autoxxx) se ignoran silenciosamente
    } else if (k0 == 'f' && k1 == 'r') {  // friends=nick1,nick2,...
        uint8_t idx = 0;
        char *tok, *p = val;
        while (idx < MAX_FRIENDS && (tok = csv_next_tok(&p)) != NULL)
            st_copy_n(friend_nicks[idx++], tok, IRC_NICK_SIZE);
    } else if (k0 == 'i' && k1 == 'g') {  // ignores=nick1,nick2,...
        char *tok, *p = val;
        while (ignore_count < MAX_IGNORES && (tok = csv_next_tok(&p)) != NULL)
            add_ignore(tok);
    } else if (k0 == 'b' && k1 == 'e') {    // beep
        beep_enabled = (uint8_t)str_to_u16(val) & 1;
    } else if (k0 == 't' && k1 == 'r') {    // traffic
        show_traffic = (uint8_t)str_to_u16(val) & 1;
    } else if (k0 == 't' && k1 == 'i') {    // timestamps (0=off, 1=on, 2=smart)
        {
            uint8_t v = (uint8_t)str_to_u16(val);
            show_timestamps = (v > 2) ? 1 : v;
        }
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
// SAFETY-M3: depends on ring_buffer[] being NUL-terminated by cfg_try_read.
// Max read = RING_BUFFER_SIZE-2 = 2046 bytes, NUL at ring_buffer[n].
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

    if (!has_esxdos) return 0;

    n = cfg_try_read(K_CFG_PRI);
    if (!n) n = cfg_try_read(K_CFG_ALT);
    if (!n) return 0;

    cfg_parse_buf();
    return 1;
}


// MAIN FUNCTION

void main(void)
{
    uint8_t c;
    uint8_t cfg_ok;
    uint8_t can_autoconnect;
    
    has_esxdos = esx_detect();

    // Load font + themes from SPECTALK.DAT
    // Layout: [298 font][75 themes][help text]
    if (has_esxdos) {
        extern uint8_t font_lut[];
        esx_fopen(K_DAT);
        if (esx_handle) {
            esx_buf = (uint16_t)font_lut;
            esx_count = 373;
            esx_fread();
            esx_fclose();
        }
    }

    cfg_ok = config_load();  // Load settings before theme/screen init
    apply_theme();
    init_screen();

    main_line = MAIN_START;
    main_col = 0;
    set_attr_sys();
    main_print(S_APPNAME);
    main_print(S_APPDESC);
    main_print(S_COPYRIGHT);
    main_hline();

    // --- Initialization ---
    {
        uint8_t retries = 2;
        while (retries--) {
            set_attr_priv();
            main_puts(S_INIT_DOTS);
            if (esp_init()) { main_newline(); break; }
            main_putc(' '); set_attr_err(); main_puts(S_FAIL); main_newline();
            if (retries) {
                ui_sys("Press any key to retry...");
                // FIX: Drenar la UART mientras esperamos la tecla
                while (!in_inkey()) { HALT(); uart_drain_to_buffer(); }
                while (in_inkey()) { HALT(); uart_drain_to_buffer(); }
            }
        }
    }
    
    // Check WiFi
    set_attr_priv();
    main_puts("Checking connection...");
    
    if (connection_state < STATE_WIFI_OK) {
        main_putc(' '); set_attr_err(); main_puts("NO WIFI"); main_newline();
        ui_sys("Join WiFi network first");
    } else {
        main_newline();
        sntp_init();  // OPT L2: inlined sync_time()
    }
    // -------------------------------------------

    set_attr_sys();
    main_hline();
    main_print("Type !help or !about for more info.");

    // Autoconnect only if ALL conditions are met
    can_autoconnect = autoconnect && cfg_ok && irc_server[0]
                      && irc_nick[0]
                      && connection_state >= STATE_WIFI_OK;

    if (cfg_ok) {
        main_puts("Config loaded.");
        if (irc_server[0]) {
            if (can_autoconnect) {
                main_puts(" Autoconnecting");
                main_puts2(S_AS_SP, irc_nick);
                main_putc(' '); main_puts(S_DOTS3);
            } else if (!autoconnect) {
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
        uint8_t sntp_timer = 0;
        uint8_t autoconnect_delay = can_autoconnect ? 200 : 0;  // ~4 sec delay

        // FIX: Ocultar cursor durante autoconnect countdown
        if (autoconnect_delay) { cursor_visible = 0; redraw_input_full(); }

        // FIX: muestrear SHIFT 1 vez por frame y reutilizarlo
        uint8_t shift_held = 0;

        // Sync frame counter before entering main loop
        last_frames_lo = *(volatile uint8_t *)23672;
        tick_accum = 0;

        while (1) {
            HALT(); // 50 Hz Sync
            
            // Read system FRAMES (23672) low byte and compute elapsed frames.
            // This correctly accounts for frames lost during long processing.
            {
            uint8_t now_lo = *(volatile uint8_t *)23672;
            uint8_t elapsed = now_lo - last_frames_lo;  // wraps correctly (uint8)
            last_frames_lo = now_lo;
            tick_accum += elapsed;
            }
            while (tick_accum >= 50) {
                tick_accum -= 50;
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
                        uart_send_string("AWAY :");
                        uart_send_line(S_AUTOAWAY);
                        st_copy_n(away_message, S_AUTOAWAY, sizeof(away_message));
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
            if (sntp_init_sent || names_pending) {
                if (sntp_init_sent && !sntp_queried) {
                    if (!sntp_waiting) {
                        if (++sntp_timer >= 100) {
                            sntp_query_time();
                            sntp_timer = 25;  // Retry after ~1.5 sec
                        }
                    }
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
            
            // 1.5. KEEP-ALIVE & LAGMETER
            if (connection_state == STATE_IRC_READY) {
                server_silence_frames++;
                lagmeter_counter++;
                
                if (keepalive_ping_sent) {
                    // Waiting for PONG - check timeout
                    if (++keepalive_timeout >= KEEPALIVE_TIMEOUT_FRAMES) {
                        // No response to PING - connection is dead
                        set_attr_err();
                        main_puts(S_TIMEOUT);
                        main_print(" (no response)");
                        force_disconnect();
                        draw_status_bar();
                    }
                } else if (server_silence_frames >= KEEPALIVE_SILENCE_FRAMES && !pagination_active) {
                    // No server activity for too long - send PING to check
                    uart_send_string("PING :keepalive\r\n");
                    keepalive_ping_sent = 1;
                    keepalive_timeout = 0;
                    lagmeter_counter = 0;
                } else if (lagmeter_counter >= LAGMETER_INTERVAL_FRAMES && 
                           !pagination_active && !buffer_pressure) {
                    // Periodic lag measurement (postponed during heavy traffic)
                    uart_send_string("PING :lag\r\n");
                    keepalive_ping_sent = 1;
                    keepalive_timeout = 0;
                    lagmeter_counter = 0;
                }
            } else {
                // Not connected - reset counters
                server_silence_frames = 0;
                keepalive_ping_sent = 0;
                lagmeter_counter = 0;
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
                        set_attr_err();
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
                cursor_show();
            }
            
            c = read_key();

            // Badge "<<<" blink: toggle INK green every 16 frames
            if (badge_flashing && !(++flash_timer & 0x0F)) {
                *(uint8_t *)(0x5800 + 30) ^= 0x04;
                *(uint8_t *)(0x5800 + 31) ^= 0x04;
                *(uint8_t *)(0x5820 + 30) ^= 0x04;
                *(uint8_t *)(0x5820 + 31) ^= 0x04;
            }

            // Any keypress resets auto-away counter and badge flash
            if (c) { autoaway_counter = 0; badge_flash_off(); }

            // D2: SHIFT+5/6/7/8 => cursor keys via lookup table
            if (c >= '5' && c <= '8' && shift_held) {
                static const uint8_t shift_keys[] = {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT};
                c = shift_keys[c - '5'];
            }

            // Help overlay (state-based, non-blocking)
            if (help_active) {
                if (c == 3) {  // BREAK
                    help_active = 0;
                    cursor_visible = 1;
                    clear_main();
                    redraw_input_full();
                } else if (c) {
                    help_page++;
                    help_render_page();
                    if (!help_active) { cursor_visible = 1; clear_main(); redraw_input_full(); }
                }
                c = 0;  // consume all keys while help is open
            }

            // Channel switcher overlay (non-blocking, state-based)
            if (sw_active) {
                // Rebuild map BEFORE key handling to avoid stale sw_map
                if (sw_dirty) switcher_render();

                // Track EDIT key release to prevent instant cancel
                if (!sw_released && in_inkey() == 0) sw_released = 1;

                if (c) {
                    sw_timeout = 0;  // reset auto-close on any key

                    if (c == KEY_LEFT) {
                        sw_sel = sw_sel ? sw_sel - 1 : sw_count - 1;  // wrap
                        sw_dirty = 1;
                    } else if (c == KEY_RIGHT) {
                        sw_sel = (sw_sel < sw_count - 1) ? sw_sel + 1 : 0;  // wrap
                        sw_dirty = 1;
                    } else if (c == KEY_ENTER) {
                        switch_to_channel(sw_map[sw_sel]);
                        switcher_close();
                    } else if (c == KEY_BACKSPACE) {
                        // Close/part selected channel (not Server)
                        uint8_t idx = sw_map[sw_sel];
                        if (idx > 0) {
                            uint8_t f = channels[idx].flags;
                            if (f & CH_FLAG_ACTIVE) {
                                if (!(f & CH_FLAG_QUERY)) {
                                    // Real channel: send PART to server
                                    irc_send_cmd1("PART", channels[idx].name);
                                }
                                remove_channel(idx);
                            }
                            // Rebuild switcher map
                            sw_count = 0;
                            sw_sel = 0;
                            { uint8_t k;
                            for (k = 0; k < MAX_CHANNELS; k++) {
                                if (channels[k].flags & CH_FLAG_ACTIVE) {
                                    if (k == current_channel_idx) sw_sel = sw_count;
                                    sw_map[sw_count++] = k;
                                }
                            }}
                            if (sw_count < 2) { switcher_close(); }
                            else sw_dirty = 1;
                        }
                    } else if (c == 7 && sw_released) {
                        switcher_close();
                    } else if (c >= '0' && c <= '9') {
                        // Direct slot jump: find slot in active map
                        uint8_t slot = c - '0';
                        uint8_t k;
                        for (k = 0; k < sw_count; k++) {
                            if (sw_map[k] == slot) {
                                switch_to_channel(slot);
                                switcher_close();
                                break;
                            }
                        }
                    }
                } else if (sw_active) {
                    // No key this frame — auto-close after ~10s (500 frames)
                    if (++sw_timeout >= 1000) switcher_close();

                    // Live refresh: detect flag CHANGES via snapshot
                    {
                        uint8_t k;
                        for (k = 0; k < sw_count; k++) {
                            uint8_t f = channels[sw_map[k]].flags;
                            if ((f & (CH_FLAG_UNREAD | CH_FLAG_MENTION)) != (sw_flags_snap[k] & (CH_FLAG_UNREAD | CH_FLAG_MENTION))) {
                                sw_dirty = 1;
                                break;
                            }
                        }
                    }
                }

                if (sw_dirty) switcher_render();
                c = 0;  // consume all keys while switcher is open
            } else if (c == 7 && !pagination_active && search_mode == SEARCH_NONE && !autoconnect_delay) {
                switcher_open();
                c = 0;
            }

            if (c != 0 && !pagination_active && search_mode == SEARCH_NONE && !autoconnect_delay) {
                if (c >= 32 && c <= 126) {
                    uint8_t c_lower = c | 32;

                    if (c_lower >= 'a' && c_lower <= 'z') {
                        c = c_lower ^ ((caps_lock_mode ^ shift_held) << 5);
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
                        cursor_show();
                     }
                }
                else if (c == KEY_BACKSPACE) input_backspace();
                else if ((c & 0xFE) == KEY_LEFT) {
                    if (c == KEY_LEFT) input_left(); else input_right();
                }
                else if ((c & 0xFE) == KEY_DOWN) {
                    if (c == KEY_UP) history_nav_up(); else history_nav_down();
                    redraw_input_full();
                }
            }
            
            process_irc_data();
        }
    }
}
