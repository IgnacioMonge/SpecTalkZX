;;
;; ay_uart.asm - AY-3-8912 UART bit-banging driver
;; SpecTalk ZX - IRC Client for ZX Spectrum
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
;; Based on AY/ZXuno driver code by Nihirash.
;;
;; TX: Port A bit 3
;; RX: Port A bit 7
;; Baud: 9600

    SECTION code_user

    PUBLIC _ay_uart_init
    PUBLIC _ay_uart_send
    PUBLIC _ay_uart_read
    PUBLIC _ay_uart_ready

;; ============================================================
;; DATA SEQUENCE FOR SPEED INITIALIZATION
;; ============================================================

dataSequence:
    defb  0xf6,  0xfe,  0xf6,  0xfe,  0xfe,  0xf6,  0xf6,  0xf6
    defb  0xf6,  0xfe,  0xf6,  0xfe,  0xfe,  0xf6,  0xf6,  0xf6
    defb  0xf6,  0xfe,  0xf6,  0xfe,  0xfe,  0xf6,  0xf6,  0xf6
    defb  0xf6,  0xfe,  0xf6,  0xfe,  0xfe,  0xf6,  0xf6,  0x00
dataSequenceEnd:

defc dataSize = dataSequenceEnd - dataSequence

;; ============================================================
;; variables
;; ============================================================

SECTION bss_user

_baud:              defs 2      ; Baud rate delay value (11 for 9600)
_baud_tx_val:       defs 2      ; PRE-CALCULATED baud - 2 for TX
_baud_half:         defs 2      ; PRE-CALCULATED baud / 2 for RX timing
_isSecondByteAvail: defs 1      ; Second byte available flag  
_secondByte:        defs 1      ; Cached second byte

    SECTION code_user

;; ============================================================
;; setSpeed - Send initialization sequence
;; ============================================================
setSpeed:
    ld hl, dataSequence
    ld bc, 0xBFFD
    di
    
    ; Send all bytes (dataSize = 280, need 16-bit counter)
    ld de, dataSize
    
setSpeedLoop:
    ld a, (hl)
    inc hl
    ld bc, 0xBFFD
    out (c), a
    nop
    
    dec de
    ld a, d
    or e
    jr nz, setSpeedLoop
    
    ei
    ret

;; ============================================================
;; ay_uart_init - Initialize AY for UART
;; ============================================================
_ay_uart_init:
    di
    
    ; Set mixer register (reg 7) to enable read mode
    ld a, 0x07
    ld bc, 0xFFFD
    out (c), a
    ld a, 0xFC              ; Enable read mode on port A
    ld b, 0xBF
    out (c), a
    
    ; Select port A register (reg 14)
    ld a, 0x0E
    ld bc, 0xFFFD
    out (c), a
    
    ; Read current value and set CTS HIGH (Busy) initially
    ld b, 0xBF
    in a, (c)
    or 0x04                 ; Set bit 2 (CTS High / Busy) - WAS: and 0xFB
    out (c), a
    
    ei
    
    ; Wait for ESP to stabilize (shorter)
    ld b, 50
    
initFlush:
    halt
    djnz initFlush
    
    ; Set baud rate (9600)
    ld hl, 11               
    ld (_baud), hl
    
    ; --- OPTIMIZACIÓN: Precalcular retardo TX (baud - 2) ---
    dec hl
    dec hl
    ld (_baud_tx_val), hl
    ; -------------------------------------------------------
    
    ; --- OPTIMIZACIÓN: Precalcular baud/2 para RX ---
    ld hl, 11
    srl h
    rr l                    ; HL = 11/2 = 5
    ld (_baud_half), hl
    ; ------------------------------------------------
    
    ; Clear second byte cache
    xor a
    ld (_isSecondByteAvail), a
    
    ; Send speed initialization sequence
    call setSpeed
    
    ret

;; ============================================================
;; ay_uart_send - Send a byte (fastcall: byte in L)
;; OPTIMIZED: Uses pre-calculated baud delay
;; ============================================================
_ay_uart_send:
    di
    push hl
    push de
    push bc
    push af
    
    ld a, l                 ; Get byte to send
    push af                 ; Save it
    
    ld c, 0xFD              ; Prepare port addresses
    ld d, 0xFF
    ld e, 0xBF
    ld b, d
    
    ld a, 0x0E
    out (c), a              ; Select AY's PORT A
    
    ; --- OPTIMIZACIÓN: Cargar valor precalculado directamente ---
    ld hl, (_baud_tx_val)   ; 16 t-states (vs math overhead before)
    ex de, hl               ; DE = baud - 2
    ; ------------------------------------------------------------
    
    pop af                  ; Get byte back
    cpl                     ; Complement it
    scf                     ; Set carry for start bit
    ld b, 11                ; 1 start + 8 data + 2 stop bits
    
transmitBit:
    push bc
    push af
    
    ld a, 0xFE
    ld h, d
    ld l, e
    ld bc, 0xBFFD
    jp nc, transmitOne
    
    ; Transmit Zero (bit 3 = 0):
    and 0xF7
    out (c), a
    jr transmitNext
    
