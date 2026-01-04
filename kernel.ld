/*
 * Linker script for 64-bit Linux 0.01 kernel
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(startup_32)

SECTIONS
{
    /* Kernel loaded at 64KB by bootloader (64-bit boot sector jumps here directly) */
    . = 0x10000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
    
    /* End of kernel - used for buffer allocation */
    end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
