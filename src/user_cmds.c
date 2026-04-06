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
static void sntp_init(void);  // forward decl — defined in spectalk.c (SCU order)

// OPT H3: extern para constantes compartidas (definidas en spectalk.c)
extern const char S_ON[];
extern const char S_OFF[];
extern const char S_DEFAULT_PORT[];
extern uint8_t autoconnect;

// Config key strings (dedup: used in both sys_config display and cmd_save)
static const char K_NICK[]     = "nick=";
static const char K_SERVER[]   = "server=";
static const char K_PORT[]     = "port=";
static const char K_PASS[]     = "pass=";
static const char K_NKPASS[]   = "nickpass=";
static const char K_AUTOCONN[] = "autoconnect=";
static const char K_THEME[]    = "theme=";
static const char K_AUTOAWAY[] = "autoaway=";
static const char K_BEEP[]     = "beep=";
static const char K_NCOLOR[]   = "nickcolor=";
static const char K_TRAFFIC[]  = "traffic=";
static const char K_TS[]       = "timestamps=";
static const char K_TOPIC[]    = "TOPIC";
static const char K_CFG_PRI[]  = "/SYS/CONFIG/SPECTALK.CFG";
static const char K_CFG_ALT[]  = "/SYS/SPECTALK.CFG";
static const char K_TZ[]       = "tz=";
static const char K_NOTIF[]    = "notif=";

static void cut_at_space(char *p) __z88dk_fastcall
{
    char *sp = strchr(p, ' ');
    if (sp) *sp = 0;
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

// sys_puts_print: frameless ASM in spectalk_asm.asm


// Help overlay state (state-based, non-blocking — main loop keeps running)
// Non-static: ASM main_puts checks this to suppress output during help
uint8_t overlay_mode;
uint8_t help_page;              // current page (0-based) — non-static for overlay access
uint8_t config_dirty;           // 1 = unsaved config changes exist

// NIVELES DE VALIDACIÓN JERÁRQUICA
#define LVL_TCP  1  // Conectado a Internet (Socket abierto)
#define LVL_IRC  2  // Conectado a Servidor (Registrado con NICK/USER)
#define LVL_CHAN 3  // Conectado a Canal (Ventana válida y activa)

uint8_t check_status(uint8_t level) __z88dk_fastcall
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
    if (current_channel_idx == 0 || !(chan_flags & CH_FLAG_ACTIVE)) {
        set_attr_err();
        main_print(S_NOWIN); // "No window..."
        return 0;
    }

    return 1;
}


#define IS_CHAN_PREFIX(c) ((c) == '#' || (c) == '&')
#define REQUIRE_CHAN() do { if (!IS_CHAN_PREFIX(irc_channel[0])) { ui_err(S_MUST_CHAN); return; } } while(0)

// ensure_args: frameless ASM in spectalk_asm.asm

// Función auxiliar para diagnosticar la respuesta de conexión
static uint8_t wait_for_connection_result(uint16_t max_frames) __z88dk_fastcall
{
    uint16_t frames = 0;
    rx_pos = 0;

    while (frames < max_frames) {
        frame_wait();
        if (in_inkey() == 7) { ui_err(S_CANCELLED); return 0; }

        uart_drain_to_buffer();

        if (try_read_line_nodrain()) {
            // FIX P0-1: Verificar longitud antes de acceder a índices fijos
            if (rx_last_len < 2) { rx_pos = 0; continue; }

            char c0 = rx_line[0];

            // FIX: "CONNECT" exacto (7 chars) o "ALREADY CONNECT", NO "WIFI CONNECTED"
            if (c0 == 'C' && rx_line[1] == 'O' && rx_last_len == 7) goto wcr_ok;
            if (c0 == 'A' && rx_line[1] == 'L') goto wcr_ok;
            if (c0 == 'O' && rx_line[1] == 'K') goto wcr_ok;

            // ERRORES - prefix checks (ya verificamos rx_last_len >= 2 arriba)
            if (c0 == 'D' && rx_line[1] == 'N') { ui_err("DNS failed"); goto wcr_fail; }
            if (c0 == 'C' && rx_line[1] == 'L') { ui_err(S_CONN_REFUSED); goto wcr_fail; }
            if (c0 == 'c' && rx_line[1] == 'o') { ui_err(S_CONN_REFUSED); goto wcr_fail; }
            if (c0 == 'E' && rx_line[1] == 'R') { ui_err("Connection error"); goto wcr_fail; }
            rx_pos = 0;
        }
        frames++;
    }

    ui_err(S_TIMEOUT);
    return 0;
wcr_fail:
    rx_pos = 0;
    return 0;
wcr_ok:
    rx_pos = 0;
    return 1;
}


