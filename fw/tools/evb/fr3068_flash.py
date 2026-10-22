#!/usr/bin/env python3
"""Program and verify an FR3068 burn image through the EVB DAPLink SWD port."""

import argparse
import hashlib
import logging
import struct
import time
import zlib
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SDK = REPO_ROOT / "fw/vendor/fr30xxc_sdk__202411"
DEFAULT_FLM = DEFAULT_SDK / "components/tools/keil/FR30xx.FLM"
DEFAULT_IMAGE = DEFAULT_SDK / "examples/application/ble_simple_periphreal/GCC/output/Project_burn.bin"
FLASH_BASE = 0x08000000
FLASH_SIZE = 0x200000
B_INFO = 0x08001000
SECTOR = 0x1000


def plan_segments(image, base, board_b):
    """Validate the entire request before erase; B-info is never queued."""
    if len(board_b) != SECTOR:
        raise ValueError("B-info read must contain a complete sector")
    end = base + len(image)
    if not image or base < FLASH_BASE or end > FLASH_BASE + FLASH_SIZE:
        raise ValueError("empty image or request outside internal 2 MB flash")
    if base < B_INFO + SECTOR and end > B_INFO:
        if base > B_INFO or end < B_INFO + SECTOR:
            raise ValueError("partial B-info overlap refused")
        offset = B_INFO - base
        if image[offset:offset + SECTOR] != board_b:
            raise ValueError("image B-info differs from board; no erase/program allowed")
        segments = [(base, image[:offset]), (B_INFO + SECTOR, image[offset + SECTOR:])]
        segments = [(address, data) for address, data in segments if data]
    else:
        segments = [(base, image)]
    if not segments:
        raise ValueError("request contains only the protected B-info sector")
    for address, data in segments:
        erase_start = address & ~(SECTOR - 1)
        erase_end = (address + len(data) + SECTOR - 1) & ~(SECTOR - 1)
        if erase_start < B_INFO + SECTOR and erase_end > B_INFO:
            raise ValueError("planned erase intersects protected B-info")
    return segments


def parse_int(value):
    return int(value, 0)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Flash Project_burn.bin to an FR3068 EVB and verify readback."
    )
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE, help="burn image path")
    parser.add_argument("--flm", type=Path, default=DEFAULT_FLM, help="FR30xx.FLM path")
    parser.add_argument("--base", type=parse_int, default=0x08000000, help="flash base address")
    parser.add_argument("--frequency", type=int, default=4_000_000, help="SWD clock in Hz")
    parser.add_argument("--raw", action="store_true", help="raw image (App at --base 0x08020000); no FR509x header")
    parser.add_argument("--no-reset", action="store_true", help="leave CPU halted after verify")
    parser.add_argument("--reset-only", action="store_true", help="reset without erase/program; verify B-info unchanged")
    return parser.parse_args()


def validate_fr509x_header(image):
    if len(image) < 48:
        raise ValueError("image is shorter than the 48-byte boot header")
    word7 = struct.unpack_from("<I", image, 28)[0]
    word8 = struct.unpack_from("<I", image, 32)[0]
    word11 = struct.unpack_from("<I", image, 44)[0]
    if (word7, word8, word11) != (0x51525251, 0, 0x51525251):
        raise ValueError(
            "expected word7=0x51525251, word8=0x00000000, word11=0x51525251"
        )
    length, offset = struct.unpack_from("<II", image, 8)
    if offset != 0x2000 or length != len(image) - offset or length < 8:
        raise ValueError("FR509x code length/offset does not match image")
    if zlib.crc32(image[offset:]) != struct.unpack_from("<I", image, 24)[0]:
        raise ValueError("FR509x payload CRC mismatch")


