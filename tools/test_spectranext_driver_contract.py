#!/usr/bin/env python3
"""Pin the SpectraNext ROM bridge header used by the SpectraNext build."""

import hashlib
from pathlib import Path
import sys


AUTHORITY_COMMIT = "a4ae350"
# SHA-256 of the LF-normalized a4ae350:driver/spxn_rom.h blob.
EXPECTED_SHA256 = "1c0fa00fdef30f134e514135d2979800f20d9d6713250662ccf7de063530fba9"


def normalized_digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def main(argv=None) -> None:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        raise SystemExit("usage: test_spectranext_driver_contract.py SPXN_DIR")

    header = Path(argv[0]) / "spxn_rom.h"
    if not header.is_file():
        raise SystemExit(f"[FATAL] missing SpectraNext driver header: {header}")
    try:
        actual = normalized_digest(header)
    except OSError as error:
        raise SystemExit(f"[FATAL] cannot read SpectraNext driver header {header}: {error}") from error
    if actual != EXPECTED_SHA256:
        raise SystemExit(
            f"[FATAL] SpectraNext driver header hash mismatch for {header}: "
            f"got {actual}, expected {EXPECTED_SHA256} ({AUTHORITY_COMMIT}:driver/spxn_rom.h)"
        )
    print(f"SpectraNext driver contract OK ({AUTHORITY_COMMIT}:driver/spxn_rom.h)")


if __name__ == "__main__":
    main()
