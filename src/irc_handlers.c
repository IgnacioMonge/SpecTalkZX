/*
 * irc_handlers.c - IRC message parsing and handling (Table-Driven Optimized)
 * SpecTalk ZX - IRC Client for ZX Spectrum
 * Copyright (C) 2026 M. Ignacio Monge Garcia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../include/spectalk.h"

// =============================================================================
// GLOBAL PARSING CONTEXT (Replaces Stack Args)
// Optimization: Saves pushing 8 bytes to stack for every handler call.
// Handlers access these directly via fast memory loads.
// =============================================================================
static char *pkt_usr;
static char *pkt_par;
static char *pkt_txt;
static char *pkt_cmd;

// Empty string sentinel to keep parser globals valid even on malformed lines.
static char pkt_empty[] = "";

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

// Helper: Marcar actividad en canales no activos (Deduplicated logic)
static void mark_channel_activity(uint8_t idx)
{
    if ((uint8_t)idx != current_channel_idx) {
        channels[idx].flags |= CH_FLAG_UNREAD;
        other_channel_activity = 1;
        status_bar_dirty = 1;
    }
}

// Helper: Respuesta CTCP optimizada
static void send_ctcp_reply(const char *target, const char *tag, const char *data)
{
    uart_send_string("NOTICE ");
    uart_send_string(target);
    uart_send_string(" :\x01");
    uart_send_string(tag);
    if (data && *data) {
        ay_uart_send(' ');
        uart_send_string(data);
    }
    uart_send_line("\x01");
}

// =============================================================================
// HANDLERS (Void signature - access context via globals)
// =============================================================================

static void h_nick(void)
{
    char *new_nick = (*pkt_txt) ? pkt_txt : pkt_par;
    if (*new_nick == ':') new_nick++;

    if (st_stricmp(pkt_usr, irc_nick) == 0) {
        strncpy(irc_nick, new_nick, sizeof(irc_nick) - 1);
        irc_nick[sizeof(irc_nick) - 1] = 0;
        draw_status_bar();
        current_attr = ATTR_MSG_SYS;
        main_puts("You are now known as "); main_print(irc_nick);
        return;
    }

    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if ((channels[i].flags & (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) == (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) {
            if (st_stricmp(channels[i].name, pkt_usr) == 0) {
                strncpy(channels[i].name, new_nick, sizeof(channels[i].name) - 1);
                channels[i].name[sizeof(channels[i].name) - 1] = 0;
                
                if (current_channel_idx == i) {
                    current_attr = ATTR_MSG_SYS;
                    main_puts("*** "); main_puts(pkt_usr); 
                    main_puts(" is now known as "); main_print(new_nick);
                }
                draw_status_bar();
            }
        }
    }
}

static void h_ignore(void) { /* Do nothing */ }

static void h_cap(void)
{
    // pkt_par contains: "* LS ..." or "* ACK ..."
    if (pkt_par && (strstr(pkt_par, " LS") || strncmp(pkt_par, "* LS", 4) == 0)) {
        uart_send_line("CAP END");
    }
}

static void h_ping(void)
{
    uart_send_string("PONG :");
    uart_send_line(pkt_txt);
}

static void h_mode(void)
{
    const char *target = irc_param(0);
    if (!target || !*target) target = pkt_par;
    
    if (irc_nick[0] && st_stricmp(target, irc_nick) == 0) {
        const char *modes = irc_param(1);
        if (!modes || !*modes) modes = pkt_txt;
        if (modes && *modes) {
            if (*modes == ':') modes++;
            strncpy(user_mode, modes, sizeof(user_mode) - 1);
            user_mode[sizeof(user_mode) - 1] = '\0';
            draw_status_bar();
        }
        return;
    }
    
    if (target[0] == '#') {
        int8_t idx = find_channel(target);
        if (idx >= 0) {
            const char *modes = irc_param(1);
            if (!modes || !*modes) modes = pkt_txt;
            if (modes && *modes) {
                strncpy(channels[idx].mode, modes, sizeof(channels[idx].mode) - 1);
                channels[idx].mode[sizeof(channels[idx].mode) - 1] = '\0';
            }
        }
        current_attr = ATTR_MSG_SYS;
        main_puts(pkt_usr); main_puts(" sets mode "); main_puts(pkt_txt);
        main_puts(" on "); main_print(target);
        return;
    }
    
    current_attr = ATTR_MSG_SYS;
    main_puts("Mode "); main_print(pkt_par);
}

