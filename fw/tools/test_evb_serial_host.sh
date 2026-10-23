#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc_flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifw/common -Ifw/installer)
sdk=fw/vendor/fr30xxc_sdk__202411
includes=(-Ifw/tests/gatt_fakes -Ifw/src -I"$sdk/components/btdm/include")
cc "${cc_flags[@]}" -DET100_SERIAL_HOST_TEST -Ifw/src \
    fw/src/serial_gchk.c fw/common/inst_status.c fw/tests/test_serial_gchk.c -o /tmp/et100_serial_test
cc "${cc_flags[@]}" -DET100_SERIAL_HOST_TEST -Ifw/src \
    fw/src/ble_reporter.c fw/tests/test_ble_reporter.c -o /tmp/et100_ble_reporter_test
cc "${cc_flags[@]}" "${includes[@]}" fw/tests/test_scan_lifecycle.c -o /tmp/et100_scan_lifecycle_test
cc "${cc_flags[@]}" "${includes[@]}" -Ifw/third_party/tinycrypt/include \
    fw/tests/test_gatt_client.c fw/src/eup_aiot_uuid_client.c \
    fw/src/eup_aiot_metadata.c fw/src/eup_aiot_auth.c \
    fw/third_party/tinycrypt/source/aes_encrypt.c fw/third_party/tinycrypt/source/utils.c \
    -o /tmp/et100_gatt_test
cc "${cc_flags[@]}" -DET100_LED_HOST_TEST -Ifw/src \
    fw/src/status_led.c fw/tests/test_status_led.c -o /tmp/et100_status_led_test
cc "${cc_flags[@]}" -DET100_SERIAL_HOST_TEST -Ifw/src \
    fw/src/ble_reporter.c fw/src/app_manager.c fw/tests/test_app_manager.c -o /tmp/et100_app_manager_test
cc "${cc_flags[@]}" -DET100_SERIAL_HOST_TEST "${includes[@]}" -I"$sdk/examples/common/ota" \
    fw/tests/test_app_gatt_server.c fw/src/app_manager.c fw/src/ble_reporter.c -o /tmp/et100_app_gatt_server_test
cc "${cc_flags[@]}" "${includes[@]}" -Ifw/include \
    fw/tests/test_eup_meta.c fw/src/eup_meta_protocol.c fw/common/inst_status.c fw/src/eup_aiot_uuid_client.c \
    fw/src/eup_aiot_metadata.c -o /tmp/et100_eup_meta_test
cc "${cc_flags[@]}" -Wno-int-to-pointer-cast -Wno-int-to-void-pointer-cast -Ifw/tests/boot_health_fakes -Ifw/src -Ifw/common -Ifw/installer \
    -Ifw/include fw/tests/test_boot_health.c fw/common/inst_status.c -o /tmp/et100_boot_health_test
cc "${cc_flags[@]}" -Ifw/tests/gatt_fakes -Ifw/src fw/tests/test_sensor_ota.c fw/src/sensor_smp_client.c \
    fw/src/sensor_ota_compat.c fw/src/cbor_min.c -o /tmp/et100_sensor_ota_test
mbedtls="$sdk/components/modules/mbedtls"
cc "${cc_flags[@]}" -DET100_SMP_BLE_HOST_TEST -Ifw/src -Ifw/tests -Ifw/common -Ifw/installer \
    -Ifw/include -I"$mbedtls/include" fw/tests/test_smp.c fw/src/cbor_min.c fw/src/smp_serial.c \
    fw/src/smp_server.c fw/src/smp_ble.c fw/src/ota_manager.c fw/src/ota_journal.c \
    fw/src/sensor_package.c fw/src/sensor_store.c fw/tests/fake_nor.c fw/common/boot_state.c \
    fw/installer/mcuboot_image.c fw/installer/candidate.c fw/common/inst_status.c \
    "$mbedtls/library/sha256.c" "$mbedtls/library/platform_util.c" -o /tmp/et100_test_smp
# test_smp needs a real signed App image (bash fw/tools/build_evb_serial.sh).
smp_image=${ET100_SIGNED_IMAGE:-fw/build_serial/output/et100.signed.bin}
[[ -f "$smp_image" ]] || { echo "missing $smp_image: run fw/tools/build_evb_serial.sh first" >&2; exit 1; }
/tmp/et100_serial_test
/tmp/et100_ble_reporter_test
/tmp/et100_scan_lifecycle_test
/tmp/et100_gatt_test
/tmp/et100_status_led_test
/tmp/et100_app_manager_test
/tmp/et100_app_gatt_server_test
/tmp/et100_eup_meta_test
/tmp/et100_boot_health_test
/tmp/et100_sensor_ota_test
/tmp/et100_test_smp "$smp_image"
