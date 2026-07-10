#!/usr/bin/env python3
"""Reference checks for the bounded ABOUT Earth packet contract."""

import bpe_build as bpe


def valid_delta(data, decoded_limit):
    src = dst = 0
    while src < len(data):
        cmd = data[src]
        src += 1
        if cmd == 0:
            return src == len(data)
        if cmd < 0x80:
            dst += cmd
        else:
            count = (cmd & 0x7F) + 1
            if src + count > len(data):
                return False
            src += count
            dst += count
        if dst > decoded_limit:
            return False
    return False


def valid_packet(packet):
    if len(packet) != bpe.EARTH_PACKET_SIZE_TARGET:
        return False
    frame_len = int.from_bytes(packet[:2], "little")
    if not 1 <= frame_len <= bpe.EARTH_PACKET_SIZE_TARGET - 4:
        return False
    attr_len_at = 2 + frame_len
    attr_len = packet[attr_len_at]
    attr_at = attr_len_at + 1
    if not attr_len or attr_at + attr_len > len(packet):
        return False
    return (valid_delta(packet[2:attr_len_at], bpe.EARTH_FRAME0_SIZE) and
            valid_delta(packet[attr_at:attr_at + attr_len], bpe.EARTH_ATTR0_SIZE))


def packet(frame, attr):
    result = bytearray(bpe.EARTH_PACKET_SIZE_TARGET)
    result[:2] = len(frame).to_bytes(2, "little")
    result[2:2 + len(frame)] = frame
    result[2 + len(frame)] = len(attr)
    result[3 + len(frame):3 + len(frame) + len(attr)] = attr
    return result


def main():
    *_, deltas, packet_size = bpe.load_earth_assets()
    packets = [deltas[i:i + packet_size] for i in range(0, len(deltas), packet_size)]
    assert len(packets) == bpe.EARTH_FRAME_COUNT
    assert all(valid_packet(item) for item in packets)

    assert valid_packet(packet(bytes((127, 127, 127, 127, 79, 0)), b"\0"))
    assert valid_packet(packet(b"\0", bytes((44, 0))))
    for bad in (
        packet(b"", b"\0"),
        packet(bytes((127, 127, 127, 127, 80, 0)), b"\0"),
        packet(b"\0", bytes((45, 0))),
        packet(bytes((0, 0)), b"\0"),
        packet(bytes((0x81, 0)), b"\0"),
    ):
        assert not valid_packet(bad)

    oversized = bytearray(packet(b"\0", b"\0"))
    oversized[:2] = (509).to_bytes(2, "little")
    assert not valid_packet(oversized)
    oversized[:2] = (0xFFFF).to_bytes(2, "little")
    assert not valid_packet(oversized)

    print("Earth packet bounds check OK")


if __name__ == "__main__":
    main()
