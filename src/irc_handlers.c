/*
 * irc_handlers.c - IRC message parsing and handling
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

// ============================================================
// IRC DISPATCH (Phase 2): table-driven command/numeric handlers
// ============================================================

typedef void (*irc_cmd_handler_t)(const char *usr, char *par, char *txtp, const char *cmd);
typedef void (*irc_num_handler_t)(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd);

static void irc_handle_nick(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)cmd; (void)txtp;
    // Formato: :OldNick NICK :NewNick
    // usr = OldNick
    // par = NewNick (o txtp dependiendo del servidor, par suele tenerlo)
    
    char *new_nick = (*txtp) ? txtp : par;
    if (*new_nick == ':') new_nick++; // Saltar dos puntos si existen

    // 1. Si SOY YO el que cambia de nick
    if (st_stricmp(usr, irc_nick) == 0) {
        strncpy(irc_nick, new_nick, sizeof(irc_nick) - 1);
        irc_nick[sizeof(irc_nick) - 1] = 0;
        draw_status_bar();
        current_attr = ATTR_MSG_SYS;
        main_puts("You are now known as "); main_print(irc_nick);
        return;
    }

    // 2. Si es OTRO user -> Buscar si tengo un privado (Query) abierto con él
    // Recorremos todos los channeles buscando queries con el nombre antiguo
    uint8_t i;
    for (i = 1; i < MAX_CHANNELS; i++) {
        if (channels[i].active && channels[i].is_query) {
            if (st_stricmp(channels[i].name, usr) == 0) {
                // ¡ENCONTRADO! Renombrar ventana
                strncpy(channels[i].name, new_nick, sizeof(channels[i].name) - 1);
                channels[i].name[sizeof(channels[i].name) - 1] = 0;
                
                // Avisar dentro de esa ventana
                // Guardamos contexto actual
                uint8_t saved_idx = current_channel_idx; 
                uint8_t saved_attr = current_attr;
                
                // Imprimimos el aviso en el buffer de esa ventana (truco visual)
                // (Nota: Esto imprime en screen si es la actual, si no, solo cambia el nombre)
                if (current_channel_idx == i) {
                    current_attr = ATTR_MSG_SYS;
                    main_puts("*** "); main_puts(usr); 
                    main_puts(" is now known as "); main_print(new_nick);
                    current_attr = saved_attr;
                }
                
                draw_status_bar(); // Para reflejar el cambio de nombre en la barra
            }
        }
    }
    
    // Opcional: Imprimir el cambio si estamos en un channel común (más complejo de trackear, lo dejamos simple)
}


static void irc_handle_ignore(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr; (void)par; (void)txtp; (void)cmd;
}

// IRCv3 CAP handler: respond with CAP END to skip capability negotiation
static void irc_handle_cap(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr; (void)txtp; (void)cmd;
    
    // par contains: "* LS ..." or "* ACK ..." or similar
    // We only care about LS (server offering capabilities)
    if (par && (strstr(par, " LS") || strncmp(par, "* LS", 4) == 0)) {
        uart_send_string("CAP END\r\n");
    }
}

static void irc_handle_ping(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr; (void)cmd;
    
    // Envío directo optimizado manteniendo lógica de parsing
    uart_send_string("PONG ");
    if (*txtp) {
        uart_send_string(":");
        uart_send_string(txtp);
    } else {
        if (*par == ':') par++;
        if (*par) {
            uart_send_string(":");
            uart_send_string(par);
        }
    }
    uart_send_string("\r\n");
}

static void irc_handle_mode(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)cmd;
    
    // Extraer el target (primer parámetro)
    const char *target = irc_param(0);
    if (!target || !*target) target = par;
    
    // Si el MODE es sobre mi nick, silenciar pero guardar los modos
    if (irc_nick[0] && st_stricmp(target, irc_nick) == 0) {
        // Guardar user modes
        const char *modes = irc_param(1);
        if (!modes || !*modes) modes = txtp;
        if (modes && *modes) {
            if (*modes == ':') modes++;
            strncpy(user_mode, modes, sizeof(user_mode) - 1);
            user_mode[sizeof(user_mode) - 1] = '\0';
            draw_status_bar();
        }
        return;  // NO mostrar
    }
    
    // Si el MODE es sobre un channel
    if (target[0] == '#') {
        // Guardar channel modes si es nuestro channel actual
        int8_t idx = find_channel(target);
        if (idx >= 0) {
            const char *modes = irc_param(1);
            if (!modes || !*modes) modes = txtp;
            if (modes && *modes) {
                strncpy(channels[idx].mode, modes, sizeof(channels[idx].mode) - 1);
                channels[idx].mode[sizeof(channels[idx].mode) - 1] = '\0';
            }
        }
        // Mostrar cambio de modo en channel
        current_attr = ATTR_MSG_SYS;
        main_puts(usr);
        main_puts(" sets mode ");
        main_puts(txtp);
        main_puts(" on ");
        main_print(target);
        return;
    }
    
    // Otros MODEs (poco común) - mostrar genérico
    current_attr = ATTR_MSG_SYS;
    main_puts("Mode "); 
    main_print(par);
}


static void send_ctcp_reply(const char *target, const char *tag, const char *data)
{
    uart_send_string("NOTICE ");
    uart_send_string(target);
    uart_send_string(" :\x01");
    uart_send_string(tag);
    if (data && *data) {
        uart_send_string(" ");
        uart_send_string(data);
    }
    uart_send_string("\x01\r\n");
}

static void irc_handle_privmsg_notice(const char *usr, char *par, char *txtp, const char *cmd)
{
    char *target = par;
    if (!target || !*target) return;
    if (!txtp || !*txtp) return;

    // Check ignore list
    if (is_ignored(usr) && !strchr(usr, '.')) return;

    // Strip IRC color/formatting codes from message text
    // (Nota: Esto NO elimina el \x01 inicial de los CTCP, así que es seguro hacerlo aquí)
    strip_irc_codes(txtp);

    // Check if this is a channel message for a non-active channel
    if (target[0] == '#') {
        int8_t idx = find_channel(target);
        if (idx >= 0 && (uint8_t)idx != current_channel_idx) {
            channels[idx].has_unread = 1;
            other_channel_activity = 1;
            status_bar_dirty = 1;
            return;  // Don't display, just notify
        }
    }

    // --- CTCP HANDLING (Optimizado) ---
    if (txtp[0] == 1) {
        char *ctcp_cmd = txtp + 1;
        
        // 1. ACTION (/me): \x01ACTION text\x01
        if (strncmp(ctcp_cmd, "ACTION ", 7) == 0) {
            char *act = ctcp_cmd + 7;
            char *end = strchr(act, 1);
            if (end) *end = 0; // Terminar cadena en el último \x01
            
            // Nota: No llamamos a strip_irc_codes(act) aquí porque ya se llamó sobre txtp arriba
            
            if (target[0] == '#') {
                // ACTION en channel
                int8_t idx = find_channel(target);
                if (idx >= 0 && (uint8_t)idx != current_channel_idx) {
                    channels[idx].has_unread = 1;
                    other_channel_activity = 1;
                    status_bar_dirty = 1;
                    return;
                }
                current_attr = ATTR_MSG_CHAN;
                main_puts("* "); main_puts(usr); main_putc(' '); main_print(act);
            } else {
                // ACTION en Privado
                int8_t query_idx = find_query(usr);
                if (query_idx < 0) {
                    query_idx = add_query(usr);
                    if (query_idx >= 0) status_bar_dirty = 1;
                }
                
                // Prefijo si no es la ventana activa
                if (query_idx < 0 || (uint8_t)query_idx != current_channel_idx) {
                    if (query_idx >= 0) {
                        channels[query_idx].has_unread = 1;
                        other_channel_activity = 1;
                        status_bar_dirty = 1;
                    }
                    current_attr = ATTR_MSG_SELF;
                    main_puts("<< ");
                    main_puts(usr);
                    main_puts(": ");
                }
                
                current_attr = ATTR_MSG_PRIV;
                main_puts("* "); main_puts(usr); main_putc(' '); main_print(act);
                notification_beep();
            }
            return;
        }
        
        // 2. RESPUESTAS AUTOMÁTICAS (Usando helper para ahorrar espacio)
        
        // CTCP VERSION
        if (strncmp(ctcp_cmd, "VERSION", 7) == 0) {
            // String acortado para ahorrar bytes
            send_ctcp_reply(usr, "VERSION", "SpecTalk ZX v" VERSION " :ZX Spectrum");
            return;
        }
        
        // CTCP PING
        if (strncmp(ctcp_cmd, "PING ", 5) == 0) {
            char *ts = ctcp_cmd + 5;
            char *end = strchr(ts, 1);
            if (end) *end = 0;
            send_ctcp_reply(usr, "PING", ts);
            return;
        }
        
        // CTCP TIME
        if (strncmp(ctcp_cmd, "TIME", 4) == 0) {
            if (time_synced) {
                // Construcción ligera de time HH:MM
                char time_buf[10];
                char *t = time_buf;
                *t++ = '0' + (time_hour / 10); *t++ = '0' + (time_hour % 10);
                *t++ = ':';
                *t++ = '0' + (time_minute / 10); *t++ = '0' + (time_minute % 10);
                *t = 0;
                send_ctcp_reply(usr, "TIME", time_buf);
            } else {
                send_ctcp_reply(usr, "TIME", "Unknown");
            }
            return;
        }
        
        return; // Ignorar otros CTCP
    }

    // --- messageS NORMALES ---

    // Server notices - filtrar ruido
    if (cmd[0] == 'N' && (target[0] == '*' || strchr(usr, '.'))) {
        // Filtrar messages de ruido del servidor
        if (st_stristr(txtp, "Ident") ||
            strstr(txtp, "Looking up") || strstr(txtp, "Found your") ||
            strstr(txtp, "Checking") || strstr(txtp, "*** ")) {
            return;  // Silenciar
        }
        current_attr = ATTR_MSG_SERVER;
        main_puts("[NOTICE] ");
        main_print(txtp);
        return;
    }

    if (target[0] == '#') {
        // Channel message
        current_attr = ATTR_MSG_CHAN;
        main_print_time_prefix();
        main_puts("<"); main_puts(usr); main_puts("> "); main_print(txtp);
        
        // MENCION EN channel: Solo Beep
        if (irc_nick[0] && st_stristr(txtp, irc_nick)) {
            notification_beep();
        }
    } else {
        // Private message
        int8_t query_idx = find_query(usr);
        
        if (query_idx < 0) {
            query_idx = add_query(usr);
            if (query_idx >= 0) status_bar_dirty = 1;
        }
        
        if (query_idx >= 0 && (uint8_t)query_idx != current_channel_idx) {
            channels[query_idx].has_unread = 1;
            other_channel_activity = 1;
            status_bar_dirty = 1;
        }
        
        main_print_time_prefix();
        
        if (query_idx < 0 || (uint8_t)query_idx != current_channel_idx) {
            // PM desde otra ventana - nick en amarillo, texto en verde
            current_attr = ATTR_MSG_SELF;
            main_puts("<< ");
            main_puts(usr);
            main_puts(": ");
            current_attr = ATTR_MSG_PRIV;
        } else {
            current_attr = ATTR_MSG_PRIV;
            main_puts("<"); main_puts(usr); main_puts("> ");
        }
        main_print(txtp);
        notification_beep();
    }
}

static void irc_handle_join(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)cmd;
    char *chan = (*txtp) ? txtp : par;
    if (*chan == ':') chan++;
    
    // CASO 1: SOY YO (Me uno a un channel)
    if (st_stricmp(usr, irc_nick) == 0) {
        current_attr = ATTR_MSG_JOIN;
        main_puts("Now talking in "); 
        main_print(chan);

        int8_t idx = find_channel(chan);
        if (idx < 0) idx = add_channel(chan);
        
        if (idx >= 0 && (uint8_t)idx != current_channel_idx) {
            switch_to_channel((uint8_t)idx);
        }
        
        channels[current_channel_idx].user_count = 0;
        counting_new_users = 1;
        
        strncpy(names_target_channel, chan, sizeof(names_target_channel) - 1);
        names_target_channel[sizeof(names_target_channel) - 1] = '\0';
        names_pending = 1;
        names_timeout_frames = 0;
        
        draw_status_bar();
        
    } else {
        // CASO 2: ES OTRO user
        int8_t idx = find_channel(chan);
        
        if (idx >= 0) {
            // Siempre actualizamos el contador real
            channels[idx].user_count++;
            
            // FILTRO: Solo imprimimos si estamos mirando ESTE channel
            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_puts(">> "); 
                main_puts(usr); 
                main_puts(" joined "); 
                main_print(chan);
                draw_status_bar();
            } else {
                // Opcional: Marcar actividad en el otro channel si entra alguien
                // channels[idx].has_unread = 1;
                // other_channel_activity = 1;
                // status_bar_dirty = 1;
            }
        }
    }
}

static void irc_handle_part(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)txtp; (void)cmd;
    
    char *chan = par;
    if (*chan == ':') chan++;

    int8_t idx = find_channel(chan);
    
    if (idx >= 0) {
        if (strcmp(usr, irc_nick) == 0) {
            // Si SOY YO el que sale, lo mostramos y cerramos
            current_attr = ATTR_MSG_JOIN;
            main_puts("<< You left "); main_print(chan);
            remove_channel((uint8_t)idx);
            draw_status_bar();
        } else {
            // Si es OTRO:
            if (channels[idx].user_count > 0) channels[idx].user_count--;
            
            // FILTRO: Solo mostrar si es el channel actual
            if ((uint8_t)idx == current_channel_idx) {
                current_attr = ATTR_MSG_JOIN;
                main_puts("<< "); main_puts(usr);
                main_puts(" left ");
                main_print(chan);
                draw_status_bar();
            }
        }
    }
}

static void irc_handle_quit(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)par; (void)cmd;
    
    // 1. Mostrar aviso global (estilo clásico) en la ventana actual
    // (Solo si estamos en un channel donde ese user estaba, simplificado aquí)
    current_attr = ATTR_MSG_JOIN;
    main_puts("<< "); main_puts(usr);
    main_puts(" quit");
    if (*txtp) {
        main_puts(" ("); main_puts(txtp); main_puts(")");
    }
    main_newline();
    
    // 2. Buscar si teníamos un PRIVADO abierto con él
    int8_t idx = find_query(usr);
    if (idx > 0) {
        // Marcamos actividad en esa ventana para que el user sepa que pasó algo
        if ((uint8_t)idx != current_channel_idx) {
            channels[idx].has_unread = 1;
            other_channel_activity = 1;
            status_bar_dirty = 1;
        } else {
            // Si ya estamos en ella, reforzamos el message
            ui_err("--- User has closed connection ---");
        }
    }
    
    // 3. Decrementar contadores (Lógica existente)
    uint8_t i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active && channels[i].user_count > 0) {
            channels[i].user_count--;
        }
    }
    if (channels[current_channel_idx].active) {
        draw_status_bar();
    }
}

static void irc_handle_numeric_401(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)cmd; (void)par;
    // 401: params[0]=me, params[1]=BadNick, txtp="No such nick/channel"
    
    const char *bad_nick = irc_param(1);
    
    // Mostrar el error en la ventana actual (o en Status)
    current_attr = ATTR_ERROR;
    main_puts("Error: ");
    if (bad_nick && *bad_nick) { main_puts(bad_nick); main_putc(' '); }
    main_print(txtp);

    // AUTO-LIMPIEZA: Si tenemos una ventana Query abierta para ese nick, ciérrala.
    if (bad_nick && *bad_nick) {
        int8_t idx = find_query(bad_nick);
        
        // Solo borramos si es una Query (Privado), no si es un channel (por seguridad)
        if (idx > 0 && channels[idx].is_query) {
            // Usamos remove_channel para hacer el "Tetris" y cerrar el hueco
            remove_channel((uint8_t)idx);
            
            // Si estábamos en esa ventana, remove_channel ya nos habrá movido al Server (0).
            // Forzamos un repintado de la barra para que desaparezca el nombre.
            draw_status_bar();
        }
    }
}


static void irc_handle_numeric_332(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)cmd;
    
    if (txtp && *txtp) {
        current_attr = ATTR_MSG_TOPIC;
        main_print_time_prefix(); // Opcional: si quieres time en el topic
        main_puts("Topic: ");
        main_print(txtp);
    }
}

static void irc_handle_numeric_353(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)cmd;
    // 353 RPL_NAMREPLY: params[0]=me, [1]=chantype(=*@), [2]=channel, txtp=names
    const char *msg_chan = irc_param(2);
    
    // Validate this 353 is for our target channel (or current channel if no target set)
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    if (!target[0]) return;  // No channel to count for
    
    // Case-insensitive channel comparison (IRC channels are case-insensitive)
    if (st_stricmp(msg_chan, target) != 0) return;  // Wrong channel, ignore
    
    // Reset timeout - we got a valid 353
    names_timeout_frames = 0;
    names_pending = 1;
    
    // Count names in txtp (space-separated, may have @+% prefixes)
    char *p = txtp;
    uint16_t count = 0;
    if (*p) count = 1;
    while (*p) { if (*p == ' ') count++; p++; }
    
    // First 353 after JOIN resets count, subsequent ones accumulate
    if (counting_new_users) {
        chan_user_count = count;
        counting_new_users = 0;
    } else {
        chan_user_count += count;
    }
    
    // Show names list if user requested /names
    if (show_names_list) {
        current_attr = ATTR_MSG_CHAN;
        main_print(txtp);
    }
}

static void irc_handle_numeric_366(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)txtp; (void)cmd;
    // 366 RPL_ENDOFNAMES - NAMES reply complete
    // Validate it's for our target channel
    const char *msg_chan = irc_param(1);  // 366: params[0]=me, [1]=channel
    const char *target = names_target_channel[0] ? names_target_channel : irc_channel;
    
    if (target[0] && st_stricmp(msg_chan, target) == 0) {
        // Clear NAMES pending state
        names_pending = 0;
        names_timeout_frames = 0;
        names_target_channel[0] = '\0';
    }
    
    show_names_list = 0;
    draw_status_bar();
}

static void irc_handle_numeric_321(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)txtp; (void)cmd;
    pagination_active = 1;
    pagination_cancelled = 0;
    pagination_count = 0;
    ui_sys("Listing...");
}

static void irc_handle_numeric_323(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)txtp; (void)cmd;
    pagination_active = 0;
    current_attr = ATTR_MSG_SYS;
    main_print((search_mode==SEARCH_CHAN) ? "End search" : "End list");
    search_mode = SEARCH_NONE;
    search_index = 0;
}


static void irc_handle_numeric_315(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)txtp; (void)cmd;
    pagination_active = 0;
    current_attr = ATTR_MSG_SYS;
    main_print((search_mode==SEARCH_USER) ? "End search" : "End WHO");
    if (search_mode==SEARCH_USER) { search_mode = SEARCH_NONE; search_index = 0; }
}

static void irc_handle_numeric_322_352(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr; (void)cmd; (void)par;

    // 1. Definir los atributos de color que faltaban (Solución error 20)
    uint8_t num_attr = ATTR_MSG_SYS;    // Color para los números "1."
    uint8_t hl_attr  = ATTR_MSG_NICK;   // Color para el dato importante (Nick/channel)
    uint8_t txt_attr = ATTR_MSG_CHAN;   // Color para la info técnica (Host/Topic)

    // Validaciones de seguridad
    if (pagination_cancelled) return;
    if (!pagination_active) return;

    // Solo mostrar si estamos en modo búsqueda/listado
    if (search_mode == SEARCH_NONE) return;

    // Pausar si la screen está llena
    if (pagination_check()) return;

    // --- CASO 1: RESPUESTA DE /LIST (Código 322) ---
    if (num == 322 && search_mode == SEARCH_CHAN) {
        // 322 params: [1]=channel, [2]=#users. txtp=topic
        const char *chan = irc_param(1);
        const char *users = irc_param(2);
        
        if (!chan[0]) return;
        
        // Filtro de búsqueda - usar strstr (más fiable que ASM)
        if (search_pattern[0] && !strstr(chan, search_pattern)) return;
        
        search_index++;
        
        // Formato: "1. #channel (N)"
        main_run_char(' ', num_attr);
        main_run_u16(search_index, num_attr, 0);
        main_run(". ", num_attr, 0);
        main_run(chan, hl_attr, 0);
        
        main_run(" (", txt_attr, 0);
        main_run(users, txt_attr, 0);
        main_run(")", txt_attr, 0);
        
        // Mostrar Topic cortado si existe
        if (txtp && *txtp) {
             main_run(" ", txt_attr, 0);
             // Recortar topic a 20 caracteres para no llenar screen
             char short_topic[21];
             uint8_t len = 0;
             const char *t = txtp;
             while (*t && len < 20) { short_topic[len++] = *t++; }
             short_topic[len] = 0;
             main_run(short_topic, txt_attr, 0);
             if (*t) main_run("...", txt_attr, 0);
        }

        main_newline();
        pagination_count++;
        return;
    }

    // --- CASO 2: RESPUESTA DE /WHO (Código 352) ---
    if (num == 352 && search_mode == SEARCH_USER) {
        // 352 params: [0]=me, [1]=channel, [2]=user, [3]=host, [4]=server, [5]=nick
        const char *user = irc_param(2);
        const char *host = irc_param(3);
        const char *nick = irc_param(5);
        
        if (!nick[0]) return;
        
        // Filtro de búsqueda: buscar en nick O en user
        if (search_pattern[0]) {
            if (!strstr(nick, search_pattern) && !strstr(user, search_pattern)) {
                return;
            }
        }

        search_index++;
        
        // Formato: "1. Nick [User@Host]"
        main_run_char(' ', num_attr);
        main_run_u16(search_index, num_attr, 0);
        main_run(". ", num_attr, 0);
        main_run(nick, hl_attr, 0); // Nick en amarillo/destacado
        
        // Info técnica discreta
        main_run(" [", txt_attr, 0);
        main_run(user, txt_attr, 0);
        main_run("@", txt_attr, 0);
        main_run(host, txt_attr, 0);
        main_run("]", txt_attr, 0);
        
        main_newline();
        pagination_count++;
        return;
    }
}

static void irc_handle_numeric_1(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)par; (void)txtp; (void)cmd;
    connection_state = STATE_IRC_READY;
    cursor_visible = 1;
    draw_status_bar();
}

static void irc_handle_numeric_5(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)num; (void)usr; (void)txtp; (void)cmd;

    char *net = strstr(par, "NETWORK=");
    if (net) {
        /* C90: declarations must come before statements inside the block */
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

