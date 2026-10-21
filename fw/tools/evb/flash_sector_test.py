#!/usr/bin/env python3
"""Program, verify and erase back one blank 4 KiB sector in the FR3068 upper internal flash."""

import argparse
import hashlib
import logging
import random
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_FLM = REPO_ROOT / "fw/vendor/fr30xxc_sdk__202411/components/tools/keil/FR30xx.FLM"
BASE = 0x08000000
FLASH_SIZE = 0x200000
UPPER_START = BASE + 0x100000
SECTOR = 0x1000
PAGE = 0x100
BLANK = b"\xff" * SECTOR


def parse_int(value):
    return int(value, 0)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Erase/program/verify/erase-back test on one blank sector of the upper 1 MiB."
    )
    parser.add_argument("--address", type=parse_int, default=0x081F7000,
                        help="4 KiB aligned sector in 0x08100000..0x081FF000 (default: 0x081F7000)")
    parser.add_argument("--flm", type=Path, default=DEFAULT_FLM, help="FR30xx.FLM path")
    parser.add_argument("--frequency", type=int, default=4_000_000, help="SWD clock in Hz")
    return parser.parse_args()


def sha(data):
    return hashlib.sha256(data).hexdigest()[:16]


def main():
    args = parse_args()
    test = args.address
    if test % SECTOR or not UPPER_START <= test < BASE + FLASH_SIZE:
        raise SystemExit(f"refusing 0x{test:08X}: must be 4 KiB aligned in 0x{UPPER_START:08X}..0x081FF000")
    flm_path = args.flm.expanduser().resolve()
    if not flm_path.is_file():
        raise SystemExit(f"FLM not found: {flm_path}")
    alias = test - 0x100000  # same offset in the lower 1 MiB; must not change

    logging.basicConfig(level=logging.WARNING)
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.memory_map import FlashRegion, MemoryMap, RamRegion
    from pyocd.coresight.coresight_target import CoreSightTarget
    from pyocd.target import TARGET
    from pyocd.target.pack.flash_algo import PackFlashAlgo

    class FR3068(CoreSightTarget):
        VENDOR = "Freqchip"

        def __init__(self, session):
            memory_map = MemoryMap(
                FlashRegion(
                    start=BASE,
                    length=FLASH_SIZE,
                    blocksize=SECTOR,
                    page_size=PAGE,
                    is_boot_memory=True,
                    flm=PackFlashAlgo(str(flm_path)),
                ),
                RamRegion(start=0x20000000, length=0x80000),
            )
            super().__init__(session, memory_map)

    TARGET["fr3068"] = FR3068
    pattern = bytes(random.Random(0xE7100).getrandbits(8) for _ in range(SECTOR))

    with ConnectHelper.session_with_chosen_probe(
        target_override="fr3068",
        options={"connect_mode": "halt", "frequency": args.frequency, "reset_type": "sw"},
    ) as session:
        target = session.board.target
        target.halt()

        def read(address, length=SECTOR):
            return bytes(target.read_memory_block8(address, length))

        header0, alias0, test0 = read(BASE, 0x2000), read(alias), read(test)
        print(f"test sector 0x{test:08X}: {'all 0xFF' if test0 == BLANK else 'NOT BLANK sha=' + sha(test0)}")
        if test0 != BLANK:
            raise SystemExit("ABORT: test sector is not blank; refusing to erase unknown data")

        flash = target.memory_map.get_region_for_address(test).flash
        ok = True

        start = time.time()
        flash.init(flash.Operation.PROGRAM)
        for offset in range(0, SECTOR, PAGE):
            flash.program_page(test + offset, list(pattern[offset:offset + PAGE]))
        flash.cleanup()
        readback = read(test)
        print(f"program   : {time.time() - start:.2f}s pattern={sha(pattern)} readback={sha(readback)} "
              f"-> {'MATCH' if readback == pattern else 'MISMATCH'}")
        ok &= readback == pattern

        alias_same, header_same = read(alias) == alias0, read(BASE, 0x2000) == header0
        print(f"lower 0x{alias:08X} unchanged: {alias_same}   boot header unchanged: {header_same}")
        ok &= alias_same and header_same
        print(f"0x{test + FLASH_SIZE:08X} (+2 MiB wrap) shows pattern: {read(test + FLASH_SIZE) == pattern}")

        start = time.time()
        flash.init(flash.Operation.ERASE)
        flash.erase_sector(test)
        flash.cleanup()
        erased = read(test)
        print(f"erase-back: {time.time() - start:.2f}s -> {'all 0xFF' if erased == BLANK else 'NOT BLANK'}")
        ok &= erased == BLANK and read(alias) == alias0 and read(BASE, 0x2000) == header0

        print(f"\nRESULT: {'PASS' if ok else 'FAIL'}")
        target.write32(0xE000EDFC, 0x00000000)
        print("DEMCR cleared; resetting (SYSRESETREQ)...")
        target.reset()

    time.sleep(2)
    with ConnectHelper.session_with_chosen_probe(
        target_override="fr3068",
        options={"connect_mode": "attach", "frequency": args.frequency},
    ) as session:
        target = session.board.target
        target.halt()
        pc = target.read_core_register("pc")
        target.resume()
        print(f"after reset: PC=0x{pc:08X} (resumed), state={target.get_state().name}")
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
