#!/usr/bin/env python3
"""SMP serial framing interop: official `smp` package <-> firmware smp_serial.c.

Usage: fw/.venv/bin/python fw/tests/test_smp_interop.py /tmp/et100_smp_echo
Encodes random packets with smp.packet.encode (as smpclient does, line_length=128),
pipes them through the C harness, and decodes the harness output with
smp.packet.decode. Every packet must round-trip byte-for-byte.
"""
import random
import subprocess
import sys

from smp import packet as smppacket


def decode_all(data: bytes):
    out, lines = [], data.split(b"\n")
    i = 0
    while i < len(lines) - 1:
        gen = smppacket.decode()
        next(gen)
        while True:
            try:
                gen.send(lines[i] + b"\n")
                i += 1
            except StopIteration as done:
                out.append(bytes(done.value))
                i += 1
                break
    return out


def main():
    harness = sys.argv[1]
    rng = random.Random(20261022)
    packets = [bytes(rng.randrange(256) for _ in range(n))
               for n in [8, 9, 10, 11, 93, 94, 95, 186, 500, 1000, 1024]]
    stream = b"BLEGW$GCHK;J082\r\n"
    for p in packets:
        for line in smppacket.encode(p, line_length=128):
            stream += line
        stream += b"BLEGW$GGTIME;J083\r\n"   # ASCII interleaved between packets
    result = subprocess.run([harness], input=stream, capture_output=True, check=True)
    decoded = decode_all(result.stdout)
    assert decoded == packets, (len(decoded), len(packets))
    for line in result.stdout.split(b"\n")[:-1]:
        assert len(line) + 1 <= 127, len(line)
    print("PASS smp interop:", len(packets), "packets;", result.stderr.decode().strip())


if __name__ == "__main__":
    main()