static void h_privmsg_notice(void)
{
    char *target = pkt_par;
    if (!target || !*target || !pkt_txt || !*pkt_txt) return;

    // Error detection in NOTICE
    if (pkt_cmd[0] == 'N' && (target[0] == '*' || strchr(pkt_usr, '.'))) {
        if (st_stristr(pkt_txt, "cannot") || st_stristr(pkt_txt, "rate") ||
            st_stristr(pkt_txt, "denied") || st_stristr(pkt_txt, "try again")) {

            if (pagination_active || search_mode != SEARCH_NONE) cancel_search_state(0);
            current_attr = ATTR_ERROR;
            main_print(pkt_txt);
            return;
        }
    }

    if (is_ignored(pkt_usr) && !strchr(pkt_usr, '.')) return;

    strip_irc_codes(pkt_txt);

    if (target[0] == '#') {
        int8_t idx = find_channel(target);
        if (idx >= 0) mark_channel_activity((uint8_t)idx);
        if (idx >= 0 && (uint8_t)idx != current_channel_idx) return;
    }

    // --- CTCP HANDLING ---
    if (pkt_txt[0] == 1) {
        char *ctcp_cmd = pkt_txt + 1;

        switch (ctcp_cmd[0]) {
            case 'A': // ACTION
                if (strncmp(ctcp_cmd, "ACTION ", 7) == 0) {
                    char *act = ctcp_cmd + 7;
                    char *end = strchr(act, 1);
                    if (end) *end = 0;

                    if (target[0] == '#') {
                        current_attr = ATTR_MSG_CHAN;
                        main_puts("* "); main_puts(pkt_usr); main_putc(' '); main_print(act);
                    } else {
                        int8_t query_idx = add_query(pkt_usr);
                        if (query_idx >= 0) {
                            status_bar_dirty = 1;
                            mark_channel_activity((uint8_t)query_idx);
                        }

                        if (query_idx < 0 || (uint8_t)query_idx != current_channel_idx) {
                            // "<< OTRO: " en amarillo (ATTR_MSG_SELF), texto en verde (ATTR_MSG_PRIV)
                            current_attr = ATTR_MSG_SELF;
                            main_puts("<< "); main_puts(pkt_usr); main_puts(": ");
                        }
                        current_attr = ATTR_MSG_PRIV;
                        main_puts("* "); main_puts(pkt_usr); main_putc(' '); main_print(act);
                        notification_beep();
                    }
                    return;
                }
                break;

            case 'V': // VERSION
                if (strncmp(ctcp_cmd, "VERSION", 7) == 0) {
                    send_ctcp_reply(pkt_usr, "VERSION", "SpecTalk ZX v" VERSION " :ZX Spectrum");
                    return;
                }
                break;

            case 'P': // PING
                if (strncmp(ctcp_cmd, "PING ", 5) == 0) {
                    char *ts = ctcp_cmd + 5;
                    char *end = strchr(ts, 1);
                    if (end) *end = 0;
                    send_ctcp_reply(pkt_usr, "PING", ts);
                    return;
                }
                break;

            case 'T': // TIME
                if (strncmp(ctcp_cmd, "TIME", 4) == 0) {
                    if (time_synced) {
                        char time_buf[6];
                        fast_u8_to_str(time_buf, time_hour);
                        time_buf[2] = ':';
                        fast_u8_to_str(time_buf + 3, time_minute);
                        time_buf[5] = 0;

                        send_ctcp_reply(pkt_usr, "TIME", time_buf);
                    } else {
                        send_ctcp_reply(pkt_usr, "TIME", "Unknown");
                    }
                    return;
                }
                break;
        }
        return;
    }

    // --- NORMAL MESSAGES ---
    if (pkt_cmd[0] == 'N' && (target[0] == '*' || strchr(pkt_usr, '.'))) {
        if (st_stristr(pkt_txt, "Ident") || strstr(pkt_txt, "Looking up") ||
            strstr(pkt_txt, "Checking") || strstr(pkt_txt, "*** ")) return;
        current_attr = ATTR_MSG_SERVER;
        main_puts("[NOTICE] "); main_print(pkt_txt);
        return;
    }

    if (target[0] == '#') {
        // Canal: hora (ATTR_MSG_TIME), nick (ATTR_MSG_NICK), msg (ATTR_MSG_CHAN)
        current_attr = ATTR_MSG_CHAN;
        main_print_time_prefix();

        // IMPORTANTÍSIMO (64 cols): NO cambiar attr hasta terminar "<nick> "
        current_attr = ATTR_MSG_NICK;
        main_puts("<");
        main_puts(pkt_usr);
        main_puts("> ");
        current_attr = ATTR_MSG_CHAN;
        main_print(pkt_txt);

        if (irc_nick[0] && st_stristr(pkt_txt, irc_nick)) notification_beep();
    } else {
        int8_t query_idx = find_query(pkt_usr);
        if (query_idx < 0) {
            query_idx = add_query(pkt_usr);
            if (query_idx >= 0) status_bar_dirty = 1;
        }
        if (query_idx >= 0) mark_channel_activity((uint8_t)query_idx);

        main_print_time_prefix();

        if (query_idx < 0 || (uint8_t)query_idx != current_channel_idx) {
            // PRIV fuera de ventana: << OTRO (amarillo): MSG (verde)
            current_attr = ATTR_MSG_SELF;
            main_puts("<< ");
            main_puts(pkt_usr);
            main_puts(": ");
            current_attr = ATTR_MSG_PRIV;
            main_print(pkt_txt);
        } else {
            // En la ventana del privado: mantener "<nick> msg" pero con nick dedicado
            current_attr = ATTR_MSG_NICK;
            main_puts("<");
            main_puts(pkt_usr);
            main_puts("> ");
            current_attr = ATTR_MSG_PRIV;
            main_print(pkt_txt);
        }

        notification_beep();
    }
}

