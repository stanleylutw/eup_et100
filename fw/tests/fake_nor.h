#ifndef ET100_FAKE_NOR_H
#define ET100_FAKE_NOR_H
#include "nor_port.h"
#include <setjmp.h>
#define FAKE_NOR_SIZE 0x462000u
struct fake_nor {
    struct nor_port port;
    uint8_t *bytes, *used;
    bool weak[FAKE_NOR_SIZE/4096];
    uint64_t operations, programs, erases, reads, cut_at, cuts;
    unsigned mode;
    jmp_buf *power_loss;
};
void fake_nor_create(struct fake_nor *f);
void fake_nor_destroy(struct fake_nor *f);
void fake_nor_select(struct fake_nor *f);
void fake_nor_clear(struct fake_nor *f);
void fake_nor_copy(struct fake_nor *to, const struct fake_nor *from);
void fake_nor_inject(struct fake_nor *f, uint64_t operation, unsigned mode, jmp_buf *env);
#endif
