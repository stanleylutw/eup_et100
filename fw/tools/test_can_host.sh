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
version_fields=$(python3 - <<'PY'
import pathlib,subprocess
names=('ET100_VERSION_FLAGS','ET100_VERSION_TEXT','ET100_VERSION_PARTS','ET100_CAN_VERSION_FLAGS')
lines=pathlib.Path('fw/targets/evb_serial.mk').read_text().splitlines()
definitions='\n'.join(line for line in lines if any(line.startswith(name+' ') for name in names))
query=definitions+"\nprint:\n\t@printf '%s\\n' '$(ET100_VERSION_TEXT) $(ET100_VERSION_PARTS)'\n"
default=subprocess.check_output(['make','--no-print-directory','-f','-','print'],input=query,text=True).strip()
# Command-line version overrides must update CAN components too.
override=subprocess.check_output(['make','--no-print-directory','-f','-','print',
    'ET100_VERSION_FLAGS=-DEUP_FIRMWARE_VERSION=\\"1.2.3-dev\\"'],input=query,text=True).strip()
assert override=='1.2.3-dev 1 2 3',override
print(default)
PY
)
read -r fw_version version_major version_minor version_patch <<< "$version_fields"
version_flags=("-DEUP_FIRMWARE_VERSION=\"$fw_version\"" "-DET100_CAN_VERSION_MAJOR=$version_major"
               "-DET100_CAN_VERSION_MINOR=$version_minor" "-DET100_CAN_VERSION_PATCH=$version_patch")
flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifw/src/can -Ifw/src -Ifw/tests)
flags+=("${version_flags[@]}")
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
# Versions that cannot fit the decimal minor/patch slots must fail compilation.
version_tmp=$(mktemp -d /tmp/et100-can-version.XXXXXX)
trap 'rm -rf "$version_tmp"' EXIT
for part in MINOR PATCH; do
  part_lower=$(printf '%s' "$part" | tr '[:upper:]' '[:lower:]')
  if cc "${flags[@]}" "-UET100_CAN_VERSION_$part" "-DET100_CAN_VERSION_$part=10" \
      -c fw/tests/test_can_task.c -o "$version_tmp/invalid.o" > "$version_tmp/error.log" 2>&1; then
    echo "FAIL CAN version $part=10 compiled" >&2; exit 1
  fi
  grep -q "CAN $part_lower version must be below 10" "$version_tmp/error.log"
done
echo "PASS CAN version: firmware $fw_version, make override, matching wire version and invalid minor/patch rejection"
if [[ ${1:-} == --nano ]]; then
  toolchain="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin"
  out=$(mktemp -d /tmp/et100-can-nano.XXXXXX)
  for name in demo_frames can_task; do
    # Unicorn lacks ARMv8-M STLB/LDAB emitted for C11 atomics on M33. Compile
    # the unchanged portable modules for M4 for this CPU-only nano check; the
    # production M33 build remains separately verified by build_evb_serial.sh.
    "$toolchain/arm-none-eabi-gcc" -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -Os \
      "${version_flags[@]}" \
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
