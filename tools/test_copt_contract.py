#!/usr/bin/env python3
"""Fail-closed checks for the custom COPT rule and evidence listing contract."""

import argparse
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "Makefile"
RULES = ROOT / "src" / "spectalk_copt.rul"


def verify_rule_file() -> None:
    raw_rules = RULES.read_bytes()
    if b"\r" in raw_rules:
        raise AssertionError("custom COPT rules must use LF; CRLF disables rule parsing")

    rules = raw_rules.decode("utf-8")
    if "OPT-COPT-35" in rules:
        raise AssertionError("COPT-35 is disabled because it does not preserve Z")
    required = ("OPT-COPT-1", "_hl_mul32", "OPT-COPT-2", "_rx_pos_reset")
    missing = [token for token in required if token not in rules]
    if missing:
        raise AssertionError("custom COPT contract missing: " + ", ".join(missing))

    makefile = MAKEFILE.read_text(encoding="utf-8")
    if "-custom-copt-rules=src/spectalk_copt.rul" not in makefile:
        raise AssertionError("Makefile does not wire the custom COPT rule file")

    probe = "\tadd\thl, hl\n" * 5
    result = subprocess.run(
        ["z88dk-copt", "-mz80", str(RULES)],
        input=probe,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise AssertionError("z88dk-copt probe failed: " + result.stderr.strip())
    if not re.search(r"\bcall\s+_hl_mul32\b", result.stdout):
        raise AssertionError("custom COPT probe did not fire OPT-COPT-1")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--listing-dir", type=Path)
    args = parser.parse_args()

    verify_rule_file()

    if args.listing_dir is None:
        print("custom COPT rule contract OK (listing not supplied)")
        return

    listings = sorted(args.listing_dir.rglob("main_build.c.lis"))
    if len(listings) != 1:
        raise AssertionError("expected exactly one resident compiler listing")
    text = listings[0].read_text(encoding="utf-8", errors="replace")
    if not re.search(r"\bcall\s+_hl_mul32\b", text):
        raise AssertionError("OPT-COPT-1 rewrite absent from resident compiler listing")
    print("custom COPT listing contract OK")


if __name__ == "__main__":
    main()
