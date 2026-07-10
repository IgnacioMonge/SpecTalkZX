#!/usr/bin/env python3
"""Fail-closed linker-map gate for SpecTalkZX fixed RAM contracts."""

import argparse
import re
from pathlib import Path


EXPECTED = {
    "_input_cache_char": 0x5B00,
    "glyph_buffer": 0x5BC0,
    "plf_left_buf": 0x5BC8,
    "plf_attr_val": 0x5BD0,
    "plf_y_val": 0x5BD1,
    "bpe_rstack": 0x5BD3,
    "_net_short_buf": 0x5BD3,
    "bpe_rsp": 0x5BE3,
    "mpwr_last_space": 0x5BE5,
    "FMT_BUF_ADDR": 0x5BE7,
    "_pkt_usr": 0x5BF0,
    "_pkt_par": 0x5BF2,
    "_pkt_rest": 0x5BF4,
    "_pkt_txt": 0x5BF6,
    "_pkt_cmd": 0x5BF8,
    "_last_cmd_id": 0x5BFA,
    "_nb_p": 0x5BFE,
    "_line_buffer": 0x5CB6,
    "_temp_input": 0x5D36,
    "_ring_buffer": 0xF500,
    "_ignore_list": 0xFD00,
    "TAR__register_sp": 0xFF58,
    "CRT_STACK_SIZE": 0x0200,
    "_friend_nicks": 0xFF58,
    "_away_message": 0xFFB2,
    "_names_target_channel": 0xFFD2,
}

ALIASES = ("_rx_line", "_overlay_slot")

PERSISTENT_SIZES = {
    "_notif_buf": 64,
    "_names_friend_pos": 1,
    "_pkt_empty": 1,
    "_plf_start_byte": 1,
    "_plf_pair_count": 1,
}

REQUIRED = ("__BSS_END_tail", *EXPECTED, *ALIASES, *PERSISTENT_SIZES)

SIZES = {
    "_ring_buffer": 2048,
    "_ignore_list": 80,
    "_friend_nicks": 90,
    "_away_message": 32,
    "_names_target_channel": 32,
}

MAP_SYMBOL = re.compile(r"^(\S+)\s*=\s*\$([0-9A-Fa-f]+)\s*;")


def read_symbols(path):
    symbols = {}
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        match = MAP_SYMBOL.match(line)
        if not match:
            continue
        name, value = match.group(1), int(match.group(2), 16)
        if name in symbols and symbols[name] != value:
            raise ValueError(f"conflicting map values for {name}")
        symbols[name] = value
    c_source = Path(__file__).resolve().parents[1] / "src" / "spectalk.c"
    match = re.search(r"^#define FMT_BUF_ADDR\s+0x([0-9A-Fa-f]+)",
                      c_source.read_text(encoding="utf-8", errors="replace"), re.MULTILINE)
    if match:
        symbols["FMT_BUF_ADDR"] = int(match.group(1), 16)
    return symbols


def validate(symbols, bss_guard=96, bss_warn=128):
    missing = [name for name in REQUIRED if name not in symbols]
    if missing:
        raise ValueError("missing map symbols: " + ", ".join(missing))

    errors = []
    for name, expected in EXPECTED.items():
        actual = symbols[name]
        if actual != expected:
            errors.append(f"{name}=0x{actual:04X}, expected 0x{expected:04X}")

    bss_end = symbols["__BSS_END_tail"]
    ring = symbols["_ring_buffer"]
    ignore = symbols["_ignore_list"]
    stack_top = symbols["TAR__register_sp"]
    stack_low = stack_top - symbols["CRT_STACK_SIZE"]
    friend = symbols["_friend_nicks"]
    away = symbols["_away_message"]
    names = symbols["_names_target_channel"]
    gap = ring - bss_end

    fixed = symbols

    persistent_floor = fixed["_rx_line"] + 512
    persistent = sorted((fixed[name], fixed[name] + size, name)
                        for name, size in PERSISTENT_SIZES.items())
    for start, end, name in persistent:
        if start < persistent_floor or end > bss_end:
            errors.append(
                f"{name}=0x{start:04X}-0x{end - 1:04X} is outside persistent BSS")
    for (_, previous_end, previous_name), (start, _, name) in zip(persistent, persistent[1:]):
        if previous_end > start:
            errors.append(f"persistent BSS overlap: {previous_name}/{name}")

    checks = (
        (fixed["_input_cache_char"] + 128 <= fixed["glyph_buffer"], "input cache overlaps render scratch"),
        (fixed["glyph_buffer"] + 7 <= fixed["plf_left_buf"], "glyph scratch overlaps PLF scratch"),
        (fixed["plf_left_buf"] + 8 <= fixed["plf_attr_val"], "PLF left scratch overlaps attr"),
        (fixed["plf_attr_val"] + 1 <= fixed["plf_y_val"], "PLF attr overlaps row scratch"),
        (fixed["plf_y_val"] + 1 <= fixed["bpe_rstack"], "PLF row overlaps BPE stack"),
        (fixed["bpe_rstack"] == fixed["_net_short_buf"] and
         fixed["_net_short_buf"] + 12 <= fixed["bpe_rstack"] + 16,
         "net scratch is not contained in BPE stack"),
        (fixed["bpe_rstack"] + 16 <= fixed["bpe_rsp"], "BPE stack overlaps pointer"),
        (fixed["bpe_rsp"] + 2 <= fixed["mpwr_last_space"], "BPE pointer overlaps wrap scratch"),
        (fixed["mpwr_last_space"] + 2 <= fixed["FMT_BUF_ADDR"], "wrap scratch overlaps fmt buffer"),
        (fixed["FMT_BUF_ADDR"] + 8 <= fixed["_pkt_usr"], "fmt buffer overlaps parser state"),
        (fixed["_nb_p"] + 2 <= 0x5C00, "parser state exceeds Printer Buffer"),
        (fixed["_line_buffer"] >= 0x5CB6, "line buffer overlaps ROM system variables"),
        (fixed["_line_buffer"] + 128 <= fixed["_temp_input"], "line buffer overlaps temp input"),
        (fixed["_temp_input"] + 128 <= 0x5DB6, "temp input exceeds CHANS workspace"),
        (fixed["_temp_input"] + 128 <= fixed["_rx_line"], "CHANS workspace overlaps rx_line"),
        (fixed["_rx_line"] == fixed["_overlay_slot"], "rx_line/overlay_slot alias moved"),
        (fixed["_overlay_slot"] + 512 <= bss_end, "overlay slot exceeds linked BSS"),
        (gap >= bss_guard, f"BSS/ring gap {gap}B < required {bss_guard}B"),
        (ring + SIZES["_ring_buffer"] <= ignore, "ring overlaps ignore list"),
        (ignore + SIZES["_ignore_list"] <= stack_low, "ignore list overlaps stack"),
        (stack_low >= 0, "stack low address underflows"),
        (stack_top <= friend, "stack overlaps friend_nicks"),
        (friend + SIZES["_friend_nicks"] <= away, "friend_nicks overlaps away_message"),
        (away + SIZES["_away_message"] <= names, "away_message overlaps NAMES target"),
        (names + SIZES["_names_target_channel"] <= 0x10000, "NAMES target exceeds RAM"),
    )
    errors.extend(message for ok, message in checks if not ok)
    if errors:
        raise ValueError("; ".join(errors))

    return {
        "bss_end": bss_end,
        "gap": gap,
        "warn": gap < bss_warn,
        "ring_end": ring + SIZES["_ring_buffer"],
        "ignore_end": ignore + SIZES["_ignore_list"],
        "stack_low": stack_low,
        "stack_top": stack_top,
        "fixed_end": names + SIZES["_names_target_channel"],
        "printer_end": fixed["_nb_p"] + 2,
        "chans_end": fixed["_temp_input"] + 128,
    }


