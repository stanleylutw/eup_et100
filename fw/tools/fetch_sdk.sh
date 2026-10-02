#!/usr/bin/env bash
# fetch_sdk.sh — 下載 Freqchip FR3068E-C SDK 到 vendor/
#
# 來源：gitee.com/qinyunti/fr3068-e-c-micropython（社群 bundled 版本）
# 版本：fr30xxc_sdk__202411
#
# 用法：./tools/fetch_sdk.sh [--force]
#
# TODO：待 Quectel/Freqchip 回覆 Q39 後改走正式授權管道。

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR_DIR="${PROJECT_ROOT}/vendor"
SDK_NAME="fr30xxc_sdk__202411"
SDK_DIR="${VENDOR_DIR}/${SDK_NAME}"
VERSION_FILE="${VENDOR_DIR}/.sdk_version"
GITEE_REPO="https://gitee.com/qinyunti/fr3068-e-c-micropython.git"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then
  FORCE=1
fi

if [[ -d "${SDK_DIR}" ]] && [[ "${FORCE}" -eq 0 ]]; then
  echo "SDK already present at ${SDK_DIR}"
  echo "Run with --force to re-fetch."
  exit 0
fi

echo "Fetching Freqchip SDK (${SDK_NAME})..."
echo "  Source: ${GITEE_REPO}"
echo "  Target: ${SDK_DIR}"

TMP_DIR="$(mktemp -d)"
trap "rm -rf '${TMP_DIR}'" EXIT

cd "${TMP_DIR}"

# shallow clone + sparse checkout 只拉 SDK 子目錄（省頻寬）
git clone --depth 1 --filter=blob:none --no-checkout "${GITEE_REPO}" repo
cd repo
git sparse-checkout init --cone
git sparse-checkout set "${SDK_NAME}"
git checkout

# 搬到 vendor/
mkdir -p "${VENDOR_DIR}"
rm -rf "${SDK_DIR}"
mv "${SDK_NAME}" "${VENDOR_DIR}/"

# 紀錄版本
cat > "${VERSION_FILE}" <<EOF
sdk_name=${SDK_NAME}
source=${GITEE_REPO}
fetched_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)
fetched_commit=$(git -C "${TMP_DIR}/repo" rev-parse HEAD)
EOF

echo ""
echo "✓ SDK downloaded to ${SDK_DIR}"
echo "  Size: $(du -sh "${SDK_DIR}" | cut -f1)"
echo "  Version info: ${VERSION_FILE}"
echo ""
echo "Next step: configure CMake"
echo "  mkdir build && cd build"
echo "  cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake .."
echo "  ninja"
