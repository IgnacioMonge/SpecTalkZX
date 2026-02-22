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

// Extern for latency indicator (defined in spectalk.c)
extern uint8_t ping_latency;

// =============================================================================
// GLOBAL PARSING CONTEXT (Replaces Stack Args)
// Optimization: Saves pushing 8 bytes to stack for every handler call.
// Handlers access these directly via fast memory loads.
// =============================================================================
static char *pkt_usr;
static char *pkt_par;
static char *pkt_txt;
static char *pkt_cmd;

// OPT L7: Cache cmd_id para evitar re-parseo en handlers
static uint16_t last_cmd_id;

// Empty string sentinel to keep parser globals valid even on malformed lines.
static char pkt_empty[] = "";

// =============================================================================

// Forward decl (used by selective numeric handlers)
static void h_numeric_default(void);
// INTERNAL HELPERS
// =============================================================================

// Helper: Marcar actividad en canales no activos (Deduplicated logic)
static void mark_channel_activity(uint8_t idx) __z88dk_fastcall

{
    if ((uint8_t)idx != current_channel_idx) {
        channels[idx].flags |= CH_FLAG_UNREAD;
        other_channel_activity = 1;
        status_bar_dirty = 1;
    }
}

// Helper: Mostrar razón entre paréntesis si existe y forzar salto
static void print_reason_and_newline(void)
{
    if (pkt_txt && *pkt_txt) { main_puts2(S_SP_PAREN, pkt_txt); main_putc(')'); }
    wrap_indent = 0;      // FIX: do not carry timestamp indent to next message
    main_newline();
}

// Helper: comprobar si un nick está en la lista de amigos (friend1..friend3)
static uint8_t is_tracked_friend(const char *nick) __z88dk_fastcall
{
    uint8_t i;
    if (!nick || !*nick) return 0;

    for (i = 0; i < 3; i++) {
        if (friend_nicks[i][0] && st_stricmp(friend_nicks[i], nick) == 0) return 1;
    }
    return 0;
}

// Helper: Decrementar user_count de un canal si > 0
#define channel_dec_users(i) do { if (channels[i].user_count > 0) channels[i].user_count--; } while(0)

// Helper: Respuesta CTCP optimizada
static void send_ctcp_reply(const char *target, const char *tag, const char *data)
{
    uart_send_string(S_NOTICE);
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
        st_copy_n(irc_nick, new_nick, sizeof(irc_nick));
        draw_status_bar();
        main_print_time_prefix();
        current_attr = ATTR_MSG_SYS;
        main_puts("You are now known as "); main_print(irc_nick);
        return;
    }

    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if ((channels[i].flags & (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) == (CH_FLAG_ACTIVE | CH_FLAG_QUERY)) {
            if (st_stricmp(channels[i].name, pkt_usr) == 0) {
                st_copy_n(channels[i].name, new_nick, sizeof(channels[0].name));
                
                if (current_channel_idx == i) {
                    main_print_time_prefix();
                    current_attr = ATTR_MSG_SYS;
                    main_puts2("*** ", pkt_usr);
                    main_puts(" is now known as ");
                    main_print(new_nick);
                }
                draw_status_bar();
            }
        }
    }
}

static void h_cap(void)
{
    // pkt_par contains: "* LS ..." or "* ACK ..."
    // Check for "* L" pattern (covers "* LS" and "* LS ...")
    if (pkt_par && pkt_par[0] == '*' && pkt_par[1] == ' ' && pkt_par[2] == 'L') {
        uart_send_line(S_CAP_END);
    }
}

static void h_ping(void)
{
    const char *token = pkt_txt;
    if (!token || !*token) token = irc_param(0);
    if (!token) token = "";

    uart_send_string(S_PONG);
    uart_send_string(irc_server);
    uart_send_string(" :");
    uart_send_line(token);
}

static void h_mode(void)
{
    const char *target = irc_param(0);
    const char *mode_text = pkt_txt;
    if (!target || !*target) target = pkt_par;

    if (!target || !*target) return;

    // --- MODE de usuario ---
    if (irc_nick[0] && st_stricmp(target, irc_nick) == 0) {
        const char *modes = pkt_txt;

        // FIX: algunos servidores envían MODE nick +i sin trailing ':' (modo en param 1)
        if (!modes || !*modes) modes = irc_param(1);

        if (modes && *modes) {
            if (*modes == ':') modes++;
            st_copy_n(user_mode, modes, sizeof(user_mode));
            draw_status_bar();
        }
        return;
    }

    // --- MODE de canal ---
    if (target[0] == '#') {
        int8_t idx = find_channel(target);
        if (idx >= 0) {
            const char *modes = irc_param(1);
            if (modes && *modes) {
                st_copy_n(channels[idx].mode, modes, sizeof(channels[0].mode));
                mode_text = modes;
            }
        }

        if (!mode_text || !*mode_text) mode_text = "(unknown)";

        current_attr = ATTR_MSG_SYS;
        main_puts2(pkt_usr, " sets mode ");
        main_puts2(mode_text, " on ");
        main_print(target);
        return;
    }

    current_attr = ATTR_MSG_SYS;
    main_puts("Mode ");
    main_print(pkt_par);
}

