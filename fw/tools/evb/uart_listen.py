#!/usr/bin/env python3
"""Capture a bounded sample from the EVB DAPLink virtual COM port."""

import argparse
import time


def parse_args():
    parser = argparse.ArgumentParser(description="Listen to the FR3068 EVB UART for a fixed duration.")
    parser.add_argument("port", help="serial device, for example /dev/cu.usbmodem...")
    parser.add_argument("baud", type=int, help="baud rate, typically 115200 or 921600")
    parser.add_argument("seconds", type=float, help="capture duration")
    parser.add_argument("--max-text", type=int, default=3000, help="maximum decoded characters to print")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.seconds <= 0:
        raise SystemExit("seconds must be greater than zero")
    if args.max_text < 0:
        raise SystemExit("--max-text cannot be negative")

    import serial

    connection = serial.Serial(args.port, args.baud, timeout=0.2)
    try:
        connection.reset_input_buffer()
        data = bytearray()
        started = time.time()
        while time.time() - started < args.seconds:
            data += connection.read(4096)
    finally:
        connection.close()

    print(f"[{args.baud} bps] received {len(data)} bytes in {args.seconds:g}s")
    if not data:
        return
    decoded = data.decode("utf-8", errors="replace")
    printable = sum(1 for byte in data if 32 <= byte < 127 or byte in (9, 10, 13))
    print(f"printable ratio: {printable / len(data):.0%}")
    print("----- text -----")
    print(decoded[: args.max_text])
    print("----- hex (first 128 B) -----")
    print(data[:128].hex(" "))


if __name__ == "__main__":
    main()