static void irc_handle_numeric_default(uint16_t num, const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr; (void)cmd; (void)par;
    
    if (num >= 400 && num <= 599) {
        current_attr = ATTR_ERROR;
        if (*txtp) main_print(txtp); else main_print("Error");
        return;
    }
    // Skip if txtp is empty or just our nick (server noise)
    if (!*txtp) return;
    if (strcmp(txtp, irc_nick) == 0) return;
    
    current_attr = ATTR_MSG_SERVER;
    main_print(txtp);
}


static void irc_handle_default_cmd(const char *usr, char *par, char *txtp, const char *cmd)
{
    (void)usr;
    // Basura final (keep same heuristics)
    if ((cmd[0] >= '0' && cmd[0] <= '9') || !cmd[1]) return;
    current_attr = ATTR_MSG_SYS;
    main_puts(">< "); main_puts(cmd);
    if (*par) { main_putc(' '); main_puts(par); }
    if (*txtp) { main_puts(" :"); main_puts(txtp); }
    main_newline();
}

typedef struct {
    const char *cmd;
    irc_cmd_handler_t fn;
} irc_cmd_dispatch_t;

typedef struct {
    uint16_t num;
    irc_num_handler_t fn;
} irc_num_dispatch_t;

// Centralized noise filtering / ignored protocol messages
static const irc_cmd_dispatch_t IRC_CMD_TABLE[] = {
    { "PING",          irc_handle_ping },
    { "PONG",          irc_handle_ignore },
    { "NICK",          irc_handle_nick },

    // Noise / optional capabilities (ignored)
    { "CAP",           irc_handle_cap },
    { "AUTHENTICATE",  irc_handle_ignore },
    { "BATCH",         irc_handle_ignore },
    { "TAGMSG",        irc_handle_ignore },

    // Core messages
    { "MODE",          irc_handle_mode },
    { "PRIVMSG",       irc_handle_privmsg_notice },
    { "NOTICE",        irc_handle_privmsg_notice },
    { "JOIN",          irc_handle_join },
    { "PART",          irc_handle_part },
    { "QUIT",          irc_handle_quit },
};

