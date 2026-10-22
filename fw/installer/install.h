#ifndef ET100_INSTALL_H
#define ET100_INSTALL_H
#include "boot_state.h"
#include "mcuboot_image.h"
typedef int (*install_hook)(unsigned phase,struct boot_record *record);
/* Hook phases 1/2/4 execute after first erase/page and during readback validation. */
int installer_copy(const struct internal_port *p,image_read_fn candidate,void *context,
                   struct boot_record *record,install_hook hook);
#endif
