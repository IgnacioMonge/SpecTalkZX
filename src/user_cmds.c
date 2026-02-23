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

// OPT H3: extern para constantes compartidas (definidas en spectalk.c)
extern const char S_ON[];
extern const char S_OFF[];
extern const char S_DEFAULT_PORT[];
extern uint8_t autoconnect;
extern uint8_t show_timestamps;
extern uint8_t last_ts_hour;
extern uint8_t last_ts_minute;

// =============================================================================
// INTERNAL HELPERS 
// =============================================================================

// NIVELES DE VALIDACIÓN JERÁRQUICA
#define LVL_TCP  1  // Conectado a Internet (Socket abierto)
#define LVL_IRC  2  // Conectado a Servidor (Registrado con NICK/USER)
#define LVL_CHAN 3  // Conectado a Canal (Ventana válida y activa)

static uint8_t check_status(uint8_t level) __z88dk_fastcall
{
    // LVL_TCP  -> requiere STATE_TCP_CONNECTED
    // LVL_IRC+ -> requiere STATE_IRC_READY (que implícitamente incluye TCP)
    uint8_t min_state = (level == LVL_TCP) ? STATE_TCP_CONNECTED : STATE_IRC_READY;

    if (connection_state < min_state) {
        ERR_NOTCONN();
        return 0;
    }

    // Si solo pedíamos validación de conexión, terminamos aquí
    if (level <= LVL_IRC) return 1;

    // Nivel 3: Contexto de Canal (Necesario para KICK, ME, etc.)
    if (current_channel_idx == 0 || !(channels[current_channel_idx].flags & CH_FLAG_ACTIVE)) {
        set_attr_err();
        main_print(S_NOWIN); // "No window..."
        return 0;
    }

    return 1;
}


static uint8_t ensure_args(const char *args, const char *usage)
{
    if (!args || !*args) {
        ui_usage(usage);
        return 0;
    }
    return 1;
}

// Función auxiliar para diagnosticar la respuesta de conexión
static uint8_t wait_for_connection_result(uint16_t max_frames) __z88dk_fastcall
{
    uint16_t frames = 0;
    rx_pos = 0;

    while (frames < max_frames) {
        HALT();
        if (in_inkey() == 7) { ui_err(S_CANCELLED); return 0; }

        uart_drain_to_buffer();

        if (try_read_line_nodrain()) {
            char c0 = rx_line[0];
            
            // FIX: "CONNECT" exacto o "ALREADY CONNECT", NO "WIFI CONNECTED"
            if (c0 == 'C' && rx_line[1] == 'O' && rx_line[7] == 0) { rx_pos = 0; return 1; }
            if (c0 == 'A' && rx_line[1] == 'L') { rx_pos = 0; return 1; }
            if (c0 == 'O' && rx_line[1] == 'K') { rx_pos = 0; return 1; }

            // ERRORES - prefix checks
            if (c0 == 'D' && rx_line[1] == 'N') { ui_err("DNS failed"); rx_pos = 0; return 0; }
            if (c0 == 'C' && rx_line[1] == 'L') { ui_err(S_CONN_REFUSED); rx_pos = 0; return 0; }
            if (c0 == 'c' && rx_line[1] == 'o') { ui_err(S_CONN_REFUSED); rx_pos = 0; return 0; }
            if (c0 == 'E' && rx_line[1] == 'R') { ui_err("Connection error"); rx_pos = 0; return 0; }
            rx_pos = 0;
        }
        frames++;
    }

    ui_err(S_TIMEOUT);
    return 0;
}


extern char ay_uart_read(void);
extern uint8_t ay_uart_ready(void);

// OPT M2: Helper para imprimir uint8 sin leading zero (usado por !config y !autoaway)
static void puts_u8_nolz(uint8_t v) {
    char buf[4];
    fast_u8_to_str(buf, v);
    if (buf[0] != '0') main_putc(buf[0]);
    main_putc(buf[1]);
}

// =============================================================================
// SEARCH COMMAND HELPER (Unifica lógica de list/who/search)
// Ahorra ~90 bytes al eliminar código duplicado
// =============================================================================
// =============================================================================
// COMMAND HANDLERS
// =============================================================================

// OPT L3: cmd_connect_abort eliminada - inlined en call site

