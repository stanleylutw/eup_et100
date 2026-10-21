#!/usr/bin/env python3
"""Attach without reset or halt and detect FR3068 internal flash size by address aliasing."""

import argparse
import hashlib
import logging


BASE = 0x08000000
# Sample points inside the first 1 MiB: header, app vector, app code, FlashDB, tail.
OFFSETS = (0x0000, 0x2000, 0x10000, 0x40000, 0xC8000, 0xF0000)
WINDOWS = (0x100000, 0x200000, 0x400000, 0x800000)
BLOCK = 256


def parse_args():
    parser = argparse.ArgumentParser(
        description="Read-only FR3068 internal flash size probe (compares base vs +1/2/4/8 MiB)."
    )
    parser.add_argument("--frequency", type=int, default=1_000_000, help="SWD clock in Hz")
    return parser.parse_args()


def read_block(target, address):
    try:
        return bytes(target.read_memory_block8(address, BLOCK))
    except Exception as exc:
        return f"ERR({type(exc).__name__}: {exc})"


def describe(data):
    if isinstance(data, str):
        return data
    if data == b"\xff" * BLOCK:
        return "all 0xFF"
    if data == b"\x00" * BLOCK:
        return "all 0x00"
    return f"data sha256={hashlib.sha256(data).hexdigest()[:12]} first={data[:16].hex()}"


def main():
    args = parse_args()
    logging.basicConfig(level=logging.WARNING)
    from pyocd.core.helpers import ConnectHelper

    with ConnectHelper.session_with_chosen_probe(
        target_override="cortex_m",
        options={"connect_mode": "attach", "frequency": args.frequency},
    ) as session:
        target = session.board.target
        print(f"core state: {target.get_state().name} (attach mode, not halted by this script)")
        reference = {offset: read_block(target, BASE + offset) for offset in OFFSETS}
        for offset in OFFSETS:
            print(f"0x{BASE + offset:08X}: {describe(reference[offset])}")

        alias_at = None
        for window in WINDOWS:
            print(f"\n--- +0x{window:X} ({window >> 20} MiB) ---")
            matches = 0
            for offset in OFFSETS:
                data = read_block(target, BASE + window + offset)
                ref = reference[offset]
                same = not isinstance(data, str) and data == ref and data != b"\xff" * BLOCK
                matches += same
                print(f"0x{BASE + window + offset:08X}: {describe(data)}"
                      f"{'   <== SAME as base (alias)' if same else ''}")
            if alias_at is None and matches:
                alias_at = window

        print(f"\ncore state after: {target.get_state().name}")
        if alias_at is None:
            print("RESULT: no alias up to +8 MiB (inconclusive)")
        else:
            print(f"RESULT: address space wraps at +0x{alias_at:X} -> internal flash = {alias_at >> 20} MiB")


if __name__ == "__main__":
    main()
