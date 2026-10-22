#!/usr/bin/env python3
"""Catch an FR3068 after manual reset release, then flash without disconnecting."""

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
        description="Recover an FR3068 whose bad boot image prevents normal SWD attach."
    )
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE, help="known-good burn image")
    parser.add_argument("--flm", type=Path, default=DEFAULT_FLM, help="FR30xx.FLM path")
    parser.add_argument("--base", type=parse_int, default=0x08000000, help="flash base address")
    parser.add_argument("--frequency", type=int, default=4_000_000, help="SWD clock in Hz")
    parser.add_argument("--timeout", type=float, default=30.0, help="seconds to wait for RST release")
    return parser.parse_args()


def valid_fr509x_header(image):
    return len(image) >= 48 and (
        struct.unpack_from("<I", image, 28)[0],
        struct.unpack_from("<I", image, 32)[0],
        struct.unpack_from("<I", image, 44)[0],
    ) == (0x51525251, 0, 0x51525251)


def main():
    args = parse_args()
    image_path = args.image.expanduser().resolve()
    flm_path = args.flm.expanduser().resolve()
    for label, path in (("image", image_path), ("FLM", flm_path)):
        if not path.is_file():
            raise SystemExit(f"{label} not found: {path}")

    image = image_path.read_bytes()
    if not valid_fr509x_header(image):
        raise SystemExit("refusing rescue flash: image is not the verified FR509x header layout")
    print(f"image OK (FR509x header): {len(image)} bytes")

    logging.basicConfig(level=logging.CRITICAL)
    from pyocd.core.memory_map import FlashRegion, MemoryMap, RamRegion
    from pyocd.core.session import Session
    from pyocd.coresight.coresight_target import CoreSightTarget
    from pyocd.flash.file_programmer import FileProgrammer
    from pyocd.probe.aggregator import DebugProbeAggregator
    from pyocd.probe.debug_probe import DebugProbe
    from pyocd.target import TARGET
    from pyocd.target.pack.flash_algo import PackFlashAlgo

    base = args.base

    class FR3068(CoreSightTarget):
        VENDOR = "Freqchip"

        def __init__(self, session):
            super().__init__(
                session,
                MemoryMap(
                    FlashRegion(
                        start=base,
                        length=0x200000,
                        blocksize=0x1000,
                        page_size=0x100,
                        is_boot_memory=True,
                        flm=PackFlashAlgo(str(flm_path)),
                    ),
                    RamRegion(start=0x20000000, length=0x80000),
                ),
            )

    TARGET["fr3068"] = FR3068
    probes = DebugProbeAggregator.get_all_connected_probes()
    if not probes:
        raise SystemExit("no debug probe found")
    if len(probes) > 1:
        raise SystemExit("multiple debug probes found; disconnect all but the target EVB")

    probe = probes[0]
    session = Session(
        probe,
        options={
            "target_override": "fr3068",
            "connect_mode": "attach",
            "frequency": args.frequency,
            "reset_type": "sw",
        },
    )
    probe.session = session
    probe.open()
    probe.connect(DebugProbe.Protocol.SWD)
    probe.set_clock(args.frequency)

    line_reset = (1 << 51) - 1
    sequence1 = line_reset | (0x33BBBBBA << 51) | (
        0x19BC0EA2E3DDAFE986852D956209F392FF << 90
    )
    sequence2 = 0x01A0 | (line_reset << 12)

    def wake():
        probe.swj_sequence(226, sequence1)
        probe.swj_sequence(65, sequence2)

    def write_memory(address, value):
        probe.write_ap(0x4, address)
        probe.write_ap(0xC, value)

    def read_memory(address):
        probe.write_ap(0x4, address)
        return probe.read_ap(0xC)

    try:
        wake()
        probe.read_dp(0x0)
        previous_alive = True
    except Exception:
        previous_alive = False
    print(f"[catch] armed, initial DP alive={previous_alive}; press and release RST once...")

    started = time.time()
    halted = False
    while time.time() - started < args.timeout and not halted:
        try:
            wake()
            probe.read_dp(0x0)
            alive = True
        except Exception:
            alive = False
        if alive and not previous_alive:
            edge_time = time.time()
            try:
                probe.write_dp(0x0, 0x1F)
                probe.write_dp(0x4, 0x50000000)
                while (probe.read_dp(0x4) & 0xA0000000) != 0xA0000000:
                    if time.time() - edge_time > 0.05:
                        raise TimeoutError("debug power-up ACK")
                probe.write_dp(0x8, 0x0)
                probe.write_ap(0x0, 0x03000002)
                write_memory(0xE000EDF0, 0xA05F0003)
                write_memory(0xE000EDFC, 0x1)
                dhcsr = read_memory(0xE000EDF0)
                if dhcsr & (1 << 17):
                    halted = True
                    elapsed_ms = 1000 * (time.time() - edge_time)
                    print(f"[catch] HALTED +{elapsed_ms:.1f}ms after RST release (DHCSR=0x{dhcsr:08X})")
            except Exception as exc:
                print(f"[catch] edge missed: {type(exc).__name__}: {exc}")
        previous_alive = alive

    if not halted:
        probe.disconnect()
        probe.close()
        raise SystemExit(f"[catch] no successful catch in {args.timeout:g}s")

    session.open()
    target = session.board.target
    print(f"[session] attached, state={target.get_state().name}, PC=0x{target.read_core_register('pc'):08X}")
    flash_start = time.time()
    FileProgrammer(session, chip_erase="sector", smart_flash=False).program(
        str(image_path), base_address=base, file_format="bin"
    )
    print(f"[flash] programmed in {time.time() - flash_start:.1f}s")
    readback = bytes(target.read_memory_block8(base, len(image)))
    if readback != image:
        session.close()
        raise SystemExit("[verify] MISMATCH")
    print("[verify] OK")

    target.write32(0xE000EDFC, 0x00000000)
    target.reset()
    time.sleep(2)
    try:
        target.halt()
        pc = target.read_core_register("pc")
        target.resume()
        where = (
            "app flash"
            if 0x08002000 <= pc < 0x08200000
            else "PRAM ram_code"
            if 0x1FFE0000 <= pc < 0x20000000
            else "SRAM"
            if 0x20000000 <= pc < 0x20080000
            else "ROM"
        )
        print(f"[run] after reset PC=0x{pc:08X} -> {where} (resumed)")
    except Exception as exc:
        print(f"[run] post-reset check failed: {type(exc).__name__}: {exc}")
    finally:
        session.close()


if __name__ == "__main__":
    main()
