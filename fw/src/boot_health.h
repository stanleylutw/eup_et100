#ifndef ET100_BOOT_HEALTH_H
#define ET100_BOOT_HEALTH_H
#include "inst_status.h"
void et100_boot_adv_started(void);
void et100_boot_poll(int storage_ready);
struct internal_port;
/* Boot-state flash port (erase/program limited to the boot state region). */
const struct internal_port *et100_boot_port(void);
int et100_boot_status(struct inst_snapshot *);
void et100_boot_refresh(void); /* Task-side refresh after SMP mutates boot state. */
size_t et100_boot_line(char *,size_t);
void et100_boot_line_sent(void); /* Serial task: after sending the drained line. */
#endif
