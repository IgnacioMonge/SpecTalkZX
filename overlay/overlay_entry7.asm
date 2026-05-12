;; overlay_entry7.asm -- Cold local management commands.

SECTION code_user

EXTERN _ignore_cmd_ovl
EXTERN _pass_cmd_ovl
EXTERN _local_setting_cmd_ovl
EXTERN _autoaway_cmd_ovl
EXTERN _friend_cmd_ovl

    dw 5
    dw _ignore_cmd_ovl
    dw _pass_cmd_ovl
    dw _local_setting_cmd_ovl
    dw _autoaway_cmd_ovl
    dw _friend_cmd_ovl
