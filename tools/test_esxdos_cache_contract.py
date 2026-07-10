#!/usr/bin/env python3
"""Lock runtime esxDOS transactions and persistent Printer-state boundaries."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

EXPECTED_IO_COUNTS = {
    "asm/overlay_loader.asm": 6,
    "asm/spectalk_asm/60_protocol_storage.asm": 6,
    "overlay/bookmark_store_ovl.c": 6,
    "overlay/bookmarks_ovl.c": 5,
    "overlay/earth_about_render.asm": 7,
    "overlay/overlay_entry2.asm": 2,
    "overlay/rtc_seed_ovl.asm": 2,
    "overlay/spectalk_ovl.c": 5,
    "overlay/spectalk_ovl4.c": 4,
    "src/spectalk.c": 6,
}

C_IO = re.compile(r"\besx_f(?:open|create|read|write|close|seek_set)\s*\(")
ASM_IO = re.compile(r"\bcall\s+_esx_f(?:open|create|read|write|close|seek_set)\b", re.I)
RST8 = re.compile(r"^\s*rst\s+8\b", re.I | re.M)


def source(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def words(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    code = "\n".join(line.split("//", 1)[0] for line in text.splitlines())
    return " ".join(code.split())


def block(text, start, end=None):
    result = text.split(start, 1)[1]
    return result.split(end, 1)[0] if end else result


def io_inventory():
    observed = {}
    for base in (ROOT / "src", ROOT / "asm", ROOT / "overlay"):
        for path in base.rglob("*"):
            if path.suffix.lower() not in (".c", ".asm"):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if path.suffix.lower() == ".c":
                count = sum(len(C_IO.findall(line)) for line in text.splitlines()
                            if "extern " not in line)
            else:
                count = len(ASM_IO.findall(text)) + len(RST8.findall(text))
            if count:
                observed[path.relative_to(ROOT).as_posix()] = count
    assert observed == EXPECTED_IO_COUNTS, (observed, EXPECTED_IO_COUNTS)


def main():
    io_inventory()

    persistent = words(block(source("src/spectalk.c"),
                             "char rx_line[RX_LINE_SIZE];", "uint16_t rx_pos;"))
    for declaration in (
        "char notif_buf[64]", "uint8_t names_friend_pos", "char pkt_empty[1]",
        "uint8_t plf_start_byte", "uint8_t plf_pair_count",
    ):
        assert declaration in persistent

    fixed_sources = (source("asm/spectalk_asm/00_preamble.asm") +
                     source("asm/spectalk_asm/80_ui_runtime.asm"))
    for symbol in ("_notif_buf", "_names_friend_pos", "_pkt_empty", "_plf_start_byte"):
        assert not re.search(rf"\bdefc\s+{symbol}\s*=", fixed_sources, re.I)
    renderer = (source("asm/spectalk_asm/30_rendering.asm") +
                source("asm/spectalk_asm/50_main_output.asm"))
    assert not re.search(r"(?<![_A-Za-z0-9])plf_pair_count\b", renderer)

    names = words(block(source("src/irc_handlers.c"),
                        "static void h_numeric_353", "static void h_numeric_321"))
    assert names.count("if (names_friend_pos >= 64) names_friend_pos = 0") == 2

    help_c = source("overlay/spectalk_ovl.c")
    help_io = words(block(help_c, "static void help_load_segment", "static uint8_t load_next_seg"))
    assert "if (!esx_handle) goto help_io_fail" in help_io
    assert "esx_fclose(); goto help_io_fail" in help_io
    assert "esx_fclose(); input_cache_invalidate()" in help_io
    assert "help_io_fail: input_cache_invalidate(); overlay_mode = 0" in help_io

    config = words(block(source("overlay/spectalk_ovl4.c"), "void save_config_ovl"))
    assert config.count("input_cache_invalidate()") == 1
    assert "done: input_cache_invalidate(); reset_rx_state()" in config

    store_c = source("overlay/bookmark_store_ovl.c")
    store_line = words(block(store_c, "static const char *bm_line", "static const char *bm_next_field"))
    store_save = words(block(store_c, "void bookmarks_save_ovl"))
    assert "if (!esx_handle) { input_cache_invalidate(); return 0; }" in store_line
    assert "esx_fclose(); input_cache_invalidate()" in store_line
    assert "if (!esx_handle) { input_cache_invalidate(); goto err; }" in store_save
    assert "esx_fclose(); input_cache_invalidate()" in store_save

    bookmarks_c = source("overlay/bookmarks_ovl.c")
    bookmarks_line = words(block(bookmarks_c, "static const char *bm_line", "static uint8_t bm_server_eq"))
    bookmarks_delete = words(block(bookmarks_c, "void bookmarks_delete_ovl"))
    assert "if (!esx_handle) { input_cache_invalidate(); return 0; }" in bookmarks_line
    assert "esx_fclose(); input_cache_invalidate()" in bookmarks_line
    assert "if (!esx_handle) { input_cache_invalidate();" in bookmarks_delete
    assert "esx_fclose(); input_cache_invalidate()" in bookmarks_delete

    rtc = source("overlay/rtc_seed_ovl.asm")
    rtc_top = words(block(rtc, "_rtc_seed_ovl:", "; --- esxDOS"))
    assert "ret" not in rtc_top
    assert "jr nz, rtc_seed_done" in rtc_top
    assert "jr rtc_seed_done" in rtc_top
    assert "rtc_seed_done: jp _input_cache_invalidate" in rtc_top

    earth = words(block(source("overlay/earth_about_render.asm"),
                        "_about_close_ovl:", "_earth_draw_frame:"))
    assert "call _esx_fclose" in earth
    assert "ld (_earth_ready),a jp _input_cache_invalidate" in earth
    assert "_input_cache_invalidate" not in source("overlay/overlay_entry2.asm")

    loader = words(source("asm/overlay_loader.asm"))
    assert "call _esx_fclose call _input_cache_invalidate" in loader
    assert "ovl_fail: pop ix" in loader and "call _overlay_exit_full" in loader
    exit_full = words(block(source("asm/spectalk_asm/10_core_helpers.asm"),
                            "_overlay_exit_full:", "; -----------------------------------------------------------------------------"))
    assert "jp _redraw_input_full" in exit_full

    primitive = words(block(source("asm/spectalk_asm/20_rx_ring_uart.asm"),
                            "_input_cache_invalidate:", "; ============================================================================="))
    assert "ld hl, _input_cache_char ld (hl), 0xFF" in primitive
    assert "ld bc, 127 ldir ret" in primitive
    fixed = source("asm/spectalk_asm/80_ui_runtime.asm")
    assert "defc _input_cache_char = 0x5B00" in fixed

    print("esxDOS and persistent Printer-state boundary check OK")


if __name__ == "__main__":
    main()