static void h_join(void)
{
    char *chan = (*pkt_txt) ? pkt_txt : pkt_par;
    if (*chan == ':') chan++;
    
    if (st_stricmp(pkt_usr, irc_nick) == 0) {
        current_attr = ATTR_MSG_JOIN;
        main_puts("Now talking in "); main_print(chan);

        int8_t idx = find_channel(chan);
        if (idx < 0) idx = add_channel(chan);
        
        if (idx < 0) { ui_err("Max channels reached"); return; }
        
        if ((uint8_t)idx != current_channel_idx) switch_to_channel((uint8_t)idx);
        
        channels[idx].user_count = 0;
        counting_new_users = 1;
        strncpy(names_target_channel, chan, sizeof(names_target_channel) - 1);
        names_target_channel[sizeof(names_target_channel) - 1] = '\0';
        names_pending = 1;
        names_timeout_frames = 0;
        draw_status_bar();
    } else {
        int8_t idx = find_channel(chan);
        if (idx >= 0) {
            channels[idx].user_count++;
            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_puts(">> "); main_puts(pkt_usr); main_puts(" joined "); main_print(chan);
                draw_status_bar();
            }
        }
    }
}

static void h_part(void)
{
    char *chan = pkt_par;
    if (*chan == ':') chan++;

    int8_t idx = find_channel(chan);
    if (idx >= 0) {
        if (st_stricmp(pkt_usr, irc_nick) == 0) {
            // Usuario local sale: cerrar ventana primero para caer en la anterior
            remove_channel((uint8_t)idx);
            
            // Feedback en la ventana de destino (ej: Status)
            current_attr = ATTR_MSG_JOIN;
            main_print_time_prefix();
            main_puts("<< You left "); 
            main_print(chan);
            draw_status_bar();
        } else {
            // Otro usuario sale
            if (channels[idx].user_count > 0) channels[idx].user_count--;
            
            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_print_time_prefix();
                main_puts("<< "); 
                main_puts(pkt_usr); 
                main_puts(" left");
                
                if (pkt_txt && *pkt_txt) {
                    main_puts(" ("); 
                    main_print(pkt_txt); 
                    main_run_char(')', ATTR_MSG_JOIN);
                } else {
                    main_newline();
                }
                
                draw_status_bar();
            } else {
                mark_channel_activity((uint8_t)idx);
            }
        }
    }
}