static void cmd_connect(const char *args) __z88dk_fastcall
{ 
    const char *port;
    const char *sep;
    uint8_t server_len;
    uint8_t result;
    uint8_t use_ssl = 0;

    if (connection_state < STATE_WIFI_OK) {
        ui_err("No WiFi. Use !init");
        return;
    }

    if (connection_state >= STATE_TCP_CONNECTED) {
        ui_err("Already connected.");
        set_attr_sys();
        main_print("Disconnect? (y/n)");
        while (1) {
            uint8_t k = in_inkey();
            HALT(); uart_drain_to_buffer();
            // Discard incoming lines to prevent ring buffer overflow
            while (try_read_line_nodrain()) { rx_pos = 0; }
            if (k == 'n' || k == 'N') { main_print(S_CANCELLED); return; }
            if (k == 'y' || k == 'Y') {
                main_print("Disconnecting...");
                force_disconnect();
                irc_server[0] = 0;
                draw_status_bar(); draw_status_bar_real();
                break; 
            }
        }
    }

    if (!args || !*args) {
        // No args: reconnect using config-preloaded server if available
        if (irc_server[0]) {
            goto do_connect;
        }
        ui_usage("server host [port]");
        return;
    }
    
    sep = strchr(args, ' ');
    if (!sep) sep = strchr(args, ':');
    
    if (sep) {
        server_len = (uint8_t)(sep - args);
        port = sep + 1;
        while (*port == ' ') port++;
    } else {
        server_len = st_strlen(args);
        port = S_DEFAULT_PORT;
    }
    
    if (server_len > sizeof(irc_server) - 1) server_len = sizeof(irc_server) - 1;
    memcpy(irc_server, args, server_len);
    irc_server[server_len] = '\0';
    
    st_copy_n(irc_port, port, sizeof(irc_port));

do_connect:

    if (!irc_nick[0]) {
        ui_sys("Set /nick first");
        return;
    }
    
    if (irc_port[0] == '6' && irc_port[1] == '6' && irc_port[2] == '9' && irc_port[3] == '7') use_ssl = 1;
    
    set_attr_priv();
    main_puts2("Connecting to ", irc_server); main_putc(':'); main_puts2(irc_port, "... ");
    
    force_disconnect();
    rb_head = 0; rb_tail = 0; rx_pos = 0; rx_overflow = 0; rx_line[0] = '\0';
    result = 0;
    
    draw_status_bar(); cursor_visible = 0; redraw_input_full(); 
    
    esp_at_cmd(S_AT_CIPMUX0);
    esp_at_cmd(S_AT_CIPSERVER0);
    esp_at_cmd("AT+CIPDINFO=0");
    if (use_ssl) { esp_at_cmd("AT+CIPSSLSIZE=4096"); }

    uart_send_string("AT+CIPSTART=\"");
    uart_send_string(use_ssl ? "SSL" : "TCP");
    uart_send_string("\",\""); uart_send_string(irc_server); uart_send_string("\","); uart_send_line(irc_port);
    
    { uint16_t fl = use_ssl ? TIMEOUT_SSL : TIMEOUT_DNS; result = wait_for_connection_result(fl); }
    
    if (result == 0) { main_newline(); goto connect_fail; }
    
    set_attr_priv(); main_print(S_OK); 
    
    wait_drain(20);
    rb_tail = rb_head; rx_pos = 0;
    
    // Query SNTP time while still in AT command mode (before CIPMODE=1)
    // sntp_init() was called at startup — config persists in ESP8266
    if (sntp_init_sent) {
        uint8_t sntp_wait;
        uart_send_line(S_AT_SNTPTIME);
        // Wait for response and parse it
        for (sntp_wait = 0; sntp_wait < 100; sntp_wait++) {
            HALT(); uart_drain_to_buffer();
            if (try_read_line_nodrain()) {
                if (rx_line[0] == '+' && rx_line[1] == 'C') {
                    sntp_process_response(rx_line);
                }
                rx_pos = 0;
                if (rx_line[0] == 'O' && rx_line[1] == 'K') break;  // OK received
            }
        }
        rx_pos = 0;
    }

    uart_send_line("AT+CIPMODE=1");
    if (!wait_for_response(S_OK, 100)) { ui_err("CIPMODE FAIL"); goto connect_fail; }
    
    wait_drain(20);
    uart_send_string("AT+CIPSEND\r\n");
    
    if (!wait_for_prompt_char('>', TIMEOUT_PROMPT)) { ui_err("No '>' prompt"); goto connect_fail; }
    
    connection_state = STATE_TCP_CONNECTED; closed_reported = 0;
    rx_pos = 0; rx_overflow = 0;
    
    if (irc_nick[0]) {
        char *line; 
        uint8_t loop_done = 0;
        uint16_t silence_frames = 0;

        const char *abort_msg = 0;
        uint8_t abort_disc = 1;
        
        set_attr_priv(); main_puts("Registering... ");
        
        if (irc_pass[0]) irc_send_cmd1("PASS", irc_pass);
        irc_send_cmd1(S_NICK_CMD, irc_nick);
        uart_send_string("USER "); uart_send_string(irc_nick); 
        uart_send_string(" 0 * :"); uart_send_line(irc_nick);
        
        rx_pos = 0;
        
        while (!loop_done) {
            HALT(); uart_drain_to_buffer();
            if (in_inkey() == 7) {
                abort_msg = "Aborted.";
                abort_disc = 1;
                goto join_fail;
            }
            
            if (try_read_line_nodrain()) {
                char *sp;
                uint16_t code;
                
                silence_frames = 0;
                line = rx_line;
                if (line[0] == '@') {
                    line = strchr(line, ' ');
                    if (line) line++; else { rx_pos = 0; continue; }
                }
                
                // Buscar código numérico de forma eficiente
                sp = strchr(line, ' ');
                if (sp && sp[1] >= '0' && sp[1] <= '9') {
                    code = str_to_u16(sp + 1);
                    switch (code) {
                        case 1: // RPL_WELCOME
                            set_attr_priv(); main_print("Connected!");
                            connection_state = STATE_IRC_READY; loop_done = 1; rx_pos = 0; continue;
                        case 433: // Nick in use - try alternate
                            // OPT-P2-B: use shared helper
                            nick_try_alternate();
                            rx_pos = 0;
                            continue;
                        case 432: case 436: abort_msg = "Invalid nick"; abort_disc = 0; goto join_fail;
                        case 464: case 461: abort_msg = "Auth failed"; abort_disc = 1; goto join_fail;
                        case 465: case 466: abort_msg = "Banned"; abort_disc = 1; goto join_fail;
                    }
                }
                
                // CAP LS - format: ":server CAP * LS ..."
                if (line[0] == ':' && sp && sp[1] == 'C' && sp[2] == 'A' && sp[3] == 'P') {
                    // Check for LS after CAP
                    char *ls = sp + 4;
                    while (*ls == ' ') ls++;
                    if (*ls == '*') { ls++; while (*ls == ' ') ls++; }
                    if (ls[0] == 'L' && ls[1] == 'S') {
                        uart_send_line(S_CAP_END);
                        rx_pos = 0; continue;
                    }
                }
                
                // PING
                {
                    char *ping_ptr = NULL;
                    if (line[0] == 'P' && line[1] == 'I') ping_ptr = line; 
                    else if (sp && sp[1] == 'P' && sp[2] == 'I' && sp[3] == 'N' && sp[4] == 'G') ping_ptr = sp + 1;
                    
                    if (ping_ptr) {
                        char *params = ping_ptr;
                        if (*params == ' ') params++;
                        params += 4; while (*params == ' ') params++;
                        uart_send_string(S_PONG); uart_send_line(params);
                        rx_pos = 0; continue;
                    }
                }

                // ERROR : - siempre al inicio de línea
                if (line[0] == 'E' && line[1] == 'R' && line[5] == 'R') {
                    abort_msg = "Server error";
                    abort_disc = 1;
                    goto join_fail;
                }
                
                if (rx_line[0] == 'C' && rx_line[1] == 'L' && rx_line[2] == 'O') {  // CLOSED
                    abort_msg = "Connection lost";
                    abort_disc = 1;
                    goto join_fail;
                }
                
                rx_pos = 0;
            } else {
                silence_frames++;
                if (silence_frames > 1500) { 
                    abort_msg = S_TIMEOUT;
                    abort_disc = 1;
                    goto join_fail;
                }
            }
        }

join_fail:
        if (abort_msg) {
            // OPT L3: inlined cmd_connect_abort
            ui_err(abort_msg);
            if (abort_disc) force_disconnect_wifi();
            abort_msg = 0;
        }

        rb_head = rb_tail = 0; rx_pos = 0; rx_overflow = 0;
    } else {
        // unreachable: nick check moved to before connect
        force_disconnect_wifi();
    }
    goto connect_cleanup;

    connect_fail:
        force_disconnect_wifi();

    connect_cleanup:
        cursor_visible = 1; draw_status_bar(); redraw_input_full();
}