static const irc_num_dispatch_t IRC_NUM_TABLE[] = {
    { 1,   irc_handle_numeric_1 },
    { 5,   irc_handle_numeric_5 },
    { 315, irc_handle_numeric_315 },
    { 321, irc_handle_numeric_321 },
    { 322, irc_handle_numeric_322_352 },
    { 323, irc_handle_numeric_323 },
    { 332, irc_handle_numeric_332 },
    { 352, irc_handle_numeric_322_352 },
    { 353, irc_handle_numeric_353 },
    { 366, irc_handle_numeric_366 },
    { 401, irc_handle_numeric_401 },
};


// Dispatch table entry
static void irc_dispatch_message(const char *usr, char *cmd, char *par, char *txtp)
{
    uint8_t i;

    if (!cmd || !*cmd) return;

    // Numerics
    if (cmd[0] >= '0' && cmd[0] <= '9') {
        uint16_t num = str_to_u16(cmd);

        for (i = 0; i < (uint8_t)(sizeof(IRC_NUM_TABLE) / sizeof(IRC_NUM_TABLE[0])); i++) {
            if (IRC_NUM_TABLE[i].num == num) {
                IRC_NUM_TABLE[i].fn(num, usr, par, txtp, cmd);
                return;
            }
        }
        irc_handle_numeric_default(num, usr, par, txtp, cmd);
        return;
    }

    // Commands
    for (i = 0; i < (uint8_t)(sizeof(IRC_CMD_TABLE) / sizeof(IRC_CMD_TABLE[0])); i++) {
        if (strcmp(cmd, IRC_CMD_TABLE[i].cmd) == 0) {
            IRC_CMD_TABLE[i].fn(usr, par, txtp, cmd);
            return;
        }
    }

    // Default
    irc_handle_default_cmd(usr, par, txtp, cmd);
}