static void h_quit(void)
{
    current_attr = ATTR_MSG_JOIN;
    main_puts("<< "); main_puts(pkt_usr); main_puts(" quit");
    if (*pkt_txt) { main_puts(" ("); main_puts(pkt_txt); main_puts(")"); }
    main_newline();
    
    {
        int8_t qidx = find_query(pkt_usr);
        if (qidx > 0) {
            mark_channel_activity((uint8_t)qidx);
            if ((uint8_t)qidx == current_channel_idx) ui_err("--- User closed connection ---");
        }
    }

    // QUIT no incluye canal. Heurística segura:
    // si SOLO hay 1 canal real (# o &) activo (no query), entonces el QUIT
    // necesariamente afecta a ese canal en este cliente -> decremento ahí.
    {
        uint8_t i;
        uint8_t joined_cnt = 0;
        uint8_t target_idx = 0;

        for (i = 1; i < MAX_CHANNELS; i++) {
            if ((channels[i].flags & CH_FLAG_ACTIVE) &&
                !(channels[i].flags & CH_FLAG_QUERY) &&
                (channels[i].name[0] == '#' || channels[i].name[0] == '&')) {

                joined_cnt++;
                target_idx = i;
                if (joined_cnt > 1) break; // no hace falta seguir
            }
        }

        if (joined_cnt == 1) {
            if (channels[target_idx].user_count > 0) {
                channels[target_idx].user_count--;
                if (target_idx == current_channel_idx) draw_status_bar();
            } else {
                // evita underflow; aun así refresca si estás mirando ese canal
                if (target_idx == current_channel_idx) draw_status_bar();
            }
        } else {
            // joined_cnt == 0 o >1: no tocar contadores (sin lista de nicks no es determinista)
            if (channels[current_channel_idx].flags & CH_FLAG_ACTIVE) draw_status_bar();
        }
    }
}

static void h_kick(void)
{
    // tokenize_params() ya ha separado los params en irc_params[] y ha eliminado espacios en pkt_par.
    const char *channel = irc_param(0);
    const char *target  = irc_param(1);

    if (!channel || !*channel) return;
    if (!target  || !*target)  return;

    int8_t idx = find_channel(channel);

    if (st_stricmp(target, irc_nick) == 0) {
        current_attr = ATTR_ERROR;
        main_puts("Kicked from "); main_puts(channel); main_puts(" by "); main_puts(pkt_usr);
        if (pkt_txt && *pkt_txt) { main_puts(" ("); main_puts(pkt_txt); main_putc(')'); }
        main_newline();
        if (idx > 0) { remove_channel((uint8_t)idx); draw_status_bar(); }
    } else {
        if (idx >= 0) {
            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_puts("* "); main_puts(target); main_puts(" kicked by "); main_puts(pkt_usr);
                if (pkt_txt && *pkt_txt) { main_puts(" ("); main_puts(pkt_txt); main_putc(')'); }
                main_newline();
            } else {
                mark_channel_activity((uint8_t)idx);
            }
            if (channels[idx].user_count > 0) {
                channels[idx].user_count--;
                if ((uint8_t)idx == current_channel_idx) draw_status_bar();
            }
        }
    }
}


static void h_kill(void)
{
    char *target = pkt_par;
    
    if (target && st_stricmp(target, irc_nick) == 0) {
        current_attr = ATTR_ERROR;
        main_puts("*** Killed by "); main_puts(pkt_usr);
        if (pkt_txt && *pkt_txt) { main_puts(": "); main_print(pkt_txt); } else main_newline();
        force_disconnect(); connection_state = STATE_WIFI_OK;
        reset_all_channels(); draw_status_bar();
    } else {
        current_attr = ATTR_MSG_JOIN;
        main_puts("* "); if (target) main_puts(target); main_puts(" killed by "); main_puts(pkt_usr);
        if (pkt_txt && *pkt_txt) { main_puts(" ("); main_puts(pkt_txt); main_putc(')'); }
        main_newline();
    }
}

static void h_error(void)
{
    current_attr = ATTR_ERROR;
    main_puts("*** Server: ");
    if (pkt_txt && *pkt_txt) main_print(pkt_txt); else main_print("Connection terminated");
    force_disconnect(); connection_state = STATE_WIFI_OK;
    reset_all_channels(); draw_status_bar();
}

static void h_numeric_401(void)
{
    const char *bad_nick = irc_param(1);
    
    current_attr = ATTR_ERROR;
    main_puts("Error: ");
    if (bad_nick && *bad_nick) { main_puts(bad_nick); main_putc(' '); }
    main_print(pkt_txt);

    if (bad_nick && *bad_nick) {
        int8_t idx = find_query(bad_nick);
        if (idx > 0 && (channels[idx].flags & CH_FLAG_QUERY)) {
            remove_channel((uint8_t)idx);
            draw_status_bar();
        }
    }
}

