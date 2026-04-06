;; overlay_entry.asm — Entry table for SPCTLK1.OVL (Help + Banner)
SECTION code_user
EXTERN _help_render_ovl
EXTERN _banner_render_ovl
    dw 2                      ; entry_count = 2
    dw _help_render_ovl       ; entry 0 → help
    dw _banner_render_ovl     ; entry 1 → banner
