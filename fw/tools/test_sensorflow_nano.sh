#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python=fw/.venv/bin/python
"$python" - <<'PY'
import importlib.metadata as m
from pathlib import Path
try:
    for entry in Path('fw/tools/requirements-nano-test.txt').read_text().splitlines():
        name,version=entry.split('==')
        assert m.version(name)==version
except (m.PackageNotFoundError, AssertionError):
    raise SystemExit('Install: fw/.venv/bin/python -m pip install -r fw/tools/requirements-nano-test.txt')
PY
toolchain="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin"
out=$(mktemp -d /tmp/et100-nano.XXXXXX)
sdk=fw/vendor/fr30xxc_sdk__202411
"$toolchain/arm-none-eabi-gcc" -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -Os \
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
  -Ifw/tests/gatt_fakes -Ifw/src -Ifw/common -Ifw/installer -I"$sdk/components/btdm/include" \
  fw/tests/test_sensor_ota.c fw/tests/nano_test_support.c fw/src/sensor_smp_client.c \
  fw/src/sensor_ota_compat.c fw/src/cbor_min.c \
  -nostartfiles --specs=nano.specs --specs=nosys.specs \
  -Wl,--gc-sections,-e,main,-Ttext=0x20000,-Tdata=0x20000000,-Map="$out/test.map" -o "$out/test.elf"
if ! grep -q 'libc_nano.a' "$out/test.map"; then echo 'nano libc was not linked' >&2;exit 1;fi
"$python" fw/tools/run_nano_test.py "$out/test.elf"
echo "Nano ELF/map: $out"
