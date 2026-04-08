/*
 * wobble_lab.c — Sine wobble variant laboratory
 * Same engine, different shift functions.
 * SPACE to return to menu.
 *
 * Compile: zcc +zx -vn -startup=31 -clib=sdcc_iy -compiler=sdcc -SO3 wobble_lab.c -o wobble_lab -create-app
 */

#include <z80.h>
#include <intrinsic.h>
#include <string.h>

#define SCR_ADDR(y, s, x) ((uint8_t *)(0x4000 + (((y) & 0x18) << 8) + ((s) << 8) + (((y) & 7) << 5) + (x)))
#define ATTR_ADDR(y, x)   ((uint8_t *)(0x5800 + (y) * 32 + (x)))

#define TOP_ROW    6
#define BOT_ROW    7
#define START_COL  6
#define ROM_FONT   0x3C00
#define BRIGHT_BIT 0x40

static const char title[] = "SPECTALK ZX v1.3.7";
#define TITLE_LEN (sizeof(title) - 1)

/* 32-entry sine, values 0-7 */
static const uint8_t sine32[] = {
    4, 5, 6, 6, 7, 7, 7, 6, 6, 5, 4, 3, 2, 1, 1, 0,
    0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 7, 7, 7, 6, 6, 5
};

/* SPACE key check: port 0x7FFE bit 0 */
static uint8_t space(void) { return !(z80_inp(0x7FFE) & 1); }

/* ---- Helpers ---- */

static void clear_screen(void)
{
    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, 0, 768);
}

static void print_rom(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    while (*s) {
        uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)(*s) * 8);
        uint8_t i;
        for (i = 0; i < 8; i++) *SCR_ADDR(row, i, col) = font[i];
        *ATTR_ADDR(row, col) = attr;
        col++; s++;
    }
}

static void draw_sep(uint8_t attr)
{
    uint8_t i;
    memset(SCR_ADDR(BOT_ROW+1, 0, START_COL), 0xFF, TITLE_LEN);
    for (i = 0; i < TITLE_LEN; i++) *ATTR_ADDR(BOT_ROW+1, START_COL+i) = attr;
}

/* Render scanline sl (0-15) of double-height title into buf[32] */
static void render_sl(uint8_t *buf, uint8_t sl)
{
    uint8_t i, fr = sl >> 1;
    memset(buf, 0, 32);
    for (i = 0; i < TITLE_LEN; i++) {
        uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)title[i] * 8);
        buf[START_COL + i] = font[fr];
    }
}

/* Write buf[32] to screen scanline */
static void write_sl(uint8_t *buf, uint8_t cr, uint8_t sc)
{
    memcpy(SCR_ADDR(cr, sc, 0), buf, 32);
}

static void buf_shr(uint8_t *buf, uint8_t n)
{
    uint8_t i, cn = 8 - n;
    for (i = 31; i > 0; i--)
        buf[i] = (buf[i] >> n) | (buf[i-1] << cn);
    buf[0] >>= n;
}

static void buf_shl(uint8_t *buf, uint8_t n)
{
    uint8_t i, cn = 8 - n;
    for (i = 0; i < 31; i++)
        buf[i] = (buf[i] << n) | (buf[i+1] >> cn);
    buf[31] <<= n;
}

/* Apply shift to buffer: positive = right, negative = left, 0 = skip */
static void apply_shift(uint8_t *buf, int8_t sh)
{
    if (sh > 0 && sh <= 7) buf_shr(buf, (uint8_t)sh);
    else if (sh < 0 && sh >= -7) buf_shl(buf, (uint8_t)(-sh));
}

/* Set wide attrs (covers shifts spilling into adjacent columns) */
static void set_wide_attrs(uint8_t at, uint8_t ab)
{
    uint8_t i;
    for (i = 0; i < 32; i++) {
        *ATTR_ADDR(TOP_ROW, i) = at;
        *ATTR_ADDR(BOT_ROW, i) = ab;
    }
}