// FIX-H6: redundant externs removed - already in spectalk.h with correct types

// puts_u8_nolz: frameless ASM in spectalk_asm.asm

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

    if (!args || !*args) {
        if (connection_state >= STATE_TCP_CONNECTED) {
            // No args + connected: show current server
            set_attr_sys();
            main_puts2("Already connected to ", irc_server);
            main_putc(':'); main_print(irc_port);
            return;
        }
        // No args + not connected: reconnect using config-preloaded server if available
        if (irc_server[0]) {
            goto do_connect;
        }
        ui_usage("server host [port]");
        return;
    }

    if (connection_state >= STATE_TCP_CONNECTED) {
        ui_err("Already connected.");
        set_attr_sys();
        main_print("Disconnect? (y/n)");
        { uint8_t tmout = 250;  // audit M01: ~5s timeout
        while (tmout--) {
            uint8_t k = in_inkey();
            frame_wait(); uart_drain_to_buffer();
            while (try_read_line_nodrain()) { rx_pos = 0; }
            if (k == 'n' || k == 'N' || !tmout) { main_print(S_CANCELLED); return; }
            if (k == 'y' || k == 'Y') {
                main_print("Disconnecting...");
                force_disconnect();
                draw_status_bar(); draw_status_bar_real();
                break;
            }
        } }
    }
    
    sep = strchr(args, ' ');
    if (!sep) sep = strchr(args, ':');
    
    if (sep) {
        server_len = (uint8_t)(sep - args);
        port = sep + 1;
        port = (const char *)skip_spaces((char *)port);
        if (!*port) port = S_DEFAULT_PORT;
    } else {
        server_len = st_strlen(args);
        port = S_DEFAULT_PORT;
    }
    
    if (server_len > sizeof(irc_server) - 1) server_len = sizeof(irc_server) - 1;
    memcpy(irc_server, args, server_len);
    irc_server[server_len] = '\0';
    if (strchr(irc_server, '"')) { ui_err("Bad server name"); return; }  // audit L04

    st_copy_n(irc_port, port, sizeof(irc_port));

