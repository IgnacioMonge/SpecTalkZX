/*
 * effects2.c — Spectacular text effects for SpecTalkZX
 * True pixel-level manipulation: sine wobble, bounce physics,
 * roulette decode, smooth scroll, shutter slices, plasma,
 * earthquake, wave fade.
 *
 * Compile: zcc +zx -vn -startup=31 -clib=sdcc_iy -compiler=sdcc -SO3 effects2.c -o effects2 -create-app
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
#define TARGET_PY  (TOP_ROW * 8)   /* 48 */

static const char title[] = "SPECTALK ZX v1.3.7";
#define TITLE_LEN (sizeof(title) - 1)

/* 32-entry sine, values 0-7 */
static const uint8_t sine32[] = {
    4, 5, 6, 6, 7, 7, 7, 6, 6, 5, 4, 3, 2, 1, 1, 0,
    0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 7, 7, 7, 6, 6, 5
};

static uint16_t rng_state = 44444;
static uint8_t rng8(void)
{
    rng_state = rng_state * 25173 + 13849;
    return (uint8_t)(rng_state >> 8);
}

/* ---- Common helpers ---- */

static void clear_screen(void)
{
    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, 0, 768);
}

static void draw_big_char(uint8_t col, uint8_t ch)
{
    uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)ch * 8);
    uint8_t i;
    for (i = 0; i < 4; i++) {
        uint8_t b = font[i];
        *SCR_ADDR(TOP_ROW, i*2,   col) = b;
        *SCR_ADDR(TOP_ROW, i*2+1, col) = b;
    }
    for (i = 0; i < 4; i++) {
        uint8_t b = font[i+4];
        *SCR_ADDR(BOT_ROW, i*2,   col) = b;
        *SCR_ADDR(BOT_ROW, i*2+1, col) = b;
    }
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

static void draw_title_pixels(void)
{
    uint8_t i;
    for (i = 0; i < TITLE_LEN; i++)
        draw_big_char(START_COL + i, title[i]);
}

static void set_title_attrs(uint8_t at, uint8_t ab)
{
    uint8_t i;
    for (i = 0; i < TITLE_LEN; i++) {
        *ATTR_ADDR(TOP_ROW, START_COL+i) = at;
        *ATTR_ADDR(BOT_ROW, START_COL+i) = ab;
    }
}

static void draw_sep(uint8_t attr)
{
    uint8_t i;
    memset(SCR_ADDR(BOT_ROW+1, 0, START_COL), 0xFF, TITLE_LEN);
    for (i = 0; i < TITLE_LEN; i++) *ATTR_ADDR(BOT_ROW+1, START_COL+i) = attr;
}

static uint8_t brk(void) { return (!(z80_inp(0xFEFE) & 1) && !(z80_inp(0x7FFE) & 1)); }

static uint8_t wait_key(void)
{
    uint8_t db = 20;
    while (db) {
        intrinsic_halt();
        if ((z80_inp(0xF7FE) & 0x1F) != 0x1F || (z80_inp(0xEFFE) & 0x1F) != 0x1F) db = 20;
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
        if (brk()) return 0;
    }
}

/* ---- Pixel-level helpers ---- */

/* Render one scanline (sl 0-15) of double-height title into buf[32] */
static void render_sl(uint8_t *buf, uint8_t sl)
{
    uint8_t i, fr = sl >> 1;
    memset(buf, 0, 32);
    for (i = 0; i < TITLE_LEN; i++) {
        uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)title[i] * 8);
        buf[START_COL + i] = font[fr];
    }
}

/* Render title scanline at column offset (int8_t, can be negative) */
static void render_sl_at(uint8_t *buf, uint8_t sl, int8_t cdelta)
{
    uint8_t i, fr = sl >> 1;
    int8_t c;
    memset(buf, 0, 32);
    for (i = 0; i < TITLE_LEN; i++) {
        c = (int8_t)(START_COL + i) + cdelta;
        if (c >= 0 && c < 32) {
            uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)title[i] * 8);
            buf[(uint8_t)c] = font[fr];
        }
    }
}

/* Write buf[32] to one screen scanline */
static void write_sl(uint8_t *buf, uint8_t cr, uint8_t sc)
{
    memcpy(SCR_ADDR(cr, sc, 0), buf, 32);
}

/* Bit-shift buf[32] right by n pixels (1-7) */
static void buf_shr(uint8_t *buf, uint8_t n)
{
    uint8_t i, cn = 8 - n;
    for (i = 31; i > 0; i--)
        buf[i] = (buf[i] >> n) | (buf[i-1] << cn);
    buf[0] >>= n;
}

/* Bit-shift buf[32] left by n pixels (1-7) */
static void buf_shl(uint8_t *buf, uint8_t n)
{
    uint8_t i, cn = 8 - n;
    for (i = 0; i < 31; i++)
        buf[i] = (buf[i] << n) | (buf[i+1] >> cn);
    buf[31] <<= n;
}

/* Draw double-height char at arbitrary pixel row py */
static void draw_at_py(uint8_t py, uint8_t col, uint8_t ch)
{
    uint8_t *font = (uint8_t *)(ROM_FONT + (uint16_t)ch * 8);
    uint8_t i, pr;
    for (i = 0; i < 16; i++) {
        pr = py + i;
        if (pr < 192)
            *SCR_ADDR(pr >> 3, pr & 7, col) = font[i >> 1];
    }
}

/* Clear 16-pixel column at pixel row py */
static void clear_at_py(uint8_t py, uint8_t col)
{
    uint8_t i, pr;
    for (i = 0; i < 16; i++) {
        pr = py + i;
        if (pr < 192)
            *SCR_ADDR(pr >> 3, pr & 7, col) = 0;
    }
}

/* Set attrs covering char rows for py..py+15 */
static void attr_at_py(uint8_t py, uint8_t col, uint8_t attr)
{
    uint8_t r, rs = py >> 3, re = (py + 15) >> 3;
    for (r = rs; r <= re && r < 24; r++)
        *ATTR_ADDR(r, col) = attr;
}

/* ========== EFFECT 1: TRUE SINE WOBBLE ========== */
/* Per-pixel horizontal sine distortion using bit-shifting.
   Each of the 16 scanlines shifts independently.             */

static void fx_sinewobble(void)
{
    uint8_t frame, sl;
    uint8_t buf[32];
    uint8_t i;

    clear_screen();
    for (i = 0; i < 32; i++) {
        *ATTR_ADDR(TOP_ROW, i) = 0x46;
        *ATTR_ADDR(BOT_ROW, i) = 0x06;
    }
    draw_sep(0x06);
    print_rom(22, 4, "1:Sine wobble", 0x46);

    for (frame = 0; ; frame++) {
        for (sl = 0; sl < 16; sl++) {
            uint8_t cr = (sl < 8) ? TOP_ROW : BOT_ROW;
            uint8_t sc = (sl < 8) ? sl : (sl - 8);
            uint8_t ph = (frame * 3 + sl * 5) & 0x1F;
            int8_t sh = (int8_t)sine32[ph] - 4;

            render_sl(buf, sl);

            if (sh > 0) buf_shr(buf, (uint8_t)sh);
            else if (sh < 0) buf_shl(buf, (uint8_t)(-sh));

            write_sl(buf, cr, sc);
        }
        intrinsic_halt();
        if (brk()) return;
    }
}

/* ========== EFFECT 2: GRAVITY BOUNCE ========== */
/* Characters drop from top with physics, pixel-precise.
   Elastic bounce at target with 50% damping. Staggered start. */