static void h_privmsg_notice(void)
{
    char *target = pkt_par;
    if (!target || !*target || !pkt_txt || !*pkt_txt) return;

    uint8_t is_notice = (pkt_cmd[0] == 'N');
    uint8_t is_server = is_notice && strchr(pkt_usr, '.') != NULL;

    // Auto-IDENTIFY: detect NickServ registration prompt from any source
    // Some networks send as :NickServ NOTICE, others as :server.name NOTICE
    if (is_notice && nickserv_pass[0] &&
        st_stristr(pkt_txt, "registered") && st_stristr(pkt_txt, "identify")) {
        uart_send_string(S_PRIVMSG);
        uart_send_string(S_NICKSERV);
        uart_send_string(" :IDENTIFY ");
        uart_send_line(nickserv_pass);
        current_attr = ATTR_MSG_SYS;
        main_print("Auto-identifying...");
        return;
    }

    // Durante búsqueda activa, NOTICE del servidor puede ser rate limit
    if (is_server && pagination_active) {
        current_attr = ATTR_ERROR;
        main_print(pkt_txt);
        return;
    }

    // Filtrar mensajes de conexión del servidor (Ident, Looking, Checking, ***)
    if (is_server && (target[0] == '*' || !target[0])) {
        char c = pkt_txt[0];
        // Estos prefijos siempre indican mensajes de conexión del servidor
        if (c == 'I' || c == 'L' || c == 'C' || c == '*') return;
    }

    if (is_ignored(pkt_usr) && !is_server) return;

    // Filtrar NOTICE de servicios de red
    if (is_notice && !is_server) {
        char c = pkt_usr[0] | 0x20;  // lowercase first char
        if (c == 'g' || c == 'n' || c == 'm' || c == 'i') {
            if (st_stricmp(pkt_usr, S_GLOBAL) == 0 ||
                st_stricmp(pkt_usr, S_NICKSERV) == 0 ||
                st_stricmp(pkt_usr, "MemoServ") == 0 ||
                st_stricmp(pkt_usr, "InfoServ") == 0) {
                // Filter verbose auth messages
                if (!pkt_txt[0]) return;
                if (pkt_txt[0] == '*') return;
                if (pkt_txt[0] == 'L' && pkt_txt[1] == 'a') return;
                if (strchr(pkt_txt, '!') && strchr(pkt_txt, '@')) return;

                current_attr = ATTR_MSG_TOPIC;
                main_print(pkt_txt);
                return;
            }
        }
    }

    // Detectar NOTICE de ChanServ con topic
    if (is_notice && st_stricmp(pkt_usr, S_CHANSERV) == 0) {
        if (pkt_txt[0] == '[') {
            char *close_bracket = strchr(pkt_txt, ']');
            if (close_bracket && close_bracket[1] == ' ') {
                main_print_time_prefix();
                current_attr = ATTR_MSG_TOPIC;
                main_puts(S_TOPIC_PFX);
                main_print(close_bracket + 2);
                return;
            }
        }
        current_attr = ATTR_MSG_SERVER;
        main_print(pkt_txt);
        return;
    }

    strip_irc_codes(pkt_txt);

    if (target[0] == '#') {
        int8_t idx = find_channel(target);
        if (idx >= 0) {
            mark_channel_activity((uint8_t)idx);

            // Mención en canal NO activo: marcar flag y salir
            if ((uint8_t)idx != current_channel_idx && irc_nick[0] && st_stristr(pkt_txt, irc_nick)) {
                channels[(uint8_t)idx].flags |= CH_FLAG_MENTION;
                status_bar_dirty = 1;
                mention_beep();                 // <-- AÑADIDO: beep de mención también en no-activo
            }

            if ((uint8_t)idx != current_channel_idx) return;
        }
    }

    // --- CTCP HANDLING ---
    if (pkt_txt[0] == 1) {
        char *ctcp_cmd = pkt_txt + 1;

        switch (ctcp_cmd[0]) {
            case 'A': // ACTION - "ACTION "
                if (ctcp_cmd[1] == 'C' && ctcp_cmd[6] == ' ') {
                    char *act = ctcp_cmd + 7;
                    char *end = strchr(act, 1);
                    if (end) *end = 0;

                    if (target[0] == '#') {
                        current_attr = ATTR_MSG_CHAN;
                        main_puts2(S_ASTERISK, pkt_usr);
                        main_putc(' '); main_print(act);
                    } else {
                        int8_t query_idx = add_query(pkt_usr);
                        if (query_idx >= 0) {
                            status_bar_dirty = 1;
                            mark_channel_activity((uint8_t)query_idx);
                        }
                        current_attr = ATTR_MSG_PRIV;
                        main_puts2(S_ASTERISK, pkt_usr);
                        main_putc(' '); main_print(act);
                    }
                    return;
                }
                break;

            case 'V': // VERSION
                if (ctcp_cmd[1] == 'E' && ctcp_cmd[2] == 'R') { send_ctcp_reply(pkt_usr, "VERSION", "SpecTalkZX"); return; }
                break;

            case 'P': // PING
                if (ctcp_cmd[1] == 'I' && ctcp_cmd[2] == 'N') {
                    const char *p = ctcp_cmd + 4;
                    if (*p == ' ') p++;
                    send_ctcp_reply(pkt_usr, "PING", p);
                    return;
                }
                break;

            case 'T': // TIME
                if (ctcp_cmd[1] == 'I' && ctcp_cmd[2] == 'M') { send_ctcp_reply(pkt_usr, "TIME", "(no rtc)"); return; }
                break;
        }
    }

    // --- STANDARD MESSAGE RENDERING ---
    if (pkt_cmd[0] == 'N') {
        current_attr = ATTR_MSG_SERVER;
        main_print(pkt_txt);
        return;
    }

    if (target[0] == '#') {
        current_attr = ATTR_MSG_CHAN;
        main_print_time_prefix();

        current_attr = ATTR_MSG_NICK;
        
        // FUSIÓN SEGURA
        main_puts2("<", pkt_usr);
        main_puts(S_PROMPT);
        
        current_attr = ATTR_MSG_CHAN;

        main_print_wrapped_ram(pkt_txt);

        // Beep para mención en canal activo
        if (irc_nick[0] && st_stristr(pkt_txt, irc_nick)) mention_beep();
    } else {
        int8_t query_idx = find_query(pkt_usr);
        if (query_idx < 0) {
            query_idx = add_query(pkt_usr);
            if (query_idx >= 0) status_bar_dirty = 1;
        }
        if (query_idx >= 0) mark_channel_activity((uint8_t)query_idx);

        main_print_time_prefix();

        if (query_idx < 0 || (uint8_t)query_idx != current_channel_idx) {
            current_attr = ATTR_MSG_SELF;
            
            // FUSIÓN SEGURA
            main_puts2(S_ARROW_IN, pkt_usr);
            main_puts(S_COLON_SP);
            
            current_attr = ATTR_MSG_PRIV;

            main_print_wrapped_ram(pkt_txt);
        } else {
            current_attr = ATTR_MSG_NICK;
            
            // FUSIÓN SEGURA
            main_puts2("<", pkt_usr);
            main_puts(S_PROMPT);
            
            current_attr = ATTR_MSG_PRIV;

            main_print_wrapped_ram(pkt_txt);
        }

        // Privado: usar exactamente el mismo beep que las menciones.
        mention_beep();
        
        // Auto-reply if away with custom message (global cooldown)
        if (irc_is_away && away_message[0] && !away_reply_cd) {
            // Use NOTICE to avoid loops/flood with other clients/bots
            uart_send_string(S_NOTICE);
            uart_send_string(pkt_usr);
            uart_send_string(S_SP_COLON);
            uart_send_line(away_message);
            away_reply_cd = 60;  // seconds
        }
    }
}

