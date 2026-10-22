ENTRY(Reset_Handler)
_Min_Heap_Size = 0x1000;
SECTIONS
{
    .isr_vector : { . = ALIGN(8); KEEP(*(.isr_vector)) . = ALIGN(8); } >FLASH
    .text : {
        . = ALIGN(8);
        *(.text .text.*) *(.glue_7) *(.glue_7t) *(.eh_frame)
        KEEP(*(.init)) KEEP(*(.fini))
        . = ALIGN(8); _etext = .;
    } >FLASH
    .rodata : { . = ALIGN(8); KEEP(*(.eup_fw_info)) *(.rodata .rodata.*) . = ALIGN(8); } >FLASH
    .ARM.extab : { . = ALIGN(8); *(.ARM.extab* .gnu.linkonce.armextab.*) . = ALIGN(8); } >FLASH
    .ARM : {
        . = ALIGN(8); __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; . = ALIGN(8);
    } >FLASH
    .preinit_array : {
        . = ALIGN(8); PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); . = ALIGN(8);
    } >FLASH
    .init_array : {
        . = ALIGN(8); PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .); . = ALIGN(8);
    } >FLASH
    .fini_array : {
        . = ALIGN(8); PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .); . = ALIGN(8);
    } >FLASH
    _siram_code_front = LOADADDR(.ram_code_front);
    .ram_code_front : {
        . = ALIGN(8); _sram_code_front = .;
        *(.ram_code_front.*) *(ram_code_front*)
        . = ALIGN(8); _eram_code_front = .;
    } >PRAM AT>FLASH
    _siram_code = LOADADDR(.ram_code);
    .ram_code : {
        . = ALIGN(8); _sram_code = .;
        *(.ram_code.*) *(ram_code*)
        . = ALIGN(8); _eram_code = .;
    } >PRAM AT>FLASH
    .share_mem (NOLOAD) : { . = ALIGN(8); *(share_mem) } >SRAM
    _sidata = LOADADDR(.data);
    .data : {
        . = ALIGN(8); _sdata = .; *(.data .data.*) . = ALIGN(8); _edata = .;
    } >SRAM AT>FLASH
    .bss (NOLOAD) : {
        _sbss = .; __bss_start__ = _sbss;
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(8); _ebss = .; __bss_end__ = _ebss;
    } >SRAM
    ._user_heap : {
        . = ALIGN(8); PROVIDE(_heap_start = .);
        . += _Min_Heap_Size; PROVIDE(_heap_end = .); . = ALIGN(8);
    } >SRAM
}
ASSERT(SIZEOF(.isr_vector) <= 0x200, "vector table exceeds 512-byte alignment")