static void h_numeric_451(void)
{
    if ((pagination_active || search_mode != SEARCH_NONE) && active_search_type != PEND_NONE) {
        pending_search_type = active_search_type;
        pending_search_requires_ready = 1;
        memcpy(pending_search_arg, active_search_arg, sizeof(pending_search_arg));
        pending_search_arg[31] = 0;
        cancel_search_state(0);
        current_attr = ATTR_MSG_SYS;
        main_print("Waiting for registration...");
        return;
    }
    
    current_attr = ATTR_ERROR;
    main_print("*** Session expired");
    force_disconnect(); connection_state = STATE_WIFI_OK;
    reset_all_channels(); draw_status_bar();
    current_attr = ATTR_MSG_SYS; main_print("Use /server to reconnect");
}

static void h_numeric_305_306(void)
{
    // 305 = RPL_UNAWAY ("You are no longer marked as being away")
    // 306 = RPL_NOWAWAY ("You have been marked as being away")
    uint16_t num = str_to_u16(pkt_cmd);
    irc_is_away = (num == 306) ? 1 : 0;
    
    if (num == 305) {
        // Ya no away - resetear sistema de auto-away
        autoaway_active = 0;
        autoaway_counter = 0;
    }
    
    draw_status_bar();
    
    if (pkt_txt && *pkt_txt) {
        current_attr = ATTR_MSG_SYS;
        main_print(pkt_txt);
    }
}

static void h_numeric_332(void)
{
    if (pkt_txt && *pkt_txt) {
        current_attr = ATTR_MSG_TOPIC;
        main_print_time_prefix(); 
        main_puts("Topic: "); main_print(pkt_txt);
    }
}

static void h_numeric_353(void)
{
    const char *msg_chan = irc_param(2);
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    if (!target[0]) return;
    if (st_stricmp(msg_chan, target) != 0) return;
    
    names_timeout_frames = 0;
    names_pending = 1;
    
    char *p = pkt_txt;
    uint16_t count = 0;
    if (*p) count = 1;
    while (*p) { if (*p == ' ') count++; p++; }
    
    if (counting_new_users) {
        chan_user_count = count;
        counting_new_users = 0;
    } else {
        chan_user_count += count;
    }
    
    if (show_names_list) {
        current_attr = ATTR_MSG_NICK;  // Magenta para lista de nicks
        main_print(pkt_txt);
        pagination_count++;
    }
}

static void h_numeric_366(void)
{
    const char *msg_chan = irc_param(1);
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    
    if (target[0] && st_stricmp(msg_chan, target) == 0) {
        names_pending = 0; names_timeout_frames = 0;
        names_target_channel[0] = '\0';
    }
    
    // Finalizar paginación de /names
    if (show_names_list && pagination_active) {
        current_attr = ATTR_MSG_SYS;
        if (pagination_count > 0) {
            char buf[8];
            extern char *u16_to_dec3(char *dst, uint16_t v);
            u16_to_dec3(buf, chan_user_count);
            main_puts("-- ");
            main_puts(buf);
            main_print(" users --");
        }
        pagination_active = 0;
        pagination_count = 0;
    }
    
    show_names_list = 0; 
    draw_status_bar();
}

static void h_numeric_321(void)
{
    if (search_mode != SEARCH_NONE || pending_search_type != PEND_NONE) {
        // Solo activar si no estaba ya activa (evitar doble reset)
        if (!pagination_active) {
            start_pagination();
        }
        current_attr = ATTR_MSG_SYS;
        main_print("Listing...");
    }
}

static void h_end_of_list(void)
{
    current_attr = ATTR_MSG_SYS;
    if (pagination_active) {
        if (pagination_count > 0) main_print("-- Done --");
        else main_print("No results");
    }
    cancel_search_state(0);
}

