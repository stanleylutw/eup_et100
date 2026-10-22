/* Project-owned relocation; vendor linker remains unchanged. */
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08020200, LENGTH = 0x1bfe00
    PRAM (xrw) : ORIGIN = 0x1ffe0000, LENGTH = 128K
    SRAM (rw) : ORIGIN = 0x20000000, LENGTH = 512K
}
INCLUDE sections.ld
