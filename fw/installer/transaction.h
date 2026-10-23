#ifndef ET100_TRANSACTION_H
#define ET100_TRANSACTION_H
#include "install.h"
#include "nor_port.h"
#include "inst_status.h"
/* Resolve NOR init failure and interrupted-attempt policy before touching App bytes. */
int transaction_prepare(const struct internal_port *,struct boot_record *,struct inst_record *,int nor_rc);
int transaction_run(const struct internal_port *p,struct nor_port *nor,
                    struct boot_record *record,install_hook hook);
#endif
