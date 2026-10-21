#!/bin/bash
set -euo pipefail
fw_root="$(cd "$(dirname "$0")/.." && pwd)"
sdk_gcc="$fw_root/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC"
toolchain="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin"
export PATH="$toolchain:$PATH"
mkdir -p "$fw_root/build_serial/Objects" "$fw_root/build_serial/output"
# -B rebuilds all inputs: vendor Makefile does not track header dependencies.
make -B -C "$sdk_gcc" -f "$fw_root/targets/evb_serial.mk" \
    OBJECT_DIR="$fw_root/build_serial/Objects" \
    OUTPUT_DIR="$fw_root/build_serial/output" "$@"
if [[ "$#" == 0 ]]; then
    image_bytes=$(wc -c < "$fw_root/build_serial/output/Project_burn.bin")
    if (( image_bytes > 800 * 1024 )); then
        echo "ERROR: burn image overlaps SDK FlashEnv partition at flash offset 0xC8000" >&2
        exit 1
    fi
fi
