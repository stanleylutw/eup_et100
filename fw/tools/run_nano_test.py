#!/usr/bin/env python3
"""唯 CPU／RAM 的 ARM nano 測試，不模擬 UART／NOR／實際 FR3068 硬體。"""
import argparse
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf",type=Path)
    args=parser.parse_args()
    cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
    cpu.mem_map(0x10000,0x100000)
    cpu.mem_map(0x20000000,0x100000)
    with args.elf.open("rb") as stream:
        elf=ELFFile(stream)
        symbols={s.name:s.entry.st_value for s in elf.get_section_by_name(".symtab").iter_symbols()}
        for segment in elf.iter_segments():
            if segment["p_type"]=="PT_LOAD":
                cpu.mem_write(segment["p_vaddr"],segment.data())
    cpu.reg_write(UC_ARM_REG_SP,0x200ff000)
    cpu.reg_write(UC_ARM_REG_LR,0x10001)
    failures={symbols[n]&~1 for n in ("abort","__assert_func","_exit","exit") if n in symbols}
    def hook(uc,address,size,context):
        if address in failures:
            raise RuntimeError(f"nano test assertion/exit at 0x{address:08x}")
    cpu.hook_add(UC_HOOK_CODE,hook)
    try:
        cpu.emu_start(symbols["main"]|1,0x10000,timeout=30_000_000,count=100_000_000)
    except UcError as error:
        raise SystemExit(f"nano emulation failed PC=0x{cpu.reg_read(UC_ARM_REG_PC):08x}: {error}")
    if cpu.reg_read(UC_ARM_REG_PC)!=0x10000 or cpu.reg_read(UC_ARM_REG_R0)!=0:
        raise SystemExit("nano test did not return success (or exceeded execution limit)")
    print("PASS ARM newlib-nano: real sensor_ota_job_uart + FLOW vectors/token/truncation and fake Sensor relay suite")


if __name__=="__main__":
    main()
