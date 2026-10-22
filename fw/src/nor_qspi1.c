/* QSPI1-only backend. Register transactions derive from Freqchip SDK 202411;
 * vendor SDK remains unchanged. No QSPI0 status/protection operations here.
 */
#include "nor_qspi1.h"
#include <string.h>
#include <stdio.h>
#ifdef NOR_HOST_TEST
#define __FR30XX_H__
#include "driver_qspi.h"
#include "driver_gpio.h"
#else
#include "fr30xx.h"
#endif

static struct nor_port port;
static bool powered, identified, writes_allowed;
static uint32_t last_cycles, micros, cycle_remainder;
static uint32_t operation_us;

/* Derived from SDK flash_erase/flash_write timing boundaries. */
uint32_t nor_qspi1_last_operation_us(void) { return operation_us; }

/* Derived from CMSIS DWT cycle-counter access used by SDK timing helpers. */
static uint32_t cycles(void)
{
#ifdef NOR_HOST_TEST
    return fake_cycles();
#else
    return DWT->CYCCNT;
#endif
}

/* Derived from SDK system_get_CoreClock. */
static uint32_t cycles_per_us(void)
{
#ifdef NOR_HOST_TEST
    return 96;
#else
    uint32_t hz = system_get_CoreClock();
    return hz >= 1000000 ? hz / 1000000 : 1;
#endif
}

/* Derived from SDK system_get_CoreClock and its DWT-based timing path. */
uint32_t nor_qspi1_now_us(void)
{
    uint32_t now = cycles(), delta = now - last_cycles;
    uint32_t scale = cycles_per_us();
    last_cycles = now;
    micros += delta / scale;
    cycle_remainder += delta % scale;
    micros += cycle_remainder / scale;
    cycle_remainder %= scale;
    return micros;
}

/* Derived from SDK system_delay_us, with a finite DWT-independent loop bound. */
void nor_qspi1_delay_us(uint32_t us)
{
    uint32_t start = nor_qspi1_now_us();
    /* A stopped DWT must not turn a delay into an infinite loop. */
    uint64_t budget = (uint64_t)us * cycles_per_us() + 1;
    while ((uint32_t)(nor_qspi1_now_us() - start) < us && budget--) {
#ifndef NOR_HOST_TEST
        __NOP();
#endif
    }
}

/* Reduced QSPI1 register subset used below, validated by Stanley's SWD scan.
 * SDK qspi_regs_t also describes absent registers; every access goes here.
 */
#define QSPI1_OFFSET_ALLOWED(o) \
    ((o) == 0x00 || (o) == 0x04 || (o) == 0x08 || (o) == 0x0c || \
     (o) == 0x14 || (o) == 0x24 || (o) == 0x38 || (o) == 0x90 || \
     (o) == 0x94 || (o) == 0xa0 || (o) == 0xa4 || (o) == 0xa8 || (o) == 0xac)

#define QSPI1_REG(member) (*(__typeof__(((struct qspi_regs_t *)0)->member) *) \
    __extension__ ({ \
        _Static_assert(QSPI1_OFFSET_ALLOWED(offsetof(struct qspi_regs_t, member)), \
                       "register is outside the reduced QSPI1 allowlist"); \
        controller_register(offsetof(struct qspi_regs_t, member)); \
    }))

/* Derived from SDK QSPI1 accesses; host validates each MMIO word separately. */
static volatile void *controller_register(size_t offset)
{
#ifdef NOR_HOST_TEST
    return fake_register(offset);
#else
    return (volatile void *)((uintptr_t)QSPI1 + offset);
#endif
}

/* Derived from SDK flash_init_controller/flash_set_read_single/write_single.
 * Configure single SPI only: G1 must not set QE or write any flash register.
 */
static void controller_configure(void)
{
    QSPI1_REG(config).enable = 0;
    QSPI1_REG(cmd_ctrl) = (struct qspi_stig_reg_t){0};
    QSPI1_REG(read_conf) = (struct qspi_read_ins_reg_t){.opcode = 0x03};
    QSPI1_REG(write_conf) = (struct qspi_write_ins_reg_t){.opcode = 0x02};
    QSPI1_REG(size_cfg).addr_bytes = 2;
    QSPI1_REG(size_cfg).page_bytes = 256;
    QSPI1_REG(poll_cfg).disable_poll = 1;
    QSPI1_REG(poll_cfg).enable_expiration = 0;
    QSPI1_REG(config).cpol = 1;
    QSPI1_REG(config).cpha = 1;
    QSPI1_REG(config).enable_DAC = 1;
    QSPI1_REG(config).enable_legacy = 0;
    QSPI1_REG(config).enable_AHB_remap = 1;
    QSPI1_REG(config).enable_AHB_decoder = 1;
    QSPI1_REG(config).baud_rate = 15; /* SPLL 192 MHz / 32 = 6 MHz. */
    QSPI1_REG(delay).sel_start_offset = 2;
    QSPI1_REG(delay).sel_end_offset = 2;
    QSPI1_REG(remap_address) = 0;
    QSPI1_REG(config).enable = 1;
}

