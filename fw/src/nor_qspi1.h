#ifndef ET100_NOR_QSPI1_H
#define ET100_NOR_QSPI1_H
#include "nor_port.h"

struct nor_port *nor_qspi1_port(void);
/* Explicit operator gate, never enabled at boot or by init/read_id. */
int nor_qspi1_authorize_writes(void);
int nor_qspi1_status(uint8_t status[2]);
/* Fixed read-only address/dummy probes; completion is not addressing proof. */
int nor_qspi1_diag_addr(void);
/* G2 diagnostic only: physically clock RDID while software power is off. */
int nor_qspi1_probe_off(uint8_t id[3]);
uint32_t nor_qspi1_now_us(void);
uint32_t nor_qspi1_last_operation_us(void);
void nor_qspi1_delay_us(uint32_t us);
/* T7/T8 only: start a single operation without waiting; same write gate. */
int nor_qspi1_start(uint32_t offset, uint32_t erase_size,
                    const uint8_t *data, size_t length);
#endif
