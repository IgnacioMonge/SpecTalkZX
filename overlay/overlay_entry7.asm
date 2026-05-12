;; overlay_entry7.asm -- Cold local management commands.

SECTION code_user

EXTERN _ignore_cmd_ovl

    dw 1
    dw _ignore_cmd_ovl