/* Derived from SDK __SYSTEM_QSPI1_RESET + flash_init_controller.
 * Disable/reset QSPI1 only. Reduced QSPI1 has no indirect-engine registers.
 * A failed physical reset is reported by the following bounded idle check.
 */
static void controller_abort(void)
{
    writes_allowed = false;
    identified = false;
    QSPI1_REG(config).enable = 0;
#ifdef NOR_HOST_TEST
    fake_reset();
#else
    __SYSTEM_QSPI1_RESET();
    __DSB();
    __ISB();
#endif
    controller_configure();
}

/* Derived from SDK qspi_stig_cmd wait loops. Dual bounds work with PRIMASK=1
 * and with a stopped/unavailable DWT; no RTOS tick or system_delay_us.
 */
static int controller_wait(bool stig)
{
    uint32_t start = cycles(), limit = cycles_per_us() * 1000;
    for (uint32_t attempts = 0; attempts < 100000; ++attempts) {
#ifdef NOR_HOST_TEST
        fake_poll(stig);
#endif
        if (stig ? !QSPI1_REG(cmd_ctrl).progress_status : QSPI1_REG(config).status)
            return NOR_OK;
        if ((uint32_t)(cycles() - start) >= limit) break;
    }
    controller_abort();
    return NOR_ETIMEOUT;
}

/* Derived from SDK qspi_stig_cmd. Zeroed data avoids SDK uninitialized-word
 * reads for short writes. STIG supports at most eight bytes per transaction.
 */
static int command(uint8_t opcode, bool address, uint32_t offset,
                   uint8_t dummy, const void *tx, void *rx, size_t length)
{
    struct qspi_stig_reg_t cmd = {0};
    uint32_t words[2] = {0};
    if (!powered) return NOR_ENOTREADY;
    if (length > 8 || (tx && rx) || (length && !tx && !rx)) return NOR_EINVAL;
#ifdef NOR_HOST_TEST
    uint32_t irq = fake_irq_save();
#else
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
#endif
    int rc = controller_wait(false);
    if (rc == NOR_OK) {
        cmd.opcode = opcode;
        cmd.enable_cmd_addr = address;
        cmd.addr_bytes = 2;
        cmd.dummy_cycles = dummy;
        cmd.enable_read = rx != NULL;
        cmd.enable_write = tx != NULL;
        if (rx) cmd.read_bytes = length - 1;
        if (tx) {
            cmd.write_bytes = length - 1;
            memcpy(words, tx, length);
            QSPI1_REG(write_data_L) = words[0];
            QSPI1_REG(write_data_H) = words[1];
        }
        QSPI1_REG(cmd_address) = offset;
        QSPI1_REG(cmd_ctrl) = cmd;
        QSPI1_REG(cmd_ctrl).execute = 1;
#ifdef NOR_HOST_TEST
        fake_execute();
#endif
        rc = controller_wait(true);
        if (rc == NOR_OK && rx) {
            words[0] = QSPI1_REG(read_data_L);
            words[1] = QSPI1_REG(read_data_H);
            memcpy(rx, words, length);
        }
    }
#ifdef NOR_HOST_TEST
    fake_irq_restore(irq);
#else
    __set_PRIMASK(irq);
#endif
    return rc;
}

/* Derived from SDK flash_init_controller, using local bounded idle wait. */
static int controller_init(void)
{
    controller_abort();
    return controller_wait(false);
}

/* Derived from SDK flash_read_id, using the bounded local STIG command. */
static int read_id(uint8_t id[3])
{
    return id ? command(0x9f, false, 0, 0, NULL, id, 3) : NOR_EINVAL;
}

