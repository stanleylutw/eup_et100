#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ifw/tests/gatt_fakes -Ifw/src -Ifw/common -Ifw/installer \
  fw/tests/test_relay_handoff.c -o /tmp/et100_relay_handoff_test
/tmp/et100_relay_handoff_test
