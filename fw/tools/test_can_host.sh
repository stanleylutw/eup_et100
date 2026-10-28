#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 - <<'PY'
import hashlib,json,pathlib
manifest=json.loads(pathlib.Path('fw/tests/fixtures/can_port_manifest.json').read_text())
for item in manifest['files']:
    data=pathlib.Path(item['path']).read_bytes()
    if item['skip_source_comment']:data=data.split(b'\n',1)[1]
    assert hashlib.sha256(data).hexdigest()==item['sha256'],item['path']
print('PASS immutable F407 a730897 sources/tests/rules/vectors:',len(manifest['files']),'files')
PY
flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifw/src/can -Ifw/src -Ifw/tests)
for name in gateway can_protocol can_bitrate demo_frames; do
  cc "${flags[@]}" "fw/tests/test_${name}.c" fw/src/can/j1939.c fw/src/can/app_gateway.c \
    fw/src/can/app_can_protocol.c fw/src/can/app_can_bitrate.c -o "/tmp/et100_can_${name}"
  "/tmp/et100_can_${name}" fw/tests/fixtures/j1939_v19_vectors.json
done
cc "${flags[@]}" fw/tests/test_can_task.c fw/src/can/j1939.c fw/src/can/app_gateway.c \
  fw/src/can/app_can_protocol.c fw/src/can/app_can_bitrate.c -o /tmp/et100_can_task
/tmp/et100_can_task
sdk=fw/vendor/fr30xxc_sdk__202411
cc "${flags[@]}" -Wno-pointer-to-int-cast -Ifw/tests/can_fakes \
  -I"$sdk/components/drivers/peripheral/Inc" -DET100_SERIAL_HOST_TEST -DET100_SYS_HOST_TEST \
  fw/tests/test_can_sdk.c fw/src/serial_gchk.c fw/src/system_status.c fw/common/inst_status.c \
  fw/src/can/j1939.c fw/src/can/app_gateway.c fw/src/can/app_can_protocol.c fw/src/can/app_can_bitrate.c \
  -Ifw/common -Ifw/include -o /tmp/et100_can_sdk
/tmp/et100_can_sdk
if [[ ${1:-} == --nano ]]; then
  toolchain="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin"
  out=$(mktemp -d /tmp/et100-can-nano.XXXXXX)
  for name in demo_frames can_task; do
    # Unicorn lacks ARMv8-M STLB/LDAB emitted for C11 atomics on M33. Compile
    # the unchanged portable modules for M4 for this CPU-only nano check; the
    # production M33 build remains separately verified by build_evb_serial.sh.
    "$toolchain/arm-none-eabi-gcc" -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -Os \
      -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Ifw/src/can -Ifw/src -Ifw/tests \
      "fw/tests/test_${name}.c" fw/tests/nano_test_support.c fw/src/can/j1939.c fw/src/can/app_gateway.c \
      fw/src/can/app_can_protocol.c fw/src/can/app_can_bitrate.c \
      -nostartfiles --specs=nano.specs --specs=nosys.specs \
      -Wl,--gc-sections,-e,main,-Ttext=0x20000,-Tdata=0x20000000,-Map="$out/$name.map" -o "$out/$name.elf"
    fw/.venv/bin/python fw/tools/run_nano_test.py "$out/$name.elf" > "$out/$name.log"
    echo "PASS ARM newlib-nano CAN $name assertions (CPU/RAM only)"
  done
  echo "CAN nano ELF/map: $out"
fi
