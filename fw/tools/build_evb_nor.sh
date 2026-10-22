#!/bin/bash
set -euo pipefail
fw_root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ "${1:-}" == --host-test ]]; then
    "${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror -g \
        -fsanitize=address,undefined -I"$fw_root/src" \
        -I"$fw_root/vendor/fr30xxc_sdk__202411/components/drivers/peripheral/Inc" \
        -I"$fw_root/vendor/fr30xxc_sdk__202411/components/drivers/device/fr30xx" \
        "$fw_root/tests/test_nor_port.c" -o /tmp/et100_test_nor
    /tmp/et100_test_nor
    exit
fi
sdk_gcc="$fw_root/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC"
export PATH="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
mkdir -p "$fw_root/build_nor/Objects" "$fw_root/build_nor/output"
make -B -C "$sdk_gcc" -f "$fw_root/targets/evb_nor.mk" \
    OBJECT_DIR="$fw_root/build_nor/Objects" OUTPUT_DIR="$fw_root/build_nor/output" "$@"
if [[ "$#" == 0 ]]; then
    python3 - "$fw_root/build_nor/output" <<'PY'
import pathlib, struct, sys, zlib
out = pathlib.Path(sys.argv[1])
raw = (out / "Project.bin").read_bytes()
burn = (out / "Project_burn.bin").read_bytes()
fields = struct.unpack_from("<12I", burn)
assert len(burn) <= 800 * 1024, "image overlaps FlashEnv"
assert fields[2] == len(raw) and fields[3] == 0x2000
assert fields[6] == zlib.crc32(raw) and fields[7] == fields[11] == 0x51525251
assert fields[8] == 0 and burn[0x2000:] == raw, "FR509x header/payload mismatch"
print(f"PASS: FR509x header/CRC/payload/800 KB guard; raw={len(raw)} burn={len(burn)}")
PY
fi
