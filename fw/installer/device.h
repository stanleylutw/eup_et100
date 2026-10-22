#ifndef ET100_INSTALLER_DEVICE_H
#define ET100_INSTALLER_DEVICE_H
#include "boot_state.h"
#include "nor_port.h"
extern const struct internal_port installer_internal;
int installer_nor_init(void);
int installer_candidate_read(void *ctx,uint32_t offset,void *data,size_t length);
int installer_journal_hash(struct nor_port *port,uint32_t offset,uint32_t length,
                           uint8_t digest[32],void *context);
#endif
