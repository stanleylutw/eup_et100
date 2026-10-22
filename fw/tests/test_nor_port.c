/* Native tests use the real SDK register layout and the production backend.
 * Fake hardware replaces only MMIO side effects, cycles and IRQ primitives.
 */
#define NOR_HOST_TEST
#define __FR30XX_H__
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "driver_qspi.h"
#include "driver_gpio.h"

#undef GPIOB
#undef GPIOC
#define GPIOB (&fake_gpio_b)
#define GPIOC (&fake_gpio_c)
#define gpio_init fake_gpio_init

static struct qspi_regs_t fake_regs;
static GPIO_TypeDef fake_gpio_b, fake_gpio_c;
static uint32_t port_b_h_mux, port_c_l_mux, function_b, function_c;
static bool alternate_b, alternate_c, inputs_at_power_on;
static uint8_t mux_ids[8][3][3], opcode_log[64];
static unsigned id_reads[8];
static uint8_t memory[0x1000000], sfdp[256], sr1, sr2;
static uint32_t clock_cycles, irq_mask, commands, programs, erases, resets;
static bool stuck_idle, stuck_stig, stuck_wip, frozen_clock, bad_id, supply;
static uint32_t last_address;
static uint32_t address_bias;
static uint8_t required_dummy;
static bool diagnostic_only;
static bool timeout_dummy11;
static struct {
    uint8_t opcode, dummy, length, data[8];
    uint32_t address, control;
} trace[64];

static void fake_gpio_init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *cfg)
{
    assert(cfg->Pull == GPIO_PULLUP);
    assert(cfg->Mode == GPIO_MODE_AF_PP || cfg->Mode == GPIO_MODE_INPUT);
    bool alternate = cfg->Mode == GPIO_MODE_AF_PP;
    assert(!alternate || (cfg->Alternate >= 8 && cfg->Alternate <= 15));
    uint32_t function = alternate ? cfg->Alternate : 0;
    if (gpio == GPIOB) {
        assert(cfg->Pin == 0xf000);
        alternate_b = alternate; function_b = function;
        port_b_h_mux = (port_b_h_mux & 0xffffu) | function * 0x11110000u;
    } else {
        assert(gpio == GPIOC && cfg->Pin == 3);
        alternate_c = alternate; function_c = function;
        port_c_l_mux = (port_c_l_mux & 0xffffff00u) | function * 0x11u;
    }
}

/* Independent physical map from Stanley's read-only SWD scan, not the SDK. */
static bool fake_present(size_t offset)
{
    if (offset & 3) return false;
    return offset <= 0x14 || offset == 0x24 || offset == 0x28 ||
           (offset >= 0x30 && offset <= 0x44) ||
           (offset >= 0x50 && offset <= 0x58) ||
           offset == 0x90 || offset == 0x94 ||
           (offset >= 0xa0 && offset <= 0xac) ||
           offset == 0xb0 || offset == 0xb4 || offset == 0xfc;
}

static volatile void *fake_register(size_t offset)
{
    if (!fake_present(offset)) abort();
    return (volatile uint8_t *)&fake_regs + offset;
}

/* Catch abort in a child so expected faults do not create macOS crash reports. */
static void expected_abort(int signum) { _exit(128 + signum); }

static void test_absent_registers(void)
{
    _Static_assert(sizeof(struct qspi_regs_t) == 0x100, "SDK register layout changed");
    for (size_t offset = 0; offset < sizeof fake_regs; offset += 4) {
        if (fake_present(offset)) {
            assert(fake_register(offset) == (volatile uint8_t *)&fake_regs + offset);
            continue;
        }
        for (unsigned write = 0; write < 2; ++write) {
            pid_t pid = fork();
            assert(pid >= 0);
            if (!pid) {
                signal(SIGABRT, expected_abort);
                volatile uint32_t *word = fake_register(offset);
                if (write) *word = 1;
                else (void)*word;
                _exit(0);
            }
            int status;
            assert(waitpid(pid, &status, 0) == pid);
            assert(WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGABRT);
        }
    }
    puts("PASS: reduced QSPI1 map, absent-register read/write aborts (including 0x60/0x70)");
}

