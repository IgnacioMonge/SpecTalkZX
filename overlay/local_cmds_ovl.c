/*
 * local_cmds_ovl.c -- Cold local management commands for SPCTLK7.OVL.
 */

#include "overlay_api.h"

void ignore_cmd_ovl(void)
{
    char *p = (char *)overlay_slot;
    uint8_t i;

    if (!*p) {
        set_attr_sys();
        if (ignore_count == 0) {
            main_print("Ignore list is empty");
        } else {
            main_puts("Ignored (");
            main_putc('0' + ignore_count);
            main_puts("): ");
            for (i = 0; i < ignore_count; i++) {
                if (i > 0) {
                    main_putc(',');
                    main_putc(' ');
                }
                main_puts(ignore_list[i]);
            }
            main_newline();
        }
        goto done;
    }

    if (p[0] == '-') {
        p++;
        p = skip_spaces(p);
        if (!*p) {
            ui_usage("ignore -nick to unignore");
            goto done;
        }

        split_at_space(p);

        if (remove_ignore(p)) {
            notify2("Unignored: ", p, ATTR_MSG_SYS);
            config_dirty = 1;
        } else {
            notify2("Not in ignore list: ", p, ATTR_MSG_SYS);
        }
        goto done;
    }

    split_at_space(p);

    if (add_ignore(p)) {
        notify2("Now ignoring: ", p, ATTR_MSG_SYS);
        config_dirty = 1;
    } else if (is_ignored(p)) {
        notify2("Already ignoring: ", p, ATTR_MSG_SYS);
    } else {
        ui_err("Ignore list full");
    }

done:
    reset_rx_state();
}

void pass_cmd_ovl(void)
{
    const char *args = (const char *)overlay_slot;

    if (!*args) {
        sys_puts_print("Server password: ", irc_pass[0] ? "(set)" : "(not set)");
        goto done;
    }

    if (st_stricmp(args, "clear") == 0 || st_stricmp(args, "none") == 0) {
        irc_pass[0] = '\0';
        notify("Password cleared", ATTR_MSG_SYS);
        config_dirty = 1;
        goto done;
    }

    st_copy_n(irc_pass, args, IRC_PASS_SIZE);
    notify("Password set", ATTR_MSG_SYS);
    config_dirty = 1;

done:
    reset_rx_state();
}
