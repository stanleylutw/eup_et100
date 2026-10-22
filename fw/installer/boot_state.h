#ifndef ET100_BOOT_STATE_H
#define ET100_BOOT_STATE_H
#include <stddef.h>
#include <stdint.h>
#define BOOT_STATE_BASE 0x1fc000u
#define BOOT_STATE_BANK 8192u
enum boot_kind { BOOT_NONE, BOOT_PENDING, BOOT_INSTALLED };
struct boot_record { uint32_t sequence, kind, length, reset_mask; uint8_t hash[32]; };
struct internal_port {
    int (*read)(uint32_t offset,void *data,size_t length);
    int (*erase)(uint32_t offset,size_t length);
    int (*program)(uint32_t offset,const void *data,size_t length);
};
/* -1 means corrupt/nonempty with no committed record: fail closed. */
int boot_state_read(const struct internal_port *p,struct boot_record *record);
int boot_state_write(const struct internal_port *p,const struct boot_record *record);
#endif
