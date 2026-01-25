/*
 * user_cmds.c - User command parsing and handling
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

// Función auxiliar para diagnosticar la respuesta de conexión
static uint8_t wait_for_connection_result(uint16_t max_frames)
{
    uint16_t frames = 0;
    rx_pos = 0;
    
    while (frames < max_frames) {
        HALT();
        
        // Permitir cancelar con EDIT (Caps Shift + 1)
        if (in_inkey() == 7) { 
            ui_err("Cancelled by user."); 
            return 0; 
        }
        
        uart_drain_to_buffer();
        
        if (try_read_line_nodrain()) {
            // 1. ÉXITO: Buscamos CONNECT, ALREADY CONNECT u OK
            if (strstr(rx_line, "CONNECT") || strstr(rx_line, "ALREADY CONNECT") || strstr(rx_line, "OK")) {
                rx_pos = 0;
                return 1;
            }
            
            // 2. ERRORES ESPECÍFICOS
            if (strstr(rx_line, "DNS FAIL")) {
                ui_err("Hostname not found");
                rx_pos = 0; return 0;
            }
            
            if (strstr(rx_line, "CLOSED") || strstr(rx_line, "conn fail")) {
                ui_err("Connection refused");
                rx_pos = 0; return 0;
            }
            
            // Ignoramos el eco del comando para no dar falso positivo en ERROR
            if (strncmp(rx_line, "AT+", 3) != 0 && strstr(rx_line, "ERROR")) {
                ui_err("ESP command failed");
                rx_pos = 0; return 0;
            }
            
            rx_pos = 0;
        }
        frames++;
    }
    
    // 3. TIMEOUT
    ui_err("Error: Connection timeout");
    return 0;
}

static void cmd_connect(const char *args)
{
    char *server;
    const char *port;
    char *separator;
    uint8_t result;
    uint8_t use_ssl = 0;
    
    uint8_t already_disconnected = 0; 
    
    if (connection_state < STATE_WIFI_OK) {
        ui_err("Error: No WiFi connection. Use /init first.");
        return;
    }

    if (!args || !*args) {
        ui_usage("server host [port]");
        return;
    }
    
    // Parsear argumentos PRIMERO para poder comparar
    strncpy(tx_buffer, args, TX_BUFFER_SIZE - 1);
    tx_buffer[TX_BUFFER_SIZE - 1] = 0;
    server = tx_buffer;
    
    separator = strchr(server, ' ');
    if (!separator) separator = strchr(server, ':'); 
    
    if (separator) {
        *separator = '\0';
        port = separator + 1;
    } else {
        port = "6667";
    }
    while (*port == ' ') port++; 

    // Verificar si ya está conectado al MISMO servidor
    if (connection_state >= STATE_TCP_CONNECTED) {
        if (st_stricmp(server, irc_server) == 0 && strcmp(port, irc_port) == 0) {
            current_attr = ATTR_MSG_SYS;
            main_puts("Already connected to ");
            main_print(irc_server);
            return;
        }
        
        // Otro servidor - preguntar si desconectar
        ui_err("Already connected.");
        current_attr = ATTR_MSG_SYS;
        main_print("Disconnect? (y/n)");
        
        while (1) {
            uint8_t k = in_inkey();
            HALT();
            uart_drain_to_buffer();
            if (k == 'n' || k == 'N') { main_print("Cancelled."); return; }
            if (k == 'y' || k == 'Y') {
                main_print("Disconnecting...");
                force_disconnect();
                irc_server[0] = 0;
                network_name[0] = 0;
                force_status_redraw = 1;
                draw_status_bar_real();
                already_disconnected = 1;
                break; 
            }
        }
    }

    if (strcmp(port, "6697") == 0) use_ssl = 1;

    strncpy(irc_server, server, sizeof(irc_server) - 1);
    irc_server[sizeof(irc_server) - 1] = 0;
    strncpy(irc_port, port, sizeof(irc_port) - 1);
    irc_port[sizeof(irc_port) - 1] = 0;
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Connecting to "); main_puts(irc_server); main_putc(':'); main_puts(irc_port); main_puts("... ");
    
    // 3. LIMPIEZA DE ESTADO INTELIGENTE
    // Si ya desconectamos arriba, NO volvemos a llamar a force_disconnect().
    // Esto evita el reinicio por desbordamiento de pila y el retraso doble.
    if (!already_disconnected) {
        force_disconnect();
    } else {
        // Solo reseteamos memoria lógica, sin tocar el hardware (que ya está limpio)
        reset_all_channels(); 
        rb_head = 0; rb_tail = 0; rx_pos = 0; rx_line[0] = '\0';
        user_mode[0] = '\0';
    }
    
    draw_status_bar(); 
    cursor_visible = 0; 
    redraw_input_full(); 
    
    // 4. INICIO DE CONEXIÓN
    uart_send_line("AT+CIPMUX=0"); wait_for_response(S_OK, 30);
    uart_send_line("AT+CIPSERVER=0"); wait_for_response(S_OK, 30);
    uart_send_line("AT+CIPDINFO=0"); wait_for_response(S_OK, 30);
    
    if (use_ssl) {
        uart_send_line("AT+CIPSSLSIZE=4096"); wait_for_response(S_OK, 30);
    }

    uart_send_string("AT+CIPSTART=\"");
    if (use_ssl) uart_send_string("SSL"); else uart_send_string("TCP");
    uart_send_string("\",\""); uart_send_string(irc_server); uart_send_string("\","); uart_send_string(irc_port); uart_send_string("\r\n");
    
    uart_allow_cancel = 0; ui_freeze_status = 1; rx_pos = 0; rb_head = rb_tail = 0; result = 0;
    
    // FASE 1: Diagnóstico
    {
        uint16_t frames_limit = use_ssl ? TIMEOUT_SSL : TIMEOUT_DNS;
        result = wait_for_connection_result(frames_limit);
    }
    uart_allow_cancel = 1; ui_freeze_status = 0;
    
    if (result == 0) {
        main_newline(); // Separar del mensaje "Connecting..."
        force_disconnect(); 
        connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; 
        draw_status_bar(); 
        redraw_input_full(); 
        return;
    }
    
    // 3. MODO TRANSPARENTE
    current_attr = ATTR_MSG_PRIV; main_print("TCP OK"); 
    
    { uint8_t i; for (i = 0; i < 20; i++) { HALT(); uart_drain_and_drop_all(); } }
    rb_head = rb_tail = 0; rx_pos = 0;
    uart_send_line("AT+CIPMODE=1");
    
    { result = wait_for_response(S_OK, 100); }

    if (result == 0) {
        ui_err("CIPMODE FAIL"); 
        force_disconnect(); 
        connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; 
        draw_status_bar(); 
        redraw_input_full(); 
        return;
    }
    
    { uint8_t i; for (i = 0; i < 20; i++) { HALT(); uart_drain_to_buffer(); } }
    uart_send_string("AT+CIPSEND\r\n");
    
    // FASE 2: Protección Zombie State
    if (!wait_for_prompt_char('>', TIMEOUT_PROMPT)) {
        ui_err("Error: No '>' prompt (Zombie state)"); 
        force_disconnect(); 
        connection_state = STATE_WIFI_OK; 
        cursor_visible = 1; 
        draw_status_bar(); 
        redraw_input_full(); 
        return;
    }
    
    connection_state = STATE_TCP_CONNECTED; closed_reported = 0;
    
    // Pequeña pausa para estabilización, pero SIN descartar datos
    // El servidor puede enviar datos inmediatamente
    { uint8_t i; for (i = 0; i < 5; i++) { HALT(); uart_drain_to_buffer(); } }
    rx_pos = 0; rx_overflow = 0;
    
    // 4. FASE DE REGISTRO IRC
    if (irc_nick[0]) {
        current_attr = ATTR_MSG_PRIV; main_puts("Registering... ");
        
        char *p = tx_buffer; p = str_append(p, "NICK "); p = str_append(p, irc_nick); esp_send_line_crlf_nowait(tx_buffer);
        wait_drain(5);
        p = tx_buffer; p = str_append(p, "USER "); p = str_append(p, irc_nick); p = str_append(p, " 0 * :"); p = str_append(p, irc_nick); esp_send_line_crlf_nowait(tx_buffer);
        main_newline();
        
        current_attr = ATTR_MSG_PRIV; main_print("Waiting for server (SPACE to cancel)...");
        
        uint8_t loop_done = 0; uint16_t silence_frames = 0; const uint16_t MAX_SILENCE = 4500; rx_pos = 0;
        
        while (!loop_done) {
            HALT(); uart_drain_to_buffer();
            uint8_t k = in_inkey();
            
            // FASE 3: Botón de Abortar
            if (k == 'q' || k == 'Q' || k == 32 || k == 7) { 
                ui_err("Aborted by user."); 
                force_disconnect(); 
                connection_state = STATE_WIFI_OK; 
                loop_done = 1; 
                break; 
            }
            
            if (try_read_line_nodrain()) {
                silence_frames = 0;
                
                if (strstr(rx_line, " 001 ") || strncmp(rx_line, ":server 001", 11) == 0) {
                    connection_state = STATE_IRC_READY; 
                    loop_done = 1; 
                    rx_pos = 0; 
                    continue; 
                }
                
                char *ping_ptr = NULL;
                if (strncmp(rx_line, "PING", 4) == 0) ping_ptr = rx_line;
                else ping_ptr = strstr(rx_line, " PING ");

                if (ping_ptr) {
                    char *params = ping_ptr;
                    if (*params == ' ') params++;
                    params += 4;
                    while (*params == ' ') params++;
                    
                    char *resp = tx_buffer; resp[0] = 0;
                    str_append(resp, "PONG ");
                    str_append_n(resp, params, (uint8_t)(TX_BUFFER_SIZE - 10));
                    esp_send_line_crlf_nowait(resp);
                    
                    current_attr = ATTR_MSG_SYS; main_print("[Ping replied]");
                    rx_pos = 0; continue;
                }

                if ((rx_line[0] == 'C' && strcmp(rx_line, S_CLOSED) == 0) || strstr(rx_line, "ERROR :Closing")) {
                    ui_err("Connection closed by server."); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue;
                }
                
                // FASE 3: Errores Críticos
                if (strstr(rx_line, " 433 ")) { ui_err("Nick in use"); connection_state = STATE_IRC_READY; loop_done = 1; rx_pos = 0; continue; }
                if (strstr(rx_line, " 464 ")) { ui_err("Password Incorrect."); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue; }
                if (strstr(rx_line, " 465 ")) { ui_err("BANNED from server."); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; rx_pos = 0; continue; }
                
                if (strstr(rx_line, " 375 ") || strstr(rx_line, " 372 ") || strstr(rx_line, " 376 ") || strstr(rx_line, " 422 ")) {
                    char *txt = strrchr(rx_line, ':'); if (txt) { txt++; current_attr = attr_with_ink(ATTR_MSG_SYS, INK_YELLOW); main_print(txt); }
                }
                rx_pos = 0;
            } else {
                silence_frames++;
                if (silence_frames > MAX_SILENCE) { ui_err("Timeout waiting for MOTD."); force_disconnect(); connection_state = STATE_WIFI_OK; loop_done = 1; }
            }
        }
        
        if (connection_state == STATE_IRC_READY) { current_attr = ATTR_MSG_PRIV; main_print("Ready!"); }
        rb_head = rb_tail = 0; rx_pos = 0; rx_overflow = 0;
    } else {
        ui_sys("Set /nick first");
    }
    cursor_visible = 1; draw_status_bar(); redraw_input_full();
}

static void cmd_nick(const char *args)
{
    if (!args || !*args) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Current nick: ");
        if (irc_nick[0]) main_print(irc_nick); else main_print(S_NOTSET);
        return;
    }
    
    strncpy(irc_nick, args, sizeof(irc_nick) - 1);
    irc_nick[sizeof(irc_nick) - 1] = '\0';
    
    // Feedback visual
    current_attr = ATTR_MSG_SYS;
    main_puts("Nick set to: ");
    main_print(irc_nick);
    
    if (connection_state >= STATE_TCP_CONNECTED) {
        char *p = tx_buffer;
        p = str_append(p, "NICK ");
        p = str_append(p, irc_nick);
        irc_send_raw(tx_buffer);
    }
    
    // Actualizar ambas barras
    draw_status_bar();
}

static void cmd_join(const char *args)
{
    if (!args || !*args) {
        ui_usage("join #channel");  // <--- ¡AHORRO! 1 line en vez de 2
        return;
    }
    
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }

    // 1. COMPROBAR SI YA ESTAMOS EN ESE channel
    int8_t idx = find_channel(args);
    
    if (idx >= 0) {
        // El channel ya existe en nuestra lista
        if ((uint8_t)idx == current_channel_idx) {
            // Y encima ya estamos mirándolo
            ui_sys("Already in ");
            main_print(channels[idx].name);
        } else {
            // Existe pero estamos en otra ventana >> Cambiamos a ella
            switch_to_channel((uint8_t)idx);
            current_attr = ATTR_MSG_SYS;
            main_puts(S_SWITCHTO); 
            main_print(channels[idx].name);
        }
        // IMPORTANTE: Volvemos aquí para no enviar el command JOIN de nuevo
        return;
    }

    // 2. SI NO ESTAMOS, COMPROBAR HUECO (Modo Pesimista)
    if (find_empty_channel_slot() == -1) {
        ui_err(S_MAXWIN);
        main_print("Use /close or /part first.");
        return;
    }
    
    // 3. ENVIAR SOLICITUD AL SERVIDOR
    char *p = tx_buffer;
    p = str_append(p, "JOIN ");
    p = str_append(p, args);
    irc_send_raw(tx_buffer);
}

static void cmd_part(const char *args)
{
    const char *chan = args && *args ? args : irc_channel;
    
    if (!chan[0]) {
        ui_err("Not in a channel");
        return;
    }
    
    char *p = tx_buffer;
    p = str_append(p, "PART ");
    p = str_append(p, chan);
    irc_send_raw(tx_buffer);
    
    // Remove channel from our list
    int8_t idx = find_channel(chan);
    if (idx >= 0) {
        remove_channel((uint8_t)idx);
        draw_status_bar();
    }
}

static void cmd_msg(const char *args)
{
    char *work_str = (char *)args; 
    char *target;
    char *space;
    char *msg;
    
    if (!work_str || !*work_str) {
        ui_usage("msg nick message");
        return;
    }
    
    target = work_str;
    space = strchr(target, ' ');
    
    if (!space) {
        ui_usage("msg nick message");
        return;
    }
    
    *space = '\0'; // Null-terminate target in-situ
    msg = space + 1;
    while (*msg == ' ') msg++; // Skip extra spaces
    
    if (!*msg) {
        ui_err("Empty message");
        return;
    }
    
    irc_send_privmsg(target, msg);
}

static void cmd_query(const char *args)
{
    if (!args || !*args) {
        ui_usage("query nick");
        return;
    }
    
    // 1. Buscar si la ventana ya existe
    int8_t idx = find_query(args);
    
    // 2. Si no existe, intentamos crearla
    if (idx < 0) {
        // Protección: ¿Hay hueco?
        if (find_empty_channel_slot() == -1) {
            ui_err(S_MAXWIN);
            main_print("Use /close first.");
            return;
        }
        
        idx = add_query(args);
        if (idx >= 0) {
            status_bar_dirty = 1;
        }
    }
    
    // 3. Cambiar a la ventana
    if (idx >= 0) {
        switch_to_channel((uint8_t)idx);
        current_attr = ATTR_MSG_SYS;
        main_puts("Query opened with ");
        main_print(channels[idx].name);
    } else {
        ui_err("Could not open query window");
    }
}

static void cmd_quit(const char *args)
{
    if (connection_state >= STATE_TCP_CONNECTED) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Disconnecting from ");
        main_puts(irc_server);
        main_print("...");
        
        char *p = tx_buffer;
        p = str_append(p, "QUIT :");
        if (args && *args) {
            p = str_append(p, args);
        } else {
            p = str_append(p, "SpecTalk ZX");
        }
        irc_send_raw(tx_buffer);
        
        // Give time for quit message to be sent
        wait_drain(25);
        
        // Use force_disconnect which handles transparent mode exit
        force_disconnect();
        
        cursor_visible = 1;  // Restore cursor after disconnect
        
        ui_sys(S_DISCONN);
        draw_status_bar();
    } else {
        ui_err(S_NOTCONN);
    }
}

static void cmd_me(const char *args)
{
    if (!args || !*args) {
        ui_usage("me action");
        return;
    }
    
    if (connection_state < STATE_IRC_READY) {
        ERR_NOTCONN();
        return;
    }
    
    if (!irc_channel[0]) {
        ui_err("No channel or query");
        return;
    }
    
    // Send CTCP ACTION: PRIVMSG target :\x01ACTION text\x01
    char *p = tx_buffer;
    p = str_append(p, S_PRIVMSG);
    p = str_append(p, irc_channel);
    p = str_append(p, " :\x01" "ACTION ");
    p = str_append(p, args);
    *p++ = '\x01';
    *p = '\0';
    irc_send_raw(tx_buffer);
    
    // Show locally
    current_attr = ATTR_MSG_SELF;
    main_puts("* "); main_puts(irc_nick); main_putc(' '); main_print(args);
}

static void cmd_away(const char *args)
{
    if (connection_state < STATE_IRC_READY) {
        ERR_NOTCONN();
        return;
    }
    
    char *p = tx_buffer;
    p = str_append(p, "AWAY");
    if (args && *args) {
        p = str_append(p, " :");
        p = str_append(p, args);
    }
    irc_send_raw(tx_buffer);
    
    current_attr = ATTR_MSG_SYS;
    if (args && *args) {
        main_puts("Away: "); main_print(args);
    } else {
        main_print("Away status cleared");
    }
}

static void cmd_raw(const char *args)
{
    if (!args || !*args) {
        ui_usage("raw IRC_COMMAND");
        return;
    }
    
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    irc_send_raw(args);
}

static void cmd_whois(const char *args)
{
    if (!args || !*args) {
        ui_usage("whois nick");
        return;
    }
    
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    char *p = tx_buffer;
    p = str_append(p, "WHOIS ");
    p = str_append(p, args);
    irc_send_raw(tx_buffer);
}

static void cmd_list(const char *args)
{
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    // REQUIRE channel argument to prevent ESP saturation
    if (!args || !*args) {
        ui_usage("list #channel");
        main_print("(Full list disabled - would saturate ESP)");
        return;
    }
    
    // FIX: Activar modo búsqueda para que el handler 322 muestre los resultados
    search_mode = SEARCH_CHAN;
    search_pattern[0] = '\0'; // Sin filtro, mostrar TODO lo que llegue
    
    // Reset pagination counters
    search_index = 0;
    pagination_count = 0;
    
    char *p = tx_buffer;
    p = str_append(p, "LIST ");
    p = str_append(p, args);
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Listing channel(s): ");
    main_print(args);
    
    irc_send_raw(tx_buffer);
}

static void cmd_who(const char *args)
{
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    // FIX: Activar modo búsqueda para que el handler 352 sepa qué hacer
    search_mode = SEARCH_USER;
    search_pattern[0] = '\0'; // Sin filtro
    
    // --- CORRECCIÓN CRÍTICA ---
    // El command WHO no tiene código de inicio (como el 321 de LIST),
    // así que debemos activar la recepción manualmente o el handler descartará TODO.
    pagination_active = 1;
    pagination_cancelled = 0;
    // ---------------------------
    
    // Reset pagination counters
    search_index = 0;
    pagination_count = 0;
    
    char *p = tx_buffer;
    p = str_append(p, "WHO ");
    if (args && *args) {
        p = str_append(p, args);
    } else if (irc_channel[0]) {
        p = str_append(p, irc_channel);
    } else {
        // Desactivamos si hubo error de uso
        pagination_active = 0; 
        ui_usage("who #channel or nick");
        return;
    }
    irc_send_raw(tx_buffer);
}

static void cmd_names(const char *args)
{
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    char *p = tx_buffer;
    p = str_append(p, "NAMES ");
    if (args && *args) {
        p = str_append(p, args);
    } else if (irc_channel[0]) {
        p = str_append(p, irc_channel);
    } else {
        ui_usage("names [#channel]");
        return;
    }
    counting_new_users = 1;  // Next 353 starts fresh count
    show_names_list = 1;     // Show user list for this NAMES request
    irc_send_raw(tx_buffer);
    
    ui_sys("Listing users...");
}

static void cmd_topic(const char *args)
{
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    char *p = tx_buffer;
    p = str_append(p, "TOPIC ");
    if (args && *args) {
        p = str_append(p, args);
    } else if (irc_channel[0]) {
        p = str_append(p, irc_channel);
    } else {
        ui_usage("topic [#channel] [text]");
        return;
    }
    irc_send_raw(tx_buffer);
}

static void cmd_search(const char *args)
{
    char *p;

    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }

    if (!args || !*args) {
        ui_usage("search #pattern or nick");
        return;
    }

    pagination_active = 1;
    pagination_cancelled = 0;
    pagination_count = 0;
    search_index = 0;

    while (*args == ' ') args++;

    if (args[0] == '#') {
        // --- MODO CANALES (LIST) ---
        const char *pattern = args + 1;
        while (*pattern == ' ') pattern++;

        if (!*pattern) {
            ui_err("Empty pattern. Use /list for all.");
            return;
        }

        search_mode = SEARCH_CHAN;
        
        {
            uint8_t i = 0;
            while (pattern[i] && pattern[i] != ' ' && i < 30) {
                search_pattern[i] = pattern[i];
                i++;
            }
            search_pattern[i] = 0;
        }

        p = tx_buffer;
        p = str_append(p, "LIST *");
        p = str_append(p, search_pattern);
        *p++ = '*';
        *p = 0;
        irc_send_raw(tx_buffer);

        current_attr = ATTR_MSG_SYS;
        main_puts("Searching channels: *");
        main_puts(search_pattern);
        main_print("*");
        
    } else {
        // --- MODO USUARIOS (WHO) ---
        search_mode = SEARCH_USER;
        
        {
            uint8_t i = 0;
            while (args[i] && args[i] != ' ' && i < 30) {
                search_pattern[i] = args[i];
                i++;
            }
            search_pattern[i] = 0;
        }

        if (!search_pattern[0]) {
            ui_err("Empty pattern");
            return;
        }

        // WHO sin asteriscos - el servidor busca coincidencias
        p = tx_buffer;
        p = str_append(p, "WHO ");
        p = str_append(p, search_pattern);
        irc_send_raw(tx_buffer);

        current_attr = ATTR_MSG_SYS;
        main_puts("Searching users: ");
        main_print(search_pattern);
    }
}

static void cmd_ignore(const char *args)
{
    uint8_t i;
    
    if (!args || !*args) {
        // Show ignore list
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
    
    // Check for -nick (unignore)
    if (args[0] == '-') {
        const char *nick = args + 1;
        while (*nick == ' ') nick++;
        if (!*nick) {
            ui_usage("ignore -nick to unignore");
            return;
        }
        if (remove_ignore(nick)) {
            current_attr = ATTR_MSG_SYS;
            main_puts("Unignored: ");
            main_print(nick);
        } else {
            current_attr = ATTR_ERROR;
            main_puts("Not in ignore list: ");
            main_print(nick);
        }
        return;
    }
    
    // Add to ignore list
    if (add_ignore(args)) {
        current_attr = ATTR_MSG_SYS;
        main_puts("Now ignoring: ");
        main_print(args);
    } else if (is_ignored(args)) {
        current_attr = ATTR_ERROR;
        main_puts("Already ignoring: ");
        main_print(args);
    } else {
        ui_err("Ignore list full (8)");
    }
}

static void cmd_kick(const char *args)
{
    char *nick;
    char *reason;
    char *p;
    
    if (connection_state < STATE_TCP_CONNECTED) {
        ERR_NOTCONN();
        return;
    }
    
    if (!args || !*args) {
        ui_usage("kick nick [reason]");
        return;
    }
    
    // Must be in a channel (not Server window or query)
    if (current_channel_idx == 0 || channels[current_channel_idx].is_query || 
        irc_channel[0] != '#') {
        ui_err("Must be in a channel to kick");
        return;
    }
    
    // Parse nick and optional reason
    nick = (char *)args;
    reason = nick;
    while (*reason && *reason != ' ') reason++;
    if (*reason) {
        *reason = '\0';  // Terminate nick
        reason++;
        while (*reason == ' ') reason++;  // Skip spaces
    }
    
    // Build KICK command: KICK #channel nick :reason
    p = tx_buffer;
    p = str_append(p, "KICK ");
    p = str_append(p, irc_channel);
    *p++ = ' ';
    p = str_append(p, nick);
    if (*reason) {
        p = str_append(p, " :");
        p = str_append(p, reason);
    }
    irc_send_raw(tx_buffer);
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Kicking "); main_puts(nick);
    if (*reason) {
        main_puts(" ("); main_puts(reason); main_puts(")");
    }
    main_newline();
}

// Forward declarations for theme system



// ============================================================
// SYSTEM COMMANDS (!)
// ============================================================

static void sys_help(const char *args);

// SYSTEM COMMANDS (!)
// ============================================================
// HELP SYSTEM - Data-driven (saves ~800 bytes vs repeated calls)
// ============================================================
// Attribute indices for help text
#define HA_TITLE  0  // Title (bright white)
#define HA_SECTION 1 // Section header (bright cyan)
#define HA_CMD    2  // Command (bright yellow)
#define HA_DESC   3  // Description (white)
#define HA_NOTE   4  // Note (cyan)

static const uint8_t help_attrs[] = {
    PAPER_BLUE | INK_WHITE | BRIGHT,  // HA_TITLE
    PAPER_BLUE | INK_CYAN | BRIGHT,   // HA_SECTION
    PAPER_BLUE | INK_YELLOW | BRIGHT, // HA_CMD
    PAPER_BLUE | INK_WHITE,           // HA_DESC
    PAPER_BLUE | INK_CYAN             // HA_NOTE
};

typedef struct {
    uint8_t row;   // Row offset from MAIN_START
    uint8_t col;   // Column
    uint8_t attr;  // Attribute index (HA_xxx)
    const char *text;
} HelpLine;

static const HelpLine help_page1[] = {
    { 0, 14, HA_TITLE,  "===== SPECTALK HELP (1/2) =====" },
    { 2,  2, HA_SECTION,"SYSTEM (!)" },
    { 3,  2, HA_CMD,    "!help !status !about !init !theme N" },
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
    {13,  2, HA_CMD,    "/whois /ignore /away /raw" },
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

static void sys_help(const char *args)
{
    (void)args;
    uint8_t key;
    uint8_t current_page = 1;
    
    while (current_page != 0) {
        show_help_page(current_page == 1 ? help_page1 : help_page2);
        
        while (in_inkey() != 0) { HALT(); }
        while ((key = in_inkey()) == 0) { HALT(); }
        while (in_inkey() != 0) { HALT(); }
        
        // EDIT (key 7) = salir, otra tecla = cambiar página
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

static void sys_status(const char *args)
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

static void sys_about(const char *args)
{
    (void)args;
    current_attr = ATTR_MSG_SYS;
    main_print("SpecTalk ZX v" VERSION);
    main_print("IRC Client for ZX Spectrum");
    main_print("(C) 2026 M. Ignacio Monge Garcia");
    main_print("Licensed under GNU GPL v2.0");
}

// esp_init() is implemented in spectalk.c and exposed via spectalk.h

static void sys_init(const char *args)
{
    (void)args;
    uint8_t result;
    
    current_attr = ATTR_MSG_PRIV;
    main_puts("Re-initializing ESP8266... ");
    
    // --- optimization (Lógica "Fail-Fast") ---
    // Antes de hacer el lento "baile de desconnection" (force_disconnect tarda 2.5s),
    // probamos si el ESP8266 ya está en modo command o si está muerto.
    
    uart_drain_and_drop_all(); // Limpiar buffer de input
    uart_send_string("AT\r\n");
    
    // waitmos respuesta brevemente (25 frames = 0.5s)
    if (wait_for_response(S_OK, 25)) {
        // CASO A: El ESP responde "OK".
        // Está vivo y en modo command. No necesitamos hacer la secuencia +++.
        // Solo cerramos cualquier socket abierto por limpieza y seguimos.
        uart_send_string("AT+CIPCLOSE\r\n");
        wait_for_response(NULL, 10); // waitr OK o error, nos da igual
    } else {
        // CASO B: No responde (Timeout).
        // Posibilidades:
        // 1. Está en modo Transparente (conectado).
        // 2. Está colgado o se ha reseteado.
        // 
        // En ambos casos, saltamos force_disconnect() para evitar waitr 2s extra.
        // Dejamos que esp_init() (que llamamos abajo) haga la secuencia de escape completa.
    }
    // ------------------------------------------
    
    // Reset state
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

static void cmd_theme(const char *args)
{
    uint8_t t;

    if (!args || !*args) {
        current_attr = ATTR_MSG_SYS;
        main_print("Usage: !theme 1|2|3");
        return;
    }

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
    main_newline();
    redraw_input_full();
}

// ============================================================
// COMMAND DISPATCHER (Saves ~600-800 bytes)
// ============================================================

typedef void (*user_cmd_handler_t)(const char *args);

typedef struct {
    const char *name;      // Command name (UPPERCASE)
    const char *alias;     // Short alias (UPPERCASE) or NULL
    user_cmd_handler_t fn; // Handler function
} UserCmd;

// Wrappers para functions de sistheme que no aceptan argumentos

// Wrapper para CLOSE (que tenía lógica inline)
static void cmd_close_wrapper(const char *a) {
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

// Wrapper para LISTAR VENTANAS (que tenía lógica inline)
static void cmd_windows_wrapper(const char *a) {
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
    // SYSTEM COMMANDS (!)
    { "HELP",    "H",     sys_help },
    { "STATUS",  "S",     sys_status },
    { "INIT",    "I",     sys_init },
    { "THEME",   NULL,    cmd_theme },
    { "ABOUT",   NULL,    sys_about },
    
    // IRC COMMANDS (/)
    { "SERVER",  "CONNECT", cmd_connect },
    { "NICK",    NULL,      cmd_nick },
    { "JOIN",    "J",       cmd_join },
    { "PART",    "P",   cmd_part },
    { "MSG",     "M",       cmd_msg },
    { "QUERY",   "Q",       cmd_query }, // Nota: Q colisiona con QUIT, lo manejamos abajo
    { "CLOSE",   NULL,      cmd_close_wrapper },
    { "QUIT",    NULL,      cmd_quit },  // Q alias manual
    { "ME",      NULL,      cmd_me },
    { "AWAY",    NULL,      cmd_away },
    { "RAW",     NULL,      cmd_raw },
    { "WHOIS",   "WI",      cmd_whois },
    { "WHO",     NULL,      cmd_who },
    { "LIST",    "LS",      cmd_list },
    { "NAMES",   NULL,      cmd_names },
    { "TOPIC",   NULL,      cmd_topic },
    { "SEARCH",  NULL,      cmd_search },
    { "IGNORE",  NULL,      cmd_ignore },
    { "KICK",    "K",       cmd_kick },
    { "CHANNELS","W",       cmd_windows_wrapper }, // Cubre /w, /windows, /chans
};


// COMMAND PARSER

void parse_user_input(char *line)
{
    char *args;
    char *cmd_str = line;
    uint8_t is_sys = 0;
    
    // 1. Detectar tipo de command
    if (cmd_str[0] == '!') {
        is_sys = 1;
        cmd_str++;
    } else if (cmd_str[0] == '/') {
        cmd_str++;
    } else {
        // Texto normal >> enviar message privado o a channel
        if (connection_state < STATE_TCP_CONNECTED) {
            current_attr = ATTR_ERROR;
            main_print("Not connected. Use /server host");
            return;
        }
        
        // Atajo "NICK: msg"
        char *colon = strchr(line, ':');
        if (colon && colon != line && (colon[1] == ' ' || colon[1] == '\t')) {
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
    
    // 2. Separar argumentos (divide el string en el primer espacio)
    args = strchr(cmd_str, ' ');
    if (args) {
        *args = '\0';
        args++;
        while (*args == ' ') args++;
    }
    
    // 3. (optimization: Bucle de mayúsculas ELIMINADO)
    // Ahorramos bytes y ciclos al no recorrer el string aquí.
    
    // 4. Casos Especiales
    
    // /0 - /9 (Cambio de ventana)
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
    
    // Alias manuales con st_stricmp (ASM)
    if (st_stricmp(cmd_str, "Q") == 0) { 
        cmd_quit(args);
        return;
    }
    if (st_stricmp(cmd_str, "WINDOWS") == 0 || st_stricmp(cmd_str, "CHANS") == 0) {
        cmd_windows_wrapper(args);
        return;
    }

    // 5. Búsqueda en Tabla usando st_stricmp (ASM)
    uint8_t i;
    for (i = 0; i < sizeof(USER_COMMANDS)/sizeof(UserCmd); i++) {
        if (is_sys && i >= 5) break; 
        if (!is_sys && i < 5) continue;
        
        const UserCmd *c = &USER_COMMANDS[i];
        
        // st_stricmp compara insensible a mayúsculas sin modificar el string original
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