do_connect:

    if (!irc_nick[0]) {
        ui_sys("Set /nick first");
        return;
    }
    
    if (str_to_u16(irc_port) == 6697) use_ssl = 1;
    
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
    uart_send_string(use_ssl ? "SSL" : S_TCP);
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
            frame_wait(); uart_drain_to_buffer();
            if (in_inkey() == 7) break;  // audit M03: BREAK cancel
            if (try_read_line_nodrain()) {
                // FIX P0-1: Verificar longitud antes de acceder a índices
                if (rx_last_len >= 2) {
                    if (rx_line[0] == '+' && rx_line[1] == 'C') {
                        sntp_process_response(rx_line);
                    }
                    if (rx_line[0] == 'O' && rx_line[1] == 'K') { rx_pos = 0; break; }
                }
                rx_pos = 0;
            }
        }
        rx_pos = 0;
    }

    uart_send_line("AT+CIPMODE=1");
    if (!wait_for_response(S_OK, 100)) { ui_err("CIPMODE FAIL"); goto connect_fail; }
    
    wait_drain(20);
    uart_send_string("AT+CIPSEND\r\n");
    
    if (!wait_for_prompt_char('>', TIMEOUT_PROMPT)) {
        // rx_line has captured data — scan for IRC ban numeric (465/466)
        char *p = (char *)rx_line;
        while (*p) {
            if (p[0] == '4' && p[1] == '6' && (p[2] == '5' || p[2] == '6')) {
                ui_err("Banned"); goto connect_fail;
            }
            p++;
        }
        ui_err("No '>' prompt"); goto connect_fail;
    }
    
    connection_state = STATE_TCP_CONNECTED; closed_reported = 0;
    rx_pos = 0; rx_overflow = 0;
    
    if (irc_nick[0]) {
        char *line; 
        uint8_t loop_done = 0;
        uint16_t silence_frames = 0;
        uint16_t total_frames = 0;

        const char *abort_msg = 0;
        uint8_t abort_disc = 1;
        
        set_attr_priv(); main_puts("Registering... ");
        
        if (irc_pass[0]) irc_send_cmd1("PASS", irc_pass);
        irc_send_cmd1(S_NICK_CMD, irc_nick);
        uart_send_string("USER "); uart_send_string(irc_nick); 
        uart_send_string(" 0 * :"); uart_send_line(irc_nick);
        
        rx_pos = 0;
        
        while (!loop_done) {
            frame_wait(); uart_drain_to_buffer();
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
                    ls = skip_spaces(ls);
                    if (*ls == '*') { ls++; ls = skip_spaces(ls); }
                    if (ls[0] == 'L' && ls[1] == 'S') {
                        uart_send_line(S_CAP_END);
                        rx_pos = 0; continue;
                    }
                }
                
                // PING
                {
                    char *ping_ptr = NULL;
                    if (line[0] == 'P' && line[1] == 'I' && line[2] == 'N' && line[3] == 'G') ping_ptr = line;
                    else if (sp && sp[1] == 'P' && sp[2] == 'I' && sp[3] == 'N' && sp[4] == 'G') ping_ptr = sp + 1;

                    if (ping_ptr) {
                        char *params = ping_ptr + 4;
                        params = skip_spaces(params);
                        uart_send_string(S_PONG); uart_send_line(params);
                        rx_pos = 0; continue;
                    }
                }

                // ERROR : - siempre al inicio de línea
                // FIX P0-1: Verificar longitud antes de acceder a índices
                // FIX-H5: usar longitud restante desde 'line', no rx_last_len global
                {
                uint16_t remaining = rx_last_len - (uint16_t)(line - rx_line);
                if (remaining >= 6 && line[0] == 'E' && line[1] == 'R' && line[5] == 'R') {
                    abort_msg = "Server error";
                    abort_disc = 1;
                    goto join_fail;
                }
                }

                // FIX P0-1: Verificar longitud antes de acceder a índices
                if (rx_last_len >= 3 && rx_line[0] == 'C' && rx_line[1] == 'L' && rx_line[2] == 'O') {  // CLOSED
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
            // Absolute timeout (~60s) prevents infinite hang if server
            // keeps sending data (throttle NOTICEs) without completing registration
            if (++total_frames > 3000) {
                abort_msg = S_TIMEOUT;
                abort_disc = 1;
                goto join_fail;
            }
        }

join_fail:
        if (abort_msg) {
            // OPT L3: inlined cmd_connect_abort
            ui_err(abort_msg);
            if (abort_disc) force_disconnect();
            abort_msg = 0;
        }

        rb_head = rb_tail = 0; rx_pos = 0; rx_overflow = 0;
    } else {
        force_disconnect();
    }
    goto connect_cleanup;

    connect_fail:
        force_disconnect();

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

        notify2("Nick set to ", irc_nick, ATTR_MSG_SYS);
        draw_status_bar();
        config_dirty = 1;
    }
}

