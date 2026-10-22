#ifndef ET100_BOOT_HEALTH_H
#define ET100_BOOT_HEALTH_H
void et100_boot_adv_started(void);
void et100_boot_poll(int storage_ready);
struct internal_port;
/* Boot-state flash port (erase/program limited to the boot state region). */
const struct internal_port *et100_boot_port(void);
#endif
