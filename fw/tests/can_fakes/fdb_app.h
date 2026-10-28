#include <stdint.h>
#include <stddef.h>
#define FDB_NO_ERR 0
size_t flashdb_get_length(uint32_t);
size_t flashdb_get(uint32_t,uint8_t *,uint32_t);
int flashdb_set(uint32_t,uint8_t *,uint32_t);
