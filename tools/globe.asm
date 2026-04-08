; ═══════════════════════════════════════════════════════════════
; EARTH — Globo terráqueo con terminador día/noche curvo
; ZX Spectrum 48K — 109 celdas (81 día, 28 noche)
; ═══════════════════════════════════════════════════════════════
;
; Mapa: NASA Blue Marble rasterizado a 64×32.
; Render: solo atributos (pixel RAM con dither $55 fijo).
; Sombra: terminador curvo precomputado con dot(normal, sol).
;   Sol a 60° del eje de visión → (0.866, 0, 0.5).
;   Bit 7 del lon_offset = 1 si noche (dot ≤ 0).
;   El terminador sigue la curvatura de la esfera y queda
;   fijo en pantalla; los continentes rotan por debajo.
;
;   Día:   agua=$09 (azul sólido), tierra=$66 (verde bright)
;   Noche: agua=$01 (azul tenue),  tierra=$04 (verde tenue)
;
; ~220 T/celda × 109 ≈ 34% frame @ 50fps
;
; Assembler: sjasmplus / pasmo
; ═══════════════════════════════════════════════════════════════

WATER_DAY   equ     $09     ; INK 1 PAPER 1             azul sólido
LAND_DAY    equ     $66     ; BRIGHT, INK 6 PAPER 4     verde/amarillo
WATER_NIGHT equ     $01     ; INK 1 PAPER 0             azul tenue
LAND_NIGHT  equ     $04     ; INK 4 PAPER 0             verde tenue

            org     $8000

; ─── Entrada ─────────────────────────────────────────────────
entry:
            call    init

main_loop:
            halt                        ; espera vsync

            call    render

            ; Rotación del globo (mod 64, cada frame)
            ; El sol es fijo; la Tierra rota debajo del terminador.
            ld      hl, rotation
            inc     (hl)
            ld      a, (hl)
            and     63
            ld      (hl), a

            ; Comprobar SPACE
            ld      a, $7F
            in      a, ($FE)
            rra
            jr      c, main_loop

            ret


; ─── Inicialización ──────────────────────────────────────────
init:
            ; Borde negro
            xor     a
            out     ($FE), a

            ; Pixel RAM ← dither $55
            ld      hl, $4000
            ld      (hl), $55
            ld      de, $4001
            ld      bc, 6143
            ldir

            ; Atributos ← negro (dither oculto)
            ld      hl, $5800
            ld      (hl), $00
            ld      de, $5801
            ld      bc, 767
            ldir

            ; Borde azul oscuro
            ld      a, 1
            out     ($FE), a

            ; Título "EARTH" en fila 1, col 12
            ld      a, 22  : rst $10    ; AT
            ld      a, 1   : rst $10    ; fila
            ld      a, 12  : rst $10    ; col
            ld      a, 16  : rst $10    ; INK
            ld      a, 5   : rst $10    ; cyan
            ld      a, 19  : rst $10    ; BRIGHT
            ld      a, 1   : rst $10    ; on
            ld      a, 'E' : rst $10
            ld      a, 'A' : rst $10
            ld      a, 'R' : rst $10
            ld      a, 'T' : rst $10
            ld      a, 'H' : rst $10

            ; "SPC TO EXIT" en fila 21, col 10
            ld      a, 22  : rst $10
            ld      a, 21  : rst $10
            ld      a, 10  : rst $10
            ld      a, 16  : rst $10    ; INK
            ld      a, 7   : rst $10    ; blanco
            ld      a, 19  : rst $10    ; BRIGHT
            ld      a, 0   : rst $10    ; off
            ld      a, 'S' : rst $10
            ld      a, 'P' : rst $10
            ld      a, 'C' : rst $10
            ld      a, ' ' : rst $10
            ld      a, 'T' : rst $10
            ld      a, 'O' : rst $10
            ld      a, ' ' : rst $10
            ld      a, 'E' : rst $10
            ld      a, 'X' : rst $10
            ld      a, 'I' : rst $10
            ld      a, 'T' : rst $10

            ret


; ─── Render (terminador curvo día/noche) ─────────────────────
;
; Tabla de celdas (5 bytes/celda):
;   +0,+1   dirección de atributo ($5800-$5AFF)
;   +2,+3   base del mapa (world_map + fila×64)
;   +4      lon_offset: bits 0-5 = longitud en la esfera
;                        bit 7   = 1 si noche (dot product ≤ 0)
;
; Fin de tabla: dirección = $0000.
;
; El shadow bit se precomputa en Python con:
;   dot = nx·sun_x + nz·sun_z
; donde (nx,nz) es la normal de superficie en cada celda.
; La curva del terminador surge naturalmente de la geometría
; esférica (nz varía con la posición en la esfera).
; ─────────────────────────────────────────────────────────────
render:
            ld      a, (rotation)
            ld      (rot_smc + 1), a    ; auto-modificar ADD

            ld      hl, cell_table

