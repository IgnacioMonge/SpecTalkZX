/*
 * themes.h - Visual theme definitions for SpecTalk ZX
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
 */

#ifndef THEMES_H
#define THEMES_H

#include <arch/zx.h>
#include <stdint.h>

// Definición de la estructura del theme
typedef struct {
    const char *name;          // Theme display name
uint8_t banner;
    uint8_t status;
    uint8_t msg_chan;
    uint8_t msg_self;
    uint8_t msg_priv;
    uint8_t main_bg;
    uint8_t input;
    uint8_t input_bg;
    uint8_t prompt;
    uint8_t msg_server;
    uint8_t msg_join;
    uint8_t msg_nick;
    uint8_t msg_time;    // Timestamp color
    uint8_t msg_topic;
    uint8_t msg_motd;    // MOTD color
    uint8_t error;
    uint8_t ind_red;     // Status indicators
    uint8_t ind_yellow;
    uint8_t ind_green;
    uint8_t border;
} Theme;

// Tabla de themes
static const Theme themes[] = {
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
        (PAPER_GREEN | INK_BLACK),             // input
        (PAPER_GREEN | INK_BLACK),             // input_bg
        (PAPER_GREEN | INK_BLACK),             // prompt
        (PAPER_BLACK | INK_GREEN),             // msg_server
        (PAPER_BLACK | INK_GREEN),             // msg_join
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_nick
        (PAPER_BLACK | INK_GREEN),             // msg_time
        (PAPER_BLACK | INK_GREEN | BRIGHT),    // msg_topic
        (PAPER_BLACK | INK_GREEN),             // msg_motd (dimmer green)
        (PAPER_BLACK | INK_GREEN | BRIGHT),      // error
        (PAPER_GREEN | INK_RED),               // ind_red
        (PAPER_GREEN | INK_YELLOW),            // ind_yellow
        (PAPER_GREEN | INK_BLACK),    // ind_green (bright para contraste)
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

#endif