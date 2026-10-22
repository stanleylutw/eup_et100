#include "eup_fw_info.h"
const struct eup_fw_info et100_fw_info __attribute__((used,section(".eup_fw_info")))={
    .magic=EUP_FW_INFO_MAGIC,.schema_version_le=1,.project_name="EUP_ET100",
    .hardware_version="ET100_EVB_DEV1.4",.firmware_version=EUP_FIRMWARE_VERSION,
    .build_time="2026-10-22",.build_id="prototype",.git_commit="uncommitted",
    .git_branch="v0.8.3_installer",.ble_name_normal="EUP_AIoT_HUB_FR",
    .ble_name_ota="EUP_AIoT_HUB_FR",.release_note="UNSIGNED prototype installer test",
    .flags_le=0,.metadata_crc32_le=0
};
