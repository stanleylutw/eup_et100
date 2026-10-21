#!/usr/bin/env python3
"""Exercise the EVB's 9600-bps GCHK subset without changing settings."""

import argparse
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    args = parser.parse_args()
    replies = 0
    identity = None

    with serial.Serial(args.port, 9600, timeout=2, write_timeout=2) as port:
        time.sleep(0.3)
        port.reset_input_buffer()

        def receive():
            nonlocal replies
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                raw = port.read_until(b"\n", 200)
                print(f"RX {raw!r}", flush=True)
                assert raw.endswith(b"\r\n"), "missing CRLF / timeout"
                if raw.startswith(b"AT$PD=G1;1;"):
                    body, checksum = raw[6:-2].rsplit(b"*", 1)
                    actual = 0
                    for byte in body:
                        actual ^= byte
                    assert actual == int(checksum, 16), "Type 1 checksum mismatch"
                    continue
                replies += 1
                return raw.decode("ascii")
            raise AssertionError("response timeout amid asynchronous reports")

        def send(data):
            print(f"TX {data!r}", flush=True)
            port.write(data)
            port.flush()

        def gchk(token):
            nonlocal identity
            line = receive()
            match = re.fullmatch(
                r"AT\$PD=BLEGW\$GCHK;1;([0-9A-F]{12});Freqchip;"
                r"DEV1\.4_fr3068ep-c;0\.7\.0-dev;[012];([A-Za-z0-9]{4})\r\n",
                line,
            )
            assert match and match[2] == token, "GCHK schema/token mismatch"
            if identity is None:
                identity = match[1]
            assert match[1] == identity, "identity changed"

        for index in range(100):
            token = f"{index:04d}"
            send(f"BLEGW$GCHK;{token}\r\n".encode("ascii"))
            gchk(token)
        print("PASS: 100 sequential GCHK queries", flush=True)

        for fragment in (b"BLE", b"GW$G", b"CHK;", b"Ab1z\r", b"\n"):
            send(fragment)
            time.sleep(0.03)
        gchk("Ab1z")
        print("PASS: fragmented command + mixed-case token", flush=True)

        send(b"BLEGW$GCHK;ABCD\r\nBLEGW$GCHK;abcd\r\n")
        gchk("ABCD")
        gchk("abcd")
        print("PASS: concatenated commands", flush=True)

        send(b"BLEGW$GMAXRPT;FAIL\r\n")
        assert receive() == "AT$PD=BLEGW$GMAXRPT;0;FAIL\r\n"
        send(b"BLEGW$GCHK;extra;BAD1\r\n")
        assert receive() == "AT$PD=BLEGW$GCHK;0;BAD1\r\n"
        print("PASS: unsupported command and invalid parameters", flush=True)

        send(b"BLEGW$GCHK;BAD\r\nBLEGW$GCHK;A!B2\r\n")
        send(b"BLEGW$GCHK;GOOD\r\n")
        gchk("GOOD")
        send(b"X" * 200 + b"\r\nBLEGW$GCHK;LONG\r\n")
        gchk("LONG")
        print("PASS: malformed token and overlong-line recovery", flush=True)

        print(f"PASS: GCHK protocol; {replies} replies; identity={identity}",
              flush=True)


if __name__ == "__main__":
    main()