// ============================================================
// MAIN PARSING FUNCTIONS (public)
// ============================================================

void parse_irc_message(char *line) __z88dk_fastcall
{
    char *usr = irc_server; 
    char *cmd;
    char *par;
    char *txtp;
    static char nickbuf[20];

    if (!line || !*line) return;

    // IRCv3: Saltar message tags (@time=...; @batch=...; etc)
    if (line[0] == '@') {
        line = strchr(line, ' ');
        if (!line) return;
        line++;
        while (*line == ' ') line++;
        if (!*line) return;
    }

    // Prefijo
    if (line[0] == ':') {
        usr = line + 1;
        cmd = skip_to(usr, ' ');
        if (*cmd == 0) return;
        
        char *bang = strchr(usr, '!');
        if (bang) {
             uint8_t len = (uint8_t)(bang - usr);
             if (len > sizeof(nickbuf) - 1) len = sizeof(nickbuf) - 1;
             memcpy(nickbuf, usr, len);
             nickbuf[len] = 0;
             usr = nickbuf;
        }
    } else {
        cmd = line;
    }

    par = skip_to(cmd, ' ');
    txtp = par;
    while (*txtp == ' ') txtp++;

    // Trailing param (:)
    {
        char *p = txtp;
        while (*p) {
            if (p[0] == ':' && (p == txtp || p[-1] == ' ')) {
                if (p > txtp && p[-1] == ' ') p[-1] = 0;
                *p++ = 0;
                txtp = p;
                break;
            }
            p++;
        }
    }

    // Tokenize params before dispatch - handlers can use irc_param(n)
    tokenize_params(par, 0);
    
    // Dispatch (Phase 2): table-driven handlers
    irc_dispatch_message(usr, cmd, par, txtp);
    return;

}