.loop:
            ld      e, (hl)             ; attr addr lo
            inc     hl
            ld      d, (hl)             ; attr addr hi
            inc     hl
            ld      a, d
            or      a
            ret     z                   ; fin de tabla

            ld      c, (hl)             ; map_base lo
            inc     hl
            ld      b, (hl)             ; map_base hi
            inc     hl
            ld      a, (hl)             ; lon_offset (bit 7 = sombra)
            inc     hl

            push    hl                  ; ── save ptr tabla
            push    de                  ; ── save attr addr (D libre)

            ld      d, a               ; D = lon_offset con shadow bit

            ; Map lookup: limpiar bit 7, sumar rotación
            and     63                  ; quitar shadow bit
rot_smc:
            add     a, 0                ; ← auto-modificado con rotación
            and     63                  ; mod 64
            ld      l, a
            ld      h, 0
            add     hl, bc              ; HL = world_map + fila*64 + col
            ld      b, (hl)             ; B = tierra/agua (0 o 1)

            ; Test sombra: copiar D a C antes de que POP DE lo machaque
            ld      c, d                ; C = shadow byte (bit 7 intacto)
            pop     de                  ; DE = attr addr (D machacado)
            bit     7, c                ; ¿noche? (dot product ≤ 0)
            jr      nz, .night

            ; ── DÍA ──
            ld      a, b
            or      a
            ld      a, WATER_DAY        ; $09
            jr      z, .write
            ld      a, LAND_DAY         ; $66
            jr      .write

            ; ── NOCHE ──
.night:
            ld      a, b
            or      a
            ld      a, WATER_NIGHT      ; $01
            jr      z, .write
            ld      a, LAND_NIGHT       ; $04

.write:
            ld      (de), a             ; escribir atributo
            pop     hl                  ; restaurar ptr tabla
            jr      .loop


; ─── Datos ───────────────────────────────────────────────────

rotation:
            db      $20                 ; inicio = 32 → Greenwich


; ─── Tabla de celdas (109 × 5 bytes) ────────────────
;
; Formato: dw attr_addr, map_base  /  db lon_offset|shadow
; shadow bit codificado en bit 7 del lon_offset.

