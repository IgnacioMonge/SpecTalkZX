;;
;; divmmc_uart.asm - Optimized UART backend for divMMC/divTIESUS
;; SpecTalk ZX
;;

SECTION code_user

EXTERN _frame_wait
PUBLIC _ay_uart_init
PUBLIC _ay_uart_send
PUBLIC uartRead

; ZX-Uno compatible register interface
UART_DATA_REG     EQU 0xC6
UART_STAT_REG     EQU 0xC7
UART_BYTE_RECIVED EQU 0x80
UART_BYTE_SENDING EQU 0x40
ZXUNO_ADDR        EQU 0xFC3B
ZXUNO_REG         EQU 0xFD3B

SECTION code_user

; -----------------------------------------------------------------------------
; Internal helper: uartRead
; Return: CF=1 and A=byte if data available, CF=0 otherwise.
; -----------------------------------------------------------------------------
uartRead:
    ; Check Hardware
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    ; OPTIMIZACIÓN: FC3B -> FD3B (4 ciclos vs 10 ciclos)
    inc b

    in a, (c)
    and UART_BYTE_RECIVED
    ret z

    ld bc, ZXUNO_ADDR
    ld a, UART_DATA_REG
    out (c), a
    
    ; OPTIMIZACIÓN: FC3B -> FD3B
    inc b
    
    in a, (c)
    scf             ; CF=1 (Data)
    ret

; -----------------------------------------------------------------------------
; _ay_uart_init
; OPTIMIZACIÓN: Reducción de tiempos de espera y bucle de flush ajustado
; -----------------------------------------------------------------------------
_ay_uart_init:
    ; Prime reads
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    
    inc b           ; OPTIMIZACIÓN
    
    in a, (c)

    dec b
    ld a, UART_DATA_REG
    out (c), a
    
    inc b           ; OPTIMIZACIÓN
    
    in a, (c)

    ld b, 10        ; OPTIMIZACIÓN: Reducido de 50 a 10 frames
uartInit_wait:
    push bc
    call uartRead
    pop bc
    call _frame_wait
    djnz uartInit_wait

    ld bc, 0x0200   ; OPTIMIZACIÓN: Reducido de 0x0800 a 512 bytes
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
    ; L = byte to send (fastcall), preserved until out (c), l at end

    ; Select status register for TX-ready polling.
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    
    inc b           ; OPTIMIZACIÓN

    ; NOTE-M13: loop sin timeout. A 115200 baud tarda ~7 iteraciones (~300 T-states).
    ; Un hang permanente aquí solo ocurriría por fallo físico del hardware UART.
uartSend_wait_tx:
    in a, (c)
    bit 6, a
    jr nz, uartSend_wait_tx

uartSend_tx_ready:
    ; OPT: dec b switches BC from ZXUNO_REG (FD3B) to ZXUNO_ADDR (FC3B)
    dec b
    ld a, UART_DATA_REG
    out (c), a

    inc b
    out (c), l          ; OPT: direct from fastcall reg, no push/pop
    ret