static void cmd_nick(const char *args) __z88dk_fastcall
{
    char *p;
    uint8_t n;

    if (!args || !*args) {
        set_attr_sys();
        main_puts("Current nick: ");
        if (irc_nick[0]) main_print(irc_nick); else main_print(S_NOTSET);
        return;
    }

    /* args viene ya sin espacios iniciales desde parse_user_input */
    p = (char *)args;
    if (!*p) return;

    /* Truncar en primer espacio o al límite del nick (mismo comportamiento que antes) */
    n = 0;
    while (p[n] && p[n] != ' ' && n < (uint8_t)(sizeof(irc_nick) - 1)) n++;
    if (p[n]) p[n] = 0;

    if (connection_state >= STATE_TCP_CONNECTED) {
        // Conectado: Solo enviar comando, el handler actualizará la UI/Variable si es exitoso
        irc_send_cmd1(S_NICK_CMD, p);
    } else {
        // Desconectado: Actualizar inmediatamente
        st_copy_n(irc_nick, p, sizeof(irc_nick));

        set_attr_sys();
        main_puts("Nick set to: ");
        main_print(irc_nick);
        draw_status_bar();
    }
}

static void cmd_pass(const char *args) __z88dk_fastcall
{
    if (!args || !*args) {
        set_attr_sys();
        main_puts("Server password: ");
        main_print(irc_pass[0] ? S_SET : S_NOTSET);
        return;
    }
    
    if ((args[0] == 'c' && args[1] == 'l') || (args[0] == 'n' && args[1] == 'o')) {
        irc_pass[0] = '\0';
        ui_sys("Password cleared");
        return;
    }
    
    st_copy_n(irc_pass, args, sizeof(irc_pass));
    ui_sys("Password set");
}

static void cmd_join(const char *args) __z88dk_fastcall
{
    char lookup_buf[22];     // mismo tamaño que ChannelInfo.name
    const char *lookup;
    int8_t idx;

    if (!ensure_args(args, "join #channel")) return;
    // Nivel 2: Servidor
    if (!check_status(LVL_IRC)) return;

    // Tomar SOLO el primer token y truncar en el primer espacio
    char *p = (char *)args;
    while (*p == ' ') p++;
    if (!*p) return;

    char *sp = strchr(p, ' ');
    if (sp) *sp = 0;

    // Fast-path: si ya trae # o &, usar el puntero directo
    if (*p == '#' || *p == '&') {
        lookup = p;
    } else {
        lookup_buf[0] = '#';
        // Sustitución validada del bucle manual por rutina C compacta
        st_copy_n(lookup_buf + 1, p, sizeof(lookup_buf) - 2);
        lookup_buf[sizeof(lookup_buf) - 1] = '\0';
        lookup = lookup_buf;
    }

    idx = find_channel(lookup);
    if (idx >= 0) {
        if ((uint8_t)idx == current_channel_idx) {
            set_attr_sys();
            main_puts2(S_ALREADY_IN, channels[idx].name);
            main_newline();
        } else {
            switch_to_channel((uint8_t)idx);
            set_attr_sys();
            main_puts2(S_SWITCHTO, channels[idx].name);
            main_newline();
        }
        return;
    }

    if (find_empty_channel_slot() == -1) { ui_err(S_MAXWIN); main_print("Use /close or /part first."); return; }

    irc_send_cmd1("JOIN", lookup);
    set_attr_sys();
    main_puts2("Joining ", lookup);
    main_print(S_DOTS3);
}

static void cmd_part(const char *args) __z88dk_fastcall
{
    // Nivel 2: Servidor (Se puede salir de un canal sin estar en él explícitamente usando args)
    if (!check_status(LVL_IRC)) return;

    char *chan_name = irc_channel;
    char *reason = NULL;
    char *input = (char *)args;
    int8_t idx = current_channel_idx;

    if (input && *input) {
        if (*input == '#' || *input == '&') {
            chan_name = input;
            char *sp = strchr(input, ' ');
            if (sp) {
                *sp = '\0';
                reason = sp + 1;
                while (*reason == ' ') reason++;
            }
            idx = find_channel(chan_name);
        } else {
            reason = input;
        }
    }

    if (idx <= 0 || idx >= MAX_CHANNELS || !(channels[idx].flags & CH_FLAG_ACTIVE)) {
        set_attr_err();
        main_print(idx == 0 ? "Cannot part Status" : "Not in that channel");
        return;
    }

    /* OPT: evitar buffer local (saved_name[32]) y strncpy().
       Imprimimos el nombre ANTES de remove_channel(), mientras sigue siendo válido. */
    char *cname_ptr = channels[idx].name;

    // FIX: si es query/privado, NO enviar PART (evita 403 -> "Cannot join ... No such channel")
    if (!(channels[idx].flags & CH_FLAG_QUERY) && (cname_ptr[0] == '#' || cname_ptr[0] == '&')) {
        // OPT: unificar envío (irc_send_cmd2 ignora p2 si es NULL/vacío)
        irc_send_cmd2("PART", cname_ptr, reason);
    }

    set_attr_sys();
    main_print_time_prefix();
    main_puts(S_YOU_LEFT);
    main_print(cname_ptr);
    main_newline();

    // remove_channel() YA llama a draw_status_bar()
    remove_channel((uint8_t)idx);
}


static void cmd_msg(const char *args) __z88dk_fastcall
{
    // Nivel 2: Requiere estar registrado para enviar PRIVMSG
    if (!check_status(LVL_IRC)) return;

    char *p = (char *)args;
    if (!ensure_args(p, S_USAGE_MSG)) return;

    char *target = p;

    // OPT: strchr() en vez de bucle manual
    char *space = strchr(p, ' ');
    if (!space || !space[1]) { ui_usage(S_USAGE_MSG); return; }

    *space = '\0';
    char *msg = space + 1;
    while (*msg == ' ') msg++;

    irc_send_privmsg(target, msg);
}

static void cmd_query(const char *args) __z88dk_fastcall
{
    char *p;
    char *sp;

    // Nivel 2: Requiere IRC para tener un NICK propio válido
    if (!check_status(LVL_IRC)) return;
    if (!ensure_args(args, "query nick")) return;

    // Zero-copy: usar el buffer de entrada in-situ
    p = (char *)args;
    while (*p == ' ') p++;
    if (!*p) return;

    sp = strchr(p, ' ');
    if (sp) *sp = 0;

    {
        int8_t idx = add_query(p);

        if (idx >= 0) {
            if ((uint8_t)idx != current_channel_idx) {
                switch_to_channel((uint8_t)idx);
                set_attr_sys();
                main_puts("Query opened with ");
                main_print(channels[idx].name);
            }
            status_bar_dirty = 1;
        } else {
            ui_err(S_MAXWIN);
            main_print("Use /close first.");
        }
    }
}

