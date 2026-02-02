;;
;; divmmc_uart.asm - Optimized UART backend for divMMC/divTIESUS
;; SpecTalk ZX v1.0
;;

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
; OPTIMIZACIÓN: Flujo unificado y reducción de saltos
; -----------------------------------------------------------------------------
uartRead:
    ld a, (_poked_byte)
    and 1
    jr nz, uartRead_retBuff

    ld a, (_is_recv)
    and 1
    jr nz, uartRead_hw_latched

    ; Check Hardware
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr nz, uartRead_hw_new

    or a            ; CF=0 (No data)
    ret

uartRead_retBuff:
    xor a
    ld (_poked_byte), a
    ld a, (_byte_buff)
    scf             ; CF=1 (Data)
    ret

uartRead_hw_latched:
    ; Flag was latched, just read data
    jr uartRead_do_read

uartRead_hw_new:
    ; New byte available
    ; Fallthrough to read

uartRead_do_read:
    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)

    ; Clear flags
    ld l, a
    xor a
    ld (_is_recv), a
    ld (_poked_byte), a
    ld a, l
    scf             ; CF=1 (Data)
    ret

; -----------------------------------------------------------------------------
; _ay_uart_init
; -----------------------------------------------------------------------------
_ay_uart_init:
    xor a
    ld (_poked_byte), a
    ld (_is_recv), a

    ; Prime reads
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

    ei
    ld b, 50
uartInit_wait:
    push bc
    call uartRead
    pop bc
    halt
    djnz uartInit_wait

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
; -----------------------------------------------------------------------------
_ay_uart_send:
    ld a, l
    push af

    ; Check receive flag update while sending
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr z, uartSend_checkSent

    ld a, 1
    ld (_is_recv), a

uartSend_checkSent:
    ld bc, ZXUNO_REG
uartSend_wait_tx:
    in a, (c)
    and UART_BYTE_SENDING
    jr nz, uartSend_wait_tx

    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a

    ld bc, ZXUNO_REG
    pop af
    out (c), a
    ret

; -----------------------------------------------------------------------------
; _ay_uart_send_block
; -----------------------------------------------------------------------------
_ay_uart_send_block:
    pop bc
    pop hl
    pop de
    push bc

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
; _ay_uart_ready / _ay_uart_ready_fast
; OPTIMIZACIÓN: Salto directo si hay byte cacheado
; -----------------------------------------------------------------------------
_ay_uart_ready:
_ay_uart_ready_fast:
    ld a, (_poked_byte)
    or a
    jr nz, uartReady_yes

    ld a, (_is_recv)
    or a
    jr nz, uartReady_yes

    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    ld bc, ZXUNO_REG
    in a, (c)
    and UART_BYTE_RECIVED
    jr z, uartReady_no

    ld a, 1
    ld (_is_recv), a

uartReady_yes:
    ld l, 1
    ret

uartReady_no:
    ld l, 0
    ret

; -----------------------------------------------------------------------------
; _ay_uart_read
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