static void h_numeric_322_352(void)
{
    // Need explicit num check here or split func. 
    // Optimization: check search_mode which implies num type
    if (!pagination_active) return;
    if (search_mode == SEARCH_NONE) return;

    if (search_mode == SEARCH_CHAN) { // 322
        const char *chan = irc_param(1);
        const char *users = irc_param(2);
        
        if (!chan[0]) return;
        if (search_pattern[0] && !strstr(chan, search_pattern)) return;
        
        search_index++;
        main_run_char(' ', ATTR_MSG_SYS);
        main_run_u16(search_index, ATTR_MSG_SYS, 0);
        main_run(". ", ATTR_MSG_SYS, 0);
        main_run(chan, ATTR_MSG_NICK, 0);
        
        main_run(" (", ATTR_MSG_CHAN, 0);
        main_run(users, ATTR_MSG_CHAN, 0);
        main_run(")", ATTR_MSG_CHAN, 0);
        
        if (pkt_txt && *pkt_txt) {
             main_run(" ", ATTR_MSG_CHAN, 0);
             uint8_t len = 0;
             const char *t = pkt_txt;
             while (*t && len < 20) { main_run_char(*t++, ATTR_MSG_CHAN); len++; }
             if (*t) main_run("...", ATTR_MSG_CHAN, 0);
        }
        main_newline();
        pagination_count++;
        return;
    }

    if (search_mode == SEARCH_USER) { // 352
        const char *user = irc_param(2);
        const char *host = irc_param(3);
        const char *nick = irc_param(5);
        
        if (!nick[0]) return;
        
        if (search_pattern[0]) {
            if (!strstr(nick, search_pattern) && !strstr(user, search_pattern)) return;
        }

        search_index++;
        main_run_char(' ', ATTR_MSG_SYS);
        main_run_u16(search_index, ATTR_MSG_SYS, 0);
        main_run(". ", ATTR_MSG_SYS, 0);
        main_run(nick, ATTR_MSG_NICK, 0); 
        
        main_run(" [", ATTR_MSG_CHAN, 0);
        main_run(user, ATTR_MSG_CHAN, 0);
        main_run("@", ATTR_MSG_CHAN, 0);
        main_run(host, ATTR_MSG_CHAN, 0);
        main_run("]", ATTR_MSG_CHAN, 0);
        
        main_newline();
        pagination_count++;
        return;
    }
}

static void h_numeric_1(void)
{
    connection_state = STATE_IRC_READY;
    cursor_visible = 1;
    draw_status_bar();
    pending_search_try_start();
}

static void h_numeric_5(void)
{
    char *net = strstr(pkt_par, "NETWORK=");
    if (net) {
        char *end;
        uint8_t len;
        net += 8;
        end = net;
        while (*end && *end != ' ') end++;
        len = (uint8_t)(end - net);
        if (len > sizeof(network_name) - 1) len = sizeof(network_name) - 1;
        memcpy(network_name, net, len);
        network_name[len] = '\0';
        draw_status_bar();
    }
}

static void h_numeric_default(void)
{
    // 1. Re-parseamos el número para saber qué estamos manejando
    uint16_t num = str_to_u16(pkt_cmd);

    // Filtrar ruido de conexión y canal (silencioso)
    // 001-005: welcome/server info, 250-266: stats, 329: channel creation time,
    // 333: topic who/time, 396: host hidden
    if ((num >= 1 && num <= 5) || (num >= 250 && num <= 266) || 
        num == 329 || num == 333 || num == 396) {
        return;
    }
    // =========================================================================

    // 2. Gestión de Errores (400-599)
    if (num >= 400 && num <= 599) {
        // Si estábamos paginando o buscando, cancelamos para ver el error
        if (pagination_active || search_mode != SEARCH_NONE) cancel_search_state(0);
        
        current_attr = ATTR_ERROR;
        main_puts("Err "); main_puts(pkt_cmd); main_puts(": ");
        
        // Imprimir parámetros de error (ej: el nick que ha fallado)
        uint8_t i;
        for (i = 1; i < irc_param_count; i++) {
            const char *p = irc_param(i);
            if (p && *p) { main_puts(p); main_putc(' '); }
        }
        
        if (*pkt_txt) main_print(pkt_txt); else main_newline();
        return;
    }

    // 3. Mensajes Informativos Genéricos (Para /raw version, time, etc.)
    
    // Usar color de MOTD si corresponde, si no, color estándar de servidor
    if (num == 372 || num == 375 || num == 376) current_attr = ATTR_MSG_MOTD;
    else current_attr = ATTR_MSG_SERVER;

    // Imprimir parámetros intermedios (saltando el 0 que es nuestro nick)
    // Esto permite ver la respuesta de comandos como /raw time o /raw version
    if (irc_param_count > 1) {
        uint8_t i;
        for (i = 1; i < irc_param_count; i++) {
            const char *p = irc_param(i);
            // PROTECCIÓN CRÍTICA: Verificar puntero antes de imprimir para evitar cuelgues
            if (p && *p) {
                main_puts(p);
                main_putc(' ');
            }
        }
    }
    
    // Imprimir texto final si existe
    if (pkt_txt && *pkt_txt) {
        main_print(pkt_txt);
    } else {
        // Solo saltamos línea si hemos impreso algo (params) pero no hay texto final
        main_newline();
    }
}

