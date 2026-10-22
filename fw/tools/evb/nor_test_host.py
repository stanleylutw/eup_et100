#!/usr/bin/env python3
"""DEV1.4 NOR console runner (115200 8N1); default is G1 read-only.

Does not flash/reset the MCU. Close other serial monitors first. Opening a
serial port may toggle modem-control lines; EVB DAPLink has no target nRESET.
Requires an existing Python environment with pyserial; installs nothing.
"""

import argparse
import datetime
import hashlib
import re
import sys
import time
from pathlib import Path


READ_ONLY = {"init", "id", "sfdp", "status", "read", "sha", "jrecover", "jquery"}
POWER = {"power", "probe-off", "g2-confirm", "cut"}


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples (run only after Stanley approves EVB access):
  nor_test_host.py /dev/cu.usbmodem... --log /tmp/et100_nor_g1.log
  nor_test_host.py PORT --allow-write --command 'boundary'
  nor_test_host.py PORT --allow-write --command 'erase-bench 0 4096 20'
  nor_test_host.py PORT --allow-write --command 'program 253 513 123'
  nor_test_host.py PORT --allow-write --timeout 14400 --command 'integrity 3'
  nor_test_host.py PORT --allow-power --command 'power off' --command 'probe-off'
  # Measure C19 OFF/ON first; G2 confirmation is never inferred from software:
  nor_test_host.py PORT --allow-write --allow-power --g2-measured \\
      --command 'cut 0 1 10 20'

Console syntax (numeric arguments use decimal or 0x hex):
  init | id | sfdp | status | read OFFSET LENGTH(<=256) | sha OFFSET LENGTH
  erase OFFSET SIZE(4096/32768/65536)
  erase-bench OFFSET SIZE COUNT(>=20)
  program OFFSET LENGTH SEED | bench OFFSET LENGTH SEED
  boundary | integrity 3 | reset | reset-op OFFSET KIND(0=program,1=erase)
  power on/off | probe-off | cut OFFSET KIND PERCENT(10/50/90) COUNT(>=20)
  jrecover | jquery | jbegin/jresume TARGET LENGTH SEED SOURCE
  jwrite SESSION_ID INDEX SEED | jfinish/jcancel SESSION_ID
  jcut PORT_CALL_COUNT (MCU reset only, no power cut)

Journal scenario (requires separate M-EVB approval):
  nor_test_host.py PORT --allow-write --timeout 120 --journal-target et100
  nor_test_host.py PORT --allow-write --timeout 120 --journal-target sensor
  Reduced M-EVB: one ET100 package, 3 mid-download MCU resets and explicit resume.
  Throughput: STIG 8-byte PP, not OTA-representative.