void cmd_quit(const char *args) __z88dk_fastcall
{
    const char *msg;

    // Nivel 1: Solo requiere TCP
    if (!check_status(LVL_TCP)) return;

    set_attr_sys();

    // FIX: Ocultar cursor inmediatamente al iniciar desconexión
    cursor_visible = 0;
    redraw_input_full();

    // FUSIÓN: main_puts("Disconnecting from "); main_puts(irc_server);
    main_puts2("Disconnecting from ", irc_server);
    main_print(S_DOTS3);

    // Mantener workaround callee/ternario (ya validado por ti)
    msg = (args && *args) ? args : "SpecTalk ZX";

    irc_send_cmd2("QUIT", NULL, msg);

    force_disconnect();

    rx_pos = 0;
    rx_overflow = 0;

    // FIX: Restaurar cursor después de la desconexión completa
    cursor_visible = 1;
    redraw_input_full();
    ui_sys(S_DISCONN);
    draw_status_bar();
}



static void cmd_me(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "me action")) return;
    
    // Nivel 3: Requiere estar DENTRO de un canal/query válido
    if (!check_status(LVL_CHAN)) return;
    
    uart_send_string(S_PRIVMSG);
    uart_send_string(irc_channel);
    uart_send_string(S_SP_COLON);
    ay_uart_send(1);            // SOH
    uart_send_string(S_ACTION);
    uart_send_string(args);
    ay_uart_send(1);            // SOH
    uart_send_crlf();
    
    current_attr = ATTR_MSG_SELF;
    main_puts2(S_ASTERISK, irc_nick);
    main_putc(' '); main_print(args);
}

static void cmd_away(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return;

    // Enviar AWAY (sin texto si args vacío/NULL; con :texto si args tiene contenido)
    irc_send_cmd2(S_AWAY_CMD, NULL, args);

    // Guardar mensaje de away para auto-reply
    if (args && *args) {
        st_copy_n(away_message, args, sizeof(away_message));
        irc_is_away = 1;
    } else {
        away_message[0] = '\0';
        irc_is_away = 0;
    }
    
    // En cualquier caso (manual o quitar), desactivamos el flag de auto-away
    autoaway_active = 0;
    
    // Si quitamos el away, reseteamos el contador
    if (!irc_is_away) {
        autoaway_counter = 0;
    }

    draw_status_bar();
}

// /id [password] - Identificarse con NickServ
// Si no se da password, usa el de la configuración (pass=...)
static void cmd_id(const char *args) __z88dk_fastcall
{
    const char *pass;
    
    if (!check_status(LVL_IRC)) return;
    
    // Usar password del argumento, o el de config si no hay
    if (args && *args) {
        pass = args;
        // Save for future /save
        {
            uint8_t i = 0;
            while (args[i] && i < IRC_PASS_SIZE - 1) {
                nickserv_pass[i] = args[i];
                i++;
            }
            nickserv_pass[i] = '\0';
        }
    } else if (nickserv_pass[0]) {
        pass = nickserv_pass;
    } else {
        ui_err("No password. Use /id <pass> or set nickpass= in config");
        return;
    }
    
    // Enviar: PRIVMSG NickServ :IDENTIFY <password>
    uart_send_string(S_PRIVMSG);
    uart_send_string(S_NICKSERV);
    uart_send_string(S_IDENTIFY_CMD);
    uart_send_line(pass);
    
    set_attr_sys();
    main_print("Identifying with NickServ...");
}

static void cmd_raw(const char *args) __z88dk_fastcall
{
    if (!ensure_args(args, "raw IRC_COMMAND")) return;
    if (!check_status(LVL_IRC)) return; // Nivel 2
    uart_send_line(args);
}

static void cmd_whois(const char *args) __z88dk_fastcall
{
    char *p;
    uint8_t n;

    if (!ensure_args(args, "whois nick")) return;
    if (!check_status(LVL_IRC)) return; // Nivel 2

    /* args viene ya sin espacios iniciales desde parse_user_input */
    p = (char *)args;
    if (!*p) return;

    /* Truncar en primer espacio o al límite (mismo comportamiento que antes: nickbuf[32]) */
    n = 0;
    while (p[n] && p[n] != ' ' && n < 31) n++;
    if (p[n]) p[n] = 0;

    irc_send_cmd1("WHOIS", p);
    
    // FIX UX: Feedback optimista
    set_attr_sys();
    
    // FUSIÓN SEGURA
    main_puts2("Querying ", p);
    
    main_print(S_DOTS3);
}


static void cmd_list(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return;
    if (!args || !*args) { ui_usage("list #channel"); main_print("(Full list disabled)"); return; }

    set_attr_sys();
    main_puts("LIST: ");
    main_print(args);

    start_search_command(PEND_LIST, args);
}

static void cmd_who(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return;
    
    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0]) { ui_usage("who #channel or nick"); return; }

    set_attr_sys();
    main_puts("WHO: ");
    main_print(target);

    start_search_command(PEND_WHO, target);
}

static void cmd_names(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return; // Nivel 2

    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0] || target[0] != '#') {
        ui_err(S_MUST_CHAN);
        return;
    }
    
    counting_new_users = 1;
    show_names_list = 1;
    start_pagination();
    
    irc_send_cmd1("NAMES", target);
    set_attr_sys();
    main_print("Listing users...");
}

static void cmd_topic(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return; // Nivel 2

    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0] || target[0] != '#') {
        ui_err(S_MUST_CHAN);
        return;
    }

    // FIX warning 196: Hard cast via void* para silenciar "lost const qualifier"
    // Sabemos que target apunta a buffers mutables (args o irc_channel)
    char *mutable_target = (char *)(void *)target;
    char *new_topic = NULL;

    char *space = strchr(mutable_target, ' ');
    if (space) {
        *space = '\0';        // Cortamos el string temporalmente
        new_topic = space + 1; // Mantener comportamiento: NO trim (igual que antes)
    }

    // OPT: llamada unificada (si new_topic==NULL -> consulta; si no -> set)
    irc_send_cmd2("TOPIC", mutable_target, new_topic);

    if (space) *space = ' '; // Restauramos
}

static void cmd_search(const char *args) __z88dk_fastcall
{
    char *src;
    char *end;
    char *mutable_args;
    uint8_t is_chan;
    uint8_t len;

    if (!ensure_args(args, "search #pattern or nick")) return;
    if (!check_status(LVL_IRC)) return;

    // args proviene de cmd_copy/line_buffer que es RAM escriturable.
    mutable_args = (char *)args;

    if (mutable_args[0] == '#') {
        is_chan = 1;
        src = mutable_args + 1; // Saltamos '#'
        while (*src == ' ') src++;
    } else {
        is_chan = 0;
        src = mutable_args;
    }

    if (!*src) { ui_err(S_EMPTY_PAT); return; }

    end = src;
    len = 0;
    while (*end && *end != ' ' && len < 30) { end++; len++; }
    if (*end) *end = '\0';

    set_attr_sys();
    if (is_chan) {
        main_puts2("Searching: *", src); main_print("*");
        start_search_command(PEND_SEARCH_CHAN, src);
    } else {
        main_puts("Searching users: "); main_print(src);
        start_search_command(PEND_SEARCH_USER, src);
    }
}