static void h_join(void)
{
    char *chan = (*pkt_txt) ? pkt_txt : pkt_par;
    if (*chan == ':') chan++;
    
    if (st_stricmp(pkt_usr, irc_nick) == 0) {
        // NO timestamp for "Now talking in ..."
        wrap_indent = 0;

        current_attr = ATTR_MSG_PRIV;
        main_puts("Now talking in "); main_print(chan);

        int8_t idx = find_channel(chan);
        if (idx < 0) idx = add_channel(chan);
        
        if (idx < 0) { ui_err("Max channels reached"); return; }
        
        if ((uint8_t)idx != current_channel_idx) switch_to_channel((uint8_t)idx);
        
        channels[idx].user_count = 0;
        counting_new_users = 1;
        st_copy_n(names_target_channel, chan, sizeof(names_target_channel));
        names_pending = 1;
        names_timeout_frames = 0;
        draw_status_bar();
    } else {
        int8_t idx = find_channel(chan);
        if (idx >= 0) {
            channels[idx].user_count++;

            if (is_tracked_friend(pkt_usr)) {
                mention_beep();
                main_print_time_prefix();
                current_attr = ATTR_MSG_PRIV;
                main_puts2("Friend alert: ", pkt_usr);
                main_puts(" joined ");
                main_print(chan);
            }

            if ((uint8_t)idx == current_channel_idx) {
                main_print_time_prefix();
                current_attr = ATTR_MSG_JOIN;
                main_puts2(S_ARROW_OUT, pkt_usr);
                main_puts(" joined ");
                main_print(chan);
                draw_status_bar();
            }
        }
    }
}