/* Derived from SDK qspi_stig_cmd READ handling; chunks to its eight-byte limit. */
static int read_sfdp(uint32_t offset, void *buffer, size_t length)
{
    uint8_t *data = buffer;
    if ((!data && length) || offset > 0xffffff || length > 0x1000000u - offset)
        return NOR_ERANGE;
    while (length) {
        size_t n = length > 8 ? 8 : length;
        /* DEV1.4 QSPI1 measurement: eight SFDP dummy clocks use raw 1.
         * Do not generalize this encoding to other controllers or counts.
         */
        int rc = command(0x5a, true, offset, 1, NULL, data, n);
        if (rc) return rc;
        offset += n; data += n; length -= n;
    }
    return NOR_OK;
}

/* Derived from SDK qspi_stig_cmd plus GD25LQ128E REMS/SFDP commands.
 * Capture registers immediately after each SFDP read, before another command.
 */
static int diagnostic_read(uint8_t opcode, uint32_t address, uint8_t dummy, size_t length)
{
    uint8_t data[8] = {0};
    uint32_t raw_command = 0, raw_size = 0, command_address = 0;
    struct qspi_stig_reg_t control = {0};
    struct qspi_device_size_cfg_t size = {0};
    int rc = command(opcode, true, address, dummy, NULL, data, length);
    if (!rc && opcode == 0x5a) {
        _Static_assert(sizeof control == 4 && sizeof size == 4, "QSPI register size changed");
        control = QSPI1_REG(cmd_ctrl);
        command_address = QSPI1_REG(cmd_address);
        size = QSPI1_REG(size_cfg);
        memcpy(&raw_command, &control, sizeof raw_command);
        memcpy(&raw_size, &size, sizeof raw_size);
    }
    printf("DIAG op=%02X addr=%06lX dummy=%u len=%u data=",
           opcode, (unsigned long)address, dummy, (unsigned)length);
    if (!rc) for (size_t i = 0; i < length; ++i) printf("%02X", data[i]);
    printf(" rc=%d\r\n", rc);
    if (!rc && opcode == 0x5a) {
        printf("DIAG REGS cmd_ctrl@90=0x%08lX cmd_address@94=0x%08lX size_cfg@14=0x%08lX\r\n",
               (unsigned long)raw_command, (unsigned long)command_address, (unsigned long)raw_size);
        printf("DIAG ENCODING addr_en=%u addr_bytes_raw=%u addr_bytes=%u dummy_raw=%u read_bytes_raw=%u read_bytes=%u size_addr_raw=%u size_addr_bytes=%u\r\n",
               control.enable_cmd_addr, control.addr_bytes, control.addr_bytes + 1,
               control.dummy_cycles, control.read_bytes, control.read_bytes + 1,
               size.addr_bytes, size.addr_bytes + 1);
    }
    return rc;
}

/* Derived from SDK flash_read_id/qspi_stig_cmd; fixed read-only diagnostics.
 * Revoke any write authorization. Raw mismatches are evidence, never patched.
 */
int nor_qspi1_diag_addr(void)
{
    uint8_t id[3] = {0};
    const uint32_t addresses[] = {0x00, 0x01, 0x16, 0x30};
    writes_allowed = false;
    int rc = read_id(id);
    printf("DIAG ID %02X%02X%02X rc=%d writes=locked\r\n", id[0], id[1], id[2], rc);
    if (rc) return rc;
    if (memcmp(id, "\xc8\x60\x18", 3)) return NOR_EID;
    for (uint32_t address = 0; address < 2; ++address)
        if ((rc = diagnostic_read(0x90, address, 0, 2))) return rc;
    for (unsigned i = 0; i < sizeof addresses / sizeof addresses[0]; ++i)
        if ((rc = diagnostic_read(0x5a, addresses[i], 8, 1))) return rc;
    for (uint8_t dummy = 0; dummy <= 16; ++dummy)
        if ((rc = diagnostic_read(0x5a, 0, dummy, 8))) return rc;
    printf("DIAG COMPLETE transport_only=1 addressing_proven=0 writes=locked\r\n");
    return NOR_OK;
}

