#!/usr/bin/env python3
"""Retry read-only SWD attachment to distinguish transient failures from WAIT lockup."""

import argparse
import logging
import time


def parse_args():
    parser = argparse.ArgumentParser(description="Retry read-only FR3068 SWD attach.")
    parser.add_argument("--attempts", type=int, default=5, help="maximum attach attempts")
    parser.add_argument("--delay", type=float, default=1.5, help="seconds between attempts")
    parser.add_argument("--frequency", type=int, default=1_000_000, help="SWD clock in Hz")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.attempts < 1:
        raise SystemExit("--attempts must be at least 1")
    logging.basicConfig(level=logging.ERROR)
    from pyocd.core.helpers import ConnectHelper

    for attempt in range(1, args.attempts + 1):
        try:
            with ConnectHelper.session_with_chosen_probe(
                target_override="cortex_m",
                options={"connect_mode": "attach", "frequency": args.frequency},
            ) as session:
                target = session.board.target
                print(
                    f"try {attempt}: OK DP=0x{target.dp.dpidr.idr:08X} "
                    f"CPUID=0x{target.read32(0xE000ED00):08X} state={target.get_state().name}"
                )
                print(
                    f"  app vector SP=0x{target.read32(0x08002000):08X} "
                    f"Reset=0x{target.read32(0x08002004):08X}"
                )
                return
        except Exception as exc:
            print(f"try {attempt}: FAIL {type(exc).__name__}: {exc}")
            if attempt < args.attempts:
                time.sleep(args.delay)
    raise SystemExit(1)


if __name__ == "__main__":
    main()