transmitOne:
    ; Transmit One (bit 3 = 1):
    or 0x08
    out (c), a
    jr transmitNext
    
transmitNext:
    dec hl
    ld a, h
    or l
    jr nz, transmitNext
    
    nop
    nop
    nop
    
    pop af
    pop bc
    or a
    rra                     ; Rotate right through carry
    djnz transmitBit
    
    ei
    
    pop af
    pop bc
    pop de
    pop hl
    ret

;; ============================================================
;; ay_uart_ready - Check if data available
;; Output: L = 1 if ready, 0 if not
;; ============================================================
_ay_uart_ready:
    ; Check cached byte first
    ld a, (_isSecondByteAvail)
    or a
    jr z, checkPortReady
    ld l, 1
    ret
    
checkPortReady:
    ; Select port A register
    ld bc, 0xFFFD
    ld a, 0x0E
    out (c), a
    
    ; Read port
    ld b, 0xBF
    in a, (c)
    
    ; RX is bit 7, start bit = 0
    and 0x80
    jr nz, notReadyRet
    ld l, 1                 ; Start bit detected
    ret
    
notReadyRet:
    ld l, 0
    ret

;; ============================================================
;; ay_uart_read - Read a byte
;; Output: L = received byte (0 if timeout)
;; ============================================================
_ay_uart_read:
    ; Check cached byte first
    ld a, (_isSecondByteAvail)
    or a
    jr z, startReadByte
    
    ; Return cached byte
    xor a
    ld (_isSecondByteAvail), a
    ld a, (_secondByte)
    ld l, a
    ret

startReadByte:
    di
    
    xor a
    exx
    ld de, (_baud)
    ld hl, (_baud_half)     ; OPTIMIZACIÓN: Carga directa del valor precalculado
    or a
    ld b, 0xFA              ; Wait loop length (timeout)
    exx
    
    ld c, 0xFD
    ld d, 0xFF
    ld e, 0xBF
    ld b, d
    
    ld a, 0x0E
    out (c), a
    in a, (c)
    or 0xF0                 ; Input lines to 1
    and 0xFB                ; CTS force to 0
    ld b, e
    out (c), a              ; Update port
    ld h, a                 ; Save port state
    
waitStartBit:
    ld b, d
    in a, (c)
    and 0x80
    jr z, startBitFound
    
readTimeOut:
    exx
    dec b
    exx
    jr nz, waitStartBit
    
    ; Timeout - return 0
    ld l, 0
    ei
    ret

startBitFound:
    ; Verify start bit (debounce)
    in a, (c)
    and 0x80
    jr nz, readTimeOut
    
    in a, (c)
    and 0x80
    jr nz, readTimeOut
    
    ; Start bit confirmed!
    exx
    ld bc, 0xFFFD
    ld a, 0x80
    ex af, af'

readTune:
    add hl, de              ; HL = 1.5 * _baud
    nop
    nop
    nop
    nop                     ; Fine tuning delay

bdDelay:
    dec hl
    ld a, h
    or l
    jr nz, bdDelay
    
    in a, (c)
    and 0x80
    jp z, zeroReceived
    
    ; One received:
    ex af, af'
    scf
    rra
    jr c, receivedByte
    ex af, af'
    jp readTune

zeroReceived:
    ex af, af'
    or a
    rra
    jr c, receivedByte
    ex af, af'
    jp readTune

receivedByte:
    scf
    push af
    exx

readFinish:
    ld a, h
    or 0x04                 ; Set CTS
    ld b, e
    out (c), a
    
    exx
    ld h, d
    ld l, e
    
    ld bc, 0x0007
    or a
    sbc hl, bc

delayForStopBit:
    dec hl
    ld a, h
    or l
    jr nz, delayForStopBit
    
    ld bc, 0xFFFD
    add hl, de
    add hl, de
    add hl, de

waitStartBitSecondByte:
    in a, (c)
    and 0x80
    jr z, secondStartBitFound
    dec hl
    ld a, h
    or l
    jr nz, waitStartBitSecondByte
    
    ; No second byte
    pop af
    ld l, a
    ei
    ret

secondStartBitFound:
    ; Verify
    in a, (c)
    and 0x80
    jr nz, waitStartBitSecondByte
    
    ld h, d
    ld l, e
    ld bc, 0x0002
    srl h
    rr l
    or a
    sbc hl, bc
    ld bc, 0xFFFD
    ld a, 0x80
    ex af, af'

secondByteTune:
    nop
    nop
    nop
    nop
    add hl, de

secondDelay:
    dec hl
    ld a, h
    or l
    jr nz, secondDelay
    
    in a, (c)
    and 0x80
    jr z, secondZeroReceived
    
    ; Second 1 received
    ex af, af'
    scf
    rra
    jr c, secondByteFinished
    ex af, af'
    jp secondByteTune

secondZeroReceived:
    ex af, af'
    or a
    rra
    jr c, secondByteFinished
    ex af, af'
    jp secondByteTune

secondByteFinished:
    ld hl, _isSecondByteAvail
    ld (hl), 1
    inc hl
    ld (hl), a
    
    pop af
    ld l, a
    ei
    ret