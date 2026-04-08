/*
 * mockup_ikkle_uses.c — Demonstrate all uses of Ikkle-4 in SpecTalkZX
 * All Ikkle text is UPPERCASE (saves 64 bytes: only chars 32-95 needed).
 * SPACE cycles through demo pages. Any other key exits.
 *
 * Build:
 *   zcc +zx -vn -startup=1 -clib=sdcc_iy -o tools/mockup_ikkle_uses tools/mockup_ikkle_uses.c
 *   z88dk-appmake +zx -b tools/mockup_ikkle_uses_CODE.bin --org 32768 -o tools/mockup_ikkle_uses.tap
 */

#include <stdint.h>
#include <string.h>

// ============================================================================
// Attributes — Default theme
// ============================================================================
#define A_BANNER    0x4F  // BRIGHT WHITE on BLUE
#define A_BAN_DIM   0x0F  // WHITE on BLUE
#define A_MAIN_BG   0x07  // WHITE on BLACK
#define A_MSG_CHAN   0x07  // WHITE on BLACK
#define A_MSG_JOIN   0x03  // MAGENTA on BLACK
#define A_MSG_TOPIC  0x46  // BRIGHT YELLOW on BLACK
#define A_MSG_SYS    0x45  // BRIGHT CYAN on BLACK
#define A_STATUS    0x39  // BLUE on WHITE
#define A_INPUT     0x29  // BLUE on CYAN
#define A_INPUT_BG  0x28  // BLACK on CYAN
#define A_PROMPT    0x29  // BLUE on CYAN
#define A_NOTIF     0x46  // BRIGHT YELLOW on BLACK
#define A_TOPIC     0x45  // BRIGHT CYAN on BLACK
#define A_INFO      0x44  // BRIGHT GREEN on BLACK
#define A_AWAY      0x43  // BRIGHT MAGENTA on BLACK
#define A_PAGIN     0x47  // BRIGHT WHITE on BLACK
#define A_PAGE_LBL  0x42  // BRIGHT RED on BLACK

// ============================================================================
// Our font (for normal 8px text)
// ============================================================================
static const uint8_t font_data[298] = {
    0x00,0x22,0x44,0x55,0x66,0x88,0xAA,0xCC,0xEE,0xFF,0x00,0x00,0x00,0x55,0x55,0x05,
    0x06,0x60,0x00,0x06,0x86,0x86,0x28,0x58,0x18,0x61,0x47,0x56,0x26,0x26,0x74,0x25,
    0x00,0x00,0x25,0x55,0x52,0x52,0x22,0x25,0x06,0x28,0x26,0x02,0x28,0x22,0x00,0x02,
    0x25,0x00,0x08,0x00,0x00,0x00,0x77,0x11,0x22,0x55,0x26,0x68,0x62,0x27,0x22,0x28,
    0x26,0x12,0x58,0x81,0x21,0x62,0x56,0x68,0x11,0x85,0x71,0x17,0x45,0x76,0x62,0x81,
    0x12,0x22,0x26,0x26,0x62,0x26,0x64,0x17,0x00,0x50,0x05,0x02,0x02,0x25,0x01,0x25,
    0x21,0x00,0x80,0x80,0x05,0x21,0x25,0x26,0x12,0x02,0x26,0x88,0x52,0x26,0x68,0x66,
    0x76,0x76,0x67,0x45,0x55,0x54,0x76,0x66,0x67,0x85,0x75,0x58,0x85,0x75,0x55,0x26,
    0x58,0x62,0x66,0x86,0x66,0x82,0x22,0x28,0x41,0x11,0x62,0x66,0x87,0x66,0x55,0x55,
    0x58,0x68,0x88,0x66,0x86,0x66,0x66,0x26,0x66,0x62,0x76,0x67,0x55,0x26,0x66,0x74,
    0x76,0x67,0x66,0x45,0x21,0x62,0x82,0x22,0x22,0x66,0x66,0x68,0x66,0x66,0x62,0x66,
    0x88,0x82,0x66,0x26,0x66,0x66,0x62,0x22,0x81,0x22,0x58,0x75,0x55,0x57,0x55,0x22,
    0x11,0x72,0x22,0x27,0x26,0x00,0x00,0x00,0x00,0x09,0x55,0x20,0x00,0x07,0x14,0x64,
    0x55,0x76,0x67,0x04,0x55,0x54,0x11,0x46,0x64,0x02,0x68,0x54,0x26,0x57,0x55,0x46,
    0x64,0x17,0x55,0x76,0x66,0x20,0x72,0x28,0x10,0x41,0x62,0x55,0x67,0x66,0x55,0x55,
    0x62,0x06,0x88,0x66,0x07,0x66,0x66,0x02,0x66,0x62,0x07,0x67,0x55,0x04,0x64,0x11,
    0x04,0x55,0x55,0x04,0x52,0x17,0x28,0x22,0x21,0x06,0x66,0x68,0x06,0x66,0x62,0x06,
    0x88,0x82,0x06,0x26,0x66,0x06,0x64,0x17,0x08,0x12,0x58,0x42,0x52,0x24,0x22,0x22,
    0x22,0x72,0x12,0x27,0x36,0x00,0x00,0x99,0x96,0x06,
};
#define our_lut    font_data
#define our_packed (font_data + 10)

