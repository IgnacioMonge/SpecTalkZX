/*
 * user_cmds.c - User command parsing and handling (SIZE OPTIMIZED)
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
void cmd_quit(const char *args) __z88dk_fastcall;

// =============================================================================
// INTERNAL HELPERS (Reduce duplication)
// =============================================================================

// Helper: Verifica conexión TCP. Retorna 1 si OK, 0 si falla (y muestra error)
static uint8_t ensure_connected(void)
{
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return 0;
    }
    return 1;
}

// Helper: Verifica argumentos. Retorna 1 si OK, 0 si falla (y static void cmd_quit(const char *args) __z88dk_fastcall usage)
static uint8_t ensure_args(const char *args, const char *usage)
{
    if (!args || !*args) {
        ui_usage(usage);
        return 0;
    }
    return 1;
}

// Función auxiliar para diagnosticar la respuesta de conexión
static uint8_t wait_for_connection_result(uint16_t max_frames)
{
    uint16_t frames = 0;
    rx_pos = 0;
    
    while (frames < max_frames) {
        HALT();
        if (in_inkey() == 7) { ui_err("Cancelled"); return 0; }
        
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            // OPTIMIZACIÓN: "CONNECT" también matchea "ALREADY CONNECT", ahorramos un strstr
            if (strstr(rx_line, "CONNECT") || strstr(rx_line, "OK")) {
                rx_pos = 0;
                return 1;
            }
            // ERRORES
            if (strstr(rx_line, "DNS FAIL")) { ui_err("Hostname not found"); rx_pos = 0; return 0; }
            if (strstr(rx_line, "CLOSED") || strstr(rx_line, "conn fail")) { ui_err("Connection refused"); rx_pos = 0; return 0; }
            
            // Ignoramos eco del comando "AT+"
            if (strncmp(rx_line, "AT+", 3) != 0 && strstr(rx_line, "ERROR")) {
                ui_err("Connection error");
                rx_pos = 0; return 0;
            }
            rx_pos = 0;
        }
        frames++;
    }
    ui_err(S_TIMEOUT);
    return 0;
}

extern char ay_uart_read(void);
extern uint8_t ay_uart_ready(void);

// =============================================================================
// COMMAND HANDLERS
// =============================================================================

static void cmd_connect(const char *args) __z88dk_fastcall
{ 
    const char *port;
    const char *sep;
    uint8_t server_len;
    uint8_t result;
    uint8_t use_ssl = 0;
    uint8_t already_disconnected = 0; 

    if (connection_state < STATE_WIFI_OK) {
        ui_err("Error: No WiFi connection. Use /init first.");
        return;
    }

    if (connection_state >= STATE_TCP_CONNECTED) {
        ui_err("Already connected.");
        current_attr = ATTR_MSG_SYS;
        main_print("Disconnect? (y/n)");
        while (1) {
            uint8_t k = in_inkey();
            HALT(); uart_drain_to_buffer();
            if (k == 'n' || k == 'N') { main_print("Cancelled"); return; }
            if (k == 'y' || k == 'Y') {
                main_print("Disconnecting...");
                force_disconnect();
                irc_server[0] = 0; network_name[0] = 0;
                draw_status_bar(); draw_status_bar_real();
                already_disconnected = 1;
                break; 
            }
        }
    }

    if (!ensure_args(args, "server host [port]")) return;
    
    sep = strchr(args, ' ');
    if (!sep) sep = strchr(args, ':');
    
    if (sep) {
        server_len = (uint8_t)(sep - args);
        port = sep + 1;
        while (*port == ' ') port++;
    } else {
        server_len = strlen(args);
        port = "6667";
    }
    
    if (server_len > sizeof(irc_server) - 1) server_len = sizeof(irc_server) - 1;
    memcpy(irc_server, args, server_len);
    irc_server[server_len] = '\0';
    
    strncpy(irc_port, port, sizeof(irc_port) - 1); 
    irc_port[sizeof(irc_port) - 1] = 0;
    
    if (strcmp(irc_port, "6697") == 0) use_ssl = 1;
    
    current_attr = ATTR_MSG_PRIV;
    main_puts("Connecting to "); main_puts(irc_server); main_putc(':'); main_puts(irc_port); main_puts("... ");
    
    if (!already_disconnected) { force_disconnect(); } 
    else { reset_all_channels(); rb_head = 0; rb_tail = 0; rx_pos = 0; rx_line[0] = '\0'; user_mode[0] = '\0'; }
    
    draw_status_bar(); cursor_visible = 0; redraw_input_full(); 
    
    esp_at_cmd("AT+CIPMUX=0");
    esp_at_cmd("AT+CIPSERVER=0");
    esp_at_cmd("AT+CIPDINFO=0");
    if (use_ssl) { esp_at_cmd("AT+CIPSSLSIZE=4096"); }

    uart_send_string("AT+CIPSTART=\"");
    uart_send_string(use_ssl ? "SSL" : "TCP");
    uart_send_string("\",\""); uart_send_string(irc_server); uart_send_string("\","); uart_send_line(irc_port);
    
    ui_freeze_status = 1; rx_pos = 0; rb_head = rb_tail = 0; result = 0;
    
    { uint16_t fl = use_ssl ? TIMEOUT_SSL : TIMEOUT_DNS; result = wait_for_connection_result(fl); }
    ui_freeze_status = 0;
    
    if (result == 0) {
        main_newline(); force_disconnect(); connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; draw_status_bar(); redraw_input_full(); return;
    }
    
    current_attr = ATTR_MSG_PRIV; main_print(S_OK); 
    
    wait_drain(20);
    rb_head = rb_tail = 0; rx_pos = 0;
    
    uart_send_line("AT+CIPMODE=1");
    if (!wait_for_response(S_OK, 100)) {
        ui_err("CIPMODE FAIL"); force_disconnect(); connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; draw_status_bar(); redraw_input_full(); return;
    }
    
    wait_drain(20);
    uart_send_string("AT+CIPSEND\r\n");
    
    if (!wait_for_prompt_char('>', TIMEOUT_PROMPT)) {
        ui_err("Error: No '>' prompt"); force_disconnect(); connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; draw_status_bar(); redraw_input_full(); return;
    }
    
    connection_state = STATE_TCP_CONNECTED; closed_reported = 0;
    rx_pos = 0; rx_overflow = 0;
    
    if (irc_nick[0]) {
        char *line; // <--- RESTAURADO: Necesario porque se modifica el puntero
        uint8_t loop_done = 0;
        uint16_t silence_frames = 0;
        
        current_attr = ATTR_MSG_PRIV; main_puts("Registering... ");
        
        if (irc_pass[0]) irc_send_cmd1("PASS", irc_pass);
        irc_send_cmd1("NICK", irc_nick);
        uart_send_string("USER "); uart_send_string(irc_nick); 
        uart_send_string(" 0 * :"); uart_send_line(irc_nick);
        
        rx_pos = 0;
        
        while (!loop_done) {
            HALT(); uart_drain_to_buffer();
            if (in_inkey() == 7) { 
                ui_err("Aborted."); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; break; 
            }
            
            if (try_read_line_nodrain()) {
                silence_frames = 0;
                line = rx_line;
                if (line[0] == '@') {
                    line = strchr(line, ' ');
                    if (line) line++; else { rx_pos = 0; continue; }
                }
                
                if (strstr(line, " 001 ")) {
                    current_attr = ATTR_MSG_PRIV; main_print("Connected!");
                    connection_state = STATE_IRC_READY; loop_done = 1; rx_pos = 0; continue; 
                }
                
                if (strstr(line, "CAP") && strstr(line, "LS")) {
                    uart_send_line("CAP END");
                    rx_pos = 0; continue;
                }
                
                {
                    char *ping_ptr = NULL;
                    if (line[0] == 'P' && line[1] == 'I') ping_ptr = line; 
                    else ping_ptr = strstr(line, " PING ");
                    
                    if (ping_ptr) {
                        char *params = ping_ptr;
                        if (*params == ' ') params++;
                        params += 4; while (*params == ' ') params++;
                        uart_send_string("PONG "); uart_send_line(params);
                        rx_pos = 0; continue;
                    }
                }

                if (strstr(line, "ERROR :")) {
                    ui_err("Server error"); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue;
                }
                
                if (rx_line[0] == 'C' && strcmp(rx_line, S_CLOSED) == 0) {
                    ui_err("Connection lost"); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue;
                }
                
                if (strstr(line, " 433 ")) { ui_err("Nick already in use"); connection_state = STATE_IRC_READY; loop_done = 1; rx_pos = 0; continue; }
                if (strstr(line, " 432 ") || strstr(line, " 436 ")) { ui_err("Invalid nickname"); connection_state = STATE_IRC_READY; loop_done = 1; rx_pos = 0; continue; }
                if (strstr(line, " 464 ") || strstr(line, " 461 ")) { ui_err("Auth failed"); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue; }
                if (strstr(line, " 465 ") || strstr(line, " 466 ")) { ui_err("Banned"); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue; }
                
                rx_pos = 0;
            } else {
                silence_frames++;
                if (silence_frames > 1500) { 
                    ui_err(S_TIMEOUT); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; 
                }
            }
        }
        rb_head = rb_tail = 0; rx_pos = 0; rx_overflow = 0;
    } else {
        ui_sys("Set /nick first");
    }
    cursor_visible = 1; draw_status_bar(); redraw_input_full();
}

static void cmd_nick(const char *args) __z88dk_fastcall
{
    if (!args || !*args) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Current nick: ");
        if (irc_nick[0]) main_print(irc_nick); else main_print(S_NOTSET);
        return;
    }
    
    strncpy(irc_nick, args, sizeof(irc_nick) - 1);
    irc_nick[sizeof(irc_nick) - 1] = '\0';
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Nick set to: ");
    main_print(irc_nick);
    
    if (connection_state >= STATE_TCP_CONNECTED) {
        irc_send_cmd1("NICK", irc_nick);
    }
    draw_status_bar();
}

static void cmd_pass(const char *args) __z88dk_fastcall
{
    if (!args || !*args) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Server password: ");
        main_print(irc_pass[0] ? "(set)" : S_NOTSET);
        return;
    }
    
    if (strcmp(args, "clear") == 0 || strcmp(args, "none") == 0) {
        irc_pass[0] = '\0';
        ui_sys("Password cleared");
        return;
    }
    
    strncpy(irc_pass, args, sizeof(irc_pass) - 1);
    irc_pass[sizeof(irc_pass) - 1] = '\0';
    ui_sys("Password set");
}

static void cmd_join(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "join #channel")) return;
    if (!ensure_connected()) return;

    while (*args == ' ') args++;

    int8_t idx = find_channel(args);
    if (idx >= 0) {
        if ((uint8_t)idx == current_channel_idx) {
            ui_sys("Already in "); main_print(channels[idx].name);
        } else {
            switch_to_channel((uint8_t)idx);
            current_attr = ATTR_MSG_SYS; main_puts(S_SWITCHTO); main_print(channels[idx].name);
        }
        return;
    }

    if (find_empty_channel_slot() == -1) { ui_err(S_MAXWIN); main_print("Use /close or /part first."); return; }
    
    if (*args != '#' && *args != '&') {
        uart_send_string("JOIN #");
        uart_send_string(args);
        uart_send_string("\r\n");
    } else {
        irc_send_cmd1("JOIN", args);
    }
}

static void cmd_part(const char *args) __z88dk_fastcall
{
    const char *chan = args && *args ? args : irc_channel;
    if (!chan[0]) { ui_err("Not in a channel"); return; }
    
    irc_send_cmd1("PART", chan);
    
    int8_t idx = find_channel(chan);
    if (idx >= 0) { remove_channel((uint8_t)idx); draw_status_bar(); }
}

static void cmd_msg(const char *args) __z88dk_fastcall
{
    char *p = (char *)args;
    if (!ensure_args(p, "msg nick message")) return;
    
    char *target = p;
    while (*p && *p != ' ') p++;
    if (!*p) { ui_usage("msg nick message"); return; }
    
    *p = '\0'; 
    char *msg = p + 1;
    while (*msg == ' ') msg++; 
    if (!*msg) { ui_err("Empty message"); return; }
    
    irc_send_privmsg(target, msg);
}

static void cmd_query(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "query nick")) return;
    
    int8_t idx = add_query(args);
    
    if (idx >= 0) {
        if ((uint8_t)idx != current_channel_idx) {
            switch_to_channel((uint8_t)idx);
            current_attr = ATTR_MSG_SYS; 
            main_puts("Query opened with "); 
            main_print(channels[idx].name);
        }
        status_bar_dirty = 1;
    } else {
        ui_err(S_MAXWIN);
        main_print("Use /close first.");
    }
}

void cmd_quit(const char *args) __z88dk_fastcall
{
    if (connection_state >= STATE_TCP_CONNECTED) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Disconnecting from ");
        main_puts(irc_server);
        main_print("...");
        
        uart_send_string("QUIT :");
        uart_send_line((args && *args) ? args : "SpecTalk ZX");
        
        wait_drain(25);
        force_disconnect();
        cursor_visible = 1;
        ui_sys(S_DISCONN);
        draw_status_bar();
    } else {
        ERR_NOTCONN();
    }
}


static void cmd_me(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "me action")) return;
    if (connection_state < STATE_IRC_READY) { ERR_NOTCONN(); return; }
    if (!irc_channel[0]) { ui_err("No channel or query"); return; }
    
    // Construcción manual del CTCP ACTION para evitar corrupción de strings
    // Formato: PRIVMSG #chan :\x01ACTION text\x01
    uart_send_string(S_PRIVMSG);
    uart_send_string(irc_channel);
    uart_send_string(" :");
    ay_uart_send(1);            // Enviar byte 0x01 (SOH) crudo
    uart_send_string("ACTION ");
    uart_send_string(args);
    ay_uart_send(1);            // Enviar byte 0x01 (SOH) crudo
    uart_send_crlf();
    
    current_attr = ATTR_MSG_SELF;
    main_puts("* "); main_puts(irc_nick); main_putc(' '); main_print(args);
}

static void cmd_away(const char *args) __z88dk_fastcall
{
    if (connection_state < STATE_IRC_READY) { 
        ERR_NOTCONN(); 
        return; 
    }
    
    uart_send_string("AWAY");
    
    if (args && *args) {
        // Ponerse AWAY (manual)
        uart_send_string(" :");
        uart_send_line(args);
        
        irc_is_away = 1;
        autoaway_active = 0;  // Away manual: no quitar automáticamente
    } else {
        // Quitar AWAY
        uart_send_crlf();
        
        irc_is_away = 0;
        autoaway_active = 0;
        autoaway_counter = 0;
    }
    
    draw_status_bar(); 
}

static void cmd_raw(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "raw IRC_COMMAND")) return;
    if (!ensure_connected()) return;
    uart_send_line(args);
}

static void cmd_whois(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "whois nick")) return;
    if (!ensure_connected()) return;
    irc_send_cmd1("WHOIS", args);
}

static void cmd_list(const char *args) __z88dk_fastcall
{
    if (!ensure_connected()) return;

    if (!args || !*args) { ui_usage("list #channel"); main_print("(Full list disabled)"); return; }
    while (*args == ' ') args++;

    current_attr = ATTR_MSG_SYS;
    main_puts("LIST: ");
    main_print(args);

    if (pagination_active || search_mode != SEARCH_NONE || search_draining ||
        pending_search_type != PEND_NONE || rb_head != rb_tail) {
        queue_search_command(PEND_LIST, args);
        return;
    }

    start_search_command(PEND_LIST, args);
}

static void cmd_who(const char *args) __z88dk_fastcall
{
    if (!ensure_connected()) return;
    
    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0]) { ui_usage("who #channel or nick"); return; }
    while (*target == ' ') target++;

    current_attr = ATTR_MSG_SYS;
    main_puts("WHO: ");
    main_print(target);

    if (pagination_active || search_mode != SEARCH_NONE || search_draining ||
        pending_search_type != PEND_NONE || rb_head != rb_tail) {
        queue_search_command(PEND_WHO, target);
        return;
    }

    start_search_command(PEND_WHO, target);
}

static void cmd_names(const char *args) __z88dk_fastcall
{
    if (!ensure_connected()) return;
    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0]) { ui_usage("names [#channel]"); return; }
    
    counting_new_users = 1;
    show_names_list = 1;
    irc_send_cmd1("NAMES", target);
    ui_sys("Listing users...");
}

static void cmd_topic(const char *args) __z88dk_fastcall
{
    if (!ensure_connected()) return;
    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0]) { ui_usage("topic [#channel] [text]"); return; }
    
    // Si hay texto después del canal, es un set topic
    const char *space = strchr(target, ' ');
    if (space) {
    
        size_t len = space - target;
        char chan[32];
        if (len > 31) len = 31;
        memcpy(chan, target, len);
        chan[len] = 0;
        
        irc_send_cmd2("TOPIC", chan, space + 1);
    } else {
        irc_send_cmd1("TOPIC", target);
    }
}

static void cmd_search(const char *args) __z88dk_fastcall
{
    char pattern[32];
    uint8_t i;
    const char *src;
    
    if (!ensure_connected()) return;
    if (!ensure_args(args, "search #pattern or nick")) return;
    
    while (*args == ' ') args++;

    if (args[0] == '#') {
        src = args + 1;
        while (*src == ' ') src++;
        if (!*src) { ui_err("Empty pattern"); return; }

        i = 0;
        while (*src && *src != ' ' && i < 30) {
            pattern[i++] = *src++;
        }
        pattern[i] = 0;

        current_attr = ATTR_MSG_SYS;
        main_puts("Searching: *"); main_puts(pattern); main_print("*");

        if (pagination_active || search_mode != SEARCH_NONE || search_draining ||
            pending_search_type != PEND_NONE || rb_head != rb_tail) {
            queue_search_command(PEND_SEARCH_CHAN, pattern);
            return;
        }
        start_search_command(PEND_SEARCH_CHAN, pattern);

    } else {
        src = args;
        i = 0;
        while (*src && *src != ' ' && i < 30) {
            pattern[i++] = *src++;
        }
        pattern[i] = 0;

        if (!pattern[0]) { ui_err("Empty pattern"); return; }

        current_attr = ATTR_MSG_SYS;
        main_puts("Searching users: "); main_print(pattern);

        if (pagination_active || search_mode != SEARCH_NONE || search_draining ||
            pending_search_type != PEND_NONE || rb_head != rb_tail) {
            queue_search_command(PEND_SEARCH_USER, pattern);
            return;
        }
        start_search_command(PEND_SEARCH_USER, pattern);
    }
}

static void cmd_ignore(const char *args) __z88dk_fastcall
{
    uint8_t i;
    
    if (!args || !*args) {
        current_attr = ATTR_MSG_SYS;
        if (ignore_count == 0) {
            main_print("Ignore list is empty");
        } else {
            main_puts("Ignored (");
            main_putc('0' + ignore_count);
            main_puts("): ");
            for (i = 0; i < ignore_count; i++) {
                if (i > 0) main_puts(", ");
                main_puts(ignore_list[i]);
            }
            main_newline();
        }
        return;
    }
    
    if (args[0] == '-') {
        const char *nick = args + 1;
        while (*nick == ' ') nick++;
        if (!*nick) { ui_usage("ignore -nick to unignore"); return; }
        
        if (remove_ignore(nick)) {
            current_attr = ATTR_MSG_SYS; main_puts("Unignored: "); main_print(nick);
        } else {
            current_attr = ATTR_ERROR; main_puts("Not in ignore list: "); main_print(nick);
        }
        return;
    }
    
    if (add_ignore(args)) {
        current_attr = ATTR_MSG_SYS; main_puts("Now ignoring: "); main_print(args);
    } else if (is_ignored(args)) {
        current_attr = ATTR_ERROR; main_puts("Already ignoring: "); main_print(args);
    } else {
        ui_err("Ignore list full (8)");
    }
}

static void cmd_kick(const char *args) __z88dk_fastcall
{
    char *nick;
    char *reason;
    
    if (!ensure_connected()) return;
    if (!ensure_args(args, "kick nick [reason]")) return;
    
    if (current_channel_idx == 0 || channels[current_channel_idx].is_query || 
        irc_channel[0] != '#') {
        ui_err("Must be in a channel");
        return;
    }
    
    nick = (char *)args;
    reason = nick;
    while (*reason && *reason != ' ') reason++;
    if (*reason) {
        *reason = '\0'; 
        reason++;
        while (*reason == ' ') reason++; 
    }
    
    // OPTIMIZADO: Uso de chars directos y crlf
    uart_send_string("KICK ");
    uart_send_string(irc_channel);
    ay_uart_send(' ');      // Optimizado: un solo byte
    uart_send_string(nick);
    if (*reason) {
        uart_send_string(" :");
        uart_send_string(reason);
    }
    uart_send_crlf();       // Optimizado
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Kicking "); main_puts(nick);
    if (*reason) {
        main_puts(" ("); main_puts(reason); main_puts(")");
    }
    main_newline();
}

static void sys_help(const char *args) __z88dk_fastcall; 

// HELP SYSTEM
#define HA_TITLE  0
#define HA_SECTION 1
#define HA_CMD    2
#define HA_DESC   3
#define HA_NOTE   4

static const uint8_t help_attrs[] = {
    PAPER_BLUE | INK_WHITE | BRIGHT,
    PAPER_BLUE | INK_CYAN | BRIGHT, 
    PAPER_BLUE | INK_YELLOW | BRIGHT,
    PAPER_BLUE | INK_WHITE,        
    PAPER_BLUE | INK_CYAN          
};

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t attr;
    const char *text;
} HelpLine;

static const HelpLine help_page1[] = {
    { 0, 14, HA_TITLE,  "===== SPECTALK HELP (1/2) =====" },
    { 2,  2, HA_SECTION,"SYSTEM (!)" },
    { 3,  2, HA_CMD,    "!help !status !about !init !theme" },
    { 5,  2, HA_SECTION,"CONNECTION" },
    { 6,  2, HA_CMD,    "/nick Name" },
    { 6, 22, HA_DESC,   "Set nick" },
    { 7,  2, HA_CMD,    "/server host[:port]" },
    { 7, 22, HA_DESC,   "Connect (6697=SSL)" },
    { 8,  2, HA_CMD,    "/quit [msg]" },
    { 8, 22, HA_DESC,   "Disconnect" },
    {10,  2, HA_SECTION,"CHANNELS" },
    {11,  2, HA_CMD,    "/join #chan" },
    {11, 22, HA_DESC,   "Join" },
    {11, 32, HA_CMD,    "/part" },
    {11, 42, HA_DESC,   "Leave" },
    {12,  2, HA_CMD,    "/topic [txt]" },
    {12, 22, HA_DESC,   "View/set topic" },
    {13,  2, HA_CMD,    "/names" },
    {13, 22, HA_DESC,   "List users" },
    {13, 32, HA_CMD,    "/kick nick" },
    {16, 14, HA_TITLE,  "-- Any key: next | EDIT: exit --" },
    { 0,  0, 0, 0 }
};

static const HelpLine help_page2[] = {
    { 0, 14, HA_TITLE,  "===== SPECTALK HELP (2/2) =====" },
    { 2,  2, HA_SECTION,"MESSAGES" },
    { 3,  2, HA_CMD,    "/msg nick txt" },
    { 3, 22, HA_DESC,   "PM (or nick: txt)" },
    { 4,  2, HA_CMD,    "/query nick" },
    { 4, 22, HA_DESC,   "Open query window" },
    { 5,  2, HA_CMD,    "/me action" },
    { 5, 22, HA_DESC,   "Send action" },
    { 7,  2, HA_SECTION,"WINDOWS" },
    { 8,  2, HA_CMD,    "/0../9" },
    { 8, 22, HA_DESC,   "Switch (0=Server)" },
    { 9,  2, HA_CMD,    "/w" },
    { 9, 22, HA_DESC,   "List" },
    { 9, 32, HA_CMD,    "/close" },
    { 9, 42, HA_DESC,   "Close" },
    {11,  2, HA_SECTION,"OTHER" },
    {12,  2, HA_CMD,    "/search #pat|nick" },
    {12, 22, HA_DESC,   "Find chan/user" },
    {13,  2, HA_CMD,    "/away /autoaway N" },
    {13, 22, HA_DESC,   "Away (N=auto min)" },
    {14,  2, HA_CMD,    "/whois /ignore /raw" },
    {16, 14, HA_TITLE,  "-- Any key: pg 1 | EDIT: exit --" },
    { 0,  0, 0, 0 }
};

static void show_help_page(const HelpLine *lines)
{
    clear_zone(MAIN_START, MAIN_LINES, PAPER_BLUE | INK_WHITE);
    while (lines->text) {
        print_str64(MAIN_START + lines->row, lines->col, 
                    lines->text, help_attrs[lines->attr]);
        lines++;
    }
}

static void sys_help(const char *args) __z88dk_fastcall
{
    (void)args; 
    uint8_t key;
    uint8_t current_page = 1;
    
    while (current_page != 0) {
        show_help_page(current_page == 1 ? help_page1 : help_page2);
        
        while (in_inkey() != 0) { HALT(); }
        while ((key = in_inkey()) == 0) { HALT(); }
        while (in_inkey() != 0) { HALT(); }
        
        if (key == 7) {
            current_page = 0;
        } else {
            current_page = (current_page == 1) ? 2 : 1;
        }
    }
    
    clear_zone(MAIN_START, MAIN_LINES, ATTR_MAIN_BG);
    main_line = MAIN_START;
    main_col = 0;
}

static void sys_status(const char *args) __z88dk_fastcall
{
    (void)args;
    uint8_t i;
    uint8_t count = 0;

    current_attr = ATTR_MSG_NICK;
    main_puts("Nick: ");
    current_attr = ATTR_MSG_CHAN;
    if (irc_nick[0]) main_print(irc_nick); else main_print(S_NOTSET);
    
    current_attr = ATTR_MSG_NICK;
    main_puts("Server: ");
    current_attr = ATTR_MSG_CHAN;
    if (irc_server[0]) {
        main_puts(irc_server);
        main_putc(':');
        main_print(irc_port);
    } else {
        main_print(S_NOTSET);
    }
    
    current_attr = ATTR_MSG_NICK;
    main_puts("State: ");
    current_attr = ATTR_MSG_CHAN;
    switch (connection_state) {
        case STATE_DISCONNECTED: main_print(S_DISCONN); break;
        case STATE_WIFI_OK: main_print("WiFi OK"); break;
        case STATE_TCP_CONNECTED: main_print("TCP connected"); break;
        case STATE_IRC_READY: main_print("IRC ready"); break;
        default: main_print("Unknown"); break;
    }
    
    current_attr = ATTR_MSG_NICK;
    main_puts("Channels: ");
    current_attr = ATTR_MSG_CHAN;
    
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active && !channels[i].is_query) {
            if (count > 0) main_puts(", ");
            main_putc('0' + i);
            main_puts(":");
            main_puts(channels[i].name);
            count++;
        }
    }
    
    if (count == 0) main_print("(none)");
    else main_newline();
}

static void sys_about(const char *args) __z88dk_fastcall
{
    (void)args;
    current_attr = ATTR_MSG_SYS;
    main_print(S_APPNAME);
    main_print(S_APPDESC);
    main_print(S_COPYRIGHT);
    main_print("Licensed under GNU GPL v2.0");
}

static void sys_init(const char *args) __z88dk_fastcall
{
    (void)args;
    uint8_t result;
    
    current_attr = ATTR_MSG_PRIV;
    main_puts("Re-initializing ESP8266... ");
    
    uart_drain_and_drop_all(); 
    uart_send_string("AT\r\n");
    
    if (wait_for_response(S_OK, 25)) {
        uart_send_string("AT+CIPCLOSE\r\n");
        wait_for_response(NULL, 10);
    } 
    
    connection_state = STATE_DISCONNECTED;
    reset_all_channels();
    network_name[0] = '\0';
    user_mode[0] = '\0';
    
    result = esp_init();
    
    if (result == 0) {
        ui_err("FAILED (no ESP response)");
    } else if (connection_state == STATE_WIFI_OK) {
        current_attr = ATTR_MSG_PRIV;
        main_print("OK (WiFi connected)");
    } else {
        ui_err("OK (no WiFi)");
    }
    draw_status_bar();
}

static void cmd_theme(const char *args) __z88dk_fastcall
{
    uint8_t t;

    if (!ensure_args(args, "!theme 1|2|3")) return;

    t = (uint8_t)(args[0] - '0');
    if (t < 1 || t > 3) {
        current_attr = ATTR_MSG_SYS;
        main_print("Usage: !theme 1|2|3");
        return;
    }

    if (t == current_theme) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Already using ");
        main_print(theme_get_name(t));
        return;
    }
    
    current_theme = t;
    apply_theme();
    current_attr = ATTR_MSG_SYS;
    main_puts("Theme set to ");
    main_print(theme_get_name(t));
    redraw_input_full();
}

static void cmd_autoaway(const char *args) __z88dk_fastcall
{
    uint8_t mins;
    
    if (!args || !*args) {
        // Mostrar estado
        current_attr = ATTR_MSG_SYS;
        main_puts("Auto-away: ");
        if (autoaway_minutes) {
            char buf[4];
            fast_u8_to_str(buf, autoaway_minutes);
            if (buf[0] == '0') main_putc(buf[1]); else { main_putc(buf[0]); main_putc(buf[1]); }
            main_print(" min");
        } else {
            main_print("off");
        }
        return;
    }
    
    // off/0 = desactivar
    if (args[0] == 'o' || args[0] == 'O' || args[0] == '0') {
        autoaway_minutes = 0;
        autoaway_counter = 0;
        autoaway_active = 0;
        ui_sys("Auto-away disabled");
        return;
    }
    
    // Parsear minutos (1-60)
    mins = (uint8_t)str_to_u16(args);
    if (mins < 1 || mins > 60) {
        ui_err("Range: 1-60 minutes (0=off)");
        return;
    }
    
    autoaway_minutes = mins;
    autoaway_counter = 0;
    current_attr = ATTR_MSG_SYS;
    main_puts("Auto-away set to ");
    {
        char buf[4];
        fast_u8_to_str(buf, mins);
        if (buf[0] == '0') main_putc(buf[1]); else { main_putc(buf[0]); main_putc(buf[1]); }
    }
    main_print(" min");
}

// ============================================================
// COMMAND DISPATCHER
// ============================================================

typedef void (*user_cmd_handler_t)(const char *args) __z88dk_fastcall;

typedef struct {
    const char *name;      
    const char *alias;     
    user_cmd_handler_t fn; 
} UserCmd;

// Wrappers
static void cmd_close_wrapper(const char *a) __z88dk_fastcall {
    (void)a;
    if (current_channel_idx == 0) {
        ui_err("Cannot close server window");
        return;
    }
    if (!channels[current_channel_idx].active) {
        ui_err("No window to close");
        return;
    }
    if (channels[current_channel_idx].is_query) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Closed query with ");
        main_print(channels[current_channel_idx].name);
        remove_channel(current_channel_idx);
        draw_status_bar();
    } else {
        cmd_part("");
    }
}

static void cmd_windows_wrapper(const char *a) __z88dk_fastcall {
    (void)a;
    uint8_t i, n = 0;
    current_attr = ATTR_MSG_SYS;
    main_puts("Open windows:");
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active) {
            main_putc(' ');
            if (i == current_channel_idx) main_puts("*");
            if (i == 9) main_putc('0'); else main_putc('1' + i);
            main_putc(':');
            if (i == 0) main_puts(S_SERVER);
            else {
                if (channels[i].is_query) main_puts("@");
                if (channels[i].has_unread) main_puts("+");
                main_puts(channels[i].name);
            }
            n++;
        }
    }
    if (n == 0) main_puts(" (none)");
    main_newline();
    main_print("/0=Server /1-/9=channels");
}

static const UserCmd USER_COMMANDS[] = {
    { "HELP",    "H",     sys_help },
    { "STATUS",  "S",     sys_status },
    { "INIT",    "I",     sys_init },
    { "THEME",   NULL,    cmd_theme },
    { "ABOUT",   NULL,    sys_about },
    { "SERVER",  "CONNECT", cmd_connect },
    { "NICK",    NULL,      cmd_nick },
    { "PASS",    NULL,      cmd_pass },
    { "JOIN",    "J",       cmd_join },
    { "PART",    "P",       cmd_part },
    { "MSG",     "M",       cmd_msg },
    { "QUERY",   "Q",       cmd_query },
    { "CLOSE",   NULL,      cmd_close_wrapper },
    { "QUIT",    NULL,      cmd_quit },
    { "ME",      NULL,      cmd_me },
    { "AWAY",    NULL,      cmd_away },
    { "AUTOAWAY","AA",      cmd_autoaway },
    { "RAW",     NULL,      cmd_raw },
    { "WHOIS",   "WI",      cmd_whois },
    { "WHO",     NULL,      cmd_who },
    { "LIST",    "LS",      cmd_list },
    { "NAMES",   NULL,      cmd_names },
    { "TOPIC",   NULL,      cmd_topic },
    { "SEARCH",  NULL,      cmd_search },
    { "IGNORE",  NULL,      cmd_ignore },
    { "KICK",    "K",       cmd_kick },
    { "CHANNELS","W",       cmd_windows_wrapper },
};

void parse_user_input(char *line) __z88dk_fastcall
{
    char *args;
    char *cmd_str;
    uint8_t is_sys = 0;
    
    while (*line == ' ') line++;
    if (!*line) return;  
    
    cmd_str = line;
    
    if (cmd_str[0] == '!') {
        is_sys = 1;
        cmd_str++;
    } else if (cmd_str[0] == '/') {
        cmd_str++;
    } else {
        if (connection_state < STATE_TCP_CONNECTED) {
            current_attr = ATTR_ERROR;
            main_print("Not connected. Use /server host");
            return;
        }
        
        char *colon = strchr(line, ':');
        if (colon && colon != line) {
            char *sp = strchr(line, ' ');
            if (!sp || sp > colon) {
                *colon = 0;
                char *msg = colon + 1;
                while (*msg == ' ' || *msg == '\t') msg++;
                if (*msg) {
                    irc_send_privmsg(line, msg);
                    return;
                }
            }
        }
        
        if (!irc_channel[0]) {
            current_attr = ATTR_ERROR;
            main_print("No window. /join #chan or /query nick");
            return;
        }
        irc_send_privmsg(irc_channel, line);
        return;
    }
    
    args = strchr(cmd_str, ' ');
    if (args) {
        *args = '\0';
        args++;
        while (*args == ' ') args++;
    }
    
    if (cmd_str[0] >= '0' && cmd_str[0] <= '9' && cmd_str[1] == 0) {
        uint8_t idx = (uint8_t)(cmd_str[0] - '0');
        
        if (idx < MAX_CHANNELS && channels[idx].active) {
            if (idx == current_channel_idx) {
                current_attr = ATTR_MSG_SYS;
                main_puts("Already in "); 
                main_print((idx == 0) ? S_SERVER : channels[idx].name);
            } else {
                switch_to_channel(idx);
                current_attr = ATTR_MSG_SYS;
                main_puts(S_SWITCHTO);
                main_print((idx == 0) ? S_SERVER : channels[idx].name);
            }
        } else {
            current_attr = ATTR_ERROR;
            main_puts("No window in slot ");
            main_putc(cmd_str[0]); main_newline();
        }
        return;
    }
    
    if (st_stricmp(cmd_str, "Q") == 0) { 
        cmd_quit(args);
        return;
    }
    if (st_stricmp(cmd_str, "WINDOWS") == 0 || st_stricmp(cmd_str, "CHANS") == 0) {
        cmd_windows_wrapper(args);
        return;
    }

    uint8_t i;
    for (i = 0; i < sizeof(USER_COMMANDS)/sizeof(UserCmd); i++) {
        if (is_sys && i >= 5) break; 
        if (!is_sys && i < 5) continue;
        
        const UserCmd *c = &USER_COMMANDS[i];
        
        if (st_stricmp(cmd_str, c->name) == 0 || (c->alias && st_stricmp(cmd_str, c->alias) == 0)) {
            c->fn(args);
            return;
        }
    }
    
    current_attr = ATTR_ERROR;
    main_puts("Unknown command: ");
    main_putc(is_sys ? '!' : '/');
    main_print(cmd_str);
}