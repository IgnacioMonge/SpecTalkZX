/*
 * spectalk_ovl3.c — "What's New" overlay for SpecTalkZX
 * Loaded into ring_buffer from SPCTLK3.OVL on demand.
 * Generated data from tools/gen_whatsnew.py (logo + changelog).
 *
 * Entry 0: whatsnew_render — What's New screen
 *
 * For each new version: update release/logo.png, changes.txt, version.txt
 * and run make. This file doesn't need editing.
 */

#include "overlay_api.h"
#include "whatsnew_data.h"

#define MAIN_START 3

/* Blit packed monochrome logo to screen at (start_row, start_col_bytes).
 * Writes explicit zero bytes so redraw over dirty screens matches raw logo. */
static void blit_logo(uint8_t start_row, uint8_t start_phys_col)
{
    const uint8_t *src = wn_logo_packed;
    uint8_t row, scanline;

    for (row = start_row; row < start_row + (WN_LOGO_H >> 3) + 1 && row < 20; row++) {
        for (scanline = 0; scanline < 8; scanline++) {
            uint8_t y_pixel = (row - start_row) * 8 + scanline;
            if (y_pixel >= WN_LOGO_H) return;
            uint16_t mask = src[0] | ((uint16_t)src[1] << 8);

            uint8_t *dst = (uint8_t *)(
                (uint16_t)(0x4000 | ((row & 0x18) << 8) | (scanline << 8)
                | ((row & 7) << 5)) + start_phys_col);

            uint8_t b;
            src += 2;
            for (b = 0; b < WN_LOGO_WB; b++) {
                dst[b] = (mask & 1) ? *src++ : 0;
                mask >>= 1;
            }
        }
    }

    /* Set attributes: bright white on black */
    {
        uint8_t r, c;
        uint8_t rows_used = (WN_LOGO_H + 7) >> 3;
        for (r = 0; r < rows_used && (start_row + r) < 20; r++) {
            uint8_t *attr = (uint8_t *)(0x5800 + (start_row + r) * 32 + start_phys_col);
            for (c = 0; c < WN_LOGO_WB; c++)
                attr[c] = 0x47; /* bright white ink, black paper */
        }
    }
}

void whatsnew_render(void)
{
    uint8_t a_chan = theme_attrs[TATTR_MSG_CHAN];
    uint8_t a_nick = theme_attrs[TATTR_MSG_NICK];
    uint8_t banner_attr = theme_attrs[0];
    uint8_t version_attr = 0x45 | (banner_attr & 0x38); /* BRIGHT cyan + banner PAPER */
    uint8_t r = MAIN_START + 3;

    /* Clear + separator, then version as centered hero title in magenta */
    overlay_header("");
    { uint8_t len = st_strlen(WN_VERSION);
      print_big_str(MAIN_START, (64 - len) >> 1, WN_VERSION, version_attr); }

    /* Logo on the left */
    blit_logo(r + 1, 0);

    /* Changelog on the right of the logo */
    {
        const char *p = wn_changes;
        uint8_t text_col = (WN_LOGO_WB + 1) * 2;
        uint8_t i, tr = r;

        print_str64(tr++, text_col, "Changes:", a_nick);
        for (i = 0; i < WN_CHANGES && *p; i++) {
            uint8_t is_last = (i == WN_CHANGES - 1);
            if (!is_last)
                print_char64(tr, text_col, '.', theme_attrs[TATTR_MSG_TIME]);
            print_str64(tr, is_last ? text_col : text_col + 2, p,
                       is_last ? a_nick : a_chan);
            tr++;
            while (*p) p++;
            p++;
        }
    }

    notif_center(S_ANYKEY, theme_attrs[TATTR_MSG_SYS]);
    reset_rx_state();
}
