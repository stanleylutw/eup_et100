/* Standalone DEV1.4 NOR console. No BLE, PSRAM, FlashDB or scheduler startup.
 * Board clock/UART initialization derives from SDK peripheral example main.c.
 */
#include "fr30xx.h"
#include "nor_qspi1.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static UART_HandleTypeDef uart;
static struct nor_port *nor;
static bool g2_confirmed;

int _write(int fd, char *data, int length)
{
    (void)fd;
    uart_transmit(&uart, (uint8_t *)data, length);
    return length;
}

/* Required SDK sleep hooks, unused by this bare-metal target. */
bool user_deep_sleep_check(void) { return false; }
void user_entry_before_sleep(void) {}
void user_entry_after_sleep(void) {}
void user_entry_after_sleep_user(void) {}

static void board_init(void)
{
    pmu_init();
    System_ClkConfig_t c = {0};
    c.SPLL_CFG.PLL_N = 8;
    c.SPLL_CFG.PowerEn = 1;
    c.MCU_Clock_Source = MCU_CLK_SEL_SPLL_CLK;
    c.SOC_DIV = 1; c.MCU_DIV = 2;
    c.APB0_DIV = c.APB1_DIV = c.APB2_DIV = 1;
    System_SPLL_config(&c.SPLL_CFG, 1000);
    System_MCU_clock_Config(&c);
    __SYSTEM_UART_CLK_SELECT_SPLL();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_PULLUP;
    g.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &g);
    __SYSTEM_UART3_CLK_ENABLE();
    uart.UARTx = UART3;
    uart.Init.BaudRate = 115200;
    uart.Init.DataLength = UART_DATA_LENGTH_8BIT;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.FIFO_Mode = UART_FIFO_ENABLE;
    uart_init(&uart);
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void hex(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) printf("%02X", bytes[i]);
}

static bool number(const char *s, uint32_t *out)
{
    char *end;
    if (!s || !*s || *s == '-') return false;
    errno = 0;
    unsigned long n = strtoul(s, &end, 0);
    if (errno || *end || n > UINT32_MAX) return false;
    *out = n; return true;
}

static void pattern(uint8_t *buffer, uint32_t offset, size_t length, uint32_t seed)
{
    for (size_t i = 0; i < length; ++i) {
        uint32_t x = (offset + i) ^ seed;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        buffer[i] = x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24);
    }
}

