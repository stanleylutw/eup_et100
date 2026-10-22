#!/usr/bin/env python3
"""ET-100 SMP-over-serial OTA client (v0.8.5), built on the official smpclient.

Writes the external NOR / boot state through the device; use only on the EVB with
Stanley's approval. Run with the project venv: fw/.venv/bin/python fw/tools/evb/smp_serial_ota.py

Examples:
  smp_serial_ota.py state
  smp_serial_ota.py upload fw/build_serial/output/dfu_application.zip --test --reset
  smp_serial_ota.py --baud 115200 upload image.signed.bin --test --reset
  smp_serial_ota.py confirm
"""
import argparse
import asyncio
import json
import time
import zipfile
from pathlib import Path

import serial
from smpclient import SMPClient
from smpclient.generics import error, success
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smpclient.requests.os_management import EchoWrite, ResetWrite
from smpclient.transport.serial import SMPSerialTransport

DEFAULT_PORT = "/dev/cu.usbmodem2024_2425_62851"


def load_image(path: Path) -> bytes:
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as z:
            manifest = json.loads(z.read("manifest.json"))
            return z.read(manifest["files"][0]["file"])
    return path.read_bytes()


def switch_baud(port: str, baud: int) -> None:
    """Ask the device (at 9600) to switch UART3 to `baud` with BLEGW$OTABAUD."""
    with serial.Serial(port, 9600, timeout=0.2) as s:
        s.reset_input_buffer()
        s.write(f"BLEGW$OTABAUD;{baud};OB01\r\n".encode())
        deadline, buf = time.time() + 3, b""
        while time.time() < deadline:
            buf += s.read(256)
            if b"OTABAUD;" in buf and buf.endswith(b"\n"):
                break
        line = next((l for l in buf.split(b"\n") if b"OTABAUD;" in l), b"")
        if f"OTABAUD;1;{baud};OB01".encode() not in line:
            raise SystemExit(f"OTABAUD rejected or no reply: {buf!r}")
    time.sleep(0.05)


def print_state(rsp) -> None:
    if not success(rsp):
        raise SystemExit(f"state error: {rsp}")
    for img in rsp.images:
        flags = [k for k in ("active", "confirmed", "pending", "bootable", "permanent") if getattr(img, k)]
        print(f"slot {img.slot}: version {img.version} hash {bytes(img.hash).hex()} {' '.join(flags)}")


async def run(args) -> None:
    if args.baud != 9600:
        switch_baud(args.port, args.baud)
    transport = SMPSerialTransport(baudrate=args.baud, max_smp_encoded_frame_size=1024,
                                   line_length=128, line_buffers=8)
    async with SMPClient(transport, args.port) as client:
        if args.cmd == "echo":
            print(await client.request(EchoWrite(d=args.text), timeout_s=5))
        elif args.cmd == "state":
            print_state(await client.request(ImageStatesRead(), timeout_s=60))
        elif args.cmd == "confirm":
            state = await client.request(ImageStatesRead(), timeout_s=60)
            print_state(state)
            running = next(i for i in state.images if i.slot == 0)
            print_state(await client.request(ImageStatesWrite(hash=bytes(running.hash), confirm=True),
                                             timeout_s=30))
        elif args.cmd == "reset":
            print(await client.request(ResetWrite(), timeout_s=5))
        elif args.cmd == "test":
            print_state(await client.request(ImageStatesWrite(hash=bytes.fromhex(args.hash), confirm=False),
                                             timeout_s=60))
        elif args.cmd == "upload":
            image = load_image(args.image)
            start, last = time.time(), -1
            async for off in client.upload(image, first_timeout_s=60, subsequent_timeout_s=30):
                pct = off * 100 // len(image)
                if pct // 10 != last // 10:
                    print(f"upload {off}/{len(image)} ({pct}%) {time.time() - start:.1f}s", flush=True)
                last = pct
            print(f"upload done {len(image)} B in {time.time() - start:.1f}s")
            state = await client.request(ImageStatesRead(), timeout_s=60)
            print_state(state)
            if args.test:
                slot1 = next((i for i in state.images if i.slot == 1), None)
                if slot1 is None:
                    raise SystemExit("candidate not listed: verification failed on device")
                print_state(await client.request(ImageStatesWrite(hash=bytes(slot1.hash), confirm=False),
                                                 timeout_s=60))
                if args.reset:
                    print(await client.request(ResetWrite(), timeout_s=5))


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", type=int, choices=(9600, 115200), default=9600)
    sub = p.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("echo"); e.add_argument("text")
    sub.add_parser("state"); sub.add_parser("confirm"); sub.add_parser("reset")
    t = sub.add_parser("test"); t.add_argument("hash", help="slot-1 image hash (hex)")
    u = sub.add_parser("upload"); u.add_argument("image", type=Path)
    u.add_argument("--test", action="store_true", help="mark the uploaded image pending")
    u.add_argument("--reset", action="store_true", help="reset after --test to install")
    asyncio.run(run(p.parse_args()))


if __name__ == "__main__":
    main()
