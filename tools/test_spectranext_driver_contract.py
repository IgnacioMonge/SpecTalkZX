#!/usr/bin/env python3
"""Pin the Spectranext ROM bridge interface used by the Spectranext build."""

import hashlib
from pathlib import Path
import sys


AUTHORITY_COMMIT = "22bf780"
# SHA-256 of the LF-normalized 22bf780 driver blobs.
EXPECTED_SHA256 = {
    "spxn_rom.h": "bfa77ffd7264dfa024ea4ff764e0f9eef0dc5b28ec3910c869b5bbb016c50830",
    "spxn_rom.asm": "b8513587d0cd6b7d049d06a65dc68dac43829e8e8a89d7324606982e789e29b3",
}


def normalized_digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def main(argv=None) -> None:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        raise SystemExit("usage: test_spectranext_driver_contract.py SPXN_DIR")

    for name, expected in EXPECTED_SHA256.items():
        path = Path(argv[0]) / name
        if not path.is_file():
            raise SystemExit(f"[FATAL] missing Spectranext driver file: {path}")
        try:
            actual = normalized_digest(path)
        except OSError as error:
            raise SystemExit(f"[FATAL] cannot read Spectranext driver file {path}: {error}") from error
        if actual != expected:
            raise SystemExit(
                f"[FATAL] Spectranext driver hash mismatch for {path}: "
                f"got {actual}, expected {expected} ({AUTHORITY_COMMIT}:driver/{name})"
            )
    print(f"Spectranext driver contract OK ({AUTHORITY_COMMIT}:driver/spxn_rom.h+asm)")


if __name__ == "__main__":
    main()