static int sha_region(uint32_t offset, uint32_t length, uint8_t digest[32])
{
    if (offset > nor->info.capacity || length > nor->info.capacity - offset)
        return NOR_ERANGE;
    uint8_t data[256];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int rc = mbedtls_sha256_starts_ret(&ctx, 0);
    while (!rc && length) {
        size_t n = length > sizeof data ? sizeof data : length;
        rc = nor->read(offset, data, n);
        if (!rc) rc = mbedtls_sha256_update_ret(&ctx, data, n);
        offset += n; length -= n;
    }
    if (!rc) rc = mbedtls_sha256_finish_ret(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    return rc;
}

static int write_pattern(uint32_t offset, uint32_t length, uint32_t seed)
{
    uint8_t data[256];
    if (offset > nor->info.capacity || length > nor->info.capacity - offset)
        return NOR_ERANGE;
    uint32_t start = nor_qspi1_now_us();
    for (uint32_t pos = 0; pos < length;) {
        size_t n = length - pos > sizeof data ? sizeof data : length - pos;
        pattern(data, offset + pos, n, seed);
        int rc = nor->program(offset + pos, data, n);
        if (rc) return rc;
        pos += n;
    }
    uint32_t elapsed = nor_qspi1_now_us() - start;
    printf("PROGRAM bytes=%lu us=%lu KBps_x1000=%lu spi_hz=%lu\r\n",
           (unsigned long)length, (unsigned long)elapsed,
           (unsigned long)(elapsed ? (uint64_t)length * 1000000 / elapsed : 0),
           (unsigned long)nor->info.spi_hz);
    return NOR_OK;
}

static int boundary(void)
{
    const uint32_t points[] = {0, 0x7ff000, 0x800000, 0xfff000};
    uint8_t data[32], expected[32];
    for (unsigned i = 0; i < 4; ++i) {
        int rc = nor->erase(points[i], 4096);
        pattern(data, points[i], sizeof data, 0x12345678 + i);
        if (!rc) rc = nor->program(points[i], data, sizeof data);
        if (rc) return rc;
    }
    for (unsigned i = 0; i < 4; ++i) {
        pattern(expected, points[i], sizeof expected, 0x12345678 + i);
        int rc = nor->read(points[i], data, sizeof data);
        if (rc) return rc;
        if (memcmp(expected, data, sizeof data)) return NOR_EVERIFY;
        printf("BOUNDARY offset=0x%06lX dac=0x%08lX data=",
               (unsigned long)points[i], (unsigned long)(0x28000000 + points[i]));
        hex(data, sizeof data); printf("\r\n");
    }
    return NOR_OK;
}

static int erase_bench(uint32_t offset, uint32_t size, uint32_t count)
{
    uint32_t min = UINT32_MAX, max = 0;
    uint64_t total = 0;
    uint8_t zero[8] = {0};
    if (count < 20 || count > 1000) return NOR_EINVAL;
    for (uint32_t i = 0; i < count; ++i) {
        int rc = nor->program(offset, zero, sizeof zero);
        uint32_t start = nor_qspi1_now_us();
        if (!rc) rc = nor->erase(offset, size);
        uint32_t verified_us = nor_qspi1_now_us() - start;
        uint32_t elapsed = nor_qspi1_last_operation_us();
        if (rc) return rc;
        if (elapsed < min) min = elapsed;
        if (elapsed > max) max = elapsed;
        total += elapsed;
        printf("ERASE sample=%lu size=%lu wip_us=%lu verified_us=%lu\r\n",
               (unsigned long)i, (unsigned long)size, (unsigned long)elapsed,
               (unsigned long)verified_us);
    }
    printf("ERASE_STATS samples=%lu min_us=%lu avg_us=%lu max_us=%lu includes_readback=0 includes_command_overhead=1\r\n",
           (unsigned long)count, (unsigned long)min, (unsigned long)(total / count),
           (unsigned long)max);
    return NOR_OK;
}

static int integrity(uint32_t runs)
{
    if (runs != 3) return NOR_EINVAL;
    const uint32_t start = 0x20000, length = 0x400000, seed = 0x83e19a47;
    uint8_t digest[32], expected[32], bytes[256];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int rc = mbedtls_sha256_starts_ret(&ctx, 0);
    for (uint32_t pos = 0; !rc && pos < length; pos += sizeof bytes) {
        pattern(bytes, start + pos, sizeof bytes, seed);
        rc = mbedtls_sha256_update_ret(&ctx, bytes, sizeof bytes);
    }
    if (!rc) rc = mbedtls_sha256_finish_ret(&ctx, expected);
    mbedtls_sha256_free(&ctx);
    if (rc) return rc;
    for (uint32_t run = 0; run < runs; ++run) {
        uint32_t then = nor_qspi1_now_us();
        for (uint32_t pos = 0; pos < length; pos += 65536) {
            if ((rc = nor->erase(start + pos, 65536))) return rc;
            printf("INTEGRITY erase_run=%lu offset=0x%06lX\r\n",
                   (unsigned long)run, (unsigned long)(start + pos));
        }
        uint32_t program_start = nor_qspi1_now_us();
        /* Keep time accumulator active at least once per page, across wraps. */
        for (uint32_t pos = 0; pos < length; pos += sizeof bytes) {
            pattern(bytes, start + pos, sizeof bytes, seed);
            if ((rc = nor->program(start + pos, bytes, sizeof bytes))) return rc;
            (void)nor_qspi1_now_us();
            if (!(pos % 65536)) printf("INTEGRITY program_offset=0x%06lX\r\n", (unsigned long)(start + pos));
        }
        uint32_t program_us = nor_qspi1_now_us() - program_start;
        if ((rc = sha_region(start, length, digest))) return rc;
        printf("INTEGRITY run=%lu bytes=%lu program_us=%lu total_us=%lu sha256=",
               (unsigned long)run, (unsigned long)length, (unsigned long)program_us,
               (unsigned long)(nor_qspi1_now_us() - then));
        hex(digest, 32); printf("\r\n");
        if (memcmp(expected, digest, 32)) return NOR_EVERIFY;
    }
    return NOR_OK;
}

static int cut_test(uint32_t offset, bool erase_op, uint32_t percent, uint32_t repeats)
{
    if (!g2_confirmed) return NOR_ELOCKED;
    if ((percent != 10 && percent != 50 && percent != 90) || repeats < 20 || repeats > 100)
        return NOR_EINVAL;
    uint8_t zeros[8] = {0};
    static uint8_t data[4096];
    for (uint32_t i = 0; i < repeats; ++i) {
        int rc = nor->erase(offset, 4096);
        if (!rc && erase_op) {
            memset(data, 0, sizeof data);
            rc = nor->program(offset, data, sizeof data);
        }
        if (rc) return rc;
        rc = nor_qspi1_start(offset, erase_op ? 4096 : 0,
                            erase_op ? NULL : zeros, erase_op ? 0 : sizeof zeros);
        if (rc) return rc;
        nor_qspi1_delay_us((erase_op ? 70000 : 500) * percent / 100);
        if ((rc = nor->power(false))) return rc;
        int off_rc = nor->wait_ready(1000); /* must return promptly, no hang */
        if (off_rc != NOR_ENOTREADY) return NOR_EVERIFY;
        nor_qspi1_delay_us(10000);
        if ((rc = nor->init()) || (rc = nor->read(offset, data, sizeof data))) return rc;
        unsigned ff = 0, zero = 0;
        for (size_t j = 0; j < sizeof data; ++j) { ff += data[j] == 0xff; zero += data[j] == 0; }
        printf("CUT kind=%s percent=%lu sample=%lu ff=%u zero=%u other=%u off_rc=%d pp_bytes=8\r\n",
               erase_op ? "erase" : "program", (unsigned long)percent, (unsigned long)i,
               ff, zero, (unsigned)(sizeof data - ff - zero), off_rc);
        if ((rc = nor_qspi1_authorize_writes()) || (rc = nor->erase(offset, 4096)) ||
            (rc = nor->program(offset, zeros, sizeof zeros))) return rc;
    }
    return NOR_OK;
}

static int dispatch(char *line)
{
    char *args[7] = {0};
    unsigned argc = 0;
    for (char *s = strtok(line, " \t"); s; s = strtok(NULL, " \t")) {
        if (argc == 7) return NOR_EINVAL;
        args[argc++] = s;
    }
    if (!argc) return NOR_EINVAL;
    const char *cmd = args[0];
    uint32_t a = 0, b = 0, c = 0, d = 0;
    if (!strcmp(cmd, "init") && argc == 1) return nor->init();
    if (!strcmp(cmd, "diag-addr") && argc == 1) return nor_qspi1_diag_addr();
    if (!strcmp(cmd, "arm") && argc == 2 && !strcmp(args[1], "U11-SCRATCH"))
        return nor_qspi1_authorize_writes();
    if (!strcmp(cmd, "g2-confirm") && argc == 2 && !strcmp(args[1], "C19-MEASURED")) {
        g2_confirmed = true; return NOR_OK;
    }
    if ((!strcmp(cmd, "id") || !strcmp(cmd, "probe-off")) && argc == 1) {
        uint8_t id[3] = {0};
        int rc = !strcmp(cmd, "id") ? nor->read_id(id) : nor_qspi1_probe_off(id);
        printf("ID "); hex(id, 3); printf(" rc=%d\r\n", rc); return rc;
    }
    if (!strcmp(cmd, "status") && argc == 1) {
        uint8_t sr[2] = {0}; int rc = nor_qspi1_status(sr);
        printf("STATUS sr1=%02X sr2=%02X\r\n", sr[0], sr[1]); return rc;
    }
    if (!strcmp(cmd, "power") && argc == 2 && (!strcmp(args[1], "on") || !strcmp(args[1], "off")))
        return nor->power(!strcmp(args[1], "on"));
    if (!strcmp(cmd, "reset") && argc == 1) return nor->soft_reset();
    if (!strcmp(cmd, "boundary") && argc == 1) return boundary();
    if (!strcmp(cmd, "sfdp") && argc == 1) {
        uint8_t bytes[256]; int rc = nor->read_sfdp(0, bytes, sizeof bytes);
        if (rc) return rc;
        for (unsigned i = 0; i < sizeof bytes; i += 16) {
            printf("SFDP %04X ", i); hex(bytes + i, 16); printf("\r\n");
        }
        printf("GEOMETRY capacity=%lu page=%lu spi_hz=%lu psram_init=0\r\n",
               (unsigned long)nor->info.capacity, (unsigned long)nor->info.page_size,
               (unsigned long)nor->info.spi_hz);
        for (unsigned i = 0; i < 4; ++i) printf("ERASE_TYPE size=%lu opcode=%02X\r\n",
               (unsigned long)nor->info.erase_size[i], nor->info.erase_opcode[i]);
        return NOR_OK;
    }
    if (!strcmp(cmd, "integrity") && argc == 2 && number(args[1], &a)) return integrity(a);
    if (argc > 1 && !number(args[1], &a)) return NOR_EINVAL;
    if (argc > 2 && !number(args[2], &b)) return NOR_EINVAL;
    if (argc > 3 && !number(args[3], &c)) return NOR_EINVAL;
    if (argc > 4 && !number(args[4], &d)) return NOR_EINVAL;
    if (!strcmp(cmd, "read") && argc == 3 && b <= 256) {
        uint8_t data[256]; int rc = nor->read(a, data, b);
        if (!rc) { printf("DATA "); hex(data, b); printf("\r\n"); } return rc;
    }
    if (!strcmp(cmd, "sha") && argc == 3) {
        uint8_t hash[32]; int rc = sha_region(a, b, hash);
        if (!rc) { printf("SHA256 "); hex(hash, 32); printf("\r\n"); } return rc;
    }
    if (!strcmp(cmd, "erase") && argc == 3) return nor->erase(a, b);
    if (!strcmp(cmd, "erase-bench") && argc == 4) return erase_bench(a, b, c);
    if ((!strcmp(cmd, "program") || !strcmp(cmd, "bench")) && argc == 4) return write_pattern(a, b, c);
    if (!strcmp(cmd, "reset-op") && argc == 3 && b <= 1) {
        uint8_t zero[8] = {0};
        int rc = nor_qspi1_start(a, b ? 4096 : 0, b ? NULL : zero, b ? 0 : sizeof zero);
        if (!rc) { __DSB(); NVIC_SystemReset(); }
        return rc;
    }
    if (!strcmp(cmd, "cut") && argc == 5 && b <= 1) return cut_test(a, b, c, d);
    return NOR_EINVAL;
}

int main(void)
{
    char line[160];
    size_t length = 0;
    bool overflow = false;
    board_init();
    nor = nor_qspi1_port();
    printf("ET100 NOR 0.8.1-dev UART=115200 BLE=off PSRAM=off\r\n");
    /* Boot scans GPIO functions with RDID only, then normal read-only G1. */
    int rc = nor->init();
    printf("GPIO PortB_H_FuncMux=0x%08lX PortC_L_FuncMux=0x%08lX OOL_FC=0x%02lX\r\n",
           (unsigned long)SYSTEM->PortB_H_FuncMux,
           (unsigned long)SYSTEM->PortC_L_FuncMux,
           (unsigned long)ool_read(0xfc));
    printf("G1 init_rc=%d writes=locked\r\nREADY\r\n", rc);
    for (;;) {
        uint8_t ch;
        uart_receive(&uart, &ch, 1);
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[length] = 0;
            rc = overflow ? NOR_EINVAL : dispatch(line);
            printf("RESULT rc=%d\r\nREADY\r\n", rc);
            length = 0; overflow = false;
        } else if (ch < 32 || ch > 126 || length == sizeof line - 1) overflow = true;
        else if (!overflow) line[length++] = ch;
    }
}