static uint32_t fake_cycles(void) { return frozen_clock ? clock_cycles : (clock_cycles += 9600); }
static uint32_t fake_irq_save(void) { uint32_t old = irq_mask; irq_mask = 1; return old; }
static void fake_irq_restore(uint32_t old) { irq_mask = old; }
static void fake_power(bool on)
{
    if (on) inputs_at_power_on = !alternate_b && !alternate_c;
    supply = on;
}
static void fake_reset(void)
{
    ++resets;
    memset(&fake_regs, 0, sizeof fake_regs);
    fake_regs.config.status = !stuck_idle;
}
static void fake_poll(bool stig)
{
    if (stig && stuck_stig) fake_regs.cmd_ctrl.progress_status = 1;
    if (!stig && stuck_idle) fake_regs.config.status = 0;
}
static void fake_execute(void)
{
    struct qspi_stig_reg_t c = fake_regs.cmd_ctrl;
    uint8_t bytes[8] = {0};
    size_t n = c.enable_write ? c.write_bytes + 1 : c.read_bytes + 1;
    uint32_t addr = fake_regs.cmd_address;
    uint32_t index = commands;
    if (commands < sizeof opcode_log) opcode_log[commands] = c.opcode;
    if (index < sizeof trace / sizeof trace[0]) {
        trace[index].opcode = c.opcode;
        trace[index].dummy = c.dummy_cycles;
        trace[index].length = n;
        trace[index].address = addr;
        memcpy(&trace[index].control, &c, sizeof c);
    }
    ++commands;
    if (diagnostic_only) {
        assert(c.opcode == 0x9f || c.opcode == 0x90 || c.opcode == 0x5a);
        assert(c.enable_read && !c.enable_write);
    }
    assert(irq_mask == 1);
    assert(n <= 8);
    if (stuck_stig) { fake_regs.cmd_ctrl.progress_status = 1; return; }
    if (timeout_dummy11 && c.opcode == 0x5a && c.dummy_cycles == 11) {
        fake_regs.cmd_ctrl.progress_status = 1;
        return;
    }
    if (c.enable_cmd_addr) {
        assert(c.addr_bytes == 2);
        assert(addr <= 0xffffff);
        last_address = addr;
    }
    if (!supply) {
        memset(bytes, 0xff, sizeof bytes);
    } else if (!alternate_b || !alternate_c || function_b != function_c) {
        memset(bytes, 0, sizeof bytes);
    } else switch (c.opcode) {
    case 0x9f: {
        unsigned index = function_b - 8;
        memcpy(bytes, mux_ids[index][id_reads[index]++ % 3], 3);
        if (bad_id) bytes[0] = 0;
        break;
    }
    case 0x90:
        assert(c.enable_cmd_addr && c.dummy_cycles == 0 && n == 2);
        bytes[0] = ((addr + address_bias) & 1) ? 0x17 : 0xc8;
        bytes[1] = ((addr + address_bias) & 1) ? 0xc8 : 0x17;
        break;
    case 0x5a:
        assert(c.enable_cmd_addr);
        for (size_t i = 0; i < n; ++i)
            bytes[i] = c.dummy_cycles == required_dummy
                ? sfdp[(addr + address_bias + i) % sizeof sfdp] : 0xa5;
        break;
    case 0x05: bytes[0] = sr1 | (stuck_wip ? 1 : 0); break;
    case 0x35: bytes[0] = sr2; break;
    case 0x03:
        assert(c.dummy_cycles == 0);
        assert(addr + n <= sizeof memory); memcpy(bytes, memory + addr, n); break;
    case 0x06: sr1 |= 2; break;
    case 0x02: {
        uint32_t words[2] = {fake_regs.write_data_L, fake_regs.write_data_H};
        memcpy(bytes, words, n);
        assert(sr1 & 2);
        assert(addr / 256 == (addr + n - 1) / 256);
        for (size_t i = 0; i < n; ++i) memory[addr + i] &= bytes[i];
        ++programs; sr1 &= ~2; break;
    }
    case 0x20: case 0x52: case 0xd8: {
        size_t size = c.opcode == 0x20 ? 4096 : c.opcode == 0x52 ? 32768 : 65536;
        assert(sr1 & 2); assert(addr % size == 0);
        memset(memory + addr, 0xff, size); ++erases; sr1 &= ~2; break;
    }
    case 0x66: case 0x99: break;
    default: assert(!"unexpected/forbidden opcode");
    }
    uint32_t words[2] = {0};
    memcpy(words, bytes, 8);
    if (index < sizeof trace / sizeof trace[0]) memcpy(trace[index].data, bytes, 8);
    fake_regs.read_data_L = words[0]; fake_regs.read_data_H = words[1];
    fake_regs.cmd_ctrl.progress_status = 0;
}