def main():
    args = parse_args()
    image_path = args.image.expanduser().resolve()
    flm_path = args.flm.expanduser().resolve()
    if args.no_reset and args.reset_only:
        raise SystemExit("--reset-only and --no-reset are mutually exclusive")
    for label, path in (("FLM", flm_path),) + (() if args.reset_only else (("image", image_path),)):
        if not path.is_file():
            raise SystemExit(f"{label} not found: {path}")

    image = b"" if args.reset_only else image_path.read_bytes()
    try:
        if not args.raw and not args.reset_only:
            if args.base != FLASH_BASE:
                raise ValueError("FR509x burn images must use base 0x08000000")
            validate_fr509x_header(image)
    except ValueError as exc:
        raise SystemExit(f"refusing to flash {image_path}: {exc}") from exc

    logging.basicConfig(level=logging.WARNING)
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.memory_map import FlashRegion, MemoryMap, RamRegion
    from pyocd.coresight.coresight_target import CoreSightTarget
    from pyocd.flash.loader import FlashLoader
    from pyocd.target import TARGET
    from pyocd.target.pack.flash_algo import PackFlashAlgo

    base = args.base

    class FR3068(CoreSightTarget):
        VENDOR = "Freqchip"

        def __init__(self, session):
            memory_map = MemoryMap(
                FlashRegion(
                    start=FLASH_BASE,
                    length=FLASH_SIZE,
                    blocksize=0x1000,
                    page_size=0x100,
                    is_boot_memory=True,
                    flm=PackFlashAlgo(str(flm_path)),
                ),
                RamRegion(start=0x20000000, length=0x80000),
            )
            super().__init__(session, memory_map)

    TARGET["fr3068"] = FR3068
    print(
        f"image: {len(image)} bytes -> 0x{base:08X}..0x{base + len(image) - 1:08X} "
        f"({(len(image) + 0xFFF) // 0x1000} sectors)"
    )
    print("mode: " + ("reset only" if args.reset_only else "raw" if args.raw else "FR509x header/CRC verified"))

    with ConnectHelper.session_with_chosen_probe(
        target_override="fr3068",
        options={"connect_mode": "halt", "frequency": args.frequency, "reset_type": "sw",
                 "resume_on_disconnect": not args.no_reset},
    ) as session:
        target = session.board.target
        target.halt()
        print(f"halted, PC=0x{target.read_core_register('pc'):08X} (ROM)")
        board_b = bytes(target.read_memory_block8(B_INFO, SECTOR))
        b_hash = hashlib.sha256(board_b).hexdigest()
        print(f"B-info before sha256={b_hash}")
        if not args.reset_only:
            segments = plan_segments(image, base, board_b)
            loader = FlashLoader(session, chip_erase="sector", smart_flash=False)
            for address, data in segments:
                print(f"queue: 0x{address:08X}..0x{address + len(data) - 1:08X}")
                loader.add_data(address, data)
            loader.commit()

        readback = bytes(target.read_memory_block8(base, len(image))) if image else b""
        if readback != image:
            offset = next(i for i in range(len(image)) if readback[i] != image[i])
            raise SystemExit(
                f"VERIFY: MISMATCH at offset 0x{offset:X}: "
                f"got {readback[offset]:02X} want {image[offset]:02X}"
            )
        print("VERIFY: OK - readback matches image byte-for-byte")
        if bytes(target.read_memory_block8(B_INFO, SECTOR)) != board_b:
            raise SystemExit("B-info CHANGED; STOP (no repair attempted)")
        print(f"B-info after sha256={b_hash} unchanged; sector skipped")

        if args.no_reset:
            print("CPU left halted; no reset requested")
            return

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
        if bytes(target.read_memory_block8(B_INFO, SECTOR)) != board_b:
            raise SystemExit("B-info changed after boot; STOP (no repair attempted)")
        pc = target.read_core_register("pc")
        sp = target.read_core_register("sp")
        vector_sp, vector_reset = target.read32(0x08002000), target.read32(0x08002004)
        target.resume()
        where = (
            "app flash"
            if 0x08002000 <= pc < 0x08200000
            else "PRAM ram_code"
            if 0x1FFE0000 <= pc < 0x20000000
            else "SRAM"
            if 0x20000000 <= pc < 0x20080000
            else "ROM/other"
        )
        print(f"after reset: PC=0x{pc:08X} SP=0x{sp:08X} (resumed)")
        print(f"app vector : SP=0x{vector_sp:08X} Reset_Handler=0x{vector_reset:08X}")
        print(f"PC region  : {where}")


if __name__ == "__main__":
    main()