static void fx_bounce(void)
{
    uint16_t y_fp[18];     /* 12.4 fixed-point pixel y */
    int16_t vy_fp[18];     /* 12.4 fixed-point velocity */
    uint8_t st[18];        /* 0=wait 1=active 2=settled */
    uint8_t opy[18];       /* previous pixel y for erase */
    uint8_t frame, i, f;
    uint16_t tgt = (uint16_t)TARGET_PY << 4;  /* 768 */
    uint8_t ndone;

    clear_screen();
    draw_sep(0x44);
    print_rom(22, 4, "2:Gravity bounce", 0x44);

    for (i = 0; i < TITLE_LEN; i++) {
        y_fp[i] = 0; vy_fp[i] = 0; st[i] = 0; opy[i] = 0;
    }

    for (frame = 0; ; frame++) {
        intrinsic_halt();

        /* activate next char every 2 frames */
        { uint8_t idx = frame >> 1;
          if (idx < TITLE_LEN && st[idx] == 0) {
              st[idx] = 1; y_fp[idx] = 0; vy_fp[idx] = 0;
          }
        }

        ndone = 0;
        for (i = 0; i < TITLE_LEN; i++) {
            uint8_t col, npy;
            int16_t ny;

            if (st[i] == 0) continue;
            if (st[i] == 2) { ndone++; continue; }

            col = START_COL + i;

            /* gravity */
            vy_fp[i] += 12;

            /* update pos */
            ny = (int16_t)y_fp[i] + vy_fp[i];
            if (ny < 0) ny = 0;

            if (ny >= (int16_t)tgt) {
                ny = (int16_t)tgt;
                vy_fp[i] = -(vy_fp[i] >> 1);
                if (vy_fp[i] > -16 && vy_fp[i] < 16) {
                    st[i] = 2; ndone++;
                }
            }
            y_fp[i] = (uint16_t)ny;
            npy = (uint8_t)(y_fp[i] >> 4);

            if (npy != opy[i] || frame == (i << 1)) {
                /* erase old */
                clear_at_py(opy[i], col);
                attr_at_py(opy[i], col, 0);

                /* draw new */
                draw_at_py(npy, col, title[i]);
                attr_at_py(npy, col, (st[i] == 2) ? 0x45 : 0x44);

                opy[i] = npy;
            }
        }

        if (ndone >= TITLE_LEN) break;
        if (brk()) return;
    }

    /* settle flash */
    set_title_attrs(0x47, 0x47);
    for (f = 0; f < 3; f++) intrinsic_halt();
    set_title_attrs(0x45, 0x05);
    draw_sep(0x05);

    for (f = 0; f < 100; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ========== EFFECT 3: ROULETTE DECODE ========== */
/* All positions spin random chars simultaneously.
   Lock left-to-right with bright flash on lock.               */

static void fx_roulette(void)
{
    uint8_t lockf[18];
    uint8_t locked[18];
    uint8_t frame, i, f;
    uint8_t alldone;

    clear_screen();
    set_title_attrs(0x44, 0x04);
    draw_sep(0x04);
    print_rom(22, 4, "3:Roulette decode", 0x04);

    for (i = 0; i < TITLE_LEN; i++) {
        lockf[i] = 30 + i * 6;
        locked[i] = 0;
    }

    for (frame = 0; ; frame++) {
        intrinsic_halt();
        alldone = 1;

        for (i = 0; i < TITLE_LEN; i++) {
            uint8_t col = START_COL + i;

            if (locked[i]) continue;
            alldone = 0;

            if (frame >= lockf[i]) {
                locked[i] = 1;
                draw_big_char(col, title[i]);
                *ATTR_ADDR(TOP_ROW, col) = 0x47;
                *ATTR_ADDR(BOT_ROW, col) = 0x47;
            } else if (frame >= lockf[i] - 12) {
                if (frame & 1)
                    draw_big_char(col, (rng8() & 0x3F) + 33);
            } else {
                draw_big_char(col, (rng8() & 0x3F) + 33);
            }
        }

        /* restore flash to green after 2 frames */
        for (i = 0; i < TITLE_LEN; i++) {
            if (locked[i] && frame == lockf[i] + 2) {
                uint8_t col = START_COL + i;
                *ATTR_ADDR(TOP_ROW, col) = 0x44;
                *ATTR_ADDR(BOT_ROW, col) = 0x04;
            }
        }

        if (alldone) break;
        if (brk()) return;
    }

    for (f = 0; f < 100; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ========== EFFECT 4: SMOOTH PIXEL SCROLL-IN ========== */
/* Text slides in from right edge, pixel by pixel.
   Decelerates as it approaches final position.               */

static void fx_scrollin(void)
{
    uint16_t off;
    uint8_t sl, i, f;
    uint8_t buf[32];

    clear_screen();
    for (i = 0; i < 32; i++) {
        *ATTR_ADDR(TOP_ROW, i) = 0x45;
        *ATTR_ADDR(BOT_ROW, i) = 0x05;
    }
    draw_sep(0x05);
    print_rom(22, 4, "4:Smooth scroll-in", 0x45);

    for (off = 208; off > 0; ) {
        { uint8_t co = (uint8_t)(off >> 3);
          uint8_t sp = (uint8_t)(off & 7);

          for (sl = 0; sl < 16; sl++) {
              uint8_t cr = (sl < 8) ? TOP_ROW : BOT_ROW;
              uint8_t sc = (sl < 8) ? sl : (sl - 8);

              render_sl_at(buf, sl, (int8_t)co);
              if (sp > 0) buf_shr(buf, sp);

              write_sl(buf, cr, sc);
          }
        }

        intrinsic_halt();

        if (off > 100) off -= 4;
        else if (off > 40) off -= 3;
        else if (off > 10) off -= 2;
        else off -= 1;

        if (brk()) return;
    }

    /* final clean draw */
    for (sl = 0; sl < 16; sl++) {
        uint8_t cr = (sl < 8) ? TOP_ROW : BOT_ROW;
        uint8_t sc = (sl < 8) ? sl : (sl - 8);
        render_sl(buf, sl);
        write_sl(buf, cr, sc);
    }

    for (f = 0; f < 120; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ========== EFFECT 5: SHUTTER SLICES ========== */
/* 16 scanlines slide in from alternating sides.
   Staggered start, deceleration, dramatic reveal.            */

static void fx_shutter(void)
{
    uint8_t sl_off[16];
    uint8_t sl_dir[16];
    uint8_t sl_dly[16];
    uint8_t frame, sl, i, f;
    uint8_t buf[32];
    uint8_t alldone;

    clear_screen();
    for (i = 0; i < 32; i++) {
        *ATTR_ADDR(TOP_ROW, i) = 0x43;
        *ATTR_ADDR(BOT_ROW, i) = 0x03;
    }
    draw_sep(0x03);
    print_rom(22, 4, "5:Shutter slices", 0x43);

    for (sl = 0; sl < 16; sl++) {
        sl_dir[sl] = sl & 1;
        sl_off[sl] = 200;
        sl_dly[sl] = sl * 3;
    }

    for (frame = 0; ; frame++) {
        intrinsic_halt();
        alldone = 1;

        for (sl = 0; sl < 16; sl++) {
            uint8_t cr = (sl < 8) ? TOP_ROW : BOT_ROW;
            uint8_t sc = (sl < 8) ? sl : (sl - 8);
            int8_t cd;
            uint8_t sp, speed;

            if (sl_dly[sl] > 0) { sl_dly[sl]--; alldone = 0; continue; }
            if (sl_off[sl] == 0) continue;

            alldone = 0;

            speed = (sl_off[sl] > 100) ? 8 : (sl_off[sl] > 40) ? 5 : 3;
            if (sl_off[sl] <= speed) sl_off[sl] = 0;
            else sl_off[sl] -= speed;

            if (sl_off[sl] == 0) {
                render_sl(buf, sl);
            } else {
                cd = (int8_t)(sl_off[sl] >> 3);
                sp = sl_off[sl] & 7;

                if (sl_dir[sl] == 0) {
                    render_sl_at(buf, sl, -cd);
                    if (sp > 0) buf_shl(buf, sp);
                } else {
                    render_sl_at(buf, sl, cd);
                    if (sp > 0) buf_shr(buf, sp);
                }
            }
            write_sl(buf, cr, sc);
        }

        if (alldone) break;
        if (brk()) return;
    }

    for (f = 0; f < 120; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ========== EFFECT 6: FULL-SCREEN PLASMA ========== */
/* All 672 attribute cells cycle with sine-based color.
   Text visible through pixel pattern. Psychedelic.            */

static void fx_plasma(void)
{
    uint8_t frame, r, c;
    uint8_t col_v[32];

    clear_screen();
    draw_title_pixels();
    draw_sep(0x07);
    print_rom(22, 4, "6:Full-screen plasma", 0x07);

    for (frame = 0; ; frame++) {
        /* precompute column sine values */
        for (c = 0; c < 32; c++)
            col_v[c] = sine32[(c * 2 + frame * 2) & 0x1F];

        for (r = 0; r < 21; r++) {
            uint8_t v1 = sine32[(r * 3 + frame) & 0x1F];
            for (c = 0; c < 32; c++) {
                uint8_t val = (v1 + col_v[c]) >> 1;
                uint8_t ink = val & 7;
                uint8_t paper = (val + 4) & 7;
                uint8_t bright = (sine32[((r + c) + frame * 3) & 0x1F] > 3) ? BRIGHT_BIT : 0;
                *ATTR_ADDR(r, c) = (paper << 3) | ink | bright;
            }
        }
        intrinsic_halt();
        if (brk()) return;
    }
}

/* ========== EFFECT 7: EARTHQUAKE ========== */
/* Per-scanline random horizontal jitter with bit-shifting.
   Amplitude decays, with surprise aftershocks.                */

static void fx_earthquake(void)
{
    uint8_t frame, sl, f;
    uint8_t buf[32];
    uint8_t amp;
    uint8_t i;

    clear_screen();
    for (i = 0; i < 32; i++) {
        *ATTR_ADDR(TOP_ROW, i) = 0x42;
        *ATTR_ADDR(BOT_ROW, i) = 0x02;
    }
    draw_sep(0x02);
    print_rom(22, 4, "7:Earthquake", 0x42);

    for (frame = 0; frame < 140; frame++) {
        /* amplitude progression with aftershocks */
        if      (frame < 10) amp = 7;
        else if (frame < 20) amp = 6;
        else if (frame < 30) amp = 4;
        else if (frame < 40) amp = 2;
        else if (frame < 50) amp = 1;
        else if (frame < 55) amp = 0;   /* brief calm */
        else if (frame < 65) amp = 6;   /* aftershock 1! */
        else if (frame < 75) amp = 3;
        else if (frame < 85) amp = 1;
        else if (frame < 92) amp = 0;   /* calm again */
        else if (frame < 100) amp = 4;  /* aftershock 2 */
        else if (frame < 110) amp = 2;
        else if (frame < 120) amp = 1;
        else amp = 0;

        for (sl = 0; sl < 16; sl++) {
            uint8_t cr = (sl < 8) ? TOP_ROW : BOT_ROW;
            uint8_t sc = (sl < 8) ? sl : (sl - 8);

            render_sl(buf, sl);

            if (amp > 0) {
                int8_t sh = (int8_t)(rng8() % (amp * 2 + 1)) - (int8_t)amp;
                if (sh > 0 && sh <= 7) buf_shr(buf, (uint8_t)sh);
                else if (sh < 0 && sh >= -7) buf_shl(buf, (uint8_t)(-sh));
            }
            write_sl(buf, cr, sc);
        }

        intrinsic_halt();
        if (brk()) return;
    }

    /* hold clean */
    for (f = 0; f < 100; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ========== EFFECT 8: WAVE FADE ========== */
/* Bit-by-bit attribute build-up as a traveling wave.
   Progressive color reveal L→R, fade-out R→L, cycles colors.  */

static void fx_wavefade(void)
{
    uint8_t frame, i, f;
    uint8_t pass;
    static const uint8_t tgt_t[] = {0x47, 0x46, 0x45};
    static const uint8_t tgt_b[] = {0x07, 0x06, 0x05};

    clear_screen();
    draw_title_pixels();
    draw_sep(0x07);
    print_rom(22, 4, "8:Wave fade", 0x47);
    set_title_attrs(0x00, 0x00);

    for (pass = 0; pass < 3; pass++) {
        uint8_t tt = tgt_t[pass], tb = tgt_b[pass];

        /* fade in: wave L → R */
        for (frame = 0; frame < TITLE_LEN + 7; frame++) {
            for (i = 0; i < TITLE_LEN; i++) {
                int8_t p = (int8_t)frame - (int8_t)i;
                uint8_t mask;
                if (p <= 0) mask = 0x00;
                else if (p >= 7) mask = 0x7F;
                else mask = (uint8_t)((1 << p) - 1);
                *ATTR_ADDR(TOP_ROW, START_COL + i) = tt & mask;
                *ATTR_ADDR(BOT_ROW, START_COL + i) = tb & mask;
            }
            for (f = 0; f < 3; f++) { intrinsic_halt(); if (brk()) return; }
        }

        /* hold */
        for (f = 0; f < 40; f++) { intrinsic_halt(); if (brk()) return; }

        /* fade out: wave R → L */
        for (frame = 0; frame < TITLE_LEN + 7; frame++) {
            for (i = 0; i < TITLE_LEN; i++) {
                int8_t p = (int8_t)frame - (int8_t)(TITLE_LEN - 1 - i);
                uint8_t mask;
                if (p <= 0) mask = 0x7F;
                else if (p >= 7) mask = 0x00;
                else mask = (uint8_t)(0x7F >> p);
                *ATTR_ADDR(TOP_ROW, START_COL + i) = tt & mask;
                *ATTR_ADDR(BOT_ROW, START_COL + i) = tb & mask;
            }
            for (f = 0; f < 3; f++) { intrinsic_halt(); if (brk()) return; }
        }

        /* brief black pause */
        for (f = 0; f < 15; f++) { intrinsic_halt(); if (brk()) return; }
    }

    /* final fade in: bright white */
    for (frame = 0; frame < TITLE_LEN + 7; frame++) {
        for (i = 0; i < TITLE_LEN; i++) {
            int8_t p = (int8_t)frame - (int8_t)i;
            uint8_t mask;
            if (p <= 0) mask = 0x00;
            else if (p >= 7) mask = 0x7F;
            else mask = (uint8_t)((1 << p) - 1);
            *ATTR_ADDR(TOP_ROW, START_COL + i) = 0x47 & mask;
            *ATTR_ADDR(BOT_ROW, START_COL + i) = 0x07 & mask;
        }
        for (f = 0; f < 3; f++) { intrinsic_halt(); if (brk()) return; }
    }

    for (f = 0; f < 80; f++) { intrinsic_halt(); if (brk()) return; }
}

/* ---- MENU ---- */

static void draw_menu(void)
{
    clear_screen();
    print_rom(1, 5, "SPECTALK ZX EFFECTS II", 0x47);
    memset(SCR_ADDR(2, 0, 5), 0xFF, 22);
    { uint8_t i; for (i = 0; i < 22; i++) *ATTR_ADDR(2, 5+i) = 0x46; }

    print_rom(4,  4, "1  Sine wobble",       0x46);
    print_rom(6,  4, "2  Gravity bounce",    0x44);
    print_rom(8,  4, "3  Roulette decode",   0x44);
    print_rom(10, 4, "4  Smooth scroll-in",  0x45);
    print_rom(12, 4, "5  Shutter slices",    0x43);
    print_rom(14, 4, "6  Full-screen plasma",0x42);
    print_rom(16, 4, "7  Earthquake",        0x42);
    print_rom(18, 4, "8  Wave fade",         0x47);

    print_rom(21, 6, "BREAK: back to menu",  0x45);
    print_rom(23, 6, "BREAK from menu: exit", 0x45);
}

void main(void)
{
    intrinsic_ei();
    z80_outp(0xFE, 0);
    for (;;) {
        draw_menu();
        { uint8_t k = wait_key();
          switch (k) {
              case '1': fx_sinewobble(); break;
              case '2': fx_bounce();     break;
              case '3': fx_roulette();   break;
              case '4': fx_scrollin();   break;
              case '5': fx_shutter();    break;
              case '6': fx_plasma();     break;
              case '7': fx_earthquake(); break;
              case '8': fx_wavefade();   break;
              case 0: return;
          }
        }
    }
}