cell_table:
    dw $58ED, world_map+320    ; scr( 7,13) lon=53 N
    db $B5
    dw $58EE, world_map+320    ; scr( 7,14) lon=58 N
    db $BA
    dw $58EF, world_map+320    ; scr( 7,15) lon=61 D
    db $3D
    dw $58F0, world_map+320    ; scr( 7,16) lon= 0 D
    db $00
    dw $58F1, world_map+320    ; scr( 7,17) lon= 3 D
    db $03
    dw $58F2, world_map+320    ; scr( 7,18) lon= 6 D
    db $06
    dw $58F3, world_map+320    ; scr( 7,19) lon=11 D
    db $0B
    dw $590C, world_map+512    ; scr( 8,12) lon=53 N
    db $B5
    dw $590D, world_map+512    ; scr( 8,13) lon=57 N
    db $B9
    dw $590E, world_map+512    ; scr( 8,14) lon=60 D
    db $3C
    dw $590F, world_map+512    ; scr( 8,15) lon=62 D
    db $3E
    dw $5910, world_map+512    ; scr( 8,16) lon= 0 D
    db $00
    dw $5911, world_map+512    ; scr( 8,17) lon= 2 D
    db $02
    dw $5912, world_map+512    ; scr( 8,18) lon= 4 D
    db $04
    dw $5913, world_map+512    ; scr( 8,19) lon= 7 D
    db $07
    dw $5914, world_map+512    ; scr( 8,20) lon=11 D
    db $0B
    dw $592B, world_map+640    ; scr( 9,11) lon=51 N
    db $B3
    dw $592C, world_map+640    ; scr( 9,12) lon=56 N
    db $B8
    dw $592D, world_map+640    ; scr( 9,13) lon=58 N
    db $BA
    dw $592E, world_map+640    ; scr( 9,14) lon=60 D
    db $3C
    dw $592F, world_map+640    ; scr( 9,15) lon=63 D
    db $3F
    dw $5930, world_map+640    ; scr( 9,16) lon= 0 D
    db $00
    dw $5931, world_map+640    ; scr( 9,17) lon= 1 D
    db $01
    dw $5932, world_map+640    ; scr( 9,18) lon= 4 D
    db $04
    dw $5933, world_map+640    ; scr( 9,19) lon= 6 D
    db $06
    dw $5934, world_map+640    ; scr( 9,20) lon= 8 D
    db $08
    dw $5935, world_map+640    ; scr( 9,21) lon=13 D
    db $0D
    dw $594B, world_map+768    ; scr(10,11) lon=53 N
    db $B5
    dw $594C, world_map+768    ; scr(10,12) lon=56 N
    db $B8
    dw $594D, world_map+768    ; scr(10,13) lon=59 N
    db $BB
    dw $594E, world_map+768    ; scr(10,14) lon=61 D
    db $3D
    dw $594F, world_map+768    ; scr(10,15) lon=63 D
    db $3F
    dw $5950, world_map+768    ; scr(10,16) lon= 0 D
    db $00
    dw $5951, world_map+768    ; scr(10,17) lon= 1 D
    db $01
    dw $5952, world_map+768    ; scr(10,18) lon= 3 D
    db $03
    dw $5953, world_map+768    ; scr(10,19) lon= 5 D
    db $05
    dw $5954, world_map+768    ; scr(10,20) lon= 8 D
    db $08
    dw $5955, world_map+768    ; scr(10,21) lon=11 D
    db $0B
    dw $596B, world_map+896    ; scr(11,11) lon=54 N
    db $B6
    dw $596C, world_map+896    ; scr(11,12) lon=57 N
    db $B9
    dw $596D, world_map+896    ; scr(11,13) lon=59 N
    db $BB
    dw $596E, world_map+896    ; scr(11,14) lon=61 D
    db $3D
    dw $596F, world_map+896    ; scr(11,15) lon=63 D
    db $3F
    dw $5970, world_map+896    ; scr(11,16) lon= 0 D
    db $00
    dw $5971, world_map+896    ; scr(11,17) lon= 1 D
    db $01
    dw $5972, world_map+896    ; scr(11,18) lon= 3 D
    db $03
    dw $5973, world_map+896    ; scr(11,19) lon= 5 D
    db $05
    dw $5974, world_map+896    ; scr(11,20) lon= 7 D
    db $07
    dw $5975, world_map+896    ; scr(11,21) lon=10 D
    db $0A
    dw $598B, world_map+1024   ; scr(12,11) lon=54 N
    db $B6
    dw $598C, world_map+1024   ; scr(12,12) lon=57 N
    db $B9
    dw $598D, world_map+1024   ; scr(12,13) lon=59 D
    db $3B
    dw $598E, world_map+1024   ; scr(12,14) lon=61 D
    db $3D
    dw $598F, world_map+1024   ; scr(12,15) lon=63 D
    db $3F
    dw $5990, world_map+1024   ; scr(12,16) lon= 0 D
    db $00
    dw $5991, world_map+1024   ; scr(12,17) lon= 1 D
    db $01
    dw $5992, world_map+1024   ; scr(12,18) lon= 3 D
    db $03
    dw $5993, world_map+1024   ; scr(12,19) lon= 5 D
    db $05
    dw $5994, world_map+1024   ; scr(12,20) lon= 7 D
    db $07
    dw $5995, world_map+1024   ; scr(12,21) lon=10 D
    db $0A
    dw $59AB, world_map+1088   ; scr(13,11) lon=54 N
    db $B6
    dw $59AC, world_map+1088   ; scr(13,12) lon=57 N
    db $B9
    dw $59AD, world_map+1088   ; scr(13,13) lon=59 N
    db $BB
    dw $59AE, world_map+1088   ; scr(13,14) lon=61 D
    db $3D
    dw $59AF, world_map+1088   ; scr(13,15) lon=63 D
    db $3F
    dw $59B0, world_map+1088   ; scr(13,16) lon= 0 D
    db $00
    dw $59B1, world_map+1088   ; scr(13,17) lon= 1 D
    db $01
    dw $59B2, world_map+1088   ; scr(13,18) lon= 3 D
    db $03
    dw $59B3, world_map+1088   ; scr(13,19) lon= 5 D
    db $05
    dw $59B4, world_map+1088   ; scr(13,20) lon= 7 D
    db $07
    dw $59B5, world_map+1088   ; scr(13,21) lon=10 D
    db $0A
    dw $59CB, world_map+1216   ; scr(14,11) lon=53 N
    db $B5
    dw $59CC, world_map+1216   ; scr(14,12) lon=56 N
    db $B8
    dw $59CD, world_map+1216   ; scr(14,13) lon=59 N
    db $BB
    dw $59CE, world_map+1216   ; scr(14,14) lon=61 D
    db $3D
    dw $59CF, world_map+1216   ; scr(14,15) lon=63 D
    db $3F
    dw $59D0, world_map+1216   ; scr(14,16) lon= 0 D
    db $00
    dw $59D1, world_map+1216   ; scr(14,17) lon= 1 D
    db $01
    dw $59D2, world_map+1216   ; scr(14,18) lon= 3 D
    db $03
    dw $59D3, world_map+1216   ; scr(14,19) lon= 5 D
    db $05
    dw $59D4, world_map+1216   ; scr(14,20) lon= 8 D
    db $08
    dw $59D5, world_map+1216   ; scr(14,21) lon=11 D
    db $0B
    dw $59EB, world_map+1344   ; scr(15,11) lon=51 N
    db $B3
    dw $59EC, world_map+1344   ; scr(15,12) lon=56 N
    db $B8
    dw $59ED, world_map+1344   ; scr(15,13) lon=58 N
    db $BA
    dw $59EE, world_map+1344   ; scr(15,14) lon=60 D
    db $3C
    dw $59EF, world_map+1344   ; scr(15,15) lon=63 D
    db $3F
    dw $59F0, world_map+1344   ; scr(15,16) lon= 0 D
    db $00
    dw $59F1, world_map+1344   ; scr(15,17) lon= 1 D
    db $01
    dw $59F2, world_map+1344   ; scr(15,18) lon= 4 D
    db $04
    dw $59F3, world_map+1344   ; scr(15,19) lon= 6 D
    db $06
    dw $59F4, world_map+1344   ; scr(15,20) lon= 8 D
    db $08
    dw $59F5, world_map+1344   ; scr(15,21) lon=13 D
    db $0D
    dw $5A0C, world_map+1472   ; scr(16,12) lon=53 N
    db $B5
    dw $5A0D, world_map+1472   ; scr(16,13) lon=57 N
    db $B9
    dw $5A0E, world_map+1472   ; scr(16,14) lon=60 D
    db $3C
    dw $5A0F, world_map+1472   ; scr(16,15) lon=62 D
    db $3E
    dw $5A10, world_map+1472   ; scr(16,16) lon= 0 D
    db $00
    dw $5A11, world_map+1472   ; scr(16,17) lon= 2 D
    db $02
    dw $5A12, world_map+1472   ; scr(16,18) lon= 4 D
    db $04
    dw $5A13, world_map+1472   ; scr(16,19) lon= 7 D
    db $07
    dw $5A14, world_map+1472   ; scr(16,20) lon=11 D
    db $0B
    dw $5A2D, world_map+1664   ; scr(17,13) lon=53 N
    db $B5
    dw $5A2E, world_map+1664   ; scr(17,14) lon=58 N
    db $BA
    dw $5A2F, world_map+1664   ; scr(17,15) lon=61 D
    db $3D
    dw $5A30, world_map+1664   ; scr(17,16) lon= 0 D
    db $00
    dw $5A31, world_map+1664   ; scr(17,17) lon= 3 D
    db $03
    dw $5A32, world_map+1664   ; scr(17,18) lon= 6 D
    db $06
    dw $5A33, world_map+1664   ; scr(17,19) lon=11 D
    db $0B

    dw $0000                    ; fin de tabla


