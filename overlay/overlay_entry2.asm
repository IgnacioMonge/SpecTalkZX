;; overlay_entry2.asm — Entry table for SPCTLK2.OVL
SECTION code_user
EXTERN _about_render_ovl
EXTERN _config_render_ovl
EXTERN _globe_tick_ovl
    dw 3                      ; entry_count = 3
    dw _about_render_ovl      ; entry 0 → about
    dw _config_render_ovl     ; entry 1 → config
    dw _globe_tick_ovl        ; entry 2 → globe animation tick
