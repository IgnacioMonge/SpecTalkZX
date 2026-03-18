#!/usr/bin/env python3
"""
BPE build orchestrator for SpecTalkZX.
Called by Makefile before compilation.

Pipeline:
  1. Copy src/*.c + include/*.h to build/bpe_src/ (working copies)
  2. Apply SB_ renames + structural patches + new SB_ constants
  3. Run BPE compression -> build/bpe_final/*.c
  4. Copy compressed files back to src/ for compilation
  5. Generate SPECTALK.DAT with BPE dict inserted

src/ originals are backed up to build/bpe_originals/ and restored
after compilation by a trap or explicit restore step.
"""

import os
import re
import sys
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(ROOT, 'build')

SRC_FILES = ['spectalk.c', 'irc_handlers.c', 'user_cmds.c']
HDR_FILES = ['spectalk.h']

# Constants audited as safe for BPE (screen-only)
SAFE_CONSTANTS = [
    'S_NOTCONN', 'S_FAIL', 'S_NOTSET', 'S_DISCONN', 'S_APPNAME', 'S_COPYRIGHT',
    'S_MAXWIN', 'S_SWITCHTO', 'S_TIMEOUT', 'S_NOWIN', 'S_RANGE_MINUTES',
    'S_MUST_CHAN', 'S_ARROW_IN', 'S_ARROW_OUT', 'S_EMPTY_PAT', 'S_ALREADY_IN',
    'S_ASTERISK', 'S_COLON_SP', 'S_SP_PAREN', 'S_TOPIC_PFX', 'S_YOU_LEFT',
    'S_CONN_REFUSED', 'S_INIT_DOTS', 'S_ON', 'S_OFF', 'S_CONNECTED', 'S_DOTS3',
    'S_NICK_INUSE', 'S_AS_SP', 'S_MIN', 'S_SET', 'S_DOT_SP', 'S_USAGE_MSG',
    'S_COMMA_SP', 'S_JOINED_SP', 'S_SMART',
]

# New SB_ constants for indirect screen-only usage
NEW_SB_CONSTANTS_DECL = """
extern const char SB_ABORTED[];
extern const char SB_INVALID_NICK[];
extern const char SB_AUTH_FAILED[];
extern const char SB_BANNED[];
extern const char SB_SERVER_ERR[];
extern const char SB_CONN_LOST[];
extern const char SB_WIFI_OK[];
extern const char SB_IRC_READY[];
extern const char SB_UNKNOWN_MODE[];"""

NEW_SB_CONSTANTS_DEF = """const char SB_ABORTED[] = "Aborted.";
const char SB_INVALID_NICK[] = "Invalid nick";
const char SB_AUTH_FAILED[] = "Auth failed";
const char SB_BANNED[] = "Banned";
const char SB_SERVER_ERR[] = "Server error";
const char SB_CONN_LOST[] = "Connection lost";
const char SB_WIFI_OK[] = "WiFi OK";
const char SB_IRC_READY[] = "IRC ready";
const char SB_UNKNOWN_MODE[] = "(unknown)";"""


