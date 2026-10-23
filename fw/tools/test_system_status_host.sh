#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DET100_SYS_HOST_TEST -Ifw/src fw/src/system_status.c fw/tests/test_system_status.c \
  -o /tmp/et100_system_status_test
/tmp/et100_system_status_test
