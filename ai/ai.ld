ENTRY(ai_entry_boot)
SECTIONS
{
    . = 0;
    .text : { *(.text*) *(.rdata*) }
    .data : { *(.data*) *(.bss*) *(COMMON) }
}