static void cmd_pass(const char *args) __z88dk_fastcall
{
    if (!args || !*args) {
        sys_puts_print("Server password: ", irc_pass[0] ? S_SET : S_NOTSET);
        return;
    }
    
    if (st_stricmp(args, "clear") == 0 || st_stricmp(args, "none") == 0) {
        irc_pass[0] = '\0';
        notify("Password cleared", ATTR_MSG_SYS);
        config_dirty = 1;
        return;
    }

    st_copy_n(irc_pass, args, sizeof(irc_pass));
    notify("Password set", ATTR_MSG_SYS);
    config_dirty = 1;
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
    if (!*p) return;

    cut_at_space(p);

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
        if ((uint8_t)idx != current_channel_idx) {
            switch_to_channel((uint8_t)idx);
            notify2("Switched to ", channels[idx].name, ATTR_MSG_JOIN);
        } else {
            notify2("Already in ", channels[idx].name, ATTR_MSG_SYS);
        }
        return;
    }

    if (find_empty_channel_slot() == -1) { ui_err(S_MAXWIN); main_print("Use /close or /part first."); return; }

    irc_send_cmd1("JOIN", lookup);
    st_copy_n(temp_input + 64, lookup, 56);
    nb_init("Joining "); nb(temp_input + 64); NB_END();
    notify(temp_input, ATTR_MSG_JOIN);
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
                reason = skip_spaces(reason);
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
        irc_send_cmd2(S_PART_CMD, cname_ptr, reason);
    }

    notify2("You have left ", cname_ptr, ATTR_MSG_JOIN);

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
    msg = skip_spaces(msg);

    irc_send_privmsg(target, msg);
}

