OUTPUT_FORMAT(pei-x86-64)
ENTRY(lifecycle_entry_boot)

SECTIONS
{
  . = 0;

  .text ALIGN(16) :
  {
    *(.text .text.*)
  }

  .rdata ALIGN(16) :
  {
    *(.rdata .rdata$* .rodata .rodata.*)
  }

  .data ALIGN(16) :
  {
    *(.data .data.*)
  }

  .bss ALIGN(16) :
  {
    *(.bss .bss.*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.eh_frame .eh_frame.*)
    *(.debug*)
    *(.comment)
  }
}