/* Common effect setup */
static void setup(const char *label, uint8_t ink)
{
    clear_screen();
    set_wide_attrs(ink | BRIGHT_BIT, ink);
    draw_sep(ink);
    print_rom(22, 2, label, ink | BRIGHT_BIT);
    print_rom(23, 9, "SPACE: menu", 0x45);
}

/* Debounced wait for SPACE release */
static void wait_space_release(void)
{
    uint8_t db = 10;
    while (db) {
        intrinsic_halt();
        if (space()) db = 10;
        else db--;
    }
}

static uint8_t wait_key(void)
{
    uint8_t db = 20;
    while (db) {
        intrinsic_halt();
        if ((z80_inp(0xF7FE) & 0x1F) != 0x1F ||
            (z80_inp(0xEFFE) & 0x1F) != 0x1F ||
            !(z80_inp(0x7FFE) & 1)) db = 20;
        else db--;
    }
    for (;;) {
        intrinsic_halt();
        if (!(z80_inp(0xF7FE) & 0x01)) return '1';
        if (!(z80_inp(0xF7FE) & 0x02)) return '2';
        if (!(z80_inp(0xF7FE) & 0x04)) return '3';
        if (!(z80_inp(0xF7FE) & 0x08)) return '4';
        if (!(z80_inp(0xF7FE) & 0x10)) return '5';
        if (!(z80_inp(0xEFFE) & 0x10)) return '6';
        if (!(z80_inp(0xEFFE) & 0x08)) return '7';
        if (!(z80_inp(0xEFFE) & 0x04)) return '8';
        if (!(z80_inp(0xEFFE) & 0x02)) return '9';
        if (!(z80_inp(0xFEFE) & 1) && !(z80_inp(0x7FFE) & 1)) return 0; /* BREAK */
    }
}

/* =====================================================
   THE ENGINE — same for all variants.
   Only the shift calculation differs per effect.
   ===================================================== */

