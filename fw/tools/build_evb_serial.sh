#!/bin/bash
set -euo pipefail
fw_root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ ! -x "$fw_root/.venv/bin/imgtool" ]] || ! "$fw_root/.venv/bin/python" - "$fw_root/tools/requirements-imgtool.txt" <<'PY'
import importlib.metadata, pathlib, sys
assert pathlib.Path(sys.prefix).resolve().name == '.venv'
assert 'imgtool==' + importlib.metadata.version('imgtool') == pathlib.Path(sys.argv[1]).read_text().strip()
PY
then
    echo "Install: python3 -m venv fw/.venv && fw/.venv/bin/python -m pip install --index-url https://pypi.org/simple -r fw/tools/requirements-imgtool.txt" >&2
    exit 1
fi
sdk_gcc="$fw_root/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC"
toolchain="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin"
export PATH="$toolchain:$PATH"
mkdir -p "$fw_root/build_serial/Objects" "$fw_root/build_serial/output"
# -B rebuilds all inputs: vendor Makefile does not track header dependencies.
make -B -C "$sdk_gcc" -f "$fw_root/targets/evb_serial.mk" \
    OBJECT_DIR="$fw_root/build_serial/Objects" \
    OUTPUT_DIR="$fw_root/build_serial/output" "$@"
if [[ "$#" == 0 ]]; then
    "$fw_root/.venv/bin/python" "$fw_root/tools/make_dfu_package.py" \
        "$fw_root/build_serial/output/Project.bin"
fi
