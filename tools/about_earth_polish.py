#!/usr/bin/env python3
"""Post-process SpecTalkZX about Earth bitmap assets.

The source generator intentionally uses heavy 1-bit dithering. This pass keeps
that style, but fills single black pinholes that are fully surrounded by lit
pixels. It leaves colors/attributes untouched and re-encodes the frame delta
stream with the same SKIP/COPY format consumed by the overlay.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
EARTH_DIR = ROOT / "release" / "about_earth"
FRAME0 = EARTH_DIR / "earth_frame0.compact.bin"
FRAME_DELTAS = EARTH_DIR / "earth_frame_deltas.bin"
SPANS_ASM = EARTH_DIR / "earth_overlay_spans.asm"
FRAME_SIZE = 587
FRAME_COUNT = 24


def zx_addr_xy(addr: int) -> tuple[int, int]:
    for y in range(192):
        base = 0x4000 + ((y & 0xC0) << 5) + ((y & 0x07) << 8) + ((y & 0x38) << 2)
        if base <= addr < base + 32:
            return (addr - base) * 8, y
    raise ValueError(f"not a ZX screen address: {addr:#04x}")


def screen_down(addr: int) -> int:
    d = (addr >> 8) & 0xFF
    e = addr & 0xFF
    d = (d + 1) & 0xFF
    if d & 7:
        return (d << 8) | e
    e2 = e + 32
    carry = e2 > 0xFF
    e = e2 & 0xFF
    if not carry:
        d = (d - 8) & 0xFF
    return (d << 8) | e


def parse_screen_spans() -> list[tuple[int, int]]:
    text = SPANS_ASM.read_text(encoding="ascii")
    text = text.split("earth_screen_spans:", 1)[1].split("earth_attr_spans:", 1)[0]
    values: list[int] = []
    for line in text.splitlines():
        line = line.strip()
        if line.startswith(("DW", "DB")):
            values.append(int(line.split()[1].replace("$", "0x"), 16))

    pos = 0
    addr = values[pos]
    pos += 1
    first_len = values[pos]
    pos += 1
    spans = [(addr, first_len)]

    while pos < len(values):
        cmd = values[pos]
        pos += 1
        if cmd == 0:
            break
        addr = screen_down(addr)
        dx = cmd & 0xF0
        if dx:
            addr = (addr + (1 if dx == 0x10 else -1)) & 0xFFFF
        spans.append((addr, cmd & 0x0F))
    return spans


def split_delta_stream(data: bytes, target_size: int) -> list[bytes]:
    parts: list[bytes] = []
    off = 0
    while off < len(data):
        start = off
        pos = 0
        while True:
            if off >= len(data):
                raise ValueError(f"truncated delta at {start}")
            cmd = data[off]
            off += 1
            if cmd == 0:
                break
            if cmd < 0x80:
                pos += cmd
            else:
                count = (cmd & 0x7F) + 1
                off += count
                pos += count
            if off > len(data) or pos > target_size:
                raise ValueError(f"malformed delta at {start}")
        parts.append(data[start:off])
    return parts


def apply_delta(frame: bytes, delta: bytes) -> bytes:
    out = bytearray(frame)
    pos = 0
    off = 0
    while True:
        cmd = delta[off]
        off += 1
        if cmd == 0:
            break
        if cmd < 0x80:
            pos += cmd
        else:
            count = (cmd & 0x7F) + 1
            out[pos : pos + count] = delta[off : off + count]
            off += count
            pos += count
    return bytes(out)


def encode_delta(old: bytes, new: bytes) -> bytes:
    """Optimal byte-size encoder for the overlay SKIP/COPY delta format."""
    size = len(old)
    inf = 1_000_000
    best = [inf] * (size + 1)
    action: list[tuple[str, int] | None] = [None] * (size + 1)
    best[size] = 1  # terminator

    for pos in range(size - 1, -1, -1):
        equal = 0
        while (
            pos + equal < size and old[pos + equal] == new[pos + equal] and equal < 127
        ):
            equal += 1
            cost = 1 + best[pos + equal]
            if cost < best[pos]:
                best[pos] = cost
                action[pos] = ("skip", equal)

        max_copy = min(128, size - pos)
        for count in range(1, max_copy + 1):
            if old[pos : pos + count] == new[pos : pos + count]:
                continue
            cost = 1 + count + best[pos + count]
            if cost < best[pos]:
                best[pos] = cost
                action[pos] = ("copy", count)

    out = bytearray()
    pos = 0
    while pos < size:
        step = action[pos]
        if step is None:
            raise AssertionError(f"no delta action at {pos}")
        kind, count = step
        if kind == "skip":
            out.append(count)
        else:
            out.append(0x80 | (count - 1))
            out.extend(new[pos : pos + count])
        pos += count
    out.append(0)
    return bytes(out)


def compact_to_image(frame: bytes, spans: list[tuple[int, int]]) -> Image.Image:
    image = Image.new("1", (256, 192), 0)
    px = image.load()
    off = 0
    for addr, length in spans:
        x, y = zx_addr_xy(addr)
        for col in range(length):
            byte = frame[off]
            off += 1
            for bit in range(8):
                if byte & (0x80 >> bit):
                    px[x + col * 8 + bit, y] = 1
    return image


def image_to_compact(image: Image.Image, spans: list[tuple[int, int]]) -> bytes:
    px = image.load()
    out = bytearray()
    for addr, length in spans:
        x, y = zx_addr_xy(addr)
        for col in range(length):
            byte = 0
            for bit in range(8):
                if px[x + col * 8 + bit, y]:
                    byte |= 0x80 >> bit
            out.append(byte)
    return bytes(out)


def fill_surrounded_black_pinhole(frame: bytes, spans: list[tuple[int, int]]) -> bytes:
    image = compact_to_image(frame, spans)
    bbox = image.getbbox()
    if bbox is None:
        return frame

    src = image.load()
    out = image.copy()
    dst = out.load()
    x0, y0, x1, y1 = bbox

    for y in range(max(0, y0 - 1), min(192, y1 + 1)):
        for x in range(max(0, x0 - 1), min(256, x1 + 1)):
            if src[x, y]:
                continue
            neighbours = 0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    xx = x + dx
                    yy = y + dy
                    if 0 <= xx < 256 and 0 <= yy < 192 and src[xx, yy]:
                        neighbours += 1
            if neighbours == 8:
                dst[x, y] = 1

    return image_to_compact(out, spans)


def main() -> None:
    spans = parse_screen_spans()
    frame = FRAME0.read_bytes()
    deltas = split_delta_stream(FRAME_DELTAS.read_bytes(), FRAME_SIZE)
    if len(deltas) != FRAME_COUNT:
        raise SystemExit(f"expected {FRAME_COUNT} frame deltas, got {len(deltas)}")

    frames: list[bytes] = []
    for delta in deltas:
        frames.append(frame)
        frame = apply_delta(frame, delta)
    if frame != frames[0]:
        raise SystemExit("cyclic frame delta stream does not return to frame 0")

    polished = [fill_surrounded_black_pinhole(item, spans) for item in frames]
    changed_bytes = sum(
        sum(1 for a, b in zip(before, after) if a != b)
        for before, after in zip(frames, polished)
    )

    new_deltas = [
        encode_delta(polished[i], polished[(i + 1) % FRAME_COUNT])
        for i in range(FRAME_COUNT)
    ]

    FRAME0.write_bytes(polished[0])
    FRAME_DELTAS.write_bytes(b"".join(new_deltas))

    print(
        "about Earth polish: "
        f"{changed_bytes} compact bytes touched, "
        f"delta stream {sum(len(d) for d in deltas)}B -> {sum(len(d) for d in new_deltas)}B, "
        f"max delta {max(len(d) for d in new_deltas)}B"
    )


if __name__ == "__main__":
    main()