/* Variant 1: SMOOTH WAVE — gentle flowing sine */
static void fx_smooth(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("1:Smooth wave  f*2+sl*4", 0x06);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t sh = (int8_t)sine32[(frame * 2 + sl * 4) & 0x1F] - 4;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 2: FAST RIPPLE — tight, rapid oscillation */
static void fx_ripple(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("2:Fast ripple  f*5+sl*8", 0x05);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t sh = (int8_t)sine32[(frame * 5 + sl * 8) & 0x1F] - 4;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 3: OCEAN SWELL — slow, majestic wave */
static void fx_ocean(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("3:Ocean swell  f+sl*2", 0x01);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t sh = (int8_t)sine32[(frame + sl * 2) & 0x1F] - 4;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 4: SHEAR — top half shifts right, bottom left */
static void fx_shear(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("4:Shear  top/bot opposite", 0x02);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t sh = (int8_t)sine32[(frame * 3 + sl * 3) & 0x1F] - 4;
            if (sl >= 8) sh = -sh;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 5: INTERFERENCE — two sines added, moiré pattern */
static void fx_interference(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("5:Interference  sin+sin", 0x03);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t s1 = (int8_t)sine32[(frame * 2 + sl * 3) & 0x1F] - 4;
            int8_t s2 = (int8_t)sine32[(frame * 3 + sl * 7) & 0x1F] - 4;
            int8_t sh = (s1 + s2) >> 1;  /* average: stays in -4..+3 */
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 6: DAMPENING — wild oscillation that calms down */
static void fx_dampen(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];
    uint8_t amp;

    setup("6:Dampening  wild->calm", 0x04);
    wait_space_release();

    for (frame = 0; ; frame++) {
        /* amplitude decays over 128 frames, then resets */
        { uint8_t phase = frame & 0x7F;
          if      (phase < 20) amp = 7;
          else if (phase < 40) amp = 6;
          else if (phase < 55) amp = 5;
          else if (phase < 70) amp = 4;
          else if (phase < 85) amp = 3;
          else if (phase < 100) amp = 2;
          else if (phase < 115) amp = 1;
          else amp = 0;
        }

        for (sl = 0; sl < 16; sl++) {
            int8_t raw = (int8_t)sine32[(frame * 4 + sl * 5) & 0x1F] - 4;
            int8_t sh = (raw * (int8_t)amp) / 7;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 7: BREATHING — amplitude pulses in and out */
static void fx_breathe(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("7:Breathing  pulse amp", 0x07);
    wait_space_release();

    for (frame = 0; ; frame++) {
        /* amplitude = sine of frame (slow) */
        uint8_t amp = sine32[(frame >> 2) & 0x1F];  /* 0-7, cycles every 128 frames */

        for (sl = 0; sl < 16; sl++) {
            int8_t raw = (int8_t)sine32[(frame * 3 + sl * 4) & 0x1F] - 4;
            int8_t sh;
            if (amp == 0) sh = 0;
            else sh = (raw * (int8_t)amp) / 7;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 8: ZIGZAG — alternating scanlines shift opposite */
static void fx_zigzag(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("8:Zigzag  alt directions", 0x06);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            int8_t sh = (int8_t)sine32[(frame * 3 + sl * 4) & 0x1F] - 4;
            if (sl & 1) sh = -sh;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* Variant 9: CASCADE — increasing phase delay per scanline */
static void fx_cascade(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];

    setup("9:Cascade  sl*12 delay", 0x03);
    wait_space_release();

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            /* large sl multiplier = big phase delay = cascading wave top to bottom */
            int8_t sh = (int8_t)sine32[(frame * 2 + sl * 12) & 0x1F] - 4;
            render_sl(buf, sl);
            apply_shift(buf, sh);
            write_sl(buf, (sl < 8) ? TOP_ROW : BOT_ROW, (sl < 8) ? sl : sl - 8);
        }
        intrinsic_halt();
        if (space()) return;
    }
}

/* ---- MENU ---- */

static void draw_menu(void)
{
    clear_screen();
    print_rom(0, 5, "SINE WOBBLE LABORATORY", 0x47);
    memset(SCR_ADDR(1, 0, 5), 0xFF, 22);
    { uint8_t i; for (i = 0; i < 22; i++) *ATTR_ADDR(1, 5+i) = 0x46; }

    print_rom(3,  2, "1  Smooth wave",      0x46);
    print_rom(3, 22, "f*2+sl*4",            0x07);
    print_rom(5,  2, "2  Fast ripple",      0x45);
    print_rom(5, 22, "f*5+sl*8",            0x07);
    print_rom(7,  2, "3  Ocean swell",      0x41);
    print_rom(7, 22, "f+sl*2",              0x07);
    print_rom(9,  2, "4  Shear",            0x42);
    print_rom(9, 22, "top/bot inv",         0x07);
    print_rom(11, 2, "5  Interference",     0x43);
    print_rom(11, 22, "sin+sin",            0x07);
    print_rom(13, 2, "6  Dampening",        0x44);
    print_rom(13, 22, "wild->calm",         0x07);
    print_rom(15, 2, "7  Breathing",        0x47);
    print_rom(15, 22, "pulse amp",          0x07);
    print_rom(17, 2, "8  Zigzag",           0x46);
    print_rom(17, 22, "alt dirs",           0x07);
    print_rom(19, 2, "9  Cascade",          0x43);
    print_rom(19, 22, "sl*12 delay",        0x07);

    print_rom(21, 4, "SPACE: back to menu",  0x45);
    print_rom(23, 4, "BREAK from menu: exit", 0x45);
}

void main(void)
{
    intrinsic_ei();
    z80_outp(0xFE, 0);
    for (;;) {
        draw_menu();
        { uint8_t k = wait_key();
          switch (k) {
              case '1': fx_smooth();       break;
              case '2': fx_ripple();       break;
              case '3': fx_ocean();        break;
              case '4': fx_shear();        break;
              case '5': fx_interference(); break;
              case '6': fx_dampen();       break;
              case '7': fx_breathe();      break;
              case '8': fx_zigzag();       break;
              case '9': fx_cascade();      break;
              case 0: return;
          }
        }
    }
}
