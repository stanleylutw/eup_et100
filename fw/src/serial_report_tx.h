#ifndef ET100_SERIAL_REPORT_TX_H
#define ET100_SERIAL_REPORT_TX_H
#include <stdbool.h>

struct et100_report_tx {
    bool (*try_lock)(void *context);
    void (*unlock)(void *context);
    bool (*paused)(bool system, void *context);
    void *context;
};

/* Claim one complete unsolicited line before producing it. A new SMP request
 * may let that claimed line finish; every subsequent line must claim again.
 * Producer runs under the TX mutex and must not acquire it again. */
static inline void et100_report_tx_poll(const struct et100_report_tx *tx,
                                        bool system, void (*produce)(void *))
{
    if (!tx->try_lock(tx->context)) return;
    if (!tx->paused(system, tx->context)) produce(tx->context);
    tx->unlock(tx->context);
}
#endif