// ============================================================================
// Ikkle-4 font (raw, full 96 chars for mockup simplicity)
// ============================================================================
static const uint8_t ikkle_font[480] = {
    0x00,0x00,0x00,0x00,0x00,0x88,0x88,0x00,0x88,0x00,0x00,0x88,0x00,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,
    0xFF,0x99,0xFF,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,0x00,0x88,0x00,0x00,0x00,
    0x44,0x88,0x88,0x44,0x00,0x88,0x44,0x44,0x88,0x00,0x00,0x88,0x00,0x00,0x00,0x00,0x44,0xEE,0x44,0x00,
    0x00,0x00,0x00,0xCC,0x00,0x00,0x00,0xCC,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x22,0x44,0x44,0x88,0x00,
    0xEE,0xAA,0xAA,0xEE,0x00,0xCC,0x44,0x44,0xEE,0x00,0xEE,0x22,0x44,0xEE,0x00,0xEE,0x66,0x22,0xEE,0x00,
    0x88,0xAA,0xEE,0x22,0x00,0xEE,0xCC,0x22,0xCC,0x00,0xEE,0xCC,0xAA,0xEE,0x00,0xEE,0x22,0x44,0x88,0x00,
    0xEE,0xEE,0xAA,0xEE,0x00,0xEE,0xAA,0x66,0xEE,0x00,0x00,0x88,0x00,0x88,0x00,0x00,0x44,0x00,0xCC,0x00,
    0x00,0x66,0x88,0x66,0x00,0x00,0xCC,0x00,0xCC,0x00,0x00,0xCC,0x22,0xCC,0x00,0xEE,0x66,0x00,0x44,0x00,
    0xFF,0x99,0xFF,0x00,0x00,0xEE,0xAA,0xEE,0xAA,0x00,0xEE,0xEE,0xAA,0xEE,0x00,0xCC,0x88,0x88,0xCC,0x00,
    0xCC,0xAA,0xAA,0xEE,0x00,0xEE,0xCC,0x88,0xEE,0x00,0xEE,0x88,0xCC,0x88,0x00,0xEE,0x88,0xAA,0xEE,0x00,
    0xAA,0xEE,0xAA,0xAA,0x00,0x88,0x88,0x88,0x88,0x00,0x44,0x44,0x44,0xCC,0x00,0xAA,0xCC,0xAA,0xAA,0x00,
    0x88,0x88,0x88,0xCC,0x00,0xAA,0xEE,0xEE,0xAA,0x00,0xCC,0xAA,0xAA,0xAA,0x00,0xEE,0xAA,0xAA,0xEE,0x00,
    0xEE,0xAA,0xEE,0x88,0x00,0xEE,0xAA,0xEE,0x22,0x00,0xEE,0xAA,0xCC,0xAA,0x00,0xEE,0xCC,0x22,0xEE,0x00,
    0xEE,0x44,0x44,0x44,0x00,0xAA,0xAA,0xAA,0xEE,0x00,0xAA,0xAA,0xAA,0x44,0x00,0xAA,0xAA,0xEE,0xEE,0x00,
    0xAA,0x44,0x44,0xAA,0x00,0xAA,0xEE,0x44,0x44,0x00,0xEE,0x66,0x88,0xEE,0x00,0xCC,0x88,0x88,0xCC,0x00,
    0x88,0x44,0x44,0x22,0x00,0xCC,0x44,0x44,0xCC,0x00,0x44,0xAA,0x00,0x00,0x00,0x00,0x00,0x00,0xCC,0x00,
    0x88,0x44,0x00,0x00,0x00,0x00,0x66,0xAA,0xEE,0x00,0x00,0x88,0xCC,0xCC,0x00,0x00,0xCC,0x88,0xCC,0x00,
    0x22,0x66,0xAA,0xEE,0x00,0x00,0xEE,0xAA,0xCC,0x00,0x00,0xCC,0xCC,0x88,0x00,0x00,0xCC,0xAA,0xEE,0x00,
    0x88,0xCC,0xAA,0xAA,0x00,0x00,0x88,0x88,0x88,0x00,0x00,0x44,0x44,0xCC,0x00,0x00,0xAA,0xCC,0xAA,0x00,
    0x88,0x88,0x88,0x88,0x00,0x00,0xEE,0xEE,0xAA,0x00,0x00,0xCC,0xAA,0xAA,0x00,0x00,0xEE,0xAA,0xEE,0x00,
    0x00,0xCC,0xCC,0x88,0x00,0x00,0xCC,0xCC,0x44,0x00,0x00,0xCC,0x88,0x88,0x00,0x00,0x66,0x44,0xCC,0x00,
    0x88,0xCC,0x88,0xCC,0x00,0x00,0xAA,0xAA,0xEE,0x00,0x00,0xAA,0xAA,0x44,0x00,0x00,0xAA,0xEE,0xEE,0x00,
    0x00,0xAA,0x44,0xAA,0x00,0x00,0xAA,0xEE,0x22,0x00,0x00,0xCC,0x44,0x66,0x00,0x44,0x88,0x88,0x44,0x00,
    0x00,0x88,0x88,0x88,0x00,0x88,0x44,0x44,0x88,0x00,0x00,0x55,0xAA,0x00,0x00,0xFF,0x99,0xFF,0x00,0x00,
};