static void handle_connection_drop(void)
{
    // FIX: Don't trigger disconnect if already disconnecting (e.g., /quit in progress)
    if (disconnecting_in_progress) return;
    force_disconnect_wifi();  // ya resetea canales internamente
    status_bar_dirty = 1;
}

static void h_part(void)
{
    char *chan = (char *)irc_param(0);

    if (!chan || !*chan) chan = pkt_par;
    if (chan && *chan == ':') chan++;

    int8_t idx = find_channel(chan);
    if (idx >= 0) {
        if (st_stricmp(pkt_usr, irc_nick) == 0) {
            remove_channel((uint8_t)idx);
            current_attr = ATTR_MSG_JOIN;
            main_print_time_prefix();
            main_puts(S_YOU_LEFT);
            main_print(chan);
        } else {
            channel_dec_users(idx);

            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_print_time_prefix();
                
                // FIX BUG-07: Era main_puts2(S_ARROW_IN, S_ARROW_IN) -> "<< << "
                main_puts(S_ARROW_IN);
                main_puts2(pkt_usr, " left");
                
                print_reason_and_newline();
                draw_status_bar();
            } else {
                mark_channel_activity((uint8_t)idx);
            }
        }
    }
}

static void h_quit(void)
{
    if (show_quits) {
        main_print_time_prefix();
        current_attr = ATTR_MSG_JOIN;
        
        // FIX BUG-07: Era main_puts2(S_ARROW_IN, S_ARROW_IN) -> "<< << "
        main_puts(S_ARROW_IN);
        main_puts2(pkt_usr, " quit");
        
        print_reason_and_newline();
    }

    {
        int8_t qidx = find_query(pkt_usr);
        if (qidx > 0) {
            mark_channel_activity((uint8_t)qidx);
            if ((uint8_t)qidx == current_channel_idx)
                ui_err("--- User quit ---");
        }
    }

    // Decrementar user_count si solo hay 1 canal
    {
        uint8_t i, joined_cnt = 0, target_idx = 0;
        for (i = 1; i < MAX_CHANNELS && joined_cnt < 2; i++) {
            uint8_t f = channels[i].flags;
            if ((f & CH_FLAG_ACTIVE) && !(f & CH_FLAG_QUERY)) {
                char c = channels[i].name[0];
                if (c == '#' || c == '&') { joined_cnt++; target_idx = i; }
            }
        }
        if (joined_cnt == 1) channel_dec_users(target_idx);
    }
    draw_status_bar();
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
        main_print_time_prefix();
        current_attr = ATTR_ERROR;
        main_puts2("Kicked from ", channel);
        main_puts2(" by ", pkt_usr);
        print_reason_and_newline();
        // remove_channel() YA llama a draw_status_bar()
        if (idx > 0) remove_channel((uint8_t)idx);
    } else {
        if (idx >= 0) {
            if ((uint8_t)idx == current_channel_idx) {
                main_print_time_prefix();
                current_attr = ATTR_MSG_JOIN;
                main_puts2(S_ASTERISK, target);
                main_puts2(" kicked by ", pkt_usr);
                print_reason_and_newline();
            } else {
                mark_channel_activity((uint8_t)idx);
            }
            channel_dec_users(idx);
            if ((uint8_t)idx == current_channel_idx) draw_status_bar();
        }
    }
}



static void h_kill(void)
{
    char *target = pkt_par;

    main_print_time_prefix();
    
    if (target && st_stricmp(target, irc_nick) == 0) {
        current_attr = ATTR_ERROR;
        main_puts2("*** Killed by ", pkt_usr);
        if (pkt_txt && *pkt_txt) {
            main_puts(S_COLON_SP);
            main_print(pkt_txt);
        } else {
            wrap_indent = 0;  
            main_newline();
        }
        handle_connection_drop();
    } else {
        current_attr = ATTR_MSG_JOIN;
        main_puts(S_ASTERISK); if (target) main_puts(target);
        main_puts2(" killed by ", pkt_usr);
    }
}



static void h_error(void)
{
    current_attr = ATTR_ERROR;
    main_puts("*** Server: ");
    if (pkt_txt && *pkt_txt) main_print(pkt_txt); else main_print("Connection terminated");
    handle_connection_drop();
}

static void h_numeric_401(void)
{
    const char *bad_nick = irc_param(1);
    
    current_attr = ATTR_ERROR;
    main_puts2("Error: ", bad_nick);
    main_putc(' ');
    main_print(pkt_txt);

    if (bad_nick && *bad_nick) {
        int8_t idx = find_query(bad_nick);
        if (idx > 0 && (channels[idx].flags & CH_FLAG_QUERY)) {
            // remove_channel() YA llama a draw_status_bar()
            remove_channel((uint8_t)idx);
        }
    }
}

