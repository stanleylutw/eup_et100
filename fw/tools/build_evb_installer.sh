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
export PATH="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
mkdir -p "$fw_root/build_installer/Objects" "$fw_root/build_installer/output"
make -B -C "$fw_root/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC" \
    -f "$fw_root/targets/evb_installer.mk" OBJECT_DIR="$fw_root/build_installer/Objects" \
    OUTPUT_DIR="$fw_root/build_installer/output" "$@"
if [[ "$#" == 0 ]]; then
    python3 - "$fw_root/build_installer/output" <<'PY'
import pathlib, struct, sys, zlib
out=pathlib.Path(sys.argv[1]); raw=(out/'Project.bin').read_bytes(); b=(out/'Project_burn.bin').read_bytes()
w=struct.unpack_from('<12I',b)
assert len(raw)<=120*1024 and b[8192:]==raw
assert w[2]==len(raw) and w[3]==8192 and w[6]==zlib.crc32(raw)
assert w[7]==w[11]==0x51525251 and w[8]==0
assert b[4096:8192]==bytes(4096), 'vendor B-info gap changed; inspect before flashing'
print(f'PASS: stub={len(raw)} <=122880; FR509x CRC covers stub only; B-info zero gap must be SKIPPED')
PY
fi