// ============================================================================
// Screen helpers
// ============================================================================
static uint8_t *scr_addr(uint8_t y, uint8_t phys_x, uint8_t scanline)
{
    uint16_t addr = 0x4000
        | ((uint16_t)(y >> 3) << 11)
        | ((uint16_t)scanline << 8)
        | ((uint16_t)(y & 0x07) << 5)
        | phys_x;
    return (uint8_t *)addr;
}

static uint8_t *attr_at(uint8_t y, uint8_t phys_x)
{
    return (uint8_t *)(0x5800 + (uint16_t)y * 32 + phys_x);
}

static uint8_t our_scan(uint8_t ch, uint8_t scanline)
{
    const uint8_t *p;
    uint8_t raw, nib;
    if (ch < 33 || ch > 127 || scanline > 5) return 0;
    p = our_packed + (uint16_t)(ch - 32) * 3;
    raw = p[scanline >> 1];
    nib = (scanline & 1) ? (raw & 0x0F) : (raw >> 4);
    return our_lut[nib];
}

static uint8_t ikkle_scan(uint8_t ch, uint8_t scanline)
{
    // UPPERCASE FOLD: chars >= 96 → subtract 32
    if (ch >= 96) ch -= 32;
    if (ch < 32 || ch > 95 || scanline > 4) return 0;
    return ikkle_font[(ch - 32) * 5 + scanline];
}

static void write_px(uint8_t *addr, uint8_t col, uint8_t pattern)
{
    uint8_t mask = (col & 1) ? 0xF0 : 0x0F;
    uint8_t nib  = (col & 1) ? (pattern & 0x0F) : (pattern & 0xF0);
    *addr = (*addr & mask) | nib;
}

static void clear_row_pixels(uint8_t row)
{
    uint8_t scan;
    for (scan = 0; scan < 8; scan++)
        memset(scr_addr(row, 0, scan), 0, 32);
}

static void fill_attr(uint8_t row, uint8_t attr)
{
    memset(attr_at(row, 0), attr, 32);
}