static void cmd_query(const char *args) __z88dk_fastcall
{
    char *p;

    // Nivel 2: Requiere IRC para tener un NICK propio válido
    if (!check_status(LVL_IRC)) return;
    if (!ensure_args(args, "query nick")) return;

    // Zero-copy: usar el buffer de entrada in-situ
    p = (char *)args;
    if (!*p) return;

    cut_at_space(p);

    {
        int8_t idx = add_query(p);

        if (idx >= 0) {
            if ((uint8_t)idx != current_channel_idx) {
                switch_to_channel((uint8_t)idx);
                notify2("Query: ", channels[idx].name, ATTR_MSG_SYS);
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

    main_puts2("Disconnecting from ", irc_server);
    main_print(S_DOTS3);

    // Mantener workaround callee/ternario (ya validado por ti)
    msg = (args && *args) ? args : S_APPSHORT;

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
        st_copy_n(nickserv_pass, args, IRC_PASS_SIZE);
    } else if (nickserv_pass[0]) {
        pass = nickserv_pass;
    } else {
        ui_err("No password. Use /id <pass> or set nickpass= in config");
        return;
    }
    
    send_identify(pass);
    notify2("Identifying with ", nickserv_nick[0] ? (const char *)nickserv_nick : S_NICKSERV, ATTR_MSG_SYS);
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

    sys_puts_print("LIST: ", args);
    start_search_command(PEND_LIST, args);
}

static void cmd_who(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return;
    
    const char *target = (args && *args) ? args : (current_channel_idx ? irc_channel : NULL);
    if (!target || !target[0]) { ui_usage("who #channel or nick"); return; }

    sys_puts_print("WHO: ", target);
    start_search_command(PEND_WHO, target);
}

static void cmd_names(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return; // Nivel 2

    const char *target = (args && *args) ? args : irc_channel;
    if (!target[0] || (target[0] != '#' && target[0] != '&')) { ui_err(S_MUST_CHAN); return; }
    
    counting_new_users = 1;
    show_names_list = 1;
    names_was_manual = 1;
    names_count_acc = 0;
    names_friend_pos = 0;
    st_copy_n(names_target_channel, target, sizeof(names_target_channel));
    names_pending = 1;
    names_timeout_frames = 0;
    start_pagination();

    irc_send_cmd1("NAMES", target);
    set_attr_sys();
    main_print("Listing users...");
}

static void cmd_topic(const char *args) __z88dk_fastcall
{
    if (!check_status(LVL_IRC)) return; // Nivel 2

    // Sin args: consultar topic del canal actual
    if (!args || !*args) {
        REQUIRE_CHAN();
        irc_send_cmd2(K_TOPIC, irc_channel, NULL);
        return;
    }

    // Si args empieza con prefijo de canal: comportamiento original (primer token = canal)
    if (IS_CHAN_PREFIX(args[0])) {
        char *mutable_target = (char *)(void *)args;
        char *new_topic = NULL;
        char *space = strchr(mutable_target, ' ');
        if (space) {
            *space = '\0';
            new_topic = space + 1;
        }
        irc_send_cmd2(K_TOPIC, mutable_target, new_topic);
        if (space) *space = ' ';
        return;
    }

    // Args sin prefijo de canal: usar canal actual como target, args entero como topic
    if (!IS_CHAN_PREFIX(irc_channel[0])) { ui_err(S_MUST_CHAN); return; }
    irc_send_cmd2(K_TOPIC, irc_channel, args);
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
        src = skip_spaces(src);
    } else {
        is_chan = 0;
        src = mutable_args;
    }

    if (!*src) { ui_err(S_EMPTY_PAT); return; }

    end = src;
    len = 0;
    while (*end && *end != ' ' && len < SEARCH_PATTERN_SIZE - 1) { end++; len++; }
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
        p = skip_spaces(p);
        if (!*p) { ui_usage("ignore -nick to unignore"); return; }

        cut_at_space(p);

        if (remove_ignore(p)) {
            notify2("Unignored: ", p, ATTR_MSG_SYS);
            config_dirty = 1;
        } else {
            notify2("Not in ignore list: ", p, ATTR_MSG_SYS);
        }
        return;
    }

    // Caso IGNORE
    cut_at_space(p);

    if (add_ignore(p)) {
        notify2("Now ignoring: ", p, ATTR_MSG_SYS);
        config_dirty = 1;
    } else if (is_ignored(p)) {
        notify2("Already ignoring: ", p, ATTR_MSG_SYS);
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

    if (chan_flags & CH_FLAG_QUERY) {
        ui_err("Not in a channel");
        return;
    }

    nick = (char *)args;

    reason = strchr(nick, ' ');
    if (reason) {
        *reason = '\0';
        reason++;
        reason = skip_spaces(reason);
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

    notify2("Kicking ", nick, ATTR_MSG_SYS);
}


static void sys_status(const char *args) __z88dk_fastcall
{
    (void)args;
    overlay_mode = OVERLAY_STATUS;
    cursor_visible = 0;
    overlay_exec(3, 0);
}


static void sys_about(const char *args) __z88dk_fastcall;

// !config - Muestra la configuración actual
// Common overlay header: clear, big font title with dual brightness, separator
uint8_t overlay_header(const char *title) __z88dk_fastcall
{
    clear_main();
    clear_line(MAIN_START, ATTR_BANNER | 0x40);
    clear_line(MAIN_START + 1, ATTR_BANNER & 0xBF);
    print_big_str(MAIN_START, 1, title, ATTR_BANNER);
    memset((void *)(SCREEN_ROW_ADDR(MAIN_START + 2)), 0xFF, 32);
    memset((void *)(0x5800 + (MAIN_START + 2) * 32), ATTR_MSG_TOPIC, 32);
    return MAIN_START + 3;
}

// overlay_config_render — moved to overlay (SPECTALK.OVL entry 2)
static void sys_config(const char *args) __z88dk_fastcall
{
    (void)args;
    overlay_mode = OVERLAY_CONFIG;
    cursor_visible = 0;
    overlay_exec(1, 1);
}

static void sys_whatsnew(const char *args) __z88dk_fastcall
{
    (void)args;
    overlay_mode = OVERLAY_WHATSNEW;
    cursor_visible = 0;
    overlay_exec(2, 0);
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
        uart_send_line(S_AT_CIPCLOSE);
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
        sntp_init();  // Sync clock after successful reinit
    } else {
        ui_err("no WiFi");
    }
    draw_status_bar();
}

static void cmd_theme(const char *args) __z88dk_fastcall
{
    uint8_t t;
    const char *p;
    /* Require exactly one digit (1..3), allow trailing spaces only */
    if (!args || args[0] < '1' || args[0] > '3') goto theme_usage;
    p = (const char *)skip_spaces((char *)(args + 1));
    if (*p) goto theme_usage;

    t = (uint8_t)(args[0] - '0');

    if (t == current_theme) {
        sys_puts_print("Already using ", theme_get_name(t));
        return;
    }

    current_theme = t;
    apply_theme();

    sys_puts_print("Theme set to ", theme_get_name(t));
    config_dirty = 1;
    return;
theme_usage:
    set_attr_err(); main_print("Usage: !theme 1|2|3");
}

static void cmd_autoaway(const char *args) __z88dk_fastcall
{
    uint8_t mins;
    const char *p;

    if (!args || !*args) {
        // Mostrar estado
        set_attr_sys();
        main_puts(S_AUTOAWAY); main_puts(S_COLON_SP);
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
        sys_puts_print(S_AUTOAWAY, " disabled");
        config_dirty = 1;
        return;
    }

    // Parsear minutos (1-60)
    { uint16_t raw = str_to_u16(p);
    if (raw < 1 || raw > 60) {
        ui_err(S_RANGE_MINUTES);
        return;
    }

    mins = (uint8_t)raw; }
    autoaway_minutes = mins;
    autoaway_counter = 0;
    set_attr_sys(); main_puts(S_AUTOAWAY); main_puts(" set to ");
    puts_u8_nolz(mins);
    main_print(S_MIN);
    config_dirty = 1;
}

// OPT H4: Helper para comandos toggle con argumento directo opcional
static void set_or_toggle_flag(uint8_t *flag, const char *label, const char *args) __z88dk_callee {
    if (args && *args) {
        if (args[0] == '1' || st_stricmp(args, S_ON) == 0) *flag = 1;
        else if (args[0] == '0' || st_stricmp(args, S_OFF) == 0) *flag = 0;
        else { ui_usage("on|off"); return; }
    } else {
        *flag = !*flag;
    }
    sys_puts_print(label, *flag ? S_ON : S_OFF);
    config_dirty = 1;
}

static void cmd_beep(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&beep_enabled, "Beep: ", args);
}

