;;
;; divmmc_uart.asm - Optimized UART backend for divMMC/divTIESUS
;; SpecTalk ZX
;;

SECTION code_user

EXTERN _frame_wait
EXTERN _rb_push
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
    add a, a        ; RX-ready bit -> Carry; port-select/read ops below preserve CF
    ret nc

    dec b
    ld a, UART_DATA_REG
    out (c), a
    
    ; OPTIMIZACIÓN: FC3B -> FD3B
    inc b
    
    in a, (c)
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
    ; While TX is busy, opportunistically drain one RX byte so outbound waits do
    ; not become receive blackouts during server bursts.
uartSend_wait_tx:
    in a, (c)
    add a, a                ; TX-busy bit -> Sign, RX-ready bit -> Carry
    jp p, uartSend_tx_ready
    jr nc, uartSend_wait_tx

    push hl                 ; preserve byte to send in L
    dec b                   ; select UART DATA register through $FC3B
    ld a, UART_DATA_REG
    out (c), a
    inc b
    in a, (c)
    ld l, a
    call _rb_push
    pop hl

    ld bc, ZXUNO_ADDR       ; _rb_push clobbers BC; restore status port
    ld a, UART_STAT_REG
    out (c), a
    inc b
    jr uartSend_wait_tx

uartSend_tx_ready:
    ; OPT: dec b switches BC from ZXUNO_REG (FD3B) to ZXUNO_ADDR (FC3B)
    dec b
    ld a, UART_DATA_REG
    out (c), a

    inc b
    out (c), l          ; OPT: direct from fastcall reg, no push/pop
    ret
