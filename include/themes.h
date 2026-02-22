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

// OPT-01: Tabla de themes definida en spectalk.c para evitar duplicación si se incluye desde múltiples .c
#define THEME_COUNT 3
extern const Theme themes[THEME_COUNT];

#endif