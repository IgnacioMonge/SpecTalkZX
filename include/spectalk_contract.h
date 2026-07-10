/*
 * spectalk_contract.h - Shared resident/overlay constants.
 *
 * Pure preprocessor contract only: no z88dk headers, structs, prototypes,
 * globals, or declarations that can pull resident dependencies into overlays.
 */

#ifndef SPECTALK_CONTRACT_H
#define SPECTALK_CONTRACT_H

#define VERSION "1.3.8"

#define RING_BUFFER_SIZE 2048
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#define LINE_BUFFER_SIZE 128
#define RX_LINE_SIZE 512
#define OVERLAY_SLOT_SIZE RX_LINE_SIZE

#define IRC_SERVER_SIZE   32
#define IRC_PORT_SIZE      6
#define IRC_NICK_SIZE     18
#define IRC_PASS_SIZE     24
#define USER_MODE_SIZE     6
#define NETWORK_NAME_SIZE 12
#define NAMES_TARGET_CHANNEL_SIZE 32
#define SEARCH_PATTERN_SIZE       64

#define MAX_FRIENDS 5
#define MAX_IGNORES 5

/* Timezone sentinel: user explicitly selected hardware RTC mode. */
#define TZ_RTC 127

/* UI overlay modes stored in overlay_mode. These are not atlas ids. */
#define OVERLAY_NONE      0
#define OVERLAY_HELP      1
#define OVERLAY_ABOUT     2
#define OVERLAY_CONFIG    3
#define OVERLAY_STATUS    4
#define OVERLAY_WHATSNEW  5
#define OVERLAY_BOOKMARKS 6

/* Theme attribute indices; must match theme_attrs[] layout. */
#define TATTR_BANNER     0
#define TATTR_STATUS     1
#define TATTR_MSG_CHAN   2
#define TATTR_MSG_SELF   3
#define TATTR_MSG_PRIV   4
#define TATTR_MAIN_BG    5
#define TATTR_INPUT      6
#define TATTR_PROMPT     8
#define TATTR_MSG_SYS    9
#define TATTR_MSG_JOIN  10
#define TATTR_MSG_NICK  11
#define TATTR_MSG_TIME  12
#define TATTR_MSG_TOPIC 13
#define TATTR_MSG_MOTD  14
#define TATTR_ERROR     15
#define TATTR_STATUS_RED    16
#define TATTR_STATUS_YELLOW 17
#define TATTR_STATUS_GREEN  18
#define TATTR_BORDER_COLOR  19

#endif /* SPECTALK_CONTRACT_H */
