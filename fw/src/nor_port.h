#ifndef ET100_NOR_PORT_H
#define ET100_NOR_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum nor_result {
    NOR_OK = 0, NOR_EINVAL = -1, NOR_ERANGE = -2, NOR_ETIMEOUT = -3,
    NOR_EID = -4, NOR_ESFDP = -5, NOR_EPROTECTED = -6,
    NOR_ENOTREADY = -7, NOR_EVERIFY = -8, NOR_ELOCKED = -9
};

struct nor_info {
    uint8_t id[3];
    uint32_t capacity, page_size;
    uint32_t erase_size[4];
    uint8_t erase_opcode[4];
    uint32_t spi_hz;
};

/* Offsets are relative to the device, never CPU/DAC addresses.
 * Caller serializes all operations. Completion means WIP clear AND readback
 * verified, not power-loss durability. On error discard authorization and init.
 */
struct nor_port {
    struct nor_info info;
    int (*init)(void);
    int (*read_id)(uint8_t id[3]);
    int (*read_sfdp)(uint32_t offset, void *data, size_t length);
    int (*read)(uint32_t offset, void *data, size_t length);
    int (*program)(uint32_t offset, const void *data, size_t length);
    int (*erase)(uint32_t offset, uint32_t length);
    int (*wait_ready)(uint32_t timeout_us);
    int (*soft_reset)(void);
    int (*power)(bool on);
};

#endif