// 433 ERR_NICKNAMEINUSE: Try alternative nick during registration
static void h_numeric_433(void)
{
    // Only auto-retry if not yet registered
    if (connection_state >= STATE_IRC_READY) {
        current_attr = ATTR_ERROR;
        main_print("Nick already in use");
        return;
    }
    
    // Append underscore to nick and retry
    // OPT-P2-B: use shared helper
    nick_try_alternate();
    draw_status_bar();
}

static void h_numeric_451(void)
{
    if (pagination_active || search_mode != SEARCH_NONE) {
        cancel_search_state();
        current_attr = ATTR_ERROR;
        main_print("Search aborted (not registered)");
        return;
    }
    
    current_attr = ATTR_ERROR;
    main_print("*** Session expired");
    handle_connection_drop();
    current_attr = ATTR_MSG_SYS; main_print("Use /server to reconnect");
}

static void h_numeric_305_306(void)
{
    // 305 = RPL_UNAWAY ("You are no longer marked as being away")
    // 306 = RPL_NOWAWAY ("You have been marked as being away")
    // OPT L5: comparar tercer carácter ('5' vs '6') en lugar de str_to_u16
    uint8_t is_306 = (pkt_cmd[2] == '6');
    irc_is_away = is_306;
    
    if (!is_306) {
        // Ya no away (305) - resetear sistema de auto-away
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
        main_puts(S_TOPIC_PFX); main_print(pkt_txt);
    }
}

// Helper: incrementa pagination_count con check de overflow
// Retorna 0 si OK, 1 si overflow (y cancela búsqueda)
static uint8_t pagination_inc(void) {
    if (pagination_count < PAGINATION_MAX_COUNT) {
        pagination_count++;
        return 0;
    }
    cancel_search_state();
    current_attr = ATTR_ERROR;
    main_print("Result limit");
    return 1;
}

static void h_numeric_353(void)
{
    const char *msg_chan = irc_param(2);
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    if (!target[0]) return;
    if (st_stricmp(msg_chan, target) != 0) return;

    names_timeout_frames = 0;
    names_pending = 1;

    // Recuento de usuarios (sin cambiar lógica)
    {
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
    }

    // Render /names
    if (show_names_list) {
        current_attr = ATTR_MSG_NICK;
        main_print_wrapped_ram(pkt_txt);
        
        if (pagination_inc()) return;
        pagination_timeout = 0;
    }
}


static void h_numeric_366(void)
{
    const char *msg_chan = irc_param(1);
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    extern char *u16_to_dec3(char *dst, uint16_t v);
    
    if (target[0] && st_stricmp(msg_chan, target) == 0) {
        names_pending = 0; names_timeout_frames = 0;
        names_target_channel[0] = '\0';
    }
    
    // Finalizar paginación de /names
    if (show_names_list && pagination_active) {
        current_attr = ATTR_MSG_SYS;
        if (pagination_count > 0) {
            char buf[8];
            u16_to_dec3(buf, chan_user_count);
            main_puts2("-- ", buf);
            main_print(search_data_lost ? " (incomplete) --" : " users --");
        }
        pagination_active = 0;
        pagination_count = 0;
    }
    
    show_names_list = 0; 
    draw_status_bar();
}

static void h_numeric_321(void)
{
    if (search_flush_state == 1) return;  // Todavía drenando
    search_header_rcvd = 1;              
    // 321 es solo el header, no hacemos nada visible
    // (ya mostramos "Searching..." al inicio)
}

static void h_end_of_list(void)
{
    if (search_flush_state == 1) return;  // Todavía drenando

    // OPT L6: Verificar segundo carácter ('2' para 323, '1' para 315)
    uint8_t c1 = pkt_cmd[1];
    if (search_mode == SEARCH_CHAN && c1 != '2') return;  // 323
    if (search_mode == SEARCH_USER && c1 != '1') return;  // 315
    if (search_mode == SEARCH_NONE) return;

    uint8_t data_lost = search_data_lost;
    
    current_attr = ATTR_MSG_SYS;
    if (pagination_count > 0) {
        main_print(data_lost ? "-- Done (incomplete) --" : "-- Done --");
    } else if (search_header_rcvd == 1 || search_mode == SEARCH_USER) {
        main_print("-- No matches --");
    } else {
        current_attr = ATTR_ERROR;
        main_print("Search denied");
    }
    
    cancel_search_state();
}

// D9: Shared preamble for search result index rendering
static void search_render_index(void) {
    search_index++;
    current_attr = ATTR_MSG_SYS;
    main_putc(' ');
    main_run_u16(search_index, ATTR_MSG_SYS);
    current_attr = ATTR_MSG_SYS;
    main_puts(S_DOT_SP);
    current_attr = ATTR_MSG_NICK;
}

