#!/usr/bin/env python3
"""Lock the compact RTC range checks to the original accepted value sets."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def bcd_to_bin(value):
    lo = value & 0x0F
    hi = value >> 4
    return None if lo >= 10 or hi >= 10 else hi * 10 + lo


def main():
    source = (ROOT / "overlay/rtc_seed_ovl.asm").read_text(encoding="utf-8")
    code = " ".join(line.split(";", 1)[0].strip() for line in source.splitlines())
    code = " ".join(code.split())

    assert "ld a, b sub 88 cp 24 jr nc, rtc_fail" in code
    assert re.findall(r"ld d, (\d+) call pcf_bcd_below", code) == [
        "60", "60", "24", "32", "13", "36",
    ]
    assert "bit 7, a jr nz, pcf_fail" in code
    assert code.count("or a jr z, pcf_fail") == 2
    assert "ld d, 36 call pcf_bcd_below ret c cp 24 jr c, pcf_fail" in code
    assert "pcf_bcd_below: call bcd_to_bin ret c cp d ccf ret" in code

    for raw in range(256):
        old_year = 88 <= raw < 112
        new_year = ((raw - 88) & 0xFF) < 24
        assert new_year == old_year

    fields = ((0x7F, 0, 60), (0x7F, 0, 60), (0x3F, 0, 24),
              (0x3F, 1, 32), (0x1F, 1, 13), (0xFF, 24, 36))
    for mask, lower, upper in fields:
        for raw in range(256):
            decoded = bcd_to_bin(raw & mask)
            old = decoded is not None and lower <= decoded < upper
            helper = decoded is not None and decoded < upper
            new = helper and decoded >= lower
            assert new == old

    print("RTC compact validation check OK")


if __name__ == "__main__":
    main()
