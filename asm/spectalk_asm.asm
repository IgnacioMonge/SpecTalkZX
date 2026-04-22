;;
;; spectalk_asm.asm - Module root for SpecTalk ZX assembly routines
;;
;; Split into logical include files on 2026-04-22.
;; Keep include order stable: code layout and local relative branches rely on it.

INCLUDE "spectalk_asm/00_preamble.asm"
INCLUDE "spectalk_asm/10_core_helpers.asm"
INCLUDE "spectalk_asm/20_rx_ring_uart.asm"
INCLUDE "spectalk_asm/30_rendering.asm"
INCLUDE "spectalk_asm/40_text_numeric_screen.asm"
INCLUDE "spectalk_asm/50_main_output.asm"
INCLUDE "spectalk_asm/60_protocol_storage.asm"
INCLUDE "spectalk_asm/70_input_lookup.asm"
INCLUDE "spectalk_asm/80_ui_runtime.asm"

