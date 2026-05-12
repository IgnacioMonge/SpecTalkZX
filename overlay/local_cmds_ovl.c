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

static void set_or_toggle_flag_ovl(uint8_t *flag, const char *label, const char *args) __z88dk_callee
{
    if (*args) {
        if (args[0] == '1' || st_stricmp(args, "on") == 0) *flag = 1;
        else if (args[0] == '0' || st_stricmp(args, "off") == 0) *flag = 0;
        else { ui_usage("on|off"); return; }
    } else {
        *flag = !*flag;
    }
    sys_puts_print(label, *flag ? "on" : "off");
    config_dirty = 1;
}

void local_setting_cmd_ovl(void)
{
    uint8_t id = overlay_slot[0];
    const char *args = (const char *)overlay_slot + 1;

    switch (id) {
    case 0:
        set_or_toggle_flag_ovl(&beep_enabled, "Beep: ", args);
        break;
    case 1:
        set_or_toggle_flag_ovl(&keyclick_enabled, "Click: ", args);
        break;
    case 2:
        set_or_toggle_flag_ovl(&nick_color_mode, "Nick color: ", args);
        break;
    case 3:
        set_or_toggle_flag_ovl(&show_traffic, "Show traffic: ", args);
        break;
    case 4:
        set_or_toggle_flag_ovl(&notif_enabled, "Notifications: ", args);
        break;
    case 5:
        if (*args) {
            if (args[0] == '0' || st_stricmp(args, "off") == 0) show_timestamps = 0;
            else if (args[0] == '1' || st_stricmp(args, "on") == 0) show_timestamps = 1;
            else if (args[0] == '2' || st_stricmp(args, "smart") == 0) show_timestamps = 2;
            else { ui_usage("off|on|smart"); goto done; }
        } else {
            if (++show_timestamps > 2) show_timestamps = 0;
        }
        if (show_timestamps == 2) {
            last_ts_hour = 0xFF;
            last_ts_minute = 0xFF;
        }
        sys_puts_print("Timestamps: ", show_timestamps == 0 ? "off" :
                       show_timestamps == 1 ? "on" : "smart");
        config_dirty = 1;
        break;
    case 6:
        set_or_toggle_flag_ovl(&autoconnect, "Autoconnect: ", args);
        break;
    case 7:
        set_or_toggle_flag_ovl(&autojoin, "Autojoin: ", args);
        break;
    }

done:
    reset_rx_state();
}