#include "../src/nor_qspi1.c"

static void fixture(void)
{
    memset(memory, 0xff, sizeof memory);
    memset(sfdp, 0, sizeof sfdp);
    memcpy(sfdp, "SFDP", 4); sfdp[4] = 6; sfdp[5] = 1;
    sfdp[8] = 0; sfdp[9] = 6; sfdp[10] = 1;
    sfdp[11] = 9; sfdp[12] = 0x40; sfdp[15] = 0xff;
    sfdp[0x44] = 0xff; sfdp[0x45] = 0xff; sfdp[0x46] = 0xff; sfdp[0x47] = 7;
    sfdp[0x5c] = 12; sfdp[0x5d] = 0x20;
    sfdp[0x5e] = 15; sfdp[0x5f] = 0x52;
    sfdp[0x60] = 16; sfdp[0x61] = 0xd8;
    stuck_idle = stuck_stig = stuck_wip = frozen_clock = bad_id = false;
    alternate_b = alternate_c = inputs_at_power_on = false;
    port_b_h_mux = 0x1234abcd;
    port_c_l_mux = 0xa5a55a5a;
    memset(mux_ids, 0, sizeof mux_ids);
    memset(mux_ids[1], 0xff, sizeof mux_ids[1]);
    for (unsigned i = 0; i < 3; ++i) memcpy(mux_ids[2][i], "\xc8\x60\x18", 3);
    memset(id_reads, 0, sizeof id_reads);
    memset(opcode_log, 0, sizeof opcode_log);
    memset(trace, 0, sizeof trace);
    address_bias = 0; required_dummy = 1; diagnostic_only = false;
    timeout_dummy11 = false;
    sr1 = sr2 = 0; irq_mask = 0;
    commands = programs = erases = 0;
}