; ─── Mapa mundial (NASA rasterizado, 64×32) ──────────────────

world_map:
    ;  0: ................................................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ;  1: ................############....................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ;  2: ...............###...#######....................####............
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0
    ;  3: ...###....##########...#####........#.......################....
    db 0,0,0,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0
    ;  4: ...##############..#...##.........##############################
    db 0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
    ;  5: ...##..########...##.............##.#####################...#...
    db 0,0,0,1,1,0,0,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0,0
    ;  6: .........#############.........#.########################.......
    db 0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0
    ;  7: ..........###########...........#########################.......
    db 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0
    ;  8: ..........#########............##..##..################.........
    db 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0
    ;  9: ..........########.............###...################...........
    db 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0
    ; 10: ...........#######............########################..........
    db 0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0
    ; 11: .............##..............########################...........
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0
    ; 12: .............##..............#############...##..##.............
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 13: ................#............############....#...##.............
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 14: ..................###.........###########.......................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 15: ..................#####...........######............#...........
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0
    ; 16: ..................#######.........#####...........#.............
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 17: ..................########........#####..................#......
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0
    ; 18: ...................######.........#####................#........
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0
    ; 19: ...................######.........####..#............#####......
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0
    ; 20: ...................#####...........###..............#######.....
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0
    ; 21: ...................####............##...............#######.....
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0
    ; 22: ...................###...................................#......
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0
    ; 23: ...................##...........................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 24: ...................#............................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 25: ...................#............................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 26: ................................................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 27: ................................................................
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 28: ...................##..........##############################...
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0
    ; 29: ......################.....##################################...
    db 0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0
    ; 30: ################################################################
    db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
    ; 31: ################################################################
    db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1

; ═══════════════════════════════════════════════════════════════