static void cmd_ignore(const char *args) __z88dk_fastcall
{
    uint8_t i;
    char *p;
    char *sp;

    if (!args || !*args) {
        set_attr_sys();
        if (ignore_count == 0) {
            main_print("Ignore list is empty");
        } else {
            main_puts("Ignored (");
            main_putc('0' + ignore_count);
            main_puts("): ");
            for (i = 0; i < ignore_count; i++) {
                if (i > 0) main_puts(S_COMMA_SP);
                main_puts(ignore_list[i]);
            }
            main_newline();
        }
        return;
    }

    p = (char *)args;

    // Caso UNIGNORE
    if (p[0] == '-') {
        p++;
        while (*p == ' ') p++;
        if (!*p) { ui_usage("ignore -nick to unignore"); return; }

        sp = strchr(p, ' ');
        if (sp) *sp = 0;

        if (remove_ignore(p)) {
            set_attr_sys(); main_puts("Unignored: "); main_print(p);
        } else {
            set_attr_err(); main_puts("Not in ignore list: "); main_print(p);
        }
        return;
    }

    // Caso IGNORE
    sp = strchr(p, ' ');
    if (sp) *sp = 0;

    if (add_ignore(p)) {
        set_attr_sys(); main_puts("Now ignoring: "); main_print(p);
    } else if (is_ignored(p)) {
        set_attr_err(); main_puts("Already ignoring: "); main_print(p);
    } else {
        ui_err("Ignore list full");
    }
}

static void cmd_kick(const char *args) __z88dk_fastcall
{
    char *nick;
    char *reason;

    if (!ensure_args(args, "kick nick [reason]")) return;
    if (!check_status(LVL_CHAN)) return;

    if (channels[current_channel_idx].flags & CH_FLAG_QUERY) {
        ui_err("Not in a channel");
        return;
    }

    nick = (char *)args;

    reason = strchr(nick, ' ');
    if (reason) {
        *reason = '\0';
        reason++;
        while (*reason == ' ') reason++;
        if (!*reason) reason = NULL;   // evita tratar "" como razón
    }

    uart_send_string("KICK ");
    uart_send_string(irc_channel);
    ay_uart_send(' ');
    uart_send_string(nick);
    if (reason) {
        uart_send_string(S_SP_COLON);
        uart_send_string(reason);
    }
    uart_send_crlf();

    set_attr_sys();

    main_puts2("Kicking ", nick);
    if (reason && *reason) {
        main_puts2(S_SP_PAREN, reason);
        main_print(")");
    } else {
        main_newline();
    }
}


// Helper: imprimir etiqueta y valor
static void status_line(const char *label, const char *value)
{
    set_attr_nick(); main_puts(label);
    set_attr_chan(); main_print(value ? value : S_NOTSET);
}

static void sys_status(const char *args) __z88dk_fastcall
{
    (void)args;
    uint8_t i, count = 0;
    static const char *states[] = {S_DISCONN, "WiFi OK", "TCP", "IRC ready"};
    ChannelInfo *ch = channels;

    status_line("Nick: ", irc_nick[0] ? irc_nick : NULL);

    set_attr_nick(); 
    
    // FUSIÓN SEGURA
    main_puts2(S_SERVER, S_COLON_SP);
    
    set_attr_chan();
    if (irc_server[0]) { main_puts(irc_server); main_putc(':'); main_print(irc_port); }
    else main_print(S_NOTSET);

    set_attr_nick(); main_puts("State: ");
    set_attr_chan();
    main_print(connection_state < 4 ? states[connection_state] : "?");

    set_attr_nick(); main_puts("Chans: ");
    set_attr_chan();

    for (i = 0; i < MAX_CHANNELS; i++, ch++) {
        if ((ch->flags & CH_FLAG_ACTIVE) && !(ch->flags & CH_FLAG_QUERY)) {
            if (count++) main_puts(S_COMMA_SP);
            main_putc('0' + i); main_putc(':');
            main_puts(ch->name);
        }
    }

    if (count == 0) main_print("(none)");
    else main_newline();
}


static void sys_about(const char *args) __z88dk_fastcall
{
    (void)args;
    set_attr_sys();
    main_print(S_APPNAME);
    main_print(S_APPDESC);
    main_print(S_COPYRIGHT);
    main_print(S_LICENSE);
}

// !config - Muestra la configuración actual
static void sys_config(const char *args) __z88dk_fastcall
{
    char buf[4];
    (void)args;
    
    set_attr_priv();
    main_puts("nick="); main_print(irc_nick[0] ? (const char*)irc_nick : S_NOTSET);
    main_puts("server="); main_print(irc_server[0] ? (const char*)irc_server : S_NOTSET);
    main_puts("port="); main_print(irc_port);
    main_puts("pass="); main_print(irc_pass[0] ? S_SET : S_NOTSET);
    main_puts("nickpass="); main_print(nickserv_pass[0] ? S_SET : S_NOTSET);
    main_puts("autoconnect="); main_print(autoconnect ? S_ON : S_OFF);
    main_puts("theme="); fast_u8_to_str(buf, current_theme); main_putc(buf[1]); main_newline();
    main_puts("autoaway="); 
    if (autoaway_minutes) { puts_u8_nolz(autoaway_minutes); main_print(S_MIN); } 
    else main_print(S_OFF);
    main_puts("beep="); main_print(beep_enabled ? S_ON : S_OFF);
    main_puts("quits="); main_print(show_quits ? S_ON : S_OFF);
    main_puts("timestamps="); main_print(show_timestamps == 0 ? S_OFF :
                                        show_timestamps == 1 ? S_ON : S_SMART);

    main_puts("friends=");
    {
        uint8_t i, count = 0;
        for (i = 0; i < 3; i++) {
            if (friend_nicks[i][0]) {
                if (count++) main_puts(S_COMMA_SP);
                // NOTE: main_print() adds newline; for inline list we must use main_puts().
                main_puts(friend_nicks[i]);
            }
        }
        if (count == 0) main_puts(S_NOTSET);
        main_newline();
    }
    main_puts("tz=");
    if (sntp_tz < 0) { main_putc('-'); fast_u8_to_str(buf, -sntp_tz); } 
    else { main_putc('+'); fast_u8_to_str(buf, sntp_tz); }
    if (buf[0] != '0') main_putc(buf[0]); 
    main_putc(buf[1]); 
    main_newline();
}