static void test_muxscan(void)
{
    struct nor_port *p = nor_qspi1_port();
    fixture();
    memset(mux_ids[2], 0, sizeof mux_ids[2]);
    assert(p->init() == NOR_EID);
    assert(!alternate_b && !alternate_c && !identified && !writes_allowed);
    assert((port_b_h_mux & 0xffff0000u) == 0 && (port_b_h_mux & 0xffffu) == 0xabcd);
    assert((port_c_l_mux & 0xffu) == 0 && (port_c_l_mux & 0xffffff00u) == 0xa5a55a00);
    assert(commands == 24 && programs == 0 && erases == 0);
    for (unsigned i = 0; i < 8; ++i) assert(id_reads[i] == 3);
    for (unsigned i = 0; i < commands; ++i) assert(opcode_log[i] == 0x9f);

    fixture();
    memcpy(mux_ids[0][0], "\xc8\x60\x18", 3);
    memcpy(mux_ids[0][1], "\xc8\x60\x18", 3);
    assert(p->init() == NOR_OK); /* Third read is zero: reject unstable func 8. */
    assert(id_reads[0] == 3 && function_b == 10 && function_c == 10);
    assert(id_reads[3] == 0 && programs == 0 && erases == 0);

    fixture();
    for (unsigned i = 0; i < 3; ++i) memcpy(mux_ids[0][i], "\xef\x40\x18", 3);
    assert(p->init() == NOR_EID); /* Stop at stable unknown ID, do not scan on. */
    assert(function_b == 8 && id_reads[0] == 4 && id_reads[1] == 0);
    assert(!memcmp(p->info.id, "\xef\x40\x18", 3));

    fixture();
    memset(mux_ids[2], 0, sizeof mux_ids[2]);
    for (unsigned i = 0; i < 3; ++i) memcpy(mux_ids[7][i], "\xc8\x60\x18", 3);
    assert(p->init() == NOR_OK && function_b == 15 && function_c == 15);
    assert(id_reads[7] == 4);

    fixture(); stuck_stig = true;
    assert(p->init() == NOR_ETIMEOUT);
    assert(!alternate_b && !alternate_c && id_reads[1] == 0);
    stuck_stig = false;
    assert(p->init() == NOR_OK);
    puts("PASS: RDID-only muxscan 8..F, three stable samples, no-match input restore, unknown ID stop, timeout");
}

static void test_address_diagnostics(void)
{
    struct nor_port *p = nor_qspi1_port();
    const uint32_t addresses[] = {0, 1, 0x16, 0x30};
    for (unsigned model = 0; model < 3; ++model) {
        fixture(); assert(p->init() == NOR_OK);
        assert(nor_qspi1_authorize_writes() == NOR_OK);
        address_bias = model == 1 ? 0x16 : 0;
        required_dummy = model == 2 ? 12 : 8;
        /* Diagnostics must work even when G1 has not validated geometry. */
        identified = false; port.info.capacity = port.info.page_size = 0;
        sr1 = 0x1c; sr2 = 0x40;
        commands = 0; diagnostic_only = true;
        assert(nor_qspi1_diag_addr() == NOR_OK);
        assert(commands == 24 && !writes_allowed && !identified);
        assert(port.info.capacity == 0 && port.info.page_size == 0);
        assert(programs == 0 && erases == 0 && sr1 == 0x1c && sr2 == 0x40);
        assert(trace[0].opcode == 0x9f);
        for (unsigned i = 1; i <= 2; ++i) {
            assert(trace[i].opcode == 0x90 && trace[i].address == i - 1);
            assert(trace[i].dummy == 0 && trace[i].length == 2);
            assert(!memcmp(trace[i].data, i == 1 ? "\xc8\x17" : "\x17\xc8", 2));
        }
        for (unsigned i = 0; i < 4; ++i) {
            unsigned at = i + 3;
            assert(trace[at].opcode == 0x5a && trace[at].address == addresses[i]);
            assert(trace[at].dummy == 8 && trace[at].length == 1);
            uint8_t expected = required_dummy == 8
                ? sfdp[(addresses[i] + address_bias) % sizeof sfdp] : 0xa5;
            assert(trace[at].data[0] == expected);
            assert((trace[at].control & ~3u) == 0x5a8a0400u);
        }
        for (unsigned dummy = 0; dummy <= 16; ++dummy) {
            unsigned at = 7 + dummy;
            assert(trace[at].opcode == 0x5a && trace[at].address == 0);
            assert(trace[at].dummy == dummy && trace[at].length == 8);
            /* Golden word independently checks SDK's zero-based counts. */
            assert((trace[at].control & ~3u) == (0x5afa0000u | dummy << 7));
            for (unsigned i = 0; i < 8; ++i)
                assert(trace[at].data[i] == (dummy == required_dummy
                    ? sfdp[(address_bias + i) % sizeof sfdp] : 0xa5));
        }
    }
    fixture(); assert(p->init() == NOR_OK);
    commands = 0; diagnostic_only = true; bad_id = true;
    assert(nor_qspi1_diag_addr() == NOR_EID && commands == 1);
    bad_id = false; stuck_stig = true; commands = 0;
    assert(nor_qspi1_diag_addr() == NOR_ETIMEOUT && commands == 1);
    assert(!writes_allowed && !identified);
    stuck_stig = false;
    assert(p->power(false) == NOR_OK);
    commands = 0;
    assert(nor_qspi1_diag_addr() == NOR_ENOTREADY && commands == 0);
    puts("PASS: diag-addr fixed reads/counts, +0x16 address model, dummy 0..16, golden encoding, no mutations, error stop");
}

