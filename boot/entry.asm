[bits 64]
default rel

section .text
global boot_stage_entry
extern boot_main

boot_stage_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x90000
    call boot_main
.halt:
    hlt
    jmp .halt
