ENTRY(efi_main)

SECTIONS
{
    . = 0;
    .text : { *(.text*) }
    .rdata : { *(.rdata*) *(.rodata*) }
    .data : { *(.data*) }
    .pdata : { *(.pdata*) }
    .xdata : { *(.xdata*) }
    .bss : { *(.bss*) *(COMMON) }
}
