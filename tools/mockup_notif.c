/*
 * mockup_notif.c — Notification bar mockup for SpecTalkZX
 *
 * Simulates the SpecTalkZX screen with a notification bar at row 20
 * (the blank separator above the status bar) using mini fonts.
 *
 * SPACE toggles between Ikkle-4 (4px) and our font (6px).
 * Any other key exits.
 *
 * Build:
 *   zcc +zx -vn -startup=1 -clib=sdcc_iy -o tools/mockup_notif tools/mockup_notif.c
 *   z88dk-appmake +zx -b tools/mockup_notif_CODE.bin --org 32768 -o tools/mockup_notif.tap
 */

#include <stdint.h>
#include <string.h>

// ============================================================================
// Attributes — Default theme values from themes[]
// ZX attr byte: FLASH(7) BRIGHT(6) PAPER(5-3) INK(2-0)
// Colors: 0=BLACK 1=BLUE 2=RED 3=MAGENTA 4=GREEN 5=CYAN 6=YELLOW 7=WHITE
// ============================================================================
#define A_BANNER    0x4F  // BRIGHT WHITE on BLUE
#define A_BAN_DIM   0x0F  // WHITE on BLUE (no bright, for banner row 1)
#define A_MODE      0x45  // BRIGHT CYAN on BLACK (mockup UI)
#define A_MAIN_BG   0x07  // WHITE on BLACK
#define A_MSG_CHAN   0x07  // WHITE on BLACK
#define A_MSG_SELF   0x46  // BRIGHT YELLOW on BLACK
#define A_MSG_PRIV   0x44  // BRIGHT GREEN on BLACK
#define A_MSG_JOIN   0x03  // MAGENTA on BLACK
#define A_MSG_TOPIC  0x46  // BRIGHT YELLOW on BLACK
#define A_MSG_TIME   0x45  // BRIGHT CYAN on BLACK
#define A_MSG_SYS    0x07  // WHITE on BLACK (server msgs)
#define A_STATUS    0x39  // BLUE on WHITE
#define A_INPUT     0x29  // BLUE on CYAN
#define A_INPUT_BG  0x28  // BLACK on CYAN (empty input area)
#define A_PROMPT    0x29  // BLUE on CYAN
#define A_NOTIF     0x46  // BRIGHT YELLOW on BLACK

// ============================================================================
// Font 1: Our SpecTalkZX 64-col font (6 scanlines, nibble-packed + LUT)
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
// Font 2: Ikkle-4 (Jack Oatley, public domain)
// 4px wide x 4px tall + 1px gap = 5 bytes per char
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
// Screen address helpers
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

// ============================================================================
// Glyph scanline decoders
// ============================================================================
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
    if (ch < 32 || ch > 127 || scanline > 4) return 0;
    return ikkle_font[(ch - 32) * 5 + scanline];
}

// ============================================================================
// Pixel-level nibble write
// ============================================================================
static void write_px(uint8_t *addr, uint8_t col, uint8_t pattern)
{
    uint8_t mask = (col & 1) ? 0xF0 : 0x0F;
    uint8_t nib  = (col & 1) ? (pattern & 0x0F) : (pattern & 0xF0);
    *addr = (*addr & mask) | nib;
}

// ============================================================================
// Clear all 8 scanlines of a row (pixels only)
// ============================================================================
static void clear_row_pixels(uint8_t row)
{
    uint8_t scan;
    for (scan = 0; scan < 8; scan++)
        memset(scr_addr(row, 0, scan), 0, 32);
}

// ============================================================================
// Fill attribute row
// ============================================================================
static void fill_attr(uint8_t row, uint8_t attr)
{
    memset(attr_at(row, 0), attr, 32);
}

// ============================================================================
// Normal 8px renderer (our font, scanlines 1-6, 0+7 clear)
// ============================================================================
static void pr_8px(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    uint8_t c, scan;
    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c < 32 || c > 127) c = 32;
        for (scan = 0; scan < 8; scan++) {
            uint8_t *p = scr_addr(row, col >> 1, scan);
            if (c == 32 || scan == 0 || scan == 7) {
                uint8_t mask = (col & 1) ? 0xF0 : 0x0F;
                *p = *p & mask;
            } else {
                write_px(p, col, our_scan(c, scan - 1));
            }
        }
        *attr_at(row, col >> 1) = attr;
        col++;
    }
}

// ============================================================================
// Full-row 8px renderer (clears row first, fills attr)
// ============================================================================
static void pr_line(uint8_t row, const char *s, uint8_t attr)
{
    clear_row_pixels(row);
    fill_attr(row, attr);
    pr_8px(row, 0, s, attr);
}

// ============================================================================
// Notification renderers — draw within a SINGLE row (row 20)
// ============================================================================
#define NOTIF_ROW 20

// Ikkle-4: 4 glyph scanlines centered at scanlines 2-5
static void notif_ikkle(uint8_t start_col, const char *s, uint8_t attr)
{
    uint8_t c, col = start_col;

    clear_row_pixels(NOTIF_ROW);
    fill_attr(NOTIF_ROW, A_MAIN_BG);  // base: same as chat background

    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c > 32 && c < 128) {
            uint8_t px = col >> 1;
            uint8_t scan;
            for (scan = 0; scan < 4; scan++)
                write_px(scr_addr(NOTIF_ROW, px, scan + 2), col,
                         ikkle_scan(c, scan));
        }
        // Set text attr only for columns where text lives
        *attr_at(NOTIF_ROW, col >> 1) = attr;
        col++;
    }
}

