;; overlay_entry4.asm — Entry table for SPCTLK4.OVL
SECTION code_user
EXTERN _status_render_ovl
EXTERN _save_config_ovl
    dw 2                      ; entry_count = 2
    dw _status_render_ovl     ; entry 0 → status
    dw _save_config_ovl       ; entry 1 → config save