static void cmd_click(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&keyclick_enabled, "Click: ", args);
}

static void cmd_nickcolor(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&nick_color_mode, "Nick color: ", args);
}

static void cmd_traffic(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&show_traffic, "Show traffic: ", args);
}

static void cmd_notif(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&notif_enabled, "Notifications: ", args);
}

static void cmd_timestamps(const char *args) __z88dk_fastcall
{
    if (args && *args) {
        if (args[0] == '0' || st_stricmp(args, S_OFF) == 0) show_timestamps = 0;
        else if (args[0] == '1' || st_stricmp(args, S_ON) == 0) show_timestamps = 1;
        else if (args[0] == '2' || st_stricmp(args, S_SMART) == 0) show_timestamps = 2;
        else { ui_usage("off|on|smart"); return; }
    } else {
        // Cycle: 0 (off) -> 1 (always) -> 2 (on-change) -> 0
        if (++show_timestamps > 2) show_timestamps = 0;
    }
    if (show_timestamps == 2) {
        last_ts_hour = 0xFF;
        last_ts_minute = 0xFF;
    }
    sys_puts_print("Timestamps: ", show_timestamps == 0 ? S_OFF :
               show_timestamps == 1 ? S_ON : S_SMART);
    config_dirty = 1;
}

static void cmd_autoconnect(const char *args) __z88dk_fastcall
{
    set_or_toggle_flag(&autoconnect, "Autoconnect: ", args);
}