/* No direct SDK equivalent; decodes a JESD216 little-endian table word. */
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Derived from SDK flash_init device identification; geometry follows JESD216 BFPT. */
static int discover(void)
{
    uint8_t h[8], table[36];
    int rc = read_id(port.info.id);
    if (rc) return rc;
    if (memcmp(port.info.id, "\xc8\x60\x18", 3)) return NOR_EID;
    if ((rc = read_sfdp(0, h, 8))) return rc;
    if (memcmp(h, "SFDP", 4) || h[5] != 1 || h[6] > 15) return NOR_ESFDP;
    unsigned count = (unsigned)h[6] + 1;
    bool found = false;
    for (unsigned i = 0; i < count; ++i) {
        if ((rc = read_sfdp(8 + i * 8, h, 8))) return rc;
        if (h[0] != 0 || h[7] != 0xff || h[2] != 1 || h[3] < 9) continue;
        uint32_t addr = h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16;
        if ((rc = read_sfdp(addr, table, sizeof table))) return rc;
        found = true; break;
    }
    if (!found) return NOR_ESFDP;
    uint32_t density = le32(table + 4);
    uint64_t bits;
    if (density & 0x80000000u) {
        unsigned exponent = density & 0x7fffffffu;
        if (exponent > 27 || exponent < 3) return NOR_ESFDP;
        bits = UINT64_C(1) << exponent;
    } else bits = (uint64_t)density + 1;
    if (bits % 8 || bits / 8 != (UINT64_C(1) << port.info.id[2])) return NOR_ESFDP;
    port.info.capacity = bits / 8;
    port.info.page_size = 256; /* GD25LQ128E Rev1.3 section 7.13. */
    port.info.spi_hz = 6000000;
    unsigned supported = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned exponent = table[28 + i * 2];
        uint8_t op = table[29 + i * 2];
        port.info.erase_size[i] = 0; port.info.erase_opcode[i] = 0;
        if ((exponent == 12 && op == 0x20) || (exponent == 15 && op == 0x52) ||
            (exponent == 16 && op == 0xd8)) {
            port.info.erase_size[i] = 1u << exponent;
            port.info.erase_opcode[i] = op;
            supported |= exponent == 12 ? 1 : exponent == 15 ? 2 : 4;
        }
    }
    return supported == 7 ? NOR_OK : NOR_ESFDP;
}

/* Weak no-op; the App overrides it to let equal-priority tasks run while NOR is busy. */
__attribute__((weak)) void nor_qspi1_yield(void) {}

/* Derived from SDK flash_wait_wip_clear, replacing its unbounded loop. */
static int wait_ready(uint32_t timeout_us)
{
    uint32_t start = nor_qspi1_now_us();
    if (!timeout_us || timeout_us > 10000000) return NOR_EINVAL;
    /* Finite fallback even if cycle counter is stalled. */
    uint64_t attempts = (uint64_t)timeout_us * 4 + 1;
    while (attempts--) {
        uint8_t sr;
        int rc = command(0x05, false, 0, 0, NULL, &sr, 1);
        if (rc) return rc;
        if (sr == 0xff) { controller_abort(); return NOR_ENOTREADY; }
        if (!(sr & 1)) return NOR_OK;
        if ((uint32_t)(nor_qspi1_now_us() - start) >= timeout_us) break;
        /* IRQs are restored between commands; RTOS integration yields here. */
        nor_qspi1_yield();
    }
    controller_abort();
    return NOR_ETIMEOUT;
}

/* Derived from SDK flash_read_status and flash_read_status_h. */
int nor_qspi1_status(uint8_t status[2])
{
    if (!status) return NOR_EINVAL;
    int rc = command(0x05, false, 0, 0, NULL, status, 1);
    return rc ? rc : command(0x35, false, 0, 0, NULL, status + 1, 1);
}

/* Derived from SDK lvgl_demo hw_dsp_xip_flash_init power control. */
static int power(bool on)
{
    writes_allowed = identified = false;
#ifndef NOR_HOST_TEST
    ool_write(0xfc, on ? ool_read(0xfc) | 1 : ool_read(0xfc) & ~1);
#else
    fake_power(on);
#endif
    powered = on;
    nor_qspi1_delay_us(on ? 10000 : 1000);
    return NOR_OK;
}

/* Derived from SDK flash_read_id for the plan's power-off diagnostic gate. */
int nor_qspi1_probe_off(uint8_t id[3])
{
    if (powered || !id) return NOR_EINVAL;
    powered = true;
    int rc = read_id(id);
    powered = false;
    return rc;
}

/* Derived from SDK gpio_init AF/input setup; DEV1.4 U11 uses normal GPIO
 * PB12 CLK, PB13 IO0, PB14 IO3, PB15 IO2, PC0 IO1 and PC1 CSN.
 */