G1 always runs before user commands; mismatched ID/geometry stops the runner.
--allow-write explicitly authorizes U11 scratch writes, not MCU flashing.
T7 reset-op expects reboot; send separately, then rerun G1/status/read and
erase/program to inspect recovery. SWD DAC comparison is a separate manual gate.
""",
    )
    parser.add_argument("port", help="DAPLink virtual serial device")
    parser.add_argument("--log", type=Path, default=Path("/tmp/et100_nor_console.log"))
    parser.add_argument("--timeout", type=float, default=60, help="per-command seconds (default 60)")
    parser.add_argument("--command", action="append", default=[], help="repeatable console line")
    parser.add_argument("--allow-write", action="store_true", help="authorize destructive U11 scratch commands")
    parser.add_argument("--allow-power", action="store_true", help="authorize U11 supply switching (G2)")
    parser.add_argument("--g2-measured", action="store_true", help="Stanley measured C19 and confirmed G2")
    parser.add_argument("--journal-target", choices=["et100", "sensor"], help="M-EVB journal pattern scenario; U11 writes")
    parser.add_argument("--journal-resets", type=int, default=3, choices=[3], help="reduced M-EVB: exactly 3 MCU resets")
    parser.add_argument("--journal-resets-done", type=int, default=0, choices=range(4),
                        help="already verified resets when reconnecting to the same test session")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.journal_target and (not args.allow_write or args.command or args.allow_power):
        parser.error("journal scenario needs --allow-write, no extra commands or power options")
    for command in args.command:
        if not command or any(ord(c) < 32 or ord(c) > 126 for c in command):
            parser.error("commands must be single-line ASCII")
        words = command.split()
        if not words:
            parser.error("empty command")
        verb = words[0]
        if verb in POWER and not args.allow_power:
            parser.error(f"{verb} requires --allow-power")
        if verb not in READ_ONLY | {"power", "probe-off"} and not args.allow_write:
            parser.error(f"{verb} requires --allow-write")
        if verb in {"cut", "g2-confirm"} and not args.g2_measured:
            parser.error(f"{verb} requires --g2-measured")
        if verb == "arm":
            parser.error("arm is managed by --allow-write")
    try:
        import serial
    except ImportError:
        parser.error("pyserial unavailable; select an existing pyserial environment (no installation performed)")

    with args.log.open("a", encoding="utf-8") as log:
        def emit(line):
            stamp = datetime.datetime.now().astimezone().isoformat(timespec="milliseconds")
            text = f"{stamp} {line}"
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()

        with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=2) as uart:
            # Only drain before the first command; never discard response bytes.
            time.sleep(0.2)
            uart.reset_input_buffer()

            def send(command, reboot=False):
                emit("TX " + command)
                uart.write((command + "\r\n").encode("ascii"))
                uart.flush()
                deadline = time.monotonic() + args.timeout
                lines, pending = [], bytearray()
                result = None
                while time.monotonic() < deadline:
                    chunk = uart.read_until(b"\n", 1024)
                    pending.extend(chunk)
                    if len(pending) > 8192:
                        raise RuntimeError("oversized console line")
                    if not pending.endswith(b"\n"):
                        continue
                    try:
                        line = pending.decode("ascii", errors="strict").strip()
                    except UnicodeDecodeError:
                        emit("RX_RAW " + pending.hex())
                        if not reboot:
                            raise
                        line = pending.decode("ascii", errors="backslashreplace").strip()
                    pending.clear()
                    emit("RX " + line)
                    lines.append(line)
                    match = re.fullmatch(r"RESULT rc=(-?\d+)", line)
                    if match:
                        result = int(match[1])
                    if reboot and line.startswith("G1 init_rc="):
                        if line != "G1 init_rc=0 writes=locked":
                            raise RuntimeError("T7 reboot init failed")
                        result = 0
                    if line == "READY" and result is not None:
                        if result:
                            raise RuntimeError(f"{command!r} failed rc={result}")
                        return lines
                raise TimeoutError(f"no complete response to {command!r}")

            # Always prove live identity before any destructive/power command.
            send("init")
            identity = send("id")
            if "ID C86018 rc=0" not in identity:
                raise RuntimeError("unexpected JEDEC ID; STOP")
            geometry = send("sfdp")
            if not any(line.startswith("GEOMETRY capacity=16777216 page=256 ") for line in geometry):
                raise RuntimeError("unexpected SFDP geometry; STOP")
            status = send("status")
            emit("G1 READ-ONLY COMPLETE; PSRAM startup excluded, hardware coexistence still requires review")
            if args.allow_write:
                send("arm U11-SCRATCH")
            if args.g2_measured:
                if not args.allow_power:
                    parser.error("--g2-measured requires --allow-power")
                send("g2-confirm C19-MEASURED")
            if args.journal_target:
                target = 1 if args.journal_target == "et100" else 2
                length = 2097152 if target == 1 else 225280
                seed = 0x83e19a47
                expected = hashlib.sha256()
                for start in range(0, length, 4096):
                    data = bytearray()
                    for offset in range(start, min(start + 4096, length)):
                        x = offset ^ seed
                        x = (x ^ (x << 13)) & 0xffffffff
                        x ^= x >> 17
                        x = (x ^ (x << 5)) & 0xffffffff
                        data.append((x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24)) & 255)
                    expected.update(data)
                def progress():
                    row = next(s for s in send("jquery") if s.startswith("JOURNAL "))
                    fields = dict(item.split("=", 1) for item in row.split()[1:])
                    return fields, bytes.fromhex(fields["bitmap"])
                send("jrecover")
                fields, bitmap = progress()
                expected_sid = f"{seed:08X}{length:08X}"
                if fields["state"] == "0":
                    if args.journal_resets_done:
                        raise RuntimeError("cannot resume reset count without original session; STOP")
                    send(f"jbegin {target} {length} {seed} 3")
                else:
                    if (fields["state"] != "1" or fields["session"] != expected_sid
                            or int(fields["target"]) != target or int(fields["length"]) != length
                            or fields["sha256"].lower() != expected.hexdigest()):
                        raise RuntimeError("existing journal belongs to another/completed session; STOP, no cancel")
                    send(f"jresume {target} {length} {seed} {args.journal_resets_done or 3}")
                fields, bitmap = progress()
                sid = "0x" + fields["session"]
                if fields["sha256"].lower() != expected.hexdigest():
                    raise RuntimeError("host/MCU pattern SHA mismatch; STOP")
                count = (length + 4095) // 4096
                resets = {count//4: (0, 200), count//2: (1, 400), 3*count//4: (2, 528)}
                bitmap = bytearray(bitmap)
                for index in range(count):
                    if index in resets and resets[index][0] >= args.journal_resets_done:
                        sample, cut = resets[index]
                        before = bytes(bitmap)
                        send("jrecover")
                        send(f"jcut {cut}")
                        rows = send(f"jwrite {sid} {index} {seed}", reboot=True)
                        if "G1 init_rc=0 writes=locked" not in rows:
                            raise RuntimeError("requested journal reset did not occur; STOP")
                        send("jrecover")
                        fields, recovered = progress()
                        if any(a & b != a for a, b in zip(before, recovered)):
                            raise RuntimeError("committed journal progress lost; STOP")
                        if "0x"+fields["session"] != sid or fields["sha256"].lower() != expected.hexdigest():
                            raise RuntimeError("recovered session identity mismatch; STOP")
                        emit(f"JOURNAL_RESET sample={sample} index={index} recovered=1")
                        send("arm U11-SCRATCH")
                        send(f"jresume {target} {length} {seed} {sample+1}")
                        fields, recovered = progress()
                        if int(fields["source"]) != sample+1:
                            raise RuntimeError("explicit takeover not persisted; STOP")
                        bitmap = bytearray(recovered)
                    if not bitmap[index//8] & (1 << (index%8)):
                        send(f"jwrite {sid} {index} {seed}")
                        bitmap[index//8] |= 1 << (index%8)
                send(f"jfinish {sid}")
                fields, bitmap = progress()
                if fields["state"] != "2" or fields["sha256"].lower() != expected.hexdigest():
                    raise RuntimeError("journal finish verification failed; STOP")
                actual = send(f"sha {0x20000 if target == 1 else 0x420000} {length}")
                if "SHA256 " + expected.hexdigest().upper() not in actual:
                    raise RuntimeError("final NOR readback SHA mismatch; STOP")
                emit("M-EVB journal target COMPLETE; STIG 8-byte PP, not OTA-representative; no power-cut proof")
            for command in args.command:
                # init/power/reset clear write authorization by design.
                lines = send(command, reboot=command.split()[0] == "reset-op")
                if command.split()[0] == "probe-off" and "ID C86018 rc=0" in lines:
                    raise RuntimeError("G2 FAIL: U11 still responds with supply off; do not run T8")
            emit("PASS: requested console commands completed; this is not a full T1-T8 acceptance")
            del status


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, TimeoutError, OSError, UnicodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