// ============================================================================
// 8px renderer (our font)
// ============================================================================
static void pr_8px(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    uint8_t c, scan;
    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c < 32 || c > 127) c = 32;
        for (scan = 0; scan < 8; scan++) {
            uint8_t *p = scr_addr(row, col >> 1, scan);
            if (c == 32 || scan == 0 || scan == 7) {
                *p = *p & ((col & 1) ? 0xF0 : 0x0F);
            } else {
                write_px(p, col, our_scan(c, scan - 1));
            }
        }
        *attr_at(row, col >> 1) = attr;
        col++;
    }
}

static void pr_line(uint8_t row, const char *s, uint8_t attr)
{
    clear_row_pixels(row);
    fill_attr(row, attr);
    pr_8px(row, 0, s, attr);
}

// ============================================================================
// Double-height renderer (banner) — each scanline is doubled across 2 rows
// ============================================================================
static void pr_big(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    uint8_t c, scan;
    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c < 32 || c > 127) c = 32;
        // 6 glyph scanlines → 12 display scanlines across 2 rows
        // Row 0: scanlines 0-1 clear, 2-7 = glyph[0-2] doubled
        // Row 1: scanlines 0-5 = glyph[3-5] doubled, 6-7 clear
        for (scan = 0; scan < 6; scan++) {
            uint8_t pattern = (c == 32) ? 0 : our_scan(c, scan);
            // Each glyph scanline draws to 2 physical scanlines
            uint8_t r = (scan < 3) ? row : row + 1;
            uint8_t s0 = (scan < 3) ? (scan * 2 + 2) : ((scan - 3) * 2);
            uint8_t *p0 = scr_addr(r, col >> 1, s0);
            uint8_t *p1 = scr_addr(r, col >> 1, s0 + 1);
            if (c == 32) {
                uint8_t mask = (col & 1) ? 0xF0 : 0x0F;
                *p0 = *p0 & mask;
                *p1 = *p1 & mask;
            } else {
                write_px(p0, col, pattern);
                write_px(p1, col, pattern);
            }
        }
        // Clear remaining scanlines (row 0: 0-1, row 1: 6-7)
        {
            uint8_t mask = (col & 1) ? 0xF0 : 0x0F;
            *scr_addr(row,   col>>1, 0) &= mask;
            *scr_addr(row,   col>>1, 1) &= mask;
            *scr_addr(row+1, col>>1, 6) &= mask;
            *scr_addr(row+1, col>>1, 7) &= mask;
        }
        *attr_at(row,   col >> 1) = attr | 0x40;  // BRIGHT for top row
        *attr_at(row+1, col >> 1) = attr & 0xBF;  // No bright for bottom
        col++;
    }
}

// ============================================================================
// Badge dither — triangle pattern in rightmost cells of rows 0-1
// ============================================================================
static void draw_badge(uint8_t count)
{
    uint8_t phys_x = 32 - count;
    uint8_t i, s;

    // Badge attr colors (Default theme: Azul→Rojo→Amarillo→Verde→Azul)
    static const uint8_t badge_top[] = {0x4A, 0x4E, 0x4C, 0x4D};
    static const uint8_t badge_bot[] = {0x4E, 0x4C, 0x4D, 0x49};

    // Solid-color first cell (banner paper = blue)
    *attr_at(0, phys_x) = A_BANNER | 0x40;
    *attr_at(1, phys_x) = badge_top[0];

    for (i = 0; i < count - 1 && i < 4; i++) {
        *attr_at(0, phys_x + 1 + i) = badge_top[i];
        *attr_at(1, phys_x + 1 + i) = (i + 1 < 4) ? badge_bot[i] : badge_bot[3];
    }

    // Draw triangle dither pattern for each badge cell
    for (i = 0; i < count; i++) {
        uint8_t px = phys_x + i;
        uint8_t val;
        // Row 0
        val = 0;
        for (s = 0; s < 8; s++) {
            *scr_addr(0, px, s) = val;
            val = (val << 1) | 1;  // 0,1,3,7,F,1F,3F,7F
        }
        // Row 1
        val = 0;
        for (s = 0; s < 8; s++) {
            *scr_addr(1, px, s) = val;
            val = (val << 1) | 1;
        }
    }
}

// ============================================================================
// 1px separator line below banner (scanline 0 of row 2)
// ============================================================================
static void draw_separator(void)
{
    uint8_t x;
    uint8_t *p = scr_addr(2, 0, 0);
    for (x = 0; x < 32; x++)
        p[x] = 0xFF;
}

