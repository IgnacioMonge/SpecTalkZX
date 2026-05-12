;; overlay_entry7.asm -- Cold local management commands.

SECTION code_user

EXTERN _ignore_cmd_ovl
EXTERN _pass_cmd_ovl

    dw 2
    dw _ignore_cmd_ovl
    dw _pass_cmd_ovl