static void sys_init(const char *args) __z88dk_fastcall
{
    (void)args;
    uint8_t result;
    
    set_attr_priv();
    main_puts("Re-initializing ESP... ");
    
    flush_all_rx_buffers(); 
    uart_send_line("AT");  // OPT M7
    
    if (wait_for_response(S_OK, 25)) {
        uart_send_string(S_AT_CIPCLOSE);
        uart_send_crlf();
        wait_for_response(NULL, 10);
    } 
    
    connection_state = STATE_DISCONNECTED;
    reset_all_channels();
    network_name[0] = '\0';
    user_mode[0] = '\0';
    
    result = esp_init();
    
    if (result == 0) {
        ui_err("FAILED: no ESP response");
    } else if (connection_state == STATE_WIFI_OK) {
        set_attr_priv();
        main_print("WiFi connected");
    } else {
        ui_err("no WiFi");
    }
    draw_status_bar();
}

static void cmd_theme(const char *args) __z88dk_fastcall
{
    uint8_t t;
    const char *p;
    static const char usage_theme[] = "!theme 1|2|3";

    if (!ensure_args(args, usage_theme)) return;

    /* Require exactly one digit (1..3), allow trailing spaces only */
    p = args + 1; while (*p == ' ') p++;
    if (args[0] < '1' || args[0] > '3' || *p) {
        ui_usage(usage_theme);
        return;
    }

    t = (uint8_t)(args[0] - '0');

    if (t == current_theme) {
        set_attr_sys();
        main_puts("Already using ");
        main_print(theme_get_name(t));
        return;
    }

    current_theme = t;
    apply_theme();

    set_attr_sys();
    main_puts("Theme set to ");
    main_print(theme_get_name(t));
}

static void cmd_autoaway(const char *args) __z88dk_fastcall
{
    uint8_t mins;
    const char *p;

    if (!args || !*args) {
        // Mostrar estado
        set_attr_sys();
        main_puts("Auto-away: ");
        if (autoaway_minutes) {
            puts_u8_nolz(autoaway_minutes);
            main_print(S_MIN);
        } else {
            main_print(S_OFF);
        }
        return;
    }

    p = args;

    // off / 0 = disable
    if (*p == '0' || st_stricmp(p, S_OFF) == 0) {
        autoaway_minutes = 0;
        autoaway_counter = 0;
        autoaway_active = 0;
        ui_sys("Auto-away disabled");
        return;
    }

    // Parsear minutos (1-60)
    mins = (uint8_t)str_to_u16(p);
    if (mins < 1 || mins > 60) {
        ui_err(S_RANGE_MINUTES);
        return;
    }

    autoaway_minutes = mins;
    autoaway_counter = 0;
    set_attr_sys();
    main_puts("Auto-away set to ");
    puts_u8_nolz(mins);
    main_print(S_MIN);
}

// OPT H4: Helper para comandos toggle
static void toggle_flag(uint8_t *flag, const char *label) {
    *flag = !*flag;
    set_attr_sys();
    main_puts(label);
    main_print(*flag ? S_ON : S_OFF);
}

static void cmd_beep(const char *args) __z88dk_fastcall
{
    (void)args;
    toggle_flag(&beep_enabled, "Beep: ");
}

static void cmd_quits(const char *args) __z88dk_fastcall
{
    (void)args;
    toggle_flag(&show_quits, "Show quits: ");
}

static void cmd_timestamps(const char *args) __z88dk_fastcall
{
    (void)args;
    // Cycle: 0 (off) → 1 (always) → 2 (on-change) → 0
    if (++show_timestamps > 2) show_timestamps = 0;
    if (show_timestamps == 2) {
        last_ts_hour = 0xFF;
        last_ts_minute = 0xFF;
    }
    set_attr_sys();
    main_puts("Timestamps: ");
    main_print(show_timestamps == 0 ? "OFF" :
               show_timestamps == 1 ? "ON" : S_SMART);
}

static void cmd_autoconnect(const char *args) __z88dk_fastcall
{
    (void)args;
    autoconnect ^= 1;
    set_attr_sys();
    main_puts("Autoconnect: ");
    main_print(autoconnect ? "ON" : "OFF");
}

static void cmd_tz(const char *args) __z88dk_fastcall
{
    if (!args || !*args) {
        char buf[3];
        set_attr_sys();
        main_puts("TZ: UTC");
        main_putc(sntp_tz >= 0 ? '+' : '-');
        fast_u8_to_str(buf, (uint8_t)(sntp_tz < 0 ? -sntp_tz : sntp_tz));
        main_putc(buf[1]);
        main_newline();
        return;
    }
    {
        int8_t tz;
        const char *p = args;
        uint8_t neg = 0;
        if (*p == '-') { neg = 1; p++; }
        else if (*p == '+') p++;
        tz = (int8_t)str_to_u16(p);
        if (neg) tz = -tz;
        if (tz < -12 || tz > 12) { ui_err("Range: -12 to +12"); return; }
        sntp_tz = tz;
        cmd_tz(NULL);
    }
}

static void cmd_friend(const char *args) __z88dk_fastcall
{
    uint8_t i;
    if (!args || !*args) {
        set_attr_sys();
        main_puts("Friends:");
        for (i = 0; i < 3; i++)
            if (friend_nicks[i][0]) { main_putc(' '); main_puts(friend_nicks[i]); }
        main_newline();
        return;
    }
    // Toggle: remove if exists, add if not
    for (i = 0; i < 3; i++) {
        if (friend_nicks[i][0] && st_stricmp(friend_nicks[i], args) == 0) {
            friend_nicks[i][0] = '\0';
            SYS_PUTS("- "); main_print(args);
            return;
        }
    }
    for (i = 0; i < 3; i++) {
        if (!friend_nicks[i][0]) {
            st_copy_n(friend_nicks[i], args, IRC_NICK_SIZE);
            SYS_PUTS("+ "); main_print(args);
            return;
        }
    }
    ui_err("Max 3 friends");
}

// Append string to buffer, return new position
static char *cfg_put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

// Write key=value\r\n to buffer. Returns new position.
// If val is NULL, skips. If (uint16_t)val <= 9, writes single digit.
static char *cfg_kv(char *p, const char *key, const char *val) {
    p = cfg_put(p, key);
    if ((uint16_t)val <= 9) {
        *p++ = '0' + (uint8_t)(uint16_t)val;
    } else {
        p = cfg_put(p, val);
    }
    *p++ = '\r'; *p++ = '\n';
    return p;
}

