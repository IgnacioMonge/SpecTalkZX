/*
 * mockup_wordinput.c — Test word-by-word navigation/deletion in input line
 * SS+DELETE = delete word back, SS+LEFT = word left, SS+RIGHT = word right
 * BREAK = exit
 *
 * Build:
 *   zcc +zx -vn -startup=1 -clib=sdcc_iy -o tools/mockup_wordinput tools/mockup_wordinput.c
 *   z88dk-appmake +zx -b tools/mockup_wordinput_CODE.bin --org 32768 -o tools/mockup_wordinput.tap
 */

#include <stdint.h>
#include <string.h>


// ============================================================================
// Font (our 64-col font)
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

// Print char at 64-col position
static void put_char(uint8_t row, uint8_t col, uint8_t ch, uint8_t attr)
{
    uint8_t scan;
    for (scan = 0; scan < 8; scan++) {
        uint8_t *p = scr_addr(row, col >> 1, scan);
        if (ch == 32 || scan == 0 || scan == 7) {
            *p = *p & ((col & 1) ? 0xF0 : 0x0F);
        } else {
            write_px(p, col, our_scan(ch, scan - 1));
        }
    }
    *attr_at(row, col >> 1) = attr;
}

static void pr_8px(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    uint8_t c;
    while ((c = (uint8_t)*s++) != 0 && col < 64) {
        put_char(row, col, c, attr);
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
// Attributes
// ============================================================================
#define A_MAIN_BG   0x07
#define A_STATUS    0x39
#define A_INPUT     0x29
#define A_INPUT_BG  0x28
#define A_PROMPT    0x29
#define A_CURSOR    0x51  // BRIGHT BLUE on CYAN (inverse-ish)
#define A_INFO      0x46
#define A_LABEL     0x45

// ============================================================================
// Input state
// ============================================================================
#define LINE_SIZE 62
#define INPUT_ROW 22
#define PROMPT_COL 2  // after "> "

static char line_buf[LINE_SIZE + 1];
static uint8_t line_len;
static uint8_t cursor_pos;

// ============================================================================
// Key codes
// ============================================================================
#define KEY_LEFT      8
#define KEY_RIGHT     9
#define KEY_BACKSPACE 12
#define KEY_ENTER     13
#define KEY_BREAK     3

// ============================================================================
// Keyboard — simplified read_key + SS detection
// ============================================================================
uint8_t ss_held;

// Keyboard rows — MUST be global (not static) for inline ASM label _kb_rows
uint8_t kb_rows[8];

static void scan_keyboard(void)
{
    __asm
        ld a, 0xFE
        in a, (0xFE)
        ld (_kb_rows + 0), a
        ld a, 0xFD
        in a, (0xFE)
        ld (_kb_rows + 1), a
        ld a, 0xFB
        in a, (0xFE)
        ld (_kb_rows + 2), a
        ld a, 0xF7
        in a, (0xFE)
        ld (_kb_rows + 3), a
        ld a, 0xEF
        in a, (0xFE)
        ld (_kb_rows + 4), a
        ld a, 0xDF
        in a, (0xFE)
        ld (_kb_rows + 5), a
        ld a, 0xBF
        in a, (0xFE)
        ld (_kb_rows + 6), a
        ld a, 0x7F
        in a, (0xFE)
        ld (_kb_rows + 7), a
    __endasm;
}

// Normal keys per row (5 keys each, bit 0..4)
// Row 0: CS Z X C V   Row 1: A S D F G   Row 2: Q W E R T   Row 3: 1 2 3 4 5
// Row 4: 0 9 8 7 6    Row 5: P O I U Y   Row 6: EN L K J H   Row 7: SP SS M N B
static const uint8_t key_normal[40] = {
    0,  'z','x','c','v',
    'a','s','d','f','g',
    'q','w','e','r','t',
    '1','2','3','4','5',
    '0','9','8','7','6',
    'p','o','i','u','y',
    13, 'l','k','j','h',
    ' ',0,  'm','n','b'
};

static const uint8_t key_caps[40] = {
    0,  'Z','X','C','V',
    'A','S','D','F','G',
    'Q','W','E','R','T',
    7,  6,  0,  0,  KEY_LEFT,
    KEY_BACKSPACE,0,KEY_RIGHT,11,10,   // CS+0=DEL, CS+9=GRAPH, CS+8=RIGHT, CS+7=UP, CS+6=DOWN
    'P','O','I','U','Y',
    13, 'L','K','J','H',
    KEY_BREAK,0,'M','N','B'
};

static uint8_t read_key(void)
{
    uint8_t row, col, val, idx, key;
    uint8_t caps;

    scan_keyboard();

    // Detect modifiers
    caps = !(kb_rows[0] & 0x01);      // CS = row 0, bit 0
    ss_held = !(kb_rows[7] & 0x02);   // SS = row 7, bit 1

    // Scan for pressed key
    for (row = 0; row < 8; row++) {
        val = kb_rows[row] & 0x1F;
        if (val == 0x1F) continue;

        for (col = 0; col < 5; col++) {
            if (val & (1 << col)) continue;

            idx = row * 5 + col;
            if (idx == 0) continue;    // skip CS
            if (idx == 36) continue;   // skip SS

            key = caps ? key_caps[idx] : key_normal[idx];
            if (key) return key;
        }
    }
    return 0;
}

// ============================================================================
// Input rendering
// ============================================================================
static void render_input(void)
{
    uint8_t i;
    clear_row_pixels(INPUT_ROW);
    fill_attr(INPUT_ROW, A_INPUT_BG);

    // Prompt
    put_char(INPUT_ROW, 0, '>', A_PROMPT);

    // Text
    for (i = 0; i < line_len && i < LINE_SIZE; i++) {
        uint8_t attr = (i == cursor_pos) ? A_CURSOR : A_INPUT;
        put_char(INPUT_ROW, PROMPT_COL + i, line_buf[i], attr);
    }

    // Cursor at end (if cursor == line_len)
    if (cursor_pos >= line_len) {
        put_char(INPUT_ROW, PROMPT_COL + line_len, ' ', A_CURSOR);
    }
}

// ============================================================================
// Word boundary helpers
// ============================================================================
static uint8_t word_boundary_left(uint8_t pos)
{
    if (pos == 0) return 0;
    // Skip spaces backward
    while (pos > 0 && line_buf[pos - 1] == ' ') pos--;
    // Skip word backward
    while (pos > 0 && line_buf[pos - 1] != ' ') pos--;
    return pos;
}

static uint8_t word_boundary_right(uint8_t pos)
{
    // Skip word forward
    while (pos < line_len && line_buf[pos] != ' ') pos++;
    // Skip spaces forward
    while (pos < line_len && line_buf[pos] == ' ') pos++;
    return pos;
}

static void delete_word_back(void)
{
    uint8_t new_pos, del_count, i;
    if (cursor_pos == 0) return;

    new_pos = word_boundary_left(cursor_pos);
    del_count = cursor_pos - new_pos;
    if (del_count == 0) return;

    // Shift remaining text left
    for (i = cursor_pos; i < line_len; i++) {
        line_buf[i - del_count] = line_buf[i];
    }
    line_len -= del_count;
    cursor_pos = new_pos;
    line_buf[line_len] = 0;
}

// ============================================================================
// Status display
// ============================================================================
static void show_status(const char *msg)
{
    pr_line(20, "", A_MAIN_BG);
    pr_8px(20, 0, msg, A_INFO);
}

// ============================================================================
// Main
// ============================================================================
void main(void)
{
    uint8_t c, last_c = 0, debounce = 0;

    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, A_MAIN_BG, 768);

    __asm
        xor a
        out (0xFE), a
    __endasm;

    // Header
    pr_line(0, "Word-by-word input test", A_LABEL);
    pr_line(2, "LEFT/RIGHT = move char", A_MAIN_BG);
    pr_line(3, "DELETE = delete char", A_MAIN_BG);
    pr_line(4, "SS+LEFT = word left", A_INFO);
    pr_line(5, "SS+RIGHT = word right", A_INFO);
    pr_line(6, "SS+DELETE = delete word", A_INFO);
    pr_line(7, "BREAK = exit", A_MAIN_BG);

    // Pre-fill with sample text
    {
        const char *sample = "Hello world this is a test of word navigation";
        uint8_t i = 0;
        while (sample[i] && i < LINE_SIZE) { line_buf[i] = sample[i]; i++; }
        line_buf[i] = 0;
        line_len = i;
        cursor_pos = line_len;  // cursor at end
    }

    // Status bar
    pr_line(21, "[ignacio(+i)] 1/3:#spectalk(@libera) [12]", A_STATUS);

    render_input();
    show_status("Ready. Type or use SS+arrows/del");

    // Enable interrupts (required for halt to work, may not be set by startup=1)
    __asm ei __endasm;

    for (;;) {
        __asm halt __endasm;

        c = read_key();

        // Debounce
        if (c == last_c) {
            if (debounce < 15) { debounce++; continue; }
            if (debounce < 18) { debounce++; continue; }
            debounce = 15;  // auto-repeat after initial delay
        } else {
            debounce = 0;
            last_c = c;
        }

        if (!c) { last_c = 0; continue; }

        if (c == KEY_BREAK) break;

        if (c == KEY_BACKSPACE) {
            if (ss_held) {
                // SS + DELETE = delete word
                delete_word_back();
                show_status("SS+DEL: word deleted");
            } else {
                // Normal delete char
                if (cursor_pos > 0) {
                    uint8_t i;
                    cursor_pos--;
                    for (i = cursor_pos; i < line_len - 1; i++)
                        line_buf[i] = line_buf[i + 1];
                    line_len--;
                    line_buf[line_len] = 0;
                    show_status("DEL: char deleted");
                }
            }
            render_input();
        }
        else if (c == KEY_LEFT) {
            if (ss_held) {
                cursor_pos = word_boundary_left(cursor_pos);
                show_status("SS+LEFT: word left");
            } else {
                if (cursor_pos > 0) cursor_pos--;
                show_status("LEFT");
            }
            render_input();
        }
        else if (c == KEY_RIGHT || c == 9) {  // KEY_RIGHT or TAB (CS+8)
            if (ss_held) {
                cursor_pos = word_boundary_right(cursor_pos);
                show_status("SS+RIGHT: word right");
            } else {
                if (cursor_pos < line_len) cursor_pos++;
                show_status("RIGHT");
            }
            render_input();
        }
        else if (c >= 32 && c <= 126) {
            // Insert char at cursor
            if (line_len < LINE_SIZE) {
                uint8_t i;
                for (i = line_len; i > cursor_pos; i--)
                    line_buf[i] = line_buf[i - 1];
                line_buf[cursor_pos] = c;
                line_len++;
                cursor_pos++;
                line_buf[line_len] = 0;
            }
            render_input();
            show_status("");
        }
    }
}
