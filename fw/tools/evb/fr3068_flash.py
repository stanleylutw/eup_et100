#!/usr/bin/env python3
"""Program and verify an FR3068 burn image through the EVB DAPLink SWD port."""

import argparse
import logging
import struct
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SDK = REPO_ROOT / "fw/vendor/fr30xxc_sdk__202411"
DEFAULT_FLM = DEFAULT_SDK / "components/tools/keil/FR30xx.FLM"
DEFAULT_IMAGE = DEFAULT_SDK / "examples/application/ble_simple_periphreal/GCC/output/Project_burn.bin"


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


def main():
    args = parse_args()
    image_path = args.image.expanduser().resolve()
    flm_path = args.flm.expanduser().resolve()
    for label, path in (("image", image_path), ("FLM", flm_path)):
        if not path.is_file():
            raise SystemExit(f"{label} not found: {path}")

    image = image_path.read_bytes()
    try:
        validate_fr509x_header(image)
    except ValueError as exc:
        raise SystemExit(f"refusing to flash {image_path}: {exc}") from exc

    logging.basicConfig(level=logging.WARNING)
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.memory_map import FlashRegion, MemoryMap, RamRegion
    from pyocd.coresight.coresight_target import CoreSightTarget
    from pyocd.flash.file_programmer import FileProgrammer
    from pyocd.target import TARGET
    from pyocd.target.pack.flash_algo import PackFlashAlgo

    base = args.base

    class FR3068(CoreSightTarget):
        VENDOR = "Freqchip"

        def __init__(self, session):
            memory_map = MemoryMap(
                FlashRegion(
                    start=base,
                    length=0x200000,
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
    print("header: FR509x layout verified (word8 tlv_length = 0)")

    with ConnectHelper.session_with_chosen_probe(
        target_override="fr3068",
        options={"connect_mode": "halt", "frequency": args.frequency, "reset_type": "sw"},
    ) as session:
        target = session.board.target
        target.halt()
        print(f"halted, PC=0x{target.read_core_register('pc'):08X} (ROM)")
        start = time.time()
        FileProgrammer(session, chip_erase="sector", smart_flash=False).program(
            str(image_path), base_address=base, file_format="bin"
        )
        print(f"program done in {time.time() - start:.1f}s")

        readback = bytes(target.read_memory_block8(base, len(image)))
        if readback != image:
            offset = next(i for i in range(len(image)) if readback[i] != image[i])
            raise SystemExit(
                f"VERIFY: MISMATCH at offset 0x{offset:X}: "
                f"got {readback[offset]:02X} want {image[offset]:02X}"
            )
        print("VERIFY: OK - readback matches image byte-for-byte")

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
