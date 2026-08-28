#!/usr/bin/env python3
"""Regression test for the registration-time IRC ERROR token boundary."""

from pathlib import Path
import re


SOURCE = Path(__file__).resolve().parents[1] / "src" / "user_cmds.c"
CMD_CONNECT = "static void cmd_connect(const char *args) __z88dk_fastcall"


ERROR_GUARD = re.compile(
    r'''uint16_tremaining=rx_last_len-\(uint16_t\)\(line-rx_line\);'''
    r'''if\(remaining>=5&&line\[0\]=='E'&&line\[1\]=='R'&&'''
    r'''line\[2\]=='R'&&line\[3\]=='O'&&line\[4\]=='R'&&'''
    r'''\(remaining==5\|\|line\[5\]==' '\)\)\{'''
    r'''abort_msg="Server error";abort_disc=1;gotojoin_fail;\}'''
)


HISTORICAL_GUARD = re.compile(
    r"remaining>=6.*line\[5\]=='R'", re.DOTALL
)


def registration_error(line: str) -> bool:
    """Model the exact five-byte command and its following token boundary."""
    return line.startswith("ERROR") and (
        len(line) == 5 or line[5] == " "
    )


def compact_c(source: str) -> str:
    """Remove formatting whitespace while preserving C literals exactly."""
    compact = []
    quote = None
    escaped = False
    for char in source:
        if quote is not None:
            compact.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char in "'\"":
            quote = char
            compact.append(char)
        elif not char.isspace():
            compact.append(char)
    return "".join(compact)


def extract_function(source: str, signature: str) -> str:
    """Extract one C function while ignoring braces in comments/literals."""
    start = source.index(signature)
    opening = source.index("{", start + len(signature))
    depth = 0
    quote = None
    escaped = False
    line_comment = False
    block_comment = False
    index = opening
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 1
        elif quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and next_char == "/":
            line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            block_comment = True
            index += 1
        elif char in "'\"":
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
        index += 1
    raise AssertionError(f"unterminated function: {signature}")


def registration_path(function: str) -> str:
    """Return only the registration branch, ending at its join-fail label."""
    branch = re.search(
        r"if\s*\(\s*irc_nick\s*\[\s*0\s*\]\s*\)\s*\{", function
    )
    label = function.find("join_fail:", branch.end() if branch else 0)
    if branch is None or label < 0:
        raise AssertionError("cmd_connect registration path/label not found")
    return function[branch.start() : label]


def assert_source_guard(source: str) -> None:
    """Require the complete guard inside cmd_connect's registration path."""
    function = extract_function(source, CMD_CONNECT)
    compact = compact_c(registration_path(function))
    matches = list(ERROR_GUARD.finditer(compact))
    if len(matches) != 1:
        raise AssertionError(
            "registration ERROR guard must contain one complete exact-token block"
        )
    if HISTORICAL_GUARD.search(compact):
        raise AssertionError("historical off-by-one ERROR guard is still present")


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    assert_source_guard(source)

    # The old guard must fail the source-level contract.
    historical = """
        static void cmd_connect(const char *args) __z88dk_fastcall {
            if (irc_nick[0]) {
                uint16_t remaining = rx_last_len - (uint16_t)(line - rx_line);
                if (remaining >= 6 && line[0] == 'E' && line[1] == 'R' && line[5] == 'R') {
                    abort_msg = "Server error";
                    abort_disc = 1;
                    goto join_fail;
                }
            join_fail:
                ;
            }
        }
    """
    try:
        assert_source_guard(historical)
    except AssertionError:
        pass
    else:
        raise AssertionError("historical ERROR guard was accepted")

    # A matching block in an unrelated function or before the registration
    # branch must not satisfy this regression test.
    exact = """
        uint16_t remaining = rx_last_len - (uint16_t)(line - rx_line);
        if (remaining >= 5 && line[0] == 'E' && line[1] == 'R' &&
            line[2] == 'R' && line[3] == 'O' && line[4] == 'R' &&
            (remaining == 5 || line[5] == ' ')) {
            abort_msg = "Server error";
            abort_disc = 1;
            goto join_fail;
        }
    """
    unrelated = f"""
        static void other(void) {{ {exact} }}
        static void cmd_connect(const char *args) __z88dk_fastcall {{
            if (irc_nick[0]) {{
            join_fail:
                ;
            }}
        }}
    """
    outside_branch = f"""
        static void cmd_connect(const char *args) __z88dk_fastcall {{
            {exact}
            if (irc_nick[0]) {{
            join_fail:
                ;
            }}
        }}
    """
    for fixture in (unrelated, outside_branch):
        try:
            assert_source_guard(fixture)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError("off-path ERROR guard was accepted")

    assert registration_error("ERROR :Closing Link")
    assert registration_error("ERROR")
    assert not registration_error("ERRORS :not an ERROR command")
    assert not registration_error("ERR")
    print("registration ERROR boundary check OK")


if __name__ == "__main__":
    main()
