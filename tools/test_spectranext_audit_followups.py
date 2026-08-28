#!/usr/bin/env python3
"""Source contracts for the SpectraNext 1.3.9 audit follow-ups."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="strict")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : pos + 1]
    raise AssertionError(f"unterminated function: {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", " ", source)


def section(source: str, start: str, end: str) -> str:
    begin = source.index(start) + len(start)
    return source[begin : source.index(end, begin)]


def response_path_contract(fetch: str) -> None:
    recv_success = "if (spxn_rom_ixcall(ROM_RECVFROM) & ROM_CARRY) break;"
    length_assignment = "length = spxn_regs.bc;"
    close_call = "close_socket(fd);"
    validation = "if (length < NTP_PACKET_SIZE"
    decode = "decode_time"

    recv = fetch.index(recv_success)
    length = fetch.index(length_assignment, recv)
    close = fetch.index(close_call, length)
    validation_start = fetch.index(validation, length)
    decode_start = fetch.index(decode, validation_start)
    length_end = length + len(length_assignment)
    close_end = close + len(close_call)

    assert length_end < close < validation_start < decode_start, (
        "response receive must close before validation"
    )
    assert not fetch[length_end:close].strip(), "response close is not immediate"
    assert not fetch[close_end:validation_start].strip(), (
        "response validation must follow close"
    )


def ntp_contract() -> None:
    source = text("overlay/spectranext_clock_ovl.c")
    fetch = function_body(source, "static uint8_t fetch_utc(")
    validation_start = fetch.index("if (length < NTP_PACKET_SIZE")
    decode_start = fetch.index("decode_time", validation_start)
    validation = fetch[validation_start:decode_start]
    normalized = compact(validation)

    for check in (
        "length < NTP_PACKET_SIZE",
        "!same_host(&source, &destination)",
        "(overlay_slot[0] & 0xC0u) == 0xC0u",
        "((overlay_slot[0] & 7u) != 4u && (overlay_slot[0] & 7u) != 5u)",
        "!overlay_slot[1]",
        "overlay_slot[1] > 15u",
    ):
        assert check in normalized, f"NTP validation lost check: {check}"

    zero_field = (
        "!(overlay_slot[NTP_TX_SECONDS] | "
        "overlay_slot[NTP_TX_SECONDS + 1u] | "
        "overlay_slot[NTP_TX_SECONDS + 2u] | "
        "overlay_slot[NTP_TX_SECONDS + 3u])"
    )
    assert zero_field in normalized, "NTP zero transmit-seconds field is not rejected"
    assert validation_start < decode_start

    response_path_contract(fetch)
    recv = fetch.index("if (spxn_rom_ixcall(ROM_RECVFROM) & ROM_CARRY) break;")
    length = fetch.index("length = spxn_regs.bc;", recv)
    close = fetch.index("close_socket(fd);", length)
    mutated = fetch[:close] + fetch[close + len("close_socket(fd);") :]
    try:
        response_path_contract(mutated)
    except AssertionError as error:
        assert str(error) == "response receive must close before validation"
    else:
        raise AssertionError("deleting the response-path close was not detected")


def copt_contract() -> None:
    rules = text("src/spectalk_copt.rul")
    classic_rules = (
        (
            "_S_SP_COLON",
            "_net_sp_colon",
        ),
        (
            "_S_PRIVMSG",
            "_net_privmsg",
        ),
    )
    for payload, helper in classic_rules:
        classic = (
            f"%0ld%1hl,{payload}\n"
            "%0call%1_uart_send_string\n"
            "=\n"
            f"\tEXTERN {helper}\n"
            f"%0call%1{helper}"
        )
        assert rules.count(classic) == 1, f"Classic copt rule changed or duplicated: {payload}"

        target = (
            f"%0ld%1hl,{payload}\n"
            "%0call%1_net_send_string\n"
            "=\n"
            f"\tEXTERN {helper}\n"
            f"%0call%1{helper}"
        )
        assert rules.count(target) == 1, f"missing SpectraNext copt rule: {payload}"

    asm = text("asm/spectalk_asm/80_ui_runtime.asm")
    assert "_net_sp_colon:\n    ld hl, _S_SP_COLON\n    jp _net_send_string" in asm
    assert "_net_privmsg:\n    ld hl, _S_PRIVMSG\n    jp _net_send_string" in asm

    header = text("include/spectalk_net.h")
    target, classic = section(header, "#ifdef SPECTALK_SPECTRANEXT", "#endif") .split("#else", 1)
    assert "void net_send_string(const char *text)" in target
    assert "#define net_send_string uart_send_string" in classic


def frame_wait_contract() -> None:
    net = text("src/net_spectranext.c")
    frame_wait = function_body(net, "void net_frame_wait(void)")
    assert frame_wait.index("frame_wait();") < frame_wait.index("net_pump_rx();")

    asm = text("asm/spectalk_asm/80_ui_runtime.asm")
    assert "PUBLIC _frame_wait_drain" in asm
    target = section(asm, "IFDEF SPECTALK_SPECTRANEXT", "ELSE")
    assert target.index("call _frame_wait") < target.index("jp _net_pump_rx")
    classic = section(asm, "ELSE", "ENDIF")
    assert "call uartRead" in classic
    assert "call _rb_push" in classic

    common = text("src/spectalk.c")
    for signature in ("uint8_t wait_for_response(", "uint8_t wait_for_prompt_char("):
        assert "frame_wait_drain();" in function_body(common, signature)
    assert "extern void frame_wait_drain(void);" in text("include/spectalk.h")

    header = text("include/spectalk_net.h")
    assert "#define net_frame_wait   frame_wait_drain" in header
    for path in ("src/user_cmds.c", "src/spectalk.c"):
        assert re.search(r"\bnet_frame_wait\s*\(", text(path)), path


def about_pump_contract() -> None:
    pump = function_body(text("src/net_spectranext.c"), "void spectranext_about_pump(void)")
    newline = pump.index("if (byte == '\\n')")
    line_end = pump.index("rx_pos = 0;", newline)
    after_reset = pump[line_end + len("rx_pos = 0;"):]
    assert after_reset.lstrip().startswith("return;"), (
        "ABOUT pump must yield after one complete LF-terminated line"
    )
    line = pump[newline:]
    assert "if (rx_overflow)" in line
    assert "parse_irc_message(rx_line);" in line


def memory_layout_contract() -> None:
    layout = text("tools/check_memory_layout.py")
    assert 'platform="classic"' in layout
    assert 'if platform == "spectranext":' in layout
    assert 'choices=("classic", "spectranext")' in layout
    assert "missing all Spectranext map symbols was accepted" in layout

    makefile = text("Makefile")
    trim = section(makefile, "trim: $(TAP) $(MAP)", "# ------------------------------------------------------------\n# OVERLAY")
    assert 'check_memory_layout.py "$(MAP)" --platform "$(PLATFORM)"' in trim


def inventory_contract() -> None:
    cache = text("tools/test_esxdos_cache_contract.py")
    assert "src/storage_spectranext.c" not in cache


def driver_contract() -> None:
    driver = text("tools/test_spectranext_driver_contract.py")
    assert 'AUTHORITY_COMMIT = "a4ae350"' in driver
    assert "EXPECTED_SHA256 = \"1c0fa00fdef30f134e514135d2979800f20d9d6713250662ccf7de063530fba9\"" in driver
    assert "replace(b\"\\r\\n\", b\"\\n\")" in driver
    assert "C:\\" not in driver and "C:/" not in driver

    makefile = text("Makefile")
    target = section(makefile, "spectranext:\n", "\ntest-spectranext-network:")
    check = "test_spectranext_driver_contract.py"
    assert target.count(check) == 1
    assert f'{check} "$(SPXN_DIR)"' in target
    assert makefile.count(check) == 1
    assert target.index(check) < target.index("$(MAKE)")


def save_args_contract() -> None:
    architecture = text("ARCHITECTURE.md")
    assert "A command handler must not read parser arguments in `temp_input` after starting an XFS directory transaction" in architecture
    assert "`READDIR` may overwrite that half of the scratch" in architecture

    save = function_body(text("src/user_cmds.c"), "void cmd_save(")
    marker = "(void)args;"
    marker_end = save.index(marker) + len(marker)
    assert "args" not in save[marker_end:], "cmd_save reads args after discarding them"


def main() -> None:
    ntp_contract()
    copt_contract()
    frame_wait_contract()
    about_pump_contract()
    memory_layout_contract()
    inventory_contract()
    driver_contract()
    save_args_contract()
    print("SpectraNext audit follow-up contracts OK")


if __name__ == "__main__":
    main()