static void h_numeric_322_352(void)
{
    const char *chan, *users, *nick, *user, *host, *t;
    uint8_t len, nick_idx, host_idx;

    if (!pagination_active) return;
    if (search_mode == SEARCH_NONE) return;
    if (search_flush_state == 1) return;

    search_header_rcvd = 2;
    pagination_timeout = 0;

    if (search_mode == SEARCH_CHAN) { // 322
        chan = irc_param(1);
        users = irc_param(2);

        if (!chan[0]) return;
        if (search_pattern[0] && !st_stristr(chan, search_pattern)) return;

        search_render_index();
        main_puts(chan);

        // Align attribute cell boundary (64-col: 2 chars per attribute cell)
        // If channel length is odd, add one padding space so following " (" starts on next cell.
        len = st_strlen(chan);
        if (len & 1) { main_putc(' '); }

        current_attr = ATTR_MSG_CHAN;
        main_puts2(" (", users);
        main_putc(')');

        if (pkt_txt && *pkt_txt) {
            current_attr = ATTR_MSG_CHAN;
            main_putc(' ');
            len = 0;
            t = pkt_txt;
            while (*t && len < 20) { main_putc(*t++); len++; }
            if (*t) main_puts(S_DOTS3);
        }

        main_newline();
        pagination_inc();
        return;
    }

    if (search_mode == SEARCH_USER) { // 352
        // FIX: Parsing agnóstico - nick/host desde el final para compatibilidad
        nick_idx = (irc_param_count > 2) ? irc_param_count - 2 : 5;
        host_idx = (irc_param_count > 4) ? irc_param_count - 4 : 3;

        user = irc_param(2);
        host = irc_param(host_idx);
        nick = irc_param(nick_idx);

        if (!nick || !nick[0]) nick = "?";
        if (!host || !host[0]) host = "?";

        if (search_pattern[0]) {
            if (!st_stristr(nick, search_pattern) && !st_stristr(user, search_pattern)) return;
        }

        search_render_index();
        main_puts(nick);

        current_attr = ATTR_MSG_CHAN;
        main_puts2(" [", user);
        main_putc('@');
        main_puts2(host, "]");

        main_newline();
        pagination_inc();
        return;
    }
}


static void h_numeric_1(void)
{
    const char *confirmed_nick = irc_param(0);
    if (confirmed_nick && *confirmed_nick) {
        st_copy_n(irc_nick, confirmed_nick, sizeof(irc_nick));
    }
    connection_state = STATE_IRC_READY;
    cursor_visible = 1;
    draw_status_bar();
}

// End of MOTD: check friends online
static void h_numeric_376(void) { irc_check_friends_online(); }
static void h_numeric_422(void) { irc_check_friends_online(); }

// RPL_ISON (303): show friends online (simple version)
static void h_numeric_303(void)
{
    const char *p = pkt_txt;
    if (!p || !*p) return;
    if (*p == ':') p++;
    if (!*p) return;  // No friends online
    
    current_attr = ATTR_MSG_PRIV;
    main_puts("Friends online: ");
    main_print(p);
}

static void h_numeric_5(void)
{
    // Busca "NETWORK=" en pkt_par — safe scan (no read past '\0')
    const char *p = pkt_par;
    const char *net = NULL;
    while (*p) {
        if (*p == 'N') {
            // Short-circuit chain: stops at first '\0' hit
            const char *q = p + 1;
            if (*q++ == 'E' && *q++ == 'T' && *q++ == 'W' &&
                *q++ == 'O' && *q++ == 'R' && *q++ == 'K' && *q == '=') {
                net = q + 1;
                break;
            }
        }
        p++;
    }
    if (net) {
        const char *end = net;
        uint8_t len;
        while (*end && *end != ' ') end++;
        len = (uint8_t)(end - net);
        if (len > sizeof(network_name) - 1) len = sizeof(network_name) - 1;
        memcpy(network_name, net, len);
        network_name[len] = '\0';
        draw_status_bar();
    }
}

static void h_join_error(void)
{
    // Param 0: Nick, Param 1: Canal
    const char *bad_chan = irc_param(1);
    
    current_attr = ATTR_ERROR;
    main_puts("Cannot join "); 
    if (bad_chan && *bad_chan) main_print(bad_chan);
    else main_print("channel");
    
    main_puts(S_COLON_SP);
    if (pkt_txt && *pkt_txt) main_print(pkt_txt); 
    else main_print("Access denied");

    // SEGURIDAD: Si la ventana existe (estado zombie), forzar su cierre inmediato.
    // Esto corrige el bug visual de la barra de estado.
    if (bad_chan && *bad_chan) {
        int8_t idx = find_channel(bad_chan);
        if (idx > 0) {
            remove_channel((uint8_t)idx);
            // Si estábamos en esa ventana, remove_channel nos mueve a otra
            // y redibuja la barra de estado automáticamente.
            if ((uint8_t)idx == current_channel_idx) draw_status_bar();
        }
    }
}