static void cmd_save(const char *args) __z88dk_fastcall
{
    char *p = (char *)ring_buffer;
    char tmp[4];

    (void)args;

    p = cfg_put(p, "; SpecTalkZX config\r\n");

    if (irc_server[0])    p = cfg_kv(p, "server=", irc_server);
    if (irc_port[0])      p = cfg_kv(p, "port=", irc_port);
    if (irc_nick[0])      p = cfg_kv(p, "nick=", irc_nick);
    if (irc_pass[0])      p = cfg_kv(p, "pass=", irc_pass);
    if (nickserv_pass[0]) p = cfg_kv(p, "nickpass=", nickserv_pass);

    // Numeric settings (cast to pointer trick: 1-9 = single digit)
    p = cfg_kv(p, "theme=", (const char *)(uint16_t)current_theme);
    p = cfg_kv(p, "beep=", (const char *)(uint16_t)beep_enabled);
    p = cfg_kv(p, "quits=", (const char *)(uint16_t)show_quits);
    p = cfg_kv(p, "timestamps=", (const char *)(uint16_t)show_timestamps);
    p = cfg_kv(p, "autoconnect=", (const char *)(uint16_t)autoconnect);

    // Autoaway (only if set)
    if (autoaway_minutes) {
        fast_u8_to_str(tmp, autoaway_minutes); tmp[2] = '\0';
        p = cfg_kv(p, "autoaway=", tmp);
    }

    // Timezone (signed)
    p = cfg_put(p, "tz=");
    if (sntp_tz < 0) {
        *p++ = '-';
        fast_u8_to_str(tmp, (uint8_t)(-sntp_tz));
    } else {
        fast_u8_to_str(tmp, (uint8_t)sntp_tz);
    }
    tmp[2] = '\0';
    p = cfg_put(p, tmp); *p++ = '\r'; *p++ = '\n';

    // Friends
    {
        uint8_t i;
        for (i = 0; i < 3; i++) {
            if (friend_nicks[i][0]) {
                p = cfg_put(p, "friend");
                *p++ = '1' + i; *p++ = '=';
                p = cfg_put(p, friend_nicks[i]);
                *p++ = '\r'; *p++ = '\n';
            }
        }
    }

    esx_fcreate("/SYS/CONFIG/SPECTALK.CFG");
    if (!esx_handle) esx_fcreate("/SYS/SPECTALK.CFG");
    if (!esx_handle) { ui_err("Cannot write config"); return; }

    esx_buf = (uint16_t)(char *)ring_buffer;
    esx_count = (uint16_t)(p - (char *)ring_buffer);
    esx_fwrite();
    esx_fclose();

    SYS_PUTS("Saved (");
    {
        char nbuf[6];
        u16_to_dec(nbuf, esx_count);
        main_puts(nbuf);
    }
    main_print(" bytes)");
}

static void cmd_clear(const char *args) __z88dk_fastcall
{
    (void)args;
    clear_main();
}


// ============================================================
// COMMAND DISPATCHER
// ============================================================

// UserCmd ya está definido arriba (forward declaration para sys_help)

// Wrappers
static void cmd_close_wrapper(const char *a) __z88dk_fastcall {
    (void)a;
    if (current_channel_idx == 0) {
        ui_err("Can't close Server");
        return;
    }
    if (!(channels[current_channel_idx].flags & CH_FLAG_ACTIVE)) {
        ui_err("No window to close");
        return;
    }
    if (channels[current_channel_idx].flags & CH_FLAG_QUERY) {
        set_attr_sys();
        main_puts("Closed query with ");
        main_print(channels[current_channel_idx].name);
        // remove_channel() YA llama a draw_status_bar()
        remove_channel(current_channel_idx);
    } else {
        cmd_part("");
    }
}


static void cmd_windows_wrapper(const char *a) __z88dk_fastcall
{
    (void)a;
    uint8_t i, n = 0;
    uint8_t f;
    ChannelInfo *ch = channels;

    set_attr_sys();
    main_puts("Open windows:");

    for (i = 0; i < MAX_CHANNELS; i++, ch++) {
        f = ch->flags;

        if (f & CH_FLAG_ACTIVE) {
            main_putc(' ');
            if (i == current_channel_idx) main_putc('*');

            main_putc('0' + i);
            main_putc(':');

            if (i == 0) {
                main_puts(S_SERVER);
            } else {
                if (f & CH_FLAG_MENTION) {
                    set_attr_priv();
                    main_putc('!');
                }
                if (f & CH_FLAG_QUERY)   main_putc('@');
                if (f & CH_FLAG_UNREAD)  main_putc('+');
                
                main_puts(ch->name);
                
                if (f & CH_FLAG_MENTION) set_attr_sys();
            }
            n++;
        }
    }

    if (n == 0) main_puts(" (none)");
    main_newline();
}


// =============================================================================
// =============================================================================
// COMMAND TABLE - SINGLE POOL (names+aliases+help) with uint8 indices
// =============================================================================

typedef void (*user_cmd_handler_t)(const char *args) __z88dk_fastcall;

typedef struct {
    uint8_t name_idx;   // index (0..N-1) of NUL-terminated string in cmd_pool
    uint8_t alias_idx;  // CMD_IDX_NONE if no alias
    user_cmd_handler_t fn;
} PackedCmd;

// Forward declaration needed because sys_help is referenced in USER_COMMANDS initializer.
static void sys_help(const char *args) __z88dk_fastcall;

#define CMD_IDX_NONE  ((uint8_t)0xFF)
#define SYS_CMDS_COUNT 6
#define CMD_HELP_BASE  ((uint8_t)55)

// One pool: command names/aliases first, then help strings in command order.
static const char cmd_pool[] =
    "help\0h\0status\0s\0init\0i\0config\0cfg\0theme\0about\0server\0connec"
    "t\0nick\0pass\0id\0join\0j\0part\0p\0msg\0m\0query\0q\0close\0quit\0me"
    "\0away\0autoaway\0aa\0raw\0whois\0wi\0who\0list\0ls\0names\0topic\0sea"
    "rch\0ignore\0kick\0k\0channels\0w\0beep\0quits\0timestamps\0ts\0clear\0cls\0"
    "save\0sv\0autoconnect\0ac\0tz\0friend\0"
    "Help\0Status info\0Reset WiFi\0Show config\0Theme 1|2|3\0About\0"
    "Connect host\0Set nick\0Set pass\0Identify\0Join #chan\0Leave chan\0Se"
    "nd msg\0Open query\0Close win\0Quit IRC\0Action /me\0Set away\0Auto-aw"
    "ay min\0Raw IRC cmd\0Whois nick\0Who #chan\0List chans\0Names #chan\0T"
    "opic\0Search\0Ignore nick\0Kick nick\0Windows\0Toggle beep\0Toggle qui"
    "ts\0Toggle timestamps\0Clear screen\0Save config\0Toggle autoconn\0"
    "Set timezone\0Add/rm friend\0"
