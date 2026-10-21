#!/usr/bin/env python3
"""Attach without reset or halt and report FR3068 debug IDs and memory words."""

import argparse
import logging


def parse_args():
    parser = argparse.ArgumentParser(description="Read-only FR3068 SWD attachment check.")
    parser.add_argument("--frequency", type=int, default=1_000_000, help="SWD clock in Hz")
    return parser.parse_args()


def read32(target, address):
    try:
        return f"0x{target.read32(address):08X}"
    except Exception as exc:
        return f"ERR({type(exc).__name__}: {exc})"


def main():
    args = parse_args()
    logging.basicConfig(level=logging.WARNING)
    from pyocd.core.helpers import ConnectHelper

    with ConnectHelper.session_with_chosen_probe(
        target_override="cortex_m",
        options={"connect_mode": "attach", "frequency": args.frequency},
    ) as session:
        target = session.board.target
        print("== Debug Port ==")
        dp = target.dp
        print(f"DP IDCODE   : 0x{dp.dpidr.idr:08X}" if hasattr(dp, "dpidr") else f"DP: {dp}")
        for ap_address, ap in target.aps.items():
            print(f"AP {ap_address}: IDR=0x{ap.idr:08X} type={type(ap).__name__}")

        print("\n== Core ==")
        cpuid = target.read32(0xE000ED00)
        implementer = cpuid >> 24
        variant = (cpuid >> 20) & 0xF
        part = (cpuid >> 4) & 0xFFF
        revision = cpuid & 0xF
        parts = {0xD21: "Cortex-M33", 0xC24: "Cortex-M4", 0xD20: "Cortex-M23", 0xC27: "Cortex-M7"}
        print(
            f"CPUID       : 0x{cpuid:08X} implementer=0x{implementer:02X} "
            f"part=0x{part:03X} ({parts.get(part, '?')}) r{variant}p{revision}"
        )
        print(f"core state  : {target.get_state().name} (attach mode, not halted by this script)")

        print("\n== Memory peek (read-only) ==")
        locations = (
            ("Flash 0x08000000 (boot header?)", 0x08000000),
            ("Flash 0x08000004", 0x08000004),
            ("Flash 0x0800001C (magic slot?)", 0x0800001C),
            ("Flash 0x08002000 (app vector: SP)", 0x08002000),
            ("Flash 0x08002004 (app vector: Reset)", 0x08002004),
            ("SRAM  0x20000000", 0x20000000),
            ("PRAM  0x1FFE0000", 0x1FFE0000),
        )
        for name, address in locations:
            print(f"{name:40s}: {read32(target, address)}")


if __name__ == "__main__":
    main()
