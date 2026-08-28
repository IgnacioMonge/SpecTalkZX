#!/usr/bin/env python3
"""Lock the replaceable Classic clock acquisition boundary."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def function_span(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*[^;{{]*\{{", source, re.S)
    if not match:
        raise AssertionError(f"missing function: {name}")
    start = match.start()
    pos = match.end() - 1
    depth = 0
    while pos < len(source):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start : pos + 1]
        pos += 1
    raise AssertionError(f"unterminated function: {name}")


def ordered(source: str, items: tuple[str, ...], owner: str) -> None:
    pos = -1
    for item in items:
        pos = source.find(item, pos + 1)
        if pos < 0:
            raise AssertionError(f"{owner}: missing ordered operation: {item}")


header = text("include/spectalk_clock.h")
for alias, target in (
    ("clock_init", "classic_clock_init"),
    ("clock_query", "classic_clock_query"),
    ("clock_poll_rx", "classic_clock_poll_rx"),
    ("clock_sync_fallback", "sntp_udp_fallback"),
    ("clock_setup_state", "sntp_init_sent"),
    ("clock_waiting", "sntp_waiting"),
    ("clock_synced", "sntp_queried"),
):
    if not re.search(rf"^#define\s+{alias}\s+{target}$", header, re.M):
        raise AssertionError(f"missing Classic clock alias: {alias} -> {target}")
if not re.search(r"^#define\s+clock_seed_local\(\)\s+overlay_exec\(4, 1\)$", header, re.M):
    raise AssertionError("missing zero-cost Classic RTC seed alias")

classic = text("src/clock_classic.c")
init = function_span(classic, "classic_clock_init")
ordered(
    init,
    (
        "_sntp_tz",
        "TZ_RTC",
        "_sntp_init_sent",
        "STATE_WIFI_OK",
        "_S_SNTP_CFG",
        "_S_NTP_POOL",
        "(_sntp_init_sent), a",
        "(_sntp_waiting), a",
    ),
    "classic_clock_init",
)
query = function_span(classic, "classic_clock_query")
ordered(
    query,
    (
        "_sntp_init_sent",
        "_sntp_waiting",
        "STATE_WIFI_OK",
        "_S_AT_SNTPTIME",
        "(_sntp_waiting), a",
    ),
    "classic_clock_query",
)
poll = function_span(classic, "classic_clock_poll_rx")
ordered(
    poll,
    (
        "connection_state != STATE_WIFI_OK",
        "net_pump_rx()",
        "try_read_line_nodrain()",
        "sntp_process_response(rx_line)",
        "sntp_init_sent = 2",
        "sntp_waiting = 0",
    ),
    "classic_clock_poll_rx",
)

mechanism_call = re.compile(
    r"\b(?:sntp_init|sntp_query_time|sntp_udp_fallback|sntp_process_response)\s*\("
)
for path in ("src/irc_handlers.c", "src/user_cmds.c", "src/spectalk.c"):
    match = mechanism_call.search(text(path))
    if match:
        line = text(path).count("\n", 0, match.start()) + 1
        raise AssertionError(f"{path}:{line}: Classic clock mechanism escaped backend")

connect = function_span(text("src/user_cmds.c"), "cmd_connect")
ordered(
    connect,
    ("net_disconnect()", "clock_init()", "net_prepare(", "clock_sync_fallback()", "net_connect("),
    "cmd_connect",
)
process = function_span(text("src/irc_handlers.c"), "process_irc_data")
if "if (clock_poll_rx()) return;" not in process:
    raise AssertionError("IRC receive loop bypasses clock response seam")

main = function_span(text("src/spectalk.c"), "main")
for call in ("clock_seed_local()", "clock_init()", "clock_sync_fallback()", "clock_query()"):
    if call not in main:
        raise AssertionError(f"main clock policy bypasses seam: {call}")

scu = text("src/main_build.c")
ordered(scu, ('#include "clock_classic.c"', '#include "spectalk.c"'), "SCU")

print("Clock seam contract OK")