static void h_numeric_default(void)
{
    // OPT L7: usar last_cmd_id en lugar de re-parsear
    uint16_t num = last_cmd_id;

    // Filtrar ruido de conexión y canal (silencioso)
    // 001-005: welcome/server info, 250-266: stats, 329: channel creation time,
    // 333: topic who/time, 396: host hidden
    if ((num >= 1 && num <= 5) || (num >= 250 && num <= 266) ||
        num == 329 || num == 333 || num == 396) {
        return;
    }

    // 2. Gestión de Errores (400-599)
    if (num >= 400 && num <= 599) {
        // Si estábamos paginando o buscando, cancelamos para ver el error
        if (pagination_active || search_mode != SEARCH_NONE) cancel_search_state();

        current_attr = ATTR_ERROR;
        main_puts2("Err ", pkt_cmd); main_puts(S_COLON_SP);

        goto print_tail;
    }

    // 3. Mensajes Informativos Genéricos (Para /raw version, time, etc.)
    if (num == 372 || num == 375 || num == 376) current_attr = ATTR_MSG_MOTD;
    else current_attr = ATTR_MSG_SERVER;

print_tail:
    // Imprimir parámetros intermedios (saltando el 0 que es nuestro nick)
    if (irc_param_count > 1) {
        uint8_t i;
        for (i = 1; i < irc_param_count; i++) {
            const char *p = irc_param(i);
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

    // FIX: Suprimir output de comandos no reconocidos durante búsqueda activa
    if (pagination_active && search_mode != SEARCH_NONE) return;
    
    current_attr = ATTR_MSG_SYS;
    main_puts2(">< ", pkt_cmd);
    if (*pkt_par) { main_putc(' '); main_puts(pkt_par); }
    if (*pkt_txt) { main_puts2(S_SP_COLON, pkt_txt); }
    main_newline();
}

// =============================================================================
// DISPATCH TABLES
// =============================================================================

typedef struct {
    uint16_t id;
    void (*fn)(void);
} CmdEntry;

static void h_ignore(void) { /* Do nothing */ }

static void h_pong(void)
{
    // Calculate latency from keepalive_timeout (frames since PING)
    // 50 frames = 1 second
    if (keepalive_timeout < 25) {
        ping_latency = 0;       // Good: < 500ms
    } else if (keepalive_timeout < 50) {
        ping_latency = 1;       // Medium: 500-1000ms
    } else {
        ping_latency = 2;       // High: > 1000ms
    }
    status_bar_dirty = 1;       // Redraw indicator
    
    // FIX ChatGPT audit: Borrar keepalive_ping_sent SOLO con PONG
    keepalive_ping_sent = 0;
    keepalive_timeout = 0;
}

// E2: Dispatcher por tercer carácter del comando
// Hash 'KI' (0x4B49) cubre KICK y KILL, distinguimos por pkt_cmd[2]: 'C'=KICK, 'L'=KILL
static void h_kick_kill(void)
{
    if (pkt_cmd[2] == 'C') h_kick();
    else if (pkt_cmd[2] == 'L') h_kill();
}

static const CmdEntry CMD_TABLE[] = {
    // PERF-01: Hot path primero (>80% del tráfico IRC)
    { 0x5052, h_privmsg_notice }, // PR (PRIVMSG) - más frecuente
    { 0x4E4F, h_privmsg_notice }, // NO (NOTICE)  - segundo más frecuente
    { 0x5049, h_ping },           // PI (PING)    - keepalive frecuente

    // Comandos Numéricos (0x0001 - 0x03E7)
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
    { 900, h_ignore },           // RPL_LOGGEDIN (after NickServ IDENTIFY)
    { 5,   h_numeric_5 },
    { 303, h_numeric_303 },
    { 305, h_numeric_305_306 },
    { 306, h_numeric_305_306 },
    { 321, h_numeric_321 },
    { 332, h_numeric_332 },
    { 376, h_numeric_376 },
    { 401, h_numeric_401 },
    { 433, h_numeric_433 },
    { 422, h_numeric_422 },
    { 451, h_numeric_451 },
    { 403, h_join_error },
    { 404, h_join_error },
    { 405, h_join_error },
    { 471, h_join_error },
    { 473, h_join_error },
    { 474, h_join_error },

    // Resto de comandos de texto (menos frecuentes)
    { 0x504F, h_pong },           // PO (PONG)
    { 0x5041, h_part },           // PA (PART)
    { 0x4E49, h_nick },           // NI (NICK)
    { 0x4A4F, h_join },           // JO (JOIN)
    { 0x5155, h_quit },           // QU (QUIT)
    { 0x4B49, h_kick_kill },      // KI (KICK/KILL)
    { 0x4D4F, h_mode },           // MO (MODE)
    { 0x4552, h_error },          // ER (ERROR)
    { 0x4341, h_cap },            // CA (CAP)

    { 0,   NULL }
};

// =============================================================================
// MAIN PARSING FUNCTIONS
// =============================================================================

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

    // --- UNIFIED DISPATCHER ---
    {
        uint16_t cmd_id;
        if (pkt_cmd[0] >= '0' && pkt_cmd[0] <= '9') {
            cmd_id = str_to_u16(pkt_cmd);
        } else if (pkt_cmd[0] && pkt_cmd[1]) {
            // FIX: Normalizar a mayúsculas para compatibilidad con bouncers/gateways
            // que envían comandos en minúsculas (ej: "privmsg" en vez de "PRIVMSG")
            uint8_t c0 = pkt_cmd[0];
            uint8_t c1 = pkt_cmd[1];
            if (c0 >= 'a' && c0 <= 'z') c0 -= 32;
            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
            cmd_id = ((uint16_t)c0 << 8) | c1;
        } else {
            h_default_cmd();
            return;
        }
        
        last_cmd_id = cmd_id;  // OPT L7: guardar para handlers

        {
            const CmdEntry *n = CMD_TABLE;
            while (n->id) {
                if (n->id == cmd_id) { n->fn(); return; }
                n++;
            }
        }

        if (cmd_id < 1000) h_numeric_default();
        else h_default_cmd();
    }
}


void process_irc_data(void)
{
    uint16_t bytes_this_call = 0;
    uint8_t lines_this_call = 0;
    uint8_t max_lines;
    static uint8_t idle_skip = 0;  // OPT: Throttle polling cuando idle

    uint16_t backlog;

    if (connection_state == STATE_WIFI_OK && sntp_waiting) {
        uart_drain_to_buffer();
        while (try_read_line_nodrain()) {
            if (rx_line[0] == '+') sntp_process_response(rx_line);
        }
        return;
    }

    if (connection_state < STATE_TCP_CONNECTED) return;

    // OPT: Skip drain cada 2 frames cuando buffer está vacío (ahorro CPU en idle)
    backlog = (uint16_t)(rb_head - rb_tail) & RING_BUFFER_MASK;
    if (backlog < 64 && rx_pos == 0) {  // Buffer casi vacío
        idle_skip++;
        if (idle_skip < 2) return;  // Skip 1 de cada 2 frames cuando idle
        idle_skip = 0;
    } else {
        idle_skip = 0;  // Reset cuando hay datos
    }

    // Fusión total: un solo paso fija drain_limit y max_lines
    uart_drain_limit = DRAIN_NORMAL;
    if (backlog > 1024)      { uart_drain_limit = RX_TICK_DRAIN_MAX; max_lines = 32; }
    else if (backlog > 512)  { uart_drain_limit = DRAIN_FAST;        max_lines = 24; }
    else if (backlog > 256)  { uart_drain_limit = 128;               max_lines = 16; }
    else if (backlog > 128)  {                                        max_lines = 10; }
    else                     {                                        max_lines = 6;  }

    uart_drain_to_buffer();

    // FIX: early-out barato cuando no hay nada que procesar
    if (rx_pos == 0 && rb_head == rb_tail) return;

    while (try_read_line_nodrain()) {

        if (pagination_active) pagination_timeout = 0;

        // FIX: Skip all message processing if disconnect is in progress
        // This prevents ERROR/CLOSED from server triggering reentrant handlers
        if (disconnecting_in_progress) continue;

        // FIX: Match completo "CLOSED" para evitar falsos positivos
        // (ej: mensaje de chat que empiece por "CLO...")
        if (rx_line[0] == 'C' && rx_line[1] == 'L' && rx_line[2] == 'O' &&
            rx_line[3] == 'S' && rx_line[4] == 'E' && rx_line[5] == 'D') {
            if (!closed_reported) {
                closed_reported = 1;
                ui_err("Connection closed by server");
                force_disconnect_wifi();
                draw_status_bar();
            }
        } else {
            // Reset silence counter on ANY server activity
            server_silence_frames = 0;
            // FIX ChatGPT audit: NO borrar keepalive_ping_sent aquí
            // Solo debe borrarse al recibir PONG (se hace en handler de PONG)
            parse_irc_message(rx_line);
        }

        lines_this_call++;

        {
            uint16_t current_budget = RX_TICK_PARSE_BYTE_BUDGET;
            if (pagination_active) {
                current_budget = RX_TICK_PARSE_BYTE_BUDGET * 8;
            }

            bytes_this_call += (rx_last_len + 1);
            if (bytes_this_call >= current_budget) return;
        }

        if (lines_this_call >= max_lines) return;
    }
}
