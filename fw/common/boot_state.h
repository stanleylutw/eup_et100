#ifndef ET100_BOOT_STATE_H
#define ET100_BOOT_STATE_H
#include <stddef.h>
#include <stdint.h>
#define BOOT_STATE_BASE 0x1fc000u
#define BOOT_STATE_BANK 8192u
enum boot_kind { BOOT_NONE, BOOT_PENDING, BOOT_CONFIRMED, BOOT_BACKUP_READY,
                 BOOT_TRIAL, BOOT_ROLLBACK_REQUIRED, BOOT_INSTALLED=BOOT_CONFIRMED };
enum update_result { UPDATE_OK, UPDATE_FAILED, UPDATE_ABORTED };
struct boot_record {
    uint32_t sequence, kind, length, reset_mask;
    union { uint8_t hash[32]; uint8_t candidate_hash[32]; };
    uint32_t confirmed_length;uint8_t confirmed_hash[32];uint32_t last_update;
};
struct internal_port {
    int (*read)(uint32_t offset,void *data,size_t length);
    int (*erase)(uint32_t offset,size_t length);
    int (*program)(uint32_t offset,const void *data,size_t length);
};
/* -1 means corrupt/nonempty with no committed record: fail closed. */
int boot_state_read(const struct internal_port *p,struct boot_record *record);
int boot_state_write(const struct internal_port *p,const struct boot_record *record);
/* App may only confirm the currently pending trial, bound to its full image hash. */
int boot_state_confirm(const struct internal_port *p,const uint8_t hash[32]);
#endif