static void cmd_tz(const char *args) __z88dk_fastcall
{
    if (args && *args) {
        int8_t tz;
        const char *p = args;
        uint8_t neg = 0;
        if (*p == '-') { neg = 1; p++; }
        else if (*p == '+') p++;
        if (*p < '0' || *p > '9') { ui_err("Range: -12 to +12"); return; }
        { uint16_t raw = str_to_u16(p);
        if (raw > 12) { ui_err("Range: -12 to +12"); return; }
        tz = (int8_t)raw; }
        if (neg) tz = -tz;
        { uint8_t h = time_hour + (uint8_t)(tz - sntp_tz + 24);
        while (h >= 24) h -= 24;
        time_hour = h; }
        sntp_tz = tz;
        status_bar_dirty = 1;
        config_dirty = 1;
    }
    {
        uint8_t abs_tz = (uint8_t)(sntp_tz < 0 ? -sntp_tz : sntp_tz);
        set_attr_sys();
        main_puts(K_TZ);
        main_putc(sntp_tz >= 0 ? '+' : '-');
        puts_u8_nolz(abs_tz);
        main_newline();
    }
}

static void cmd_friend(const char *args) __z88dk_fastcall
{
    uint8_t i;
    char *fn;
    if (!args || !*args) {
        set_attr_sys();
        main_puts("Friends:");
        for (i = 0, fn = friend_nicks[0]; i < MAX_FRIENDS; i++, fn += IRC_NICK_SIZE)
            if (*fn) { main_putc(' '); main_puts(fn); }
        main_newline();
        return;
    }
    // Truncar en primer espacio (igual que cmd_ignore)
    cut_at_space((char *)args);
    // Toggle: remove if exists, add if not (single pass)
    { char *free_fn = NULL;
    for (i = 0, fn = friend_nicks[0]; i < MAX_FRIENDS; i++, fn += IRC_NICK_SIZE) {
        if (*fn) {
            if (st_stricmp(fn, args) == 0) {
                *fn = '\0';
                SYS_PUTS("- "); main_print(args);
                config_dirty = 1;
                return;
            }
        } else if (!free_fn) {
            free_fn = fn;
        }
    }
    if (free_fn) {
        st_copy_n(free_fn, args, IRC_NICK_SIZE);
        SYS_PUTS("+ "); main_print(args);
        config_dirty = 1;
        return;
    }
    }
    ui_err("Max 5 friends");
}

// cmd_save — moved to overlay (SPCTLK4.OVL entry 1)
void cmd_save(const char *args) __z88dk_fastcall
{
    (void)args;
    overlay_exec(3, 1);
}

// OPT-C14: cmd_clear eliminated — command table points directly to clear_main


// ============================================================
// COMMAND DISPATCHER
// ============================================================

// UserCmd ya está definido arriba (forward declaration para sys_help)

