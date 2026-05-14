;; overlay_entry3.asm — Entry table for SPCTLK3.OVL
;; Must be linked FIRST so the table sits at offset 0.

SECTION code_user

EXTERN _whatsnew_render

    dw 1                      ; entry_count = 1
    dw _whatsnew_render       ; entry 0 → what's new