static void configure_gpio(uint32_t function, bool alternate)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode = alternate ? GPIO_MODE_AF_PP : GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Alternate = function;
    gpio_init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_init(GPIOC, &gpio);
}

/* Derived from SDK gpio_init + flash_read_id; diagnostic boot scan only.
 * Send RDID exclusively until a nonzero/non-FF ID agrees on three reads.
 */
static int muxscan(void)
{
    for (uint32_t function = 8; function <= 15; ++function) {
        configure_gpio(function, true);
        int rc = controller_init();
        if (rc) {
            configure_gpio(0, false);
            printf("MUXSCAN func=0x%lX controller_rc=%d GPIO=input\r\n",
                   (unsigned long)function, rc);
            return rc;
        }
        uint8_t first[3] = {0};
        bool stable = true;
        for (unsigned sample = 0; sample < 3; ++sample) {
            uint8_t id[3] = {0};
            rc = read_id(id);
            printf("MUXSCAN func=0x%lX read=%u id=%02X%02X%02X rc=%d\r\n",
                   (unsigned long)function, sample + 1, id[0], id[1], id[2], rc);
            if (rc) { configure_gpio(0, false); return rc; }
            memcpy(port.info.id, id, sizeof id);
            if (!sample) memcpy(first, id, sizeof first);
            else if (memcmp(first, id, sizeof first)) stable = false;
            if (!memcmp(id, "\0\0\0", 3) || !memcmp(id, "\xff\xff\xff", 3))
                stable = false;
        }
        if (stable) {
            printf("MUXSCAN selected=0x%lX stable=3\r\n", (unsigned long)function);
            return NOR_OK;
        }
    }
    configure_gpio(0, false);
    printf("MUXSCAN no stable ID; GPIO=input\r\n");
    return NOR_EID;
}

/* Derived from SDK GPIO/QSPI initialization; no dedicated OSPI-pad routing. */
static int init(void)
{
    int rc;
    writes_allowed = identified = false;
    memset(&port.info, 0, sizeof port.info);
#ifndef NOR_HOST_TEST
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __SYSTEM_QSPI1_CLK_SELECT_SPLL();
    __SYSTEM_QSPI1_CLK_DIV(1);
    __SYSTEM_QSPI1_CLK_ENABLE();
#endif
    configure_gpio(0, false);
    last_cycles = cycles();
    if ((rc = power(true)) || (rc = muxscan())) return rc;
    /* No NOR reset/WREN/status writes in G1; allow an existing WIP to settle. */
    if ((rc = wait_ready(4500000)) || (rc = discover())) return rc;
    identified = true;
    return NOR_OK;
}

/* Derived from SDK flash_read/write range checks, using discovered capacity. */
static int range(uint32_t offset, size_t length)
{
    if (!identified) return NOR_ENOTREADY;
    return offset <= port.info.capacity && length <= port.info.capacity - offset
        ? NOR_OK : NOR_ERANGE;
}

/* Derived from SDK flash_read; uses bounded STIG reads pending DAC coherence proof. */
static int read_data(uint32_t offset, void *buffer, size_t length)
{
    int rc = range(offset, length);
    uint8_t *data = buffer;
    if (rc) return rc;
    if (!data && length) return NOR_EINVAL;
    if ((rc = wait_ready(4500000))) return rc;
    /* STIG 0x03 fallback: DAC cache invalidation is not documented in SDK. */
    while (length) {
        size_t n = length > 8 ? 8 : length;
        if ((rc = command(0x03, true, offset, 0, NULL, data, n))) return rc;
        offset += n; data += n; length -= n;
    }
    return NOR_OK;
}

/* Derived from SDK flash_protect_bit_set status inspection, without changing bits. */
int nor_qspi1_authorize_writes(void)
{
    uint8_t sr[2];
    writes_allowed = false;
    if (!identified) return NOR_ENOTREADY;
    int rc = nor_qspi1_status(sr);
    if (rc) return rc;
    /* Fail closed for BP, SRP, CMP, suspend, or absent/unpowered device. */
    if ((sr[0] & 0xfc) || (sr[1] & 0xc5)) return NOR_EPROTECTED;
    writes_allowed = true;
    return NOR_OK;
}

