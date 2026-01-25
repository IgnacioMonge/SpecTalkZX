;;
;; divmmc_uart.asm - UART backend for divMMC/divTIESUS hardware
;; SpecTalk ZX v1.0 - IRC Client for ZX Spectrum
;; Copyright (C) 2026 M. Ignacio Monge Garcia
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License version 2 as
;; published by the Free Software Foundation.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program. If not, see <https://www.gnu.org/licenses/>.
;;
;; The divTIESUS Maple Edition implements a ZX-Uno register file accessed through
;; ports 0xFC3B (register select) and 0xFD3B (register read/write). Its UART is
;; exposed in registers 0xC6 (data) and 0xC7 (status).
;;
;; This file exports the same symbols as ay_uart.asm so spectalk code does not
;; need any changes.

SECTION code_user

PUBLIC _ay_uart_init
PUBLIC _ay_uart_send
PUBLIC _ay_uart_send_block
PUBLIC _ay_uart_read
PUBLIC _ay_uart_ready
PUBLIC _ay_uart_ready_fast

; ZX-Uno compatible register interface
UART_DATA_REG     EQU 0xC6
UART_STAT_REG     EQU 0xC7
UART_BYTE_RECIVED EQU 0x80
UART_BYTE_SENDING EQU 0x40
ZXUNO_ADDR        EQU 0xFC3B
ZXUNO_REG         EQU 0xFD3B

SECTION bss_user

_poked_byte: DEFS 1
_byte_buff:  DEFS 1
_is_recv:    DEFS 1

SECTION code_user

; -----------------------------------------------------------------------------
; Internal helper: uartRead
;   Returns: CF=1 if a byte was read, A=byte
;            CF=0 if nothing to read
; -----------------------------------------------------------------------------

uartRead:
    ld a, (_poked_byte)
    and 1
    jr nz, uartRead_retBuff

    ld a, (_is_recv)
    and 1
    jr nz, uartRead_recvRet

    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr nz, uartRead_retReadByte

    or a            ; clear carry
    ret

uartRead_retBuff:
    xor a
    ld (_poked_byte), a
    ld a, (_byte_buff)
    scf
    ret

uartRead_retReadByte:
    xor a
    ld (_poked_byte), a
    ld (_is_recv), a

    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a

    ld bc, ZXUNO_REG
    in a, (c)

    scf
    ret

uartRead_recvRet:
    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a

    ld bc, ZXUNO_REG
    in a, (c)

    ; Preserve byte in L while clearing flags
    ld l, a
    xor a
    ld (_is_recv), a
    ld (_poked_byte), a
    ld a, l

    scf
    ret

; -----------------------------------------------------------------------------
; _ay_uart_init
; -----------------------------------------------------------------------------

_ay_uart_init:
    xor a
    ld (_poked_byte), a
    ld (_is_recv), a

    ; Prime reads (helps settle the UART registers)
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)

    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)

    ; Brief boot wait + drain RX garbage. HALT requires interrupts.
    ei
    ld b, 50
uartInit_wait:
    push bc
    call uartRead      ; discard if available
    pop bc
    halt
    djnz uartInit_wait

    ; Additional bounded drain (no HALT) to clear any leftover bytes.
    ld bc, 0x0800
uartInit_flush:
    push bc
    call uartRead
    pop bc
    dec bc
    ld a, b
    or c
    jr nz, uartInit_flush

    ret

; -----------------------------------------------------------------------------
; _ay_uart_send
;   fastcall: byte in L
; -----------------------------------------------------------------------------

_ay_uart_send:
    ld a, l
    push af

    ; Select status register and check for received byte.
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr z, uartSend_checkSent

    ; Latch the fact that RX became ready (some cores may clear RX flag on status read).
    ld a, 1
    ld (_is_recv), a

uartSend_checkSent:
    ; Wait until TX is not busy.
    ld bc, ZXUNO_REG
uartSend_wait_tx:
    in a, (c)
    and UART_BYTE_SENDING
    jr nz, uartSend_wait_tx

    ; Select data register and output byte.
    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a

    ld bc, ZXUNO_REG
    pop af
    out (c), a

    ret

; -----------------------------------------------------------------------------
; _ay_uart_send_block
;   callee: HL=buffer, DE=len
; -----------------------------------------------------------------------------

_ay_uart_send_block:
    pop bc          ; return address
    pop hl          ; buffer
    pop de          ; length
    push bc         ; restore return address

    ld a, d
    or e
    ret z

uartSendBlock_loop:
    ld a, (hl)
    push hl
    ld l, a
    call _ay_uart_send
    pop hl

    inc hl
    dec de
    ld a, d
    or e
    jr nz, uartSendBlock_loop

    ret

; -----------------------------------------------------------------------------
; _ay_uart_ready
;   Returns L=1 if there is at least one byte available, else L=0
; -----------------------------------------------------------------------------

_ay_uart_ready:
    ld a, (_poked_byte)
    and 1
    jr nz, uartReady_yes

    ld a, (_is_recv)
    and 1
    jr nz, uartReady_yes

    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr z, uartReady_no

    ; Latch RX-ready state for subsequent read (see comment in _ay_uart_send).
    ld a, 1
    ld (_is_recv), a

uartReady_yes:
    ld l, 1
    ret

uartReady_no:
    ld l, 0
    ret

; Same semantics for this backend.
_ay_uart_ready_fast:
    jp _ay_uart_ready

; -----------------------------------------------------------------------------
; _ay_uart_read
;   Returns L=byte if available, else L=0
; -----------------------------------------------------------------------------

_ay_uart_read:
    call uartRead
    jr nc, uartRead_none
    ld l, a
    ret

uartRead_none:
    xor a
    ld l, a
    ret
