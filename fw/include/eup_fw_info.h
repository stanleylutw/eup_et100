#ifndef ET100_EUP_FW_INFO_H
#define ET100_EUP_FW_INFO_H
#include <stdint.h>
#include <stddef.h>
#ifndef EUP_FIRMWARE_VERSION
#define EUP_FIRMWARE_VERSION "0.8.3-dev"
#endif
#define EUP_FW_INFO_MAGIC "EUPFWV1"
#define EUP_FW_INFO_MAGIC_SIZE 8
#define EUP_FW_INFO_SCHEMA_VERSION 1
#define EUP_FW_INFO_PROJECT_NAME_SIZE 24
#define EUP_FW_INFO_HARDWARE_VERSION_SIZE 20
#define EUP_FW_INFO_FIRMWARE_VERSION_SIZE 32
#define EUP_FW_INFO_BUILD_TIME_SIZE 20
#define EUP_FW_INFO_BUILD_ID_SIZE 16
#define EUP_FW_INFO_GIT_COMMIT_SIZE 12
#define EUP_FW_INFO_GIT_BRANCH_SIZE 32
#define EUP_FW_INFO_BLE_NAME_SIZE 24
#define EUP_FW_INFO_RELEASE_NOTE_SIZE 64
struct __attribute__((packed)) eup_fw_info {
    char magic[8]; uint16_t schema_version_le;
    char project_name[24], hardware_version[20], firmware_version[32];
    char build_time[20], build_id[16], git_commit[12], git_branch[32];
    char ble_name_normal[24], ble_name_ota[24], release_note[64];
    uint32_t flags_le, metadata_crc32_le;
};
#define EUP_OFFSET(f,n) _Static_assert(offsetof(struct eup_fw_info,f)==n,"EUPFWV1 offset: " #f)
EUP_OFFSET(magic,0);EUP_OFFSET(schema_version_le,8);EUP_OFFSET(project_name,10);
EUP_OFFSET(hardware_version,34);EUP_OFFSET(firmware_version,54);EUP_OFFSET(build_time,86);
EUP_OFFSET(build_id,106);EUP_OFFSET(git_commit,122);EUP_OFFSET(git_branch,134);
EUP_OFFSET(ble_name_normal,166);EUP_OFFSET(ble_name_ota,190);EUP_OFFSET(release_note,214);
EUP_OFFSET(flags_le,278);EUP_OFFSET(metadata_crc32_le,282);
_Static_assert(sizeof(struct eup_fw_info)==286,"EUPFWV1 packed size");
#endif
