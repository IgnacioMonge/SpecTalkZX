;; overlay_entry3.asm — Entry table for SPCTLK3.OVL
;; Must be linked FIRST so the table sits at offset 0.

SECTION code_user

EXTERN _whatsnew_render
EXTERN _bookmarks_apply_ovl
EXTERN _bookmarks_save_ovl

    dw 3                      ; entry_count = 3
    dw _whatsnew_render       ; entry 0 → what's new
    dw _bookmarks_apply_ovl   ; entry 1
    dw _bookmarks_save_ovl    ; entry 2
