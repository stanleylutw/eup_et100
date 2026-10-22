#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc_flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined)
sdk=fw/vendor/fr30xxc_sdk__202411
includes=(-Ifw/tests/gatt_fakes -Ifw/src -I"$sdk/components/btdm/include")
cc "${cc_flags[@]}" -DET100_SERIAL_HOST_TEST -Ifw/src \
    fw/src/serial_gchk.c fw/tests/test_serial_gchk.c -o /tmp/et100_serial_test
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
    fw/tests/test_eup_meta.c fw/src/eup_meta_protocol.c fw/src/eup_aiot_uuid_client.c \
    fw/src/eup_aiot_metadata.c -o /tmp/et100_eup_meta_test
/tmp/et100_serial_test
/tmp/et100_ble_reporter_test
/tmp/et100_scan_lifecycle_test
/tmp/et100_gatt_test
/tmp/et100_status_led_test
/tmp/et100_app_manager_test
/tmp/et100_app_gatt_server_test
/tmp/et100_eup_meta_test