// ============================================================================
// Ikkle-4 renderer — generic, works on any row, uppercase auto-fold
// Renders at scanlines start_sc..start_sc+3 within the row.
// ============================================================================
static void clear_row_range(uint8_t row, uint8_t from_sc, uint8_t to_sc)
{
    uint8_t scan;
    for (scan = from_sc; scan <= to_sc; scan++)
        memset(scr_addr(row, 0, scan), 0, 32);
}

static void ikkle_render(uint8_t row, uint8_t start_sc, uint8_t start_col,
                         const char *s, uint8_t text_attr, uint8_t bg_attr)
{
    uint8_t c, col = start_col;

    // Only clear the scanlines we'll use + margins, preserve others (e.g. separator)
    clear_row_range(row, start_sc > 0 ? start_sc - 1 : 0, start_sc + 4 < 8 ? start_sc + 4 : 7);
    fill_attr(row, bg_attr);

    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c > 32 && c < 128) {
            uint8_t px = col >> 1;
            uint8_t scan;
            for (scan = 0; scan < 4; scan++)
                write_px(scr_addr(row, px, start_sc + scan), col,
                         ikkle_scan(c, scan));
        }
        *attr_at(row, col >> 1) = text_attr;
        col++;
    }
}

// Helpers: right-aligned / centered
static uint8_t slen(const char *s)
{
    uint8_t n = 0;
    while (*s++) n++;
    return n;
}

static void ikkle_right(uint8_t row, uint8_t sc, const char *s,
                        uint8_t ta, uint8_t ba)
{
    uint8_t l = slen(s);
    ikkle_render(row, sc, (l < 64) ? (64 - l) : 0, s, ta, ba);
}

static void ikkle_center(uint8_t row, uint8_t sc, const char *s,
                         uint8_t ta, uint8_t ba)
{
    uint8_t l = slen(s);
    ikkle_render(row, sc, (l < 64) ? ((64 - l) >> 1) : 0, s, ta, ba);
}

static void ikkle_left(uint8_t row, uint8_t sc, const char *s,
                       uint8_t ta, uint8_t ba)
{
    ikkle_render(row, sc, 0, s, ta, ba);
}

// ============================================================================
// Static screen elements
// ============================================================================
static void draw_static(void)
{
    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, A_MAIN_BG, 768);

    __asm
        xor a
        out (0xFE), a
    __endasm;

    // Banner — double height + badge + separator
    fill_attr(0, A_BANNER | 0x40);
    fill_attr(1, A_BANNER & 0xBF);
    pr_big(0, 0, "SPECTALK ZX v1.3.6 - ", A_BANNER);
    pr_big(0, 21, "ZX SPECTRUM IRC CLIENT", A_BANNER);
    draw_badge(5);
    draw_separator();

    // Chat (rows 3-19)
    pr_line( 3, "<john> Hello everyone, welcome!",   A_MSG_CHAN);
    pr_line( 4, "<mary> Hey john! How's it going?",  A_MSG_CHAN);
    pr_line( 5, "*** alex has joined #spectalk",     A_MSG_JOIN);
    pr_line( 6, "<alex> Hi all!",                    A_MSG_CHAN);
    pr_line( 7, "<john> Good to see you alex",       A_MSG_CHAN);
    pr_line( 8, "<mary> We were testing SpecTalkZX", A_MSG_CHAN);
    pr_line( 9, "<alex> Oh cool, how is it working?",A_MSG_CHAN);
    pr_line(10, "<john> Pretty well actually!",      A_MSG_CHAN);
    pr_line(11, "<mary> The 64-col font is readable",A_MSG_CHAN);
    pr_line(12, "*** topic: Welcome to #spectalk",   A_MSG_TOPIC);
    pr_line(13, "<alex> Nice, I like the interface",  A_MSG_CHAN);
    pr_line(14, "<john> Thanks! Still improving it",  A_MSG_CHAN);
    pr_line(15, "<mary> What are you working on now?",A_MSG_CHAN);
    pr_line(16, "<alex> I heard about 4px fonts?",   A_MSG_CHAN);
    pr_line(17, "<john> Yeah, for a notification bar",A_MSG_CHAN);
    pr_line(18, "<mary> Sounds interesting!",        A_MSG_CHAN);
    pr_line(19, "<alex> Can't wait to see it",       A_MSG_CHAN);

    // Status bar
    pr_line(21, "[ignacio(+i)] 1/3:#spectalk(@libera) [12]", A_STATUS);

    // Input
    clear_row_pixels(22);
    clear_row_pixels(23);
    fill_attr(22, A_INPUT_BG);
    fill_attr(23, A_INPUT_BG);
    pr_8px(22, 0, ">", A_PROMPT);
    pr_8px(22, 2, "hello everyone", A_INPUT);
}

