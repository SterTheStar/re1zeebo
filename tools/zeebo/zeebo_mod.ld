/* zeebo_mod.ld - Linker script for Qualcomm BREW / Zeebo .mod flat binary.
 * Places AEEMod_Load (.text.entry) at guest address 0x0 (ro-base = 0x0)
 * as expected by zeebx-emu and BREW's module loader.
 */
ENTRY(AEEMod_Load)

SECTIONS
{
    . = 0x00010000;

    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*)
    }

    .init_array : ALIGN(4) {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    .data : ALIGN(4) {
        *(.data .data.*)
    }

    .bss : ALIGN(4) {
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end__ = .;
    }

    /* Materialize zeroed .bss into the flat .mod binary so zeebx-emu maps
     * all static game buffers (which exceed MODULE_BSS_SLACK = 1 MiB). */
    .bss_pad : ALIGN(4) {
        LONG(0);
    }

    __exidx_start = .;
    __exidx_end = .;

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.comment)
        *(.note*)
    }
}
