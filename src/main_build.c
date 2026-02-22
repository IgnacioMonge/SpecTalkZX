/*
 * main_build.c - Single Compilation Unit
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

/*
 * STRUCT-01: Single Compilation Unit (SCU) - ORDEN CRÍTICO
 * 
 * El orden de includes es importante porque:
 * 1. spectalk.c DEBE ser el último porque define todas las variables globales
 * 2. irc_handlers.c y user_cmds.c pueden ir en cualquier orden entre sí
 * 3. Todas las funciones 'static' en un módulo son visibles en los otros
 * 4. Conflictos de nombres entre módulos serían silenciosos
 * 
 * Este patrón permite que SDCC optimice cross-module (inlining, dead code elimination)
 */

#include "irc_handlers.c"
#include "user_cmds.c"
#include "spectalk.c"
