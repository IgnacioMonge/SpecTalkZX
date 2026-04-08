/*
 * mockup_prompt.c — Prompt indicator mockup: > (channel) vs @ (query/PM)
 * Shows all 3 themes side by side, channel and query states.
 * SPACE cycles pages. Any other key exits.
 *
 * Build:
 *   zcc +zx -vn -startup=1 -clib=sdcc_iy -o tools/mockup_prompt tools/mockup_prompt.c
 *   z88dk-appmake +zx -b tools/mockup_prompt_CODE.bin --org 32768 -o tools/mockup_prompt.tap
 */

#include <stdint.h>
#include <string.h>

// ============================================================================
// Our font
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
// Theme definitions (input-relevant attrs only)
// ============================================================================
typedef struct {
    uint8_t input;       // input text attr
    uint8_t input_bg;    // input background attr
    uint8_t prompt;      // prompt attr (channel mode)
    uint8_t prompt_pm;   // prompt attr (query/PM mode)
    uint8_t main_bg;     // chat background
    uint8_t msg_chan;     // channel message attr
    uint8_t status;      // status bar attr
    uint8_t label;       // label color for mockup
    const char *name;
} ThemeInput;

// Theme 1: Default — cyan bg, blue ink. PM: red ink on cyan
// Theme 2: Terminal — black bg, green ink. PM: green BRIGHT
// Theme 3: Commander — white bg, blue ink. PM: red ink on white
static const ThemeInput themes[3] = {
    { 0x29, 0x28, 0x29, 0x2A, 0x07, 0x07, 0x39, 0x46, "Default" },
    //                   ^^^^ PM: PAPER_CYAN | INK_RED = 0x2A
    { 0x44, 0x44, 0x04, 0x44, 0x04, 0x04, 0x24, 0x44, "Terminal" },
    //              ^^^^ prompt dim   ^^^^ PM: bright
    { 0x39, 0x39, 0x39, 0x3A, 0x3F, 0x2D, 0x29, 0x56, "Commander" },
    //                   ^^^^ PM: PAPER_WHITE | INK_RED = 0x3A
};

// ============================================================================
// Draw one theme panel (6 rows high)
// ============================================================================
static void draw_panel(uint8_t start_row, uint8_t theme_idx, uint8_t is_pm)
{
    const ThemeInput *t = &themes[theme_idx];
    uint8_t r = start_row;

    // Row 0: Theme label
    pr_line(r, "", t->main_bg);
    pr_8px(r, 2, t->name, t->label);
    pr_8px(r, 20, is_pm ? "- QUERY/PM" : "- CHANNEL", t->label);
    r++;

    // Row 1-2: Simulated chat
    pr_line(r, "", t->main_bg);
    pr_8px(r, 0, "<john> Hello everyone!", t->msg_chan);
    r++;
    pr_line(r, "", t->main_bg);
    pr_8px(r, 0, "<mary> Hey there!", t->msg_chan);
    r++;

    // Row 3: Status bar
    if (is_pm)
        pr_line(r, "[ignacio(+i)] 2/3:alex", t->status);
    else
        pr_line(r, "[ignacio(+i)] 1/3:#spectalk(@lib) [12]", t->status);
    r++;

    // Row 4-5: Input area (the important part!)
    clear_row_pixels(r);
    clear_row_pixels(r + 1);
    fill_attr(r, t->input_bg);
    fill_attr(r + 1, t->input_bg);

    if (is_pm) {
        // PM mode: @ prompt with PM attr, text with PM attr
        pr_8px(r, 0, "@", t->prompt_pm);
        pr_8px(r, 2, "hey alex, whats up?", t->prompt_pm);
    } else {
        // Channel mode: > prompt with normal attr
        pr_8px(r, 0, ">", t->prompt);
        pr_8px(r, 2, "hello everyone", t->input);
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
#define NUM_PAGES 2

void main(void)
{
    uint8_t page = 0;

    __asm
        xor a
        out (0xFE), a
    __endasm;

    for (;;) {
        memset((void *)0x4000, 0, 6144);
        memset((void *)0x5800, 0x07, 768);

        if (page == 0) {
            // CHANNEL mode — all 3 themes
            pr_line(0, "", 0x47);
            pr_8px(0, 8, "[SPACE] toggle CH/PM", 0x47);

            draw_panel(2, 0, 0);   // Default, channel
            draw_panel(10, 1, 0);  // Terminal, channel
            draw_panel(18, 2, 0);  // Commander, channel
        } else {
            // PM mode — all 3 themes
            pr_line(0, "", 0x47);
            pr_8px(0, 8, "[SPACE] toggle CH/PM", 0x47);

            draw_panel(2, 0, 1);   // Default, PM
            draw_panel(10, 1, 1);  // Terminal, PM
            draw_panel(18, 2, 1);  // Commander, PM
        }

        wait_release();
        if (wait_press() == 1) {
            page ^= 1;
        } else {
            break;
        }
    }
}