/* Derived from SDK flash_erase/flash_write WREN and command sequence. */
int nor_qspi1_start(uint32_t offset, uint32_t erase_size,
                    const uint8_t *data, size_t length)
{
    int rc = range(offset, erase_size ? erase_size : length);
    uint8_t opcode = 0x02, sr[2];
    if (rc) return rc;
    if (!writes_allowed) return NOR_ELOCKED;
    if (erase_size) {
        if (offset % erase_size) return NOR_EINVAL;
        opcode = 0;
        for (unsigned i = 0; i < 4; ++i)
            if (port.info.erase_size[i] == erase_size) opcode = port.info.erase_opcode[i];
        if (!opcode || data || length) return NOR_EINVAL;
    } else if (!data || !length || length > 8 ||
               length > 256 - (offset % 256)) return NOR_EINVAL;
    if ((rc = wait_ready(4500000)) || (rc = nor_qspi1_status(sr))) return rc;
    if ((sr[0] & 0xfc) || (sr[1] & 0xc5)) return NOR_EPROTECTED;
    if ((rc = command(0x06, false, 0, 0, NULL, NULL, 0))) return rc;
    if ((rc = command(0x05, false, 0, 0, NULL, sr, 1))) return rc;
    if (!(sr[0] & 2)) return NOR_EPROTECTED;
    return command(opcode, true, offset, 0, data, NULL, length);
}

/* Derived from SDK flash_write; adds page bounds, timeout and readback checks. */
static int program(uint32_t offset, const void *buffer, size_t length)
{
    const uint8_t *data = buffer;
    uint8_t old[8], verify[8];
    int rc = range(offset, length);
    if (rc) return rc;
    if (!data && length) return NOR_EINVAL;
    if (!writes_allowed) return NOR_ELOCKED;
    /* Preflight the whole request before changing any byte: refuse 0 -> 1. */
    for (size_t pos = 0; pos < length;) {
        size_t n = length - pos > 8 ? 8 : length - pos;
        if ((rc = read_data(offset + pos, old, n))) return rc;
        for (size_t i = 0; i < n; ++i)
            if ((old[i] & data[pos + i]) != data[pos + i]) return NOR_EVERIFY;
        pos += n;
    }
    while (length) {
        size_t n = 256 - offset % 256;
        if (n > 8) n = 8;
        if (n > length) n = length;
        uint32_t start = nor_qspi1_now_us();
        if ((rc = nor_qspi1_start(offset, 0, data, n)) ||
            (rc = wait_ready(6000))) return rc; /* max 4 ms * 1.5 */
        operation_us = nor_qspi1_now_us() - start;
        if ((rc = read_data(offset, verify, n))) return rc;
        if (memcmp(verify, data, n)) { writes_allowed = false; return NOR_EVERIFY; }
        offset += n; data += n; length -= n;
    }
    return NOR_OK;
}

/* Derived from SDK flash_erase; supports SFDP-selected erase types and verifies. */
static int erase(uint32_t offset, uint32_t length)
{
    uint8_t data[128];
    uint32_t start = nor_qspi1_now_us();
    int rc = nor_qspi1_start(offset, length, NULL, 0);
    if (rc) return rc;
    uint32_t timeout = length == 4096 ? 750000 : length == 32768 ? 2250000 : 4500000;
    if ((rc = wait_ready(timeout))) return rc;
    operation_us = nor_qspi1_now_us() - start;
    for (uint32_t pos = 0; pos < length; pos += sizeof data) {
        if ((rc = read_data(offset + pos, data, sizeof data))) return rc;
        for (size_t i = 0; i < sizeof data; ++i)
            if (data[i] != 0xff) { writes_allowed = false; return NOR_EVERIFY; }
    }
    return NOR_OK;
}

/* Derived from SDK flash_reset sequence (0x66/0x99), with bounded recovery. */
static int soft_reset(void)
{
    if (!identified || !writes_allowed) return NOR_ELOCKED;
    int rc = command(0x66, false, 0, 0, NULL, NULL, 0);
    if (!rc) rc = command(0x99, false, 0, 0, NULL, NULL, 0);
    writes_allowed = identified = false;
    nor_qspi1_delay_us(18000); /* max reset recovery 12 ms * 1.5. */
    return rc;
}

/* Derived from SDK peripheral-driver registration pattern for flash operations. */
struct nor_port *nor_qspi1_port(void)
{
    port.init = init; port.read_id = read_id; port.read_sfdp = read_sfdp;
    port.read = read_data; port.program = program; port.erase = erase;
    port.wait_ready = wait_ready; port.soft_reset = soft_reset; port.power = power;
    return &port;
}