;

static const PackedCmd USER_COMMANDS[] = {
    {   0,   1, sys_help },
    {   2,   3, sys_status },
    {   4,   5, sys_init },
    {   6,   7, sys_config },
    {   8, 255, cmd_theme },
    {   9, 255, sys_about },
    {  10,  11, cmd_connect },
    {  12, 255, cmd_nick },
    {  13, 255, cmd_pass },
    {  14, 255, cmd_id },
    {  15,  16, cmd_join },
    {  17,  18, cmd_part },
    {  19,  20, cmd_msg },
    {  21,  22, cmd_query },
    {  23, 255, cmd_close_wrapper },
    {  24, 255, cmd_quit },
    {  25, 255, cmd_me },
    {  26, 255, cmd_away },
    {  27,  28, cmd_autoaway },
    {  29, 255, cmd_raw },
    {  30,  31, cmd_whois },
    {  32, 255, cmd_who },
    {  33,  34, cmd_list },
    {  35, 255, cmd_names },
    {  36, 255, cmd_topic },
    {  37, 255, cmd_search },
    {  38, 255, cmd_ignore },
    {  39,  40, cmd_kick },
    {  41,  42, cmd_windows_wrapper },
    {  43, 255, cmd_beep },
    {  44, 255, cmd_quits },
    {  45,  46, cmd_timestamps },
    {  47,  48, cmd_clear },
    {  49,  50, cmd_save },
    {  51,  52, cmd_autoconnect },
    {  53, 255, cmd_tz },
    {  54, 255, cmd_friend },
};

#define USER_COMMANDS_COUNT ((uint8_t)(sizeof(USER_COMMANDS) / sizeof(USER_COMMANDS[0])))

static const char *pool_nth(uint8_t idx) __z88dk_fastcall
{
    const char *p = cmd_pool;

    while (idx--) {
        while (*p++) {}
    }
    return p;
}

static void sys_help(const char *args) __z88dk_fastcall
{
    (void)args;
    uint8_t page = 0;
    uint8_t total_pages = (USER_COMMANDS_COUNT + 11u) / 12u;
    uint8_t key, start, end, row, i;
    const PackedCmd *cmd;

    start = 0;

    while (1) {
        end = start + 12u;
        if (end > USER_COMMANDS_COUNT) end = USER_COMMANDS_COUNT;

        clear_main();
        print_str64(MAIN_START, 10, "=== HELP ===", ATTR_MSG_TOPIC);

        row = 2;
        for (i = start; i < end; i++) {
            cmd = &USER_COMMANDS[i];

            print_char64(MAIN_START + row, 2, (i < SYS_CMDS_COUNT) ? '!' : '/', ATTR_MSG_NICK);
            print_str64(MAIN_START + row, 3, pool_nth(cmd->name_idx), ATTR_MSG_NICK);

            // Help strings are stored after command strings, in USER_COMMANDS order.
            print_str64(MAIN_START + row, 18, pool_nth((uint8_t)(CMD_HELP_BASE + i)), ATTR_MSG_CHAN);
            row++;
        }

        print_str64(MAIN_START + 15, 10, "Page", ATTR_MSG_TIME);
        {
            char pg[5] = {' ', (char)('0' + page + 1), '/', (char)('0' + total_pages), 0};
            print_str64(MAIN_START + 15, 14, pg, ATTR_MSG_TIME);
        }
        print_str64(MAIN_START + 15, 19, "ANY:next SPACE:exit", ATTR_MSG_TIME);

        while (in_inkey()) { HALT(); uart_drain_to_buffer(); }
        while (!(key = in_inkey())) { HALT(); uart_drain_to_buffer(); }
        while (in_inkey()) { HALT(); uart_drain_to_buffer(); }

        if (key == ' ') break;

        page++;
        start += 12u;
        if (page >= total_pages) { page = 0; start = 0; }
    }

    clear_main();
}

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
            set_attr_err();
            ERR_NOTCONN();
            return;
        }

        // Check for "nick: message" format first (works from any window)
        {
            char *colon = strchr(line, ':');
            if (colon && colon != line) {
                char *sp = strchr(line, ' ');
                if (!sp || sp > colon) {
                    *colon = 0;
                    {
                        char *msg = colon + 1;
                        while (*msg == ' ' || *msg == '\t') msg++;
                        if (*msg) {
                            // Open query window for the nick
                            int8_t qidx = find_query(line);
                            if (qidx < 0) {
                                qidx = add_query(line);
                                if (qidx >= 0) {
                                    status_bar_dirty = 1;
                                    switch_to_channel((uint8_t)qidx);
                                }
                            }
                            irc_send_privmsg(line, msg);
                            return;
                        }
                    }
                }
            }
        }

        // For regular messages, need to be in a channel/query window
        if (current_channel_idx == 0 || !irc_channel[0]) {
            set_attr_err();
            main_print(S_NOWIN);
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

        if (idx < MAX_CHANNELS && (channels[idx].flags & CH_FLAG_ACTIVE)) {
            // GEMINI OPT 3: Pre-calcular nombre para evitar ternarios (SDCC los odia)
            const char *chn_name = (idx == 0) ? S_SERVER : channels[idx].name;

            if (idx == current_channel_idx) {
                set_attr_sys();
                main_puts(S_ALREADY_IN);
                main_print(chn_name);
            } else {
                switch_to_channel(idx);
                set_attr_sys();
                main_puts(S_SWITCHTO);
                main_print(chn_name);
            }
        } else {
            set_attr_err();
            main_puts("No window ");
            main_putc(cmd_str[0]);
            main_newline();
        }
        return;
    }

    {
        const PackedCmd *cmd = is_sys ? USER_COMMANDS : (USER_COMMANDS + SYS_CMDS_COUNT);
        const PackedCmd *end = is_sys ? (USER_COMMANDS + SYS_CMDS_COUNT) : (USER_COMMANDS + USER_COMMANDS_COUNT);

        for (; cmd < end; cmd++) {
            if (st_stricmp(cmd_str, pool_nth(cmd->name_idx)) == 0 ||
                (cmd->alias_idx != CMD_IDX_NONE && st_stricmp(cmd_str, pool_nth(cmd->alias_idx)) == 0)) {
                cmd->fn(args);
                return;
            }
        }
    }

    set_attr_err();
    main_puts("Unknown command: ");
    main_putc(is_sys ? '!' : '/');
    main_print(cmd_str);
}