static void h_default_cmd(void)
{
    // Ignore numerics and single-char commands
    if ((pkt_cmd[0] >= '0' && pkt_cmd[0] <= '9') || !pkt_cmd[1]) return;
    
    // Ignore corrupted/fragmented lines: IRC commands are ALL UPPERCASE
    // If any lowercase letter exists, it's likely a fragment (e.g., "PublicWiFi", "spectalk")
    const char *p = pkt_cmd;
    while (*p && *p != ' ') {
        if (*p >= 'a' && *p <= 'z') return;
        p++;
    }
    
    current_attr = ATTR_MSG_SYS;
    main_puts(">< "); main_puts(pkt_cmd);
    if (*pkt_par) { main_putc(' '); main_puts(pkt_par); }
    if (*pkt_txt) { main_puts(" :"); main_puts(pkt_txt); }
    main_newline();
}

// =============================================================================
// DISPATCH TABLES
// =============================================================================

typedef struct {
    const char *cmd;
    void (*fn)(void);
} CmdEntry;

typedef struct {
    uint16_t num;
    void (*fn)(void);
} NumEntry;

static const CmdEntry CMD_TABLE[] = {
    { "PRIVMSG", h_privmsg_notice },
    { "NOTICE",  h_privmsg_notice },
    { "PING",    h_ping },
    { "JOIN",    h_join },
    { "PART",    h_part },
    { "NICK",    h_nick },
    { "QUIT",    h_quit },
    { "KICK",    h_kick },
    { "MODE",    h_mode },
    { "ERROR",   h_error },
    { "KILL",    h_kill },
    { "CAP",     h_cap },
    { "PONG",    h_ignore },
    { NULL, NULL }
};

static const NumEntry NUM_TABLE[] = {
    { 322, h_numeric_322_352 },
    { 352, h_numeric_322_352 },
    { 323, h_end_of_list },
    { 315, h_end_of_list },
    { 353, h_numeric_353 },
    { 366, h_numeric_366 },
    { 1,   h_numeric_1 },
    { 2,   h_ignore },
    { 3,   h_ignore },
    { 4,   h_ignore },
    { 5,   h_numeric_5 },
    { 305, h_numeric_305_306 },
    { 306, h_numeric_305_306 },
    { 321, h_numeric_321 },
    { 332, h_numeric_332 },
    { 401, h_numeric_401 },
    { 451, h_numeric_451 },
    { 0,   NULL }
};

// ============================================================
// MAIN PARSING FUNCTIONS
// ============================================================

