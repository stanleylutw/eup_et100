#ifndef ET100_INST_STATUS_H
#define ET100_INST_STATUS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "boot_state.h"
#include "../src/nor_port.h"
struct inst_progress { unsigned stage, bucket; bool seen; };
bool inst_progress_due(struct inst_progress *, unsigned stage, uint32_t done, uint32_t total);

#define INST_LINE_MAX 120u
#define INST_RECORD_BASE 0x460000u
#define INST_RECORD_END 0x462000u
#if defined(ET100_TEST_BAD_CANDIDATE) && ET100_TEST_BAD_CANDIDATE
#define INST_VERSION "0.8.11-badcand"
#else
#define INST_VERSION "0.8.11"
#endif
enum inst_code { IC_OK, IC_NOR_INIT, IC_SRC_INVALID, IC_BACKUP_FAIL, IC_ERASE_FAIL,
    IC_WRITE_FAIL, IC_VERIFY_FAIL, IC_STATE_WRITE, IC_STATE_READ, IC_TRIAL_TIMEOUT,
    IC_TRIAL_RESET, IC_RETRY_LIMIT, IC_RESULT_UNAVAILABLE };
enum inst_stage { IS_NONE, IS_NOR, IS_STATE_READ, IS_STATE_WRITE, IS_CHECK,
    IS_BACKUP, IS_ERASE, IS_WRITE, IS_VERIFY, IS_BOOT };
enum inst_result { IR_NONE, IR_RUNNING, IR_INSTALLED, IR_ABORTED, IR_ROLLED_BACK,
    IR_TIMEOUT, IR_HALT, IR_CONFIRMED };
struct inst_record {
    uint8_t hash[32];
    uint32_t sequence, state, attempts, result, code, stage;
};
struct inst_snapshot {
    uint32_t state;
    uint8_t running_hash[32]; /* Full signed-file SHA-256, not MCUboot TLV hash. */
    struct inst_record record;
    bool ready, has_record;
};
uint16_t inst_crc16(const void *, size_t);
size_t inst_line(char *, size_t, char src, const char *event, const uint8_t hash[32], const char *fields);
void inst_txn(const uint8_t hash[32], char out[9]);
const char *inst_state_name(uint32_t);
const char *inst_result_name(uint32_t);
const char *inst_stage_name(uint32_t);
/* No valid/readable record => 0; valid record => 1. Diagnostic data never boots an image. */
int inst_record_read(struct nor_port *, struct inst_record *);
int inst_record_write(struct nor_port *, const struct inst_record *);
enum inst_retry { INST_CONTINUE, INST_ABORT, INST_ROLLBACK };
enum inst_retry inst_attempt(struct inst_record *, const struct boot_record *);
/* Report callback is optional; existing install/reset-hook APIs remain unchanged. */
typedef void (*inst_report_fn)(enum inst_stage, uint32_t done, uint32_t total, unsigned code);
void inst_set_reporter(inst_report_fn);
void inst_report(enum inst_stage, uint32_t, uint32_t, unsigned);
void inst_set_feed(void (*feed)(void));
void inst_feed(void);
#endif
