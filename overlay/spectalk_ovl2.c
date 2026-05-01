/*
 * spectalk_ovl2.c — About overlay for SpecTalkZX.
 * Loaded into ring_buffer from SPCTLK2.OVL on demand.
 */

#include "overlay_api.h"

extern uint8_t earth_frame_buffer[];
extern uint8_t earth_attr_buffer[];
extern void earth_draw_frame(void);
extern uint8_t earth_read_logo(void);
extern void earth_ikkle_draw(uint8_t row, uint8_t col, const char *str, uint8_t attr);
extern void earth_draw_separator(void);
extern uint8_t earth_seek(uint16_t offset) __z88dk_fastcall;

/* ASM-defined state in overlay_entry2.asm */
extern uint8_t earth_ready;
extern uint8_t frame_idx;

#define ABOUT_YELLOW 0x46
#define ABOUT_WHITE  0x47

static const char s_line1[] = "SPECTALKZX " VERSION ": IRC CLIENT FOR ZX SPECTRUM";
static const char s_line2[] = "GITHUB.COM/IGNACIOMONGE/SPECTALKZX GPL-2.0";
static const char s_foot[]  = "N:What'sNew Any:Exit";

#define READ_DAT(buf, sz) esx_buf = (uint16_t)(buf); esx_count = (sz); esx_fread(); if (esx_result != (sz)) goto fail;

void about_close_ovl(void)
{
    if (esx_handle) {
        esx_fclose();
        esx_handle = 0;
    }
    earth_ready = 0;
}

void about_render_ovl(void)
{
    uint8_t r;

    about_close_ovl();

    for (r = 2; r <= 20; r++) clear_line(r, theme_attrs[TATTR_MAIN_BG]);
    earth_draw_separator();

    esx_fopen(K_DAT);
    if (!esx_handle) goto fail;
    if (!earth_seek(EARTH_FRAME0_OFFSET)) goto fail;

    READ_DAT(earth_frame_buffer, EARTH_FRAME0_SIZE);
    READ_DAT(earth_attr_buffer, EARTH_ATTR0_SIZE);

    earth_draw_frame();
    if (!earth_read_logo()) goto fail;
    if (!earth_seek(EARTH_DELTA_OFFSET)) goto fail;

    frame_idx = 0;
    earth_ready = 1;

    if (current_theme == 1) {
        earth_ikkle_draw(16, 10, s_line1, ABOUT_YELLOW);
        earth_ikkle_draw(17, 11, s_line2, ABOUT_WHITE);
    } else {
        earth_ikkle_draw(16, 10, s_line1, theme_attrs[TATTR_MSG_NICK]);
        earth_ikkle_draw(17, 11, s_line2, theme_attrs[TATTR_MSG_CHAN]);
    }
    notif_center(s_foot, theme_attrs[TATTR_MSG_SYS]);

    rb_head = 0; rb_tail = 0; rx_pos = 0; rx_last_len = 0; rx_overflow = 0;
    return;

fail:
    about_close_ovl();
    notif_center(s_foot, theme_attrs[TATTR_MSG_SYS]);
    rb_head = 0; rb_tail = 0; rx_pos = 0; rx_last_len = 0; rx_overflow = 0;
}