static void test_dummy_timeout_recovery(void)
{
    struct nor_port *p = nor_qspi1_port();
    fixture();
    assert(p->init() == NOR_OK);
    assert(p->info.capacity == sizeof memory);
    for (unsigned i = 0; i < commands; ++i)
        if (trace[i].opcode == 0x5a) assert(trace[i].dummy == 1);
    timeout_dummy11 = true;
    commands = 0;
    unsigned before = resets;
    diagnostic_only = true;
    irq_mask = 1; frozen_clock = true;
    assert(nor_qspi1_diag_addr() == NOR_ETIMEOUT);
    assert(commands == 19 && trace[18].opcode == 0x5a && trace[18].dummy == 11);
    assert(resets == before + 1 && !identified && !writes_allowed && irq_mask == 1);
    assert(programs == 0 && erases == 0 && sr1 == 0 && sr2 == 0);
    diagnostic_only = false;
    assert(p->init() == NOR_OK);
    assert(p->info.capacity == sizeof memory && identified && !writes_allowed);
    assert(irq_mask == 1 && programs == 0 && erases == 0);
    puts("PASS: SFDP raw=1, real-command dummy=11 timeout, IRQ-disabled/stalled-DWT recovery and next init");
}

int main(void)
{
    test_absent_registers();
    test_muxscan();
    test_address_diagnostics();
    test_dummy_timeout_recovery();
    fixture();
    struct nor_port *p = nor_qspi1_port();
    assert(p->init() == NOR_OK);
    assert(inputs_at_power_on && alternate_b && alternate_c);
    assert(port_b_h_mux == 0xaaaaabcd && port_c_l_mux == 0xa5a55aaa);
    assert(id_reads[0] == 3 && id_reads[1] == 3 && id_reads[2] == 4);
    for (unsigned i = 0; i < 9; ++i) assert(opcode_log[i] == 0x9f);
    assert(opcode_log[9] == 0x05 && opcode_log[10] == 0x9f);
    puts("PASS: six GPIO AF/pull-up requests, unrelated mux bits preserved, scan precedes RDSR/SFDP");
    assert(p->info.capacity == sizeof memory);
    assert(programs == 0 && erases == 0 && sr1 == 0);
    uint8_t pattern[513], got[513];
    for (size_t i = 0; i < sizeof pattern; ++i) pattern[i] = i * 37;
    assert(p->program(0, pattern, 1) == NOR_ELOCKED);
    assert(nor_qspi1_authorize_writes() == NOR_OK);
    assert(p->program(253, pattern, sizeof pattern) == NOR_OK);
    assert(p->read(253, got, sizeof got) == NOR_OK && !memcmp(pattern, got, sizeof got));
    uint8_t ones = 0xff;
    uint32_t before = programs;
    assert(p->program(254, &ones, 1) == NOR_EVERIFY && programs == before);
    assert(p->read(0xffffff, got, 1) == NOR_OK && last_address == 0xffffff);
    assert(p->read(0x1000000, got, 0) == NOR_OK);
    assert(p->read(0xffffff, got, 2) == NOR_ERANGE);
    assert(p->program(UINT32_MAX, pattern, SIZE_MAX) == NOR_ERANGE);
    assert(p->erase(1, 4096) == NOR_EINVAL);
    assert(p->erase(0, 8192) == NOR_EINVAL);
    assert(p->erase(0xfff000, 65536) == NOR_ERANGE);
    assert(p->erase(0, 4096) == NOR_OK);
    assert(p->erase(0x800000, 32768) == NOR_OK);
    assert(p->erase(0xff0000, 65536) == NOR_OK);
    sr1 = 0x04;
    assert(p->erase(0, 4096) == NOR_EPROTECTED);
    assert(nor_qspi1_authorize_writes() == NOR_EPROTECTED);
    assert(p->erase(0, 4096) == NOR_ELOCKED);
    sr1 = 0;
    assert(nor_qspi1_authorize_writes() == NOR_OK);
    sr2 = 0x04;
    assert(nor_qspi1_authorize_writes() == NOR_EPROTECTED);
    assert(!writes_allowed);
    sr2 = 0;
    puts("PASS: G1 read-only, SFDP, boundaries, page splitting, erase geometry/protection");

    for (unsigned mode = 0; mode < 3; ++mode) {
        fixture(); assert(p->init() == NOR_OK);
        irq_mask = 1;
        uint32_t reset_before = resets;
        stuck_idle = mode != 1; stuck_stig = mode == 1; frozen_clock = mode == 2;
        assert(p->read_id(got) == NOR_ETIMEOUT);
        assert(resets > reset_before && irq_mask == 1);
        assert(!writes_allowed && !identified);
        stuck_idle = stuck_stig = frozen_clock = false;
        assert(p->init() == NOR_OK && irq_mask == 1);
        assert(p->read_id(got) == NOR_OK);
        assert(p->program(0, pattern, 1) == NOR_ELOCKED);
    }
    stuck_idle = true;
    assert(p->init() == NOR_ETIMEOUT);
    stuck_idle = false;
    assert(p->init() == NOR_OK);
    stuck_wip = true;
    assert(p->wait_ready(500) == NOR_ETIMEOUT);
    stuck_wip = false;
    assert(p->init() == NOR_OK);
    frozen_clock = stuck_wip = true;
    assert(p->wait_ready(50) == NOR_ETIMEOUT);
    frozen_clock = stuck_wip = false;
    assert(p->init() == NOR_OK);
    sr1 = 0xff;
    assert(p->wait_ready(500) == NOR_ENOTREADY);
    sr1 = 0;
    assert(p->init() == NOR_OK);
    puts("PASS: idle/STIG/init/WIP timeouts, stalled DWT, IRQ-mask restore, next init");

    bad_id = true; assert(p->init() == NOR_EID);
    bad_id = false; sfdp[0] = 0;
    assert(p->init() == NOR_ESFDP);
    fixture(); sfdp[0x5d] = 0xd8;
    assert(p->init() == NOR_ESFDP && !identified);
    fixture(); sfdp[0x47] = 0x80; sfdp[0x44] = 27;
    sfdp[0x45] = sfdp[0x46] = 0;
    assert(p->init() == NOR_OK);
    sfdp[0x44] = 31;
    assert(p->init() == NOR_ESFDP);
    fixture(); assert(p->init() == NOR_OK);
    assert(p->power(false) == NOR_OK);
    assert(p->read_id(got) == NOR_ENOTREADY);
    assert(nor_qspi1_probe_off(got) == NOR_OK);
    assert(got[0] == 0xff && got[1] == 0xff && got[2] == 0xff);
    assert(p->init() == NOR_OK);
    clock_cycles = UINT32_MAX - 500;
    assert(p->read_id(got) == NOR_OK);
    clock_cycles = UINT32_MAX - 500;
    stuck_stig = true;
    assert(p->read_id(got) == NOR_ETIMEOUT);
    stuck_stig = false;
    assert(p->init() == NOR_OK);
    puts("PASS: invalid ID/SFDP, density exponent, power gate, cycle wrap");
    return 0;
}