void process_irc_data(void)
{
    int16_t c;
    uint16_t bytes_this_call = 0;
    uint8_t lines_this_call = 0;
    uint8_t max_lines;

    // Process AT responses (SNTP) if WiFi connected but not IRC
    if (connection_state == STATE_WIFI_OK && sntp_waiting) {
        uart_drain_to_buffer();
        
        while ((c = rb_pop()) != -1) {
            uint8_t b = (uint8_t)c;
            
            if (b == '\r') continue;
            
            if (b == '\n') {
                if (!rx_overflow && rx_pos > 0) {
                    rx_line[rx_pos] = '\0';
                    
                    // Process SNTP response
                    if (rx_line[0] == '+') {
                        sntp_process_response(rx_line);
                    }
                }
                rx_pos = 0;
                rx_overflow = 0;
                continue;
            }
            
            if (!rx_overflow) {
                if (rx_pos < (sizeof(rx_line) - 1)) {
                    rx_line[rx_pos++] = (char)b;
                } else {
                    rx_overflow = 1;
                }
            }
        }
        return;
    }

    if (connection_state < STATE_TCP_CONNECTED) return;

    // Phase 3.5: deterministic per-tick drain/parse budgets.
    // Drain a bounded amount of UART data into the ring buffer each tick, with
    // a small adaptive step based on current backlog (pre-drain).
    {
        uint16_t backlog_pre = (uint16_t)(rb_head - rb_tail) & RING_BUFFER_MASK;
        if (backlog_pre > 1024)      uart_drain_limit = RX_TICK_DRAIN_MAX;
        else if (backlog_pre > 512)  uart_drain_limit = DRAIN_FAST;
        else if (backlog_pre > 256)  uart_drain_limit = 128;
        else                         uart_drain_limit = DRAIN_NORMAL;
    }

    // Drain UART into ring buffer (budgeted)
    uart_drain_to_buffer();

    // Adaptive line budget: more lines if backlog is high (post-drain)
    {
        uint16_t backlog = (uint16_t)(rb_head - rb_tail) & RING_BUFFER_MASK;
        if (backlog > 1024)     max_lines = 32;  // Heavy burst (MOTD)
        else if (backlog > 512) max_lines = 24;
        else if (backlog > 256) max_lines = 16;
        else if (backlog > 128) max_lines = 10;
        else                    max_lines = 6;
    }

    // Process bytes from ring buffer
    while ((c = rb_pop()) != -1) {
        uint8_t b = (uint8_t)c;

        bytes_this_call++;
        // Limit total bytes per tick to protect UI responsiveness.
        if (bytes_this_call >= RX_TICK_PARSE_BYTE_BUDGET) {
            return;
        }

        if (b == '\r') continue;

        if (b == '\n') {
            if (!rx_overflow && rx_pos > 0) {
                rx_line[rx_pos] = '\0';

                // Check for CLOSED
                if (strcmp(rx_line, S_CLOSED) == 0) {
                    if (!closed_reported) {
                        closed_reported = 1;
                        ui_err("Connection closed by server");
                        connection_state = STATE_WIFI_OK;
                        cursor_visible = 1;  // Restore cursor
                        reset_all_channels();
                        draw_status_bar();
                    }
                } else {
                    parse_irc_message(rx_line);
                    lines_this_call++;
                }
            }
            rx_pos = 0;
            rx_overflow = 0;
            
            // Exit if processed enough lines this frame
            if (lines_this_call >= max_lines) {
                return;
            }
            continue;
        }

        // Accumulate character
        if (!rx_overflow) {
            if (rx_pos < (sizeof(rx_line) - 1)) {
                rx_line[rx_pos++] = (char)b;
            } else {
                rx_overflow = 1;
            }
        }
    }
}
