#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ifw/common -Ifw/installer -Ifw/src -Ifw/tests \
  fw/common/inst_status.c fw/tests/fake_nor.c fw/tests/test_inst_status.c -o /tmp/et100_inst_status_test
/tmp/et100_inst_status_test
sdk=fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined \
  -Ifw/include -Ifw/common -Ifw/installer -Ifw/src -I"$sdk/include" \
  fw/tests/test_trial_rollback.c fw/installer/transaction.c fw/installer/install.c \
  fw/common/boot_state.c fw/common/inst_status.c fw/installer/candidate.c fw/installer/mcuboot_image.c \
  "$sdk/library/sha256.c" "$sdk/library/platform_util.c" -o /tmp/et100_inst_transaction_test
/tmp/et100_inst_transaction_test
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined \
  -DET100_TEST_BAD_CANDIDATE=1 -Ifw/include -Ifw/common -Ifw/installer -Ifw/src -I"$sdk/include" \
  fw/tests/test_trial_rollback.c fw/installer/transaction.c fw/installer/install.c \
  fw/common/boot_state.c fw/common/inst_status.c fw/installer/candidate.c fw/installer/mcuboot_image.c \
  "$sdk/library/sha256.c" "$sdk/library/platform_util.c" -o /tmp/et100_bad_candidate_test
/tmp/et100_bad_candidate_test
cc -std=c11 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-int-to-void-pointer-cast \
  -fsanitize=address,undefined -DET100_TEST_NO_CONFIRM=1 -Ifw/tests/boot_health_fakes \
  -Ifw/include -Ifw/common -Ifw/installer -Ifw/src \
  fw/tests/test_boot_health.c fw/common/inst_status.c -o /tmp/et100_no_confirm_test
/tmp/et100_no_confirm_test
