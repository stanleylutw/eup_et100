#ifndef ET100_TRANSACTION_H
#define ET100_TRANSACTION_H
#include "install.h"
#include "nor_port.h"
int transaction_run(const struct internal_port *p,struct nor_port *nor,
                    struct boot_record *record,install_hook hook);
#endif