// ============================================================================
// Page definitions
// ============================================================================
#define NUM_PAGES 6

static void draw_page(uint8_t page)
{
    // Clear rows 2 and 20 (the Ikkle zones), preserve separator on row 2
    clear_row_range(2, 1, 7);  // keep scanline 0 = separator
    fill_attr(2, A_MAIN_BG);
    clear_row_pixels(20);
    fill_attr(20, A_MAIN_BG);

    // Restore row 19 (pagination demo overwrites it)
    pr_line(19, "<alex> Can't wait to see it", A_MSG_CHAN);

    switch (page) {
    case 0:
        // --- PM NOTIFICATION ---
        ikkle_right(20, 2,
            "* PM FROM ALEX: HEY, ARE YOU THERE?",
            A_NOTIF, A_MAIN_BG);
        break;

    case 1:
        // --- TOPIC BAR ---
        ikkle_left(2, 3,
            "#SPECTALK: WELCOME TO THE ZX SPECTRUM IRC CHANNEL",
            A_TOPIC, A_MAIN_BG);
        draw_separator();
        break;

    case 2:
        // --- TOPIC + PM (both rows at once!) ---
        ikkle_left(2, 3,
            "#SPECTALK: WELCOME TO THE ZX SPECTRUM IRC CHANNEL",
            A_TOPIC, A_MAIN_BG);
        draw_separator();
        ikkle_right(20, 2,
            "* PM FROM ALEX: HEY, ARE YOU THERE?",
            A_NOTIF, A_MAIN_BG);
        break;

    case 3:
        // --- PAGINATION PROMPT (frees row 19 for content) ---
        pr_line(19, "<alex> Can't wait to see it", A_MSG_CHAN);
        ikkle_center(20, 2,
            "-- MORE: ANY KEY | EDIT: CANCEL --",
            A_PAGIN, A_MAIN_BG);
        break;

    case 4:
        // --- CHANNEL ACTIVITY + MENTION ---
        ikkle_left(2, 3,
            "#GENERAL(3) #HELP(1) #SPECTALK",
            A_INFO, A_MAIN_BG);
        draw_separator();
        ikkle_right(20, 2,
            "* JOHN MENTIONED YOU IN #GENERAL",
            A_NOTIF, A_MAIN_BG);
        break;

    case 5:
        // --- CONNECTION INFO + AWAY ---
        ikkle_left(2, 3,
            "LIBERA.CHAT | LAG: 85MS | 14:32:07",
            A_INFO, A_MAIN_BG);
        draw_separator();
        ikkle_right(20, 2,
            "AWAY: BACK IN 10 MIN",
            A_AWAY, A_MAIN_BG);
        break;
    }
}

// ============================================================================
// Keyboard
// ============================================================================
uint8_t g_key;

static void wait_release(void)
{
    do {
        __asm
            xor a
            in a, (0xFE)
            cpl
            and 0x1F
            ld (_g_key), a
        __endasm;
    } while (g_key);
}

static uint8_t check_space(void)
{
    __asm
        ld a, 0x7F
        in a, (0xFE)
        cpl
        and 0x01
        ld (_g_key), a
    __endasm;
    return g_key;
}

static uint8_t wait_press(void)
{
    for (;;) {
        __asm
            xor a
            in a, (0xFE)
            cpl
            and 0x1F
            ld (_g_key), a
        __endasm;
        if (g_key) {
            if (check_space()) return 1;
            return 2;
        }
    }
}

// ============================================================================
// Main
// ============================================================================
void main(void)
{
    uint8_t page = 0;

    draw_static();
    draw_page(page);

    for (;;) {
        wait_release();
        if (wait_press() == 1) {
            page++;
            if (page >= NUM_PAGES) page = 0;
            draw_page(page);
        } else {
            break;
        }
    }
}