// Our font: 6 glyph scanlines at scanlines 1-6
static void notif_ours(uint8_t start_col, const char *s, uint8_t attr)
{
    uint8_t c, col = start_col;

    clear_row_pixels(NOTIF_ROW);
    fill_attr(NOTIF_ROW, A_MAIN_BG);

    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        if (c > 32 && c < 128) {
            uint8_t px = col >> 1;
            uint8_t scan;
            for (scan = 0; scan < 6; scan++)
                write_px(scr_addr(NOTIF_ROW, px, scan + 1), col,
                         our_scan(c, scan));
        }
        *attr_at(NOTIF_ROW, col >> 1) = attr;
        col++;
    }
}

// ============================================================================
// Draw the full simulated SpecTalkZX screen (static parts)
// ============================================================================
static void draw_screen(void)
{
    // Clear entire screen
    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, A_MAIN_BG, 768);  // base: all BLACK paper

    // Border = black
    __asm
        xor a
        out (0xFE), a
    __endasm;

    // --- Banner (rows 0-1) ---
    fill_attr(0, A_BANNER);
    fill_attr(1, A_BAN_DIM);
    pr_8px(0, 7, "SpecTalkZX v1.3.6", A_BANNER);
    pr_8px(1, 5, "ZX Spectrum IRC Client", A_BAN_DIM);

    // --- Row 2: mode indicator (updated separately) ---

    // --- Chat area (rows 3-19) ---
    pr_line( 3, "<john> Hello everyone, welcome!",      A_MSG_CHAN);
    pr_line( 4, "<mary> Hey john! How's it going?",     A_MSG_CHAN);
    pr_line( 5, "*** alex has joined #spectalk",        A_MSG_JOIN);
    pr_line( 6, "<alex> Hi all!",                       A_MSG_CHAN);
    pr_line( 7, "<john> Good to see you alex",          A_MSG_CHAN);
    pr_line( 8, "<mary> We were testing SpecTalkZX",    A_MSG_CHAN);
    pr_line( 9, "<alex> Oh cool, how is it working?",   A_MSG_CHAN);
    pr_line(10, "<john> Pretty well actually!",         A_MSG_CHAN);
    pr_line(11, "<mary> The 64-col font is readable",   A_MSG_CHAN);
    pr_line(12, "*** topic: Welcome to #spectalk",      A_MSG_TOPIC);
    pr_line(13, "<alex> Nice, I like the interface",    A_MSG_CHAN);
    pr_line(14, "<john> Thanks! Still improving it",    A_MSG_CHAN);
    pr_line(15, "<mary> What are you working on now?",  A_MSG_CHAN);
    pr_line(16, "<alex> I heard about 4px fonts?",      A_MSG_CHAN);
    pr_line(17, "<john> Yeah, for a notification bar",  A_MSG_CHAN);
    pr_line(18, "<mary> Sounds interesting!",           A_MSG_CHAN);
    pr_line(19, "<alex> Can't wait to see it",          A_MSG_CHAN);

    // --- Row 20: notification (drawn separately) ---

    // --- Status bar (row 21) — PAPER_WHITE + INK_BLUE ---
    pr_line(21, "[ignacio(+i)] 1/3:#spectalk(@libera) [12]",
            A_STATUS);

    // --- Input (rows 22-23) — PAPER_CYAN background ---
    clear_row_pixels(22);
    clear_row_pixels(23);
    fill_attr(22, A_INPUT_BG);
    fill_attr(23, A_INPUT_BG);
    pr_8px(22, 0, ">", A_PROMPT);
    pr_8px(22, 2, "hello everyone", A_INPUT);
}

// ============================================================================
// Update the mode indicator at row 2
// ============================================================================
static void draw_mode_label(uint8_t mode)
{
    clear_row_pixels(2);
    fill_attr(2, A_MODE);
    if (mode == 0)
        pr_8px(2, 2, "[SPACE] toggle  -  Ikkle-4 (4px)", A_MODE);
    else
        pr_8px(2, 2, "[SPACE] toggle  -  Our font (6px)", A_MODE);
}

// ============================================================================
// Draw or refresh the notification bar
// ============================================================================
static const char notif_msg[] = "* PM from alex: Hey, are you there?";

static void draw_notif(uint8_t mode)
{
    uint8_t len = strlen(notif_msg);
    uint8_t start = (len < 64) ? (64 - len) : 0;

    if (mode == 0)
        notif_ikkle(start, notif_msg, A_NOTIF);
    else
        notif_ours(start, notif_msg, A_NOTIF);
}

// ============================================================================
// Keyboard helpers
// ============================================================================

// Key result (set by ASM)
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

// Returns 1 if SPACE pressed, 0 otherwise
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

// Wait for any key press; return 1=SPACE, 2=other
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
    uint8_t mode = 0;  // 0 = Ikkle-4, 1 = Our font

    draw_screen();
    draw_mode_label(mode);
    draw_notif(mode);

    for (;;) {
        wait_release();
        if (wait_press() == 1) {
            // SPACE: toggle mode
            mode ^= 1;
            draw_mode_label(mode);
            draw_notif(mode);
        } else {
            // Any other key: exit
            break;
        }
    }
}