def self_test():
    persistent = {
        "_notif_buf": 0xEA00,
        "_names_friend_pos": 0xEA40,
        "_pkt_empty": 0xEA41,
        "_plf_start_byte": 0xEA42,
        "_plf_pair_count": 0xEA43,
    }
    symbols = {
        "__BSS_END_tail": 0xEF4A,
        **EXPECTED,
        "_rx_line": 0xE800,
        "_overlay_slot": 0xE800,
        **persistent,
    }
    validate(symbols)
    for name, value in (
        ("_ignore_list", 0xFCF0),
        ("CRT_STACK_SIZE", 0x0210),
        ("_friend_nicks", 0xFF50),
        ("_names_target_channel", 0xFFF0),
    ):
        broken = dict(symbols)
        broken[name] = value
        try:
            validate(broken)
        except ValueError:
            pass
        else:
            raise AssertionError(f"mutation was accepted: {name}")
    for name, value in (("_notif_buf", 0x5B80), ("_pkt_empty", 0xEA40)):
        broken = dict(symbols)
        broken[name] = value
        try:
            validate(broken)
        except ValueError:
            pass
        else:
            raise AssertionError(f"persistent-state mutation was accepted: {name}")
    broken = dict(symbols)
    broken["_overlay_slot"] += 1
    try:
        validate(broken)
    except ValueError:
        pass
    else:
        raise AssertionError("rx_line/overlay_slot alias mutation was accepted")
    broken = dict(symbols)
    broken["_rx_line"] = broken["_overlay_slot"] = 0x5D00
    try:
        validate(broken)
    except ValueError:
        pass
    else:
        raise AssertionError("CHANS/rx_line overlap mutation was accepted")
    broken = dict(symbols)
    del broken["_away_message"]
    try:
        validate(broken)
    except ValueError:
        pass
    else:
        raise AssertionError("missing symbol was accepted")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("map", nargs="?")
    parser.add_argument("--bss-guard", type=lambda value: int(value, 0), default=96)
    parser.add_argument("--bss-warn", type=lambda value: int(value, 0), default=128)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        print("Memory layout checker self-test OK")
        return
    if not args.map:
        parser.error("map is required unless --self-test is used")

    try:
        result = validate(read_symbols(args.map), args.bss_guard, args.bss_warn)
    except (OSError, ValueError) as error:
        raise SystemExit(f"[FATAL] Memory layout: {error}") from error

    level = "WARN" if result["warn"] else "OK"
    print(f"[{level}] Memory layout: BSS 0x{result['bss_end']:04X}, "
          f"{result['gap']}B to ring; ring ends 0x{result['ring_end']:04X}; "
          f"ignore ends 0x{result['ignore_end']:04X}; "
          f"stack 0x{result['stack_low']:04X}-0x{result['stack_top'] - 1:04X}; "
          f"fixed data ends 0x{result['fixed_end']:04X}; "
          f"Printer/CHANS end 0x{result['printer_end']:04X}/0x{result['chans_end']:04X}")


if __name__ == "__main__":
    main()