def read_file(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


def apply_sb_renames(content):
    """Rename S_xxx -> SB_xxx for audited safe constants."""
    for old in SAFE_CONSTANTS:
        new = 'SB_' + old[2:]
        content = re.sub(r'\b' + re.escape(old) + r'\b', new, content)
    return content


def patch_spectalk_c(content):
    """Apply structural patches for BPE support."""
    # esx_count for SPECTALK.DAT with dict
    content = content.replace('esx_count = 373;', 'esx_count = 691;')

    # BPE bypass in main_print fast path
    content = content.replace(
        '        while (*p && len < SCREEN_COLS) { p++; len++; }',
        '        while (*p && len < SCREEN_COLS) {\n'
        '            if ((uint8_t)*p >= 0x80) goto slow_path;\n'
        '            p++; len++;\n'
        '        }'
    )
    content = content.replace(
        '        // Texto largo - usar slow path para wrap\n    }',
        '        // Texto largo - usar slow path para wrap\n    }\n    slow_path:'
    )

    # Add new SB_ constant definitions after S_AUTOAWAY
    content = content.replace(
        'const char S_AUTOAWAY[] = "Auto-away";',
        'const char S_AUTOAWAY[] = "Auto-away";\n' + NEW_SB_CONSTANTS_DEF
    )
    return content


def patch_spectalk_h(content):
    """Add new SB_ extern declarations."""
    content = content.replace(
        'extern const char S_AUTOAWAY[];',
        'extern const char S_AUTOAWAY[];' + NEW_SB_CONSTANTS_DECL
    )
    return content


def patch_user_cmds_c(content):
    """Apply help offset and abort_msg/status patches."""
    content = content.replace('ring_buffer + 373', 'ring_buffer + 691')

    # Replace inline literals with SB_ constants
    content = content.replace('abort_msg = "Aborted.";', 'abort_msg = SB_ABORTED;')
    content = content.replace('abort_msg = "Invalid nick";', 'abort_msg = SB_INVALID_NICK;')
    content = content.replace('abort_msg = "Auth failed";', 'abort_msg = SB_AUTH_FAILED;')
    content = content.replace('abort_msg = "Banned";', 'abort_msg = SB_BANNED;')
    content = content.replace('abort_msg = "Server error";', 'abort_msg = SB_SERVER_ERR;')
    content = content.replace('abort_msg = "Connection lost";', 'abort_msg = SB_CONN_LOST;')
    content = content.replace('st = "WiFi OK";', 'st = SB_WIFI_OK;')
    content = content.replace('st = "IRC ready";', 'st = SB_IRC_READY;')
    return content


def patch_irc_handlers_c(content):
    """Replace (unknown) mode text."""
    content = content.replace(
        'mode_text = "(unknown)";',
        'mode_text = SB_UNKNOWN_MODE;'
    )
    return content


def main():
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    from bpe_compress import (extract_string_literals, build_corpus,
                               bpe_compress, generate_compressed_sources,
                               generate_dict_binary)

    bpe_src = os.path.join(BUILD_DIR, 'bpe_src')
    bpe_final = os.path.join(BUILD_DIR, 'bpe_final')
    os.makedirs(bpe_src, exist_ok=True)
    os.makedirs(bpe_final, exist_ok=True)

    # Step 1: Copy and patch sources
    for f in SRC_FILES:
        content = read_file(os.path.join(ROOT, 'src', f))
        content = apply_sb_renames(content)
        if f == 'spectalk.c':
            content = patch_spectalk_c(content)
        elif f == 'user_cmds.c':
            content = patch_user_cmds_c(content)
        elif f == 'irc_handlers.c':
            content = patch_irc_handlers_c(content)
        write_file(os.path.join(bpe_src, f), content)

    for f in HDR_FILES:
        content = read_file(os.path.join(ROOT, 'include', f))
        content = apply_sb_renames(content)
        content = patch_spectalk_h(content)
        write_file(os.path.join(bpe_src, f), content)

    # Step 2: Run BPE compression
    src_paths = [os.path.join(bpe_src, f) for f in SRC_FILES]
    strings = extract_string_literals(src_paths)
    corpus = build_corpus(strings)
    compressed, dictionary = bpe_compress(corpus)

    screen_count = sum(1 for s in strings if s['screen_only'])
    content_bytes = sum(1 for b in corpus if b != 0)
    compressed_bytes = sum(1 for b in compressed if b != 0)

    # Step 3: Generate compressed sources
    generate_compressed_sources(src_paths, strings, dictionary, bpe_final)

    # Step 4: Generate dict binary
    dict_bin = generate_dict_binary(dictionary)
    dict_path = os.path.join(BUILD_DIR, 'bpe_dict.bin')
    with open(dict_path, 'wb') as f:
        f.write(dict_bin)

    # Step 5: Generate SPECTALK.DAT with dict
    dat_orig = os.path.join(ROOT, 'src', 'SPECTALK.DAT')
    dat_out = os.path.join(BUILD_DIR, 'SPECTALK.DAT')
    with open(dat_orig, 'rb') as f:
        orig = f.read()
    with open(dat_out, 'wb') as f:
        f.write(orig[:373] + dict_bin + orig[373:])

    # Step 6: Install compressed files for compilation
    for f in SRC_FILES:
        shutil.copy2(os.path.join(bpe_final, f), os.path.join(ROOT, 'src', f))
    shutil.copy2(os.path.join(bpe_src, 'spectalk.h'), os.path.join(ROOT, 'include', 'spectalk.h'))
    shutil.copy2(dat_out, os.path.join(ROOT, 'src', 'SPECTALK.DAT'))

    # Report
    saved = content_bytes - compressed_bytes
    print(f"  BPE: {screen_count} strings, {len(dictionary)} tokens, "
          f"{content_bytes}B -> {compressed_bytes}B (-{saved}B)")


if __name__ == '__main__':
    main()