// Wrappers
static void cmd_close_wrapper(const char *a) __z88dk_fastcall {
    uint8_t f;
    (void)a;
    if (current_channel_idx == 0) {
        ui_err("Can't close Server");
        return;
    }
    f = chan_flags;
    if (!(f & CH_FLAG_ACTIVE)) {
        ui_err("No window to close");
        return;
    }
    if (f & CH_FLAG_QUERY) {
        notify2("Closed ", irc_channel, ATTR_MSG_SYS);
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
#define SYS_CMDS_COUNT 19

// Command names/aliases pool (help strings moved to /SYS/SPECTALK.HLP)
static const char cmd_pool[] =
    "help\0h\0status\0s\0init\0i\0config\0cfg\0theme\0about\0server\0connec"
    "t\0nick\0pass\0id\0join\0j\0part\0p\0msg\0m\0query\0q\0close\0quit\0me"
    "\0away\0autoaway\0aa\0raw\0whois\0wi\0who\0list\0ls\0names\0topic\0sea"
    "rch\0ignore\0kick\0k\0channels\0w\0beep\0traffic\0timestamps\0ts\0clear\0cls\0"
    "save\0sv\0autoconnect\0ac\0tz\0friend\0nickcolor\0nc\0notif\0nf\0"
    "changelog\0click\0"
;

static const PackedCmd USER_COMMANDS[] = {
    // --- System commands (! prefix) - SYS_CMDS_COUNT entries ---
    {   0,   1, sys_help },
    {   2,   3, sys_status },
    {   4,   5, sys_init },
    {   6,   7, sys_config },
    {   8, 255, cmd_theme },
    {   9, 255, sys_about },
    {  43, 255, cmd_beep },
    {  57,  58, cmd_notif },
    {  59, 255, sys_whatsnew },
    {  27,  28, cmd_autoaway },
    {  44, 255, cmd_traffic },
    {  45,  46, cmd_timestamps },
    {  47,  48, (void (*)(const char *))clear_main },
    {  49,  50, cmd_save },
    {  51,  52, cmd_autoconnect },
    {  53, 255, cmd_tz },
    {  54, 255, cmd_friend },
    {  55,  56, cmd_nickcolor },
    {  60, 255, cmd_click },
    // --- IRC commands (/ prefix) ---
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

const char K_DAT[] = "SPECTALK.DAT";

// help_render_page — moved to overlay (SPECTALK.OVL entry 0)
static void help_render_page(void)
{
    overlay_exec(0, 0);
}

static void sys_help(const char *args) __z88dk_fastcall
{
    (void)args;
    help_page = 0;
    overlay_mode = OVERLAY_HELP;
    cursor_visible = 0;
    help_render_page();
}

// overlay_about_render — moved to overlay (SPECTALK.OVL entry 1)
static void sys_about(const char *args) __z88dk_fastcall
{
    (void)args;
    overlay_mode = OVERLAY_ABOUT;
    cursor_visible = 0;
    overlay_exec(1, 0);
}

void parse_user_input(char *line) __z88dk_fastcall
{
    char *args;
    char *cmd_str;
    uint8_t is_sys = 0;

    line = skip_spaces(line);
    if (!*line) return;

    cmd_str = line;

    if (cmd_str[0] == '!') {
        is_sys = 1;
        cmd_str++;
    } else if (cmd_str[0] == '/') {
        cmd_str++;
    } else {
        // FIX-M10: require IRC_READY, not just TCP_CONNECTED
        if (connection_state < STATE_IRC_READY) {
            ERR_NOTCONN();
            return;
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
        *args++ = '\0';
        args = skip_spaces(args);
    }

    {
        uint8_t c0 = cmd_str[0];
    if (c0 >= '0' && c0 <= '9' && cmd_str[1] == 0) {
        uint8_t idx = c0 - '0';

        if (idx < MAX_CHANNELS && (channels[idx].flags & CH_FLAG_ACTIVE)) {
            if (idx != current_channel_idx) {
                switch_to_channel(idx);
                notify2("Switched to ", channels[idx].name, ATTR_MSG_JOIN);
            } else {
                notify2("Already in ", channels[idx].name, ATTR_MSG_SYS);
            }
        } else {
            set_attr_err();
            main_puts("No window ");
            main_putc(c0);
            main_newline();
        }
        return;
    }}

    {
        uint8_t cmds_left = is_sys ? SYS_CMDS_COUNT : (USER_COMMANDS_COUNT - SYS_CMDS_COUNT);
        const PackedCmd *cmd = is_sys ? USER_COMMANDS : (USER_COMMANDS + SYS_CMDS_COUNT);

        for (; cmds_left != 0; cmds_left--, cmd++) {
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