void parse_irc_message(char *line) __z88dk_fastcall
{
    // Populate Globals directly (always initialize to safe values)
    pkt_usr = irc_server;
    pkt_par = pkt_empty;
    pkt_txt = pkt_empty;
    pkt_cmd = pkt_empty;
    irc_param_count = 0;
    
    if (!line || !*line) return;

    if (line[0] == '@') {
        line = strchr(line, ' ');
        if (!line) return;
        line++;
        while (*line == ' ') line++;
        if (!*line) return;
    }

    char *cmd_start;
    if (line[0] == ':') {
        pkt_usr = line + 1;
        cmd_start = skip_to(pkt_usr, ' ');
        if (*cmd_start == 0) return;
        char *bang = strchr(pkt_usr, '!');
        if (bang) *bang = '\0';
    } else {
        cmd_start = line;
    }
    pkt_cmd = cmd_start;

    // Drain filter - ignora resultados de búsquedas/listings cancelados
    if (search_draining && pkt_cmd[0] >= '0' && pkt_cmd[0] <= '9') {
        uint16_t num = str_to_u16(pkt_cmd);
        if (num == 322 || num == 352 || num == 353) return;  // LIST, WHO, NAMES results
        if (num == 323 || num == 315 || num == 366) { search_draining = 0; return; }  // End markers
    }

    pkt_par = skip_to(cmd_start, ' ');
    pkt_txt = pkt_par;
    while (*pkt_txt == ' ') pkt_txt++;

    // Trailing logic
    char *p = pkt_txt;
    while (*p) {
        if (p[0] == ':' && (p == pkt_txt || p[-1] == ' ')) {
            if (p > pkt_txt && p[-1] == ' ') p[-1] = 0;
            *p++ = 0;
            pkt_txt = p;
            break;
        }
        p++;
    }

    // Tokenize params (modifies pkt_par in place)
    tokenize_params(pkt_par, 0);

    // --- DISPATCHER ---
    // 1. Numerics
    if (pkt_cmd[0] >= '0' && pkt_cmd[0] <= '9') {
        uint16_t num = str_to_u16(pkt_cmd);
        const NumEntry *n = NUM_TABLE;
        while (n->num) {
            if (n->num == num) { n->fn(); return; }
            n++;
        }
        h_numeric_default();
        return;
    }

    // 2. Commands (Linear Search)
    const CmdEntry *c = CMD_TABLE;
    while (c->cmd) {
        if (strcmp(c->cmd, pkt_cmd) == 0) {
            c->fn();
            return;
        }
        c++;
    }

    h_default_cmd();
}

void process_irc_data(void)
{
    int16_t c;
    uint16_t bytes_this_call = 0;
    uint8_t lines_this_call = 0;
    uint8_t max_lines;

    if (connection_state == STATE_WIFI_OK && sntp_waiting) {
        uart_drain_to_buffer();
        while ((c = rb_pop()) != -1) {
            uint8_t b = (uint8_t)c;
            if (b == '\r') continue;
            if (b == '\n') {
                if (!rx_overflow && rx_pos > 0) {
                    rx_line[rx_pos] = '\0';
                    if (rx_line[0] == '+') sntp_process_response(rx_line);
                }
                rx_pos = 0; rx_overflow = 0;
                continue;
            }
            if (!rx_overflow) {
                if (rx_pos < (sizeof(rx_line) - 1)) rx_line[rx_pos++] = (char)b;
                else rx_overflow = 1;
            }
        }
        return;
    }

    if (connection_state < STATE_TCP_CONNECTED) return;

    {
        uint16_t backlog_pre = (uint16_t)(rb_head - rb_tail) & RING_BUFFER_MASK;
        if (backlog_pre > 1024)      uart_drain_limit = RX_TICK_DRAIN_MAX;
        else if (backlog_pre > 512)  uart_drain_limit = DRAIN_FAST;
        else if (backlog_pre > 256)  uart_drain_limit = 128;
        else                         uart_drain_limit = DRAIN_NORMAL;
    }

    uart_drain_to_buffer();

    {
        uint16_t backlog = (uint16_t)(rb_head - rb_tail) & RING_BUFFER_MASK;
        if (backlog > 1024)     max_lines = 32;
        else if (backlog > 512) max_lines = 24;
        else if (backlog > 256) max_lines = 16;
        else if (backlog > 128) max_lines = 10;
        else                    max_lines = 6;
    }

    while ((c = rb_pop()) != -1) {
        uint8_t b = (uint8_t)c;
        bytes_this_call++;
        if (bytes_this_call >= RX_TICK_PARSE_BYTE_BUDGET) return;

        if (b == '\r') continue;

        if (b == '\n') {
            if (!rx_overflow && rx_pos > 0) {
                rx_line[rx_pos] = '\0';
                if (strcmp(rx_line, S_CLOSED) == 0) {
                    if (!closed_reported) {
                        closed_reported = 1;
                        ui_err("Connection closed by server");
                        connection_state = STATE_WIFI_OK;
                        cursor_visible = 1;
                        reset_all_channels();
                        draw_status_bar();
                    }
                } else {
                    server_silence_frames = 0;
                    keepalive_ping_sent = 0;
                    parse_irc_message(rx_line);
                    lines_this_call++;
                }
            }
            rx_pos = 0; rx_overflow = 0;
            if (lines_this_call >= max_lines) return;
            continue;
        }

        if (!rx_overflow) {
            if (rx_pos < (sizeof(rx_line) - 1)) rx_line[rx_pos++] = (char)b;
            else rx_overflow = 1;
        }
    }
}