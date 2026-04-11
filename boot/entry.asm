[bits 64]
default rel

section .text
global boot_stage_entry
global boot_uefi_entry
extern boot_main

enable_boot_cpu:
    mov rax, cr0
    and rax, ~((1 << 2) | (1 << 3))
    or rax, (1 << 1) | (1 << 5)
    mov cr0, rax
    clts
    mov rax, cr4
    or rax, (1 << 9) | (1 << 10)
    mov cr4, rax
    fninit
    ret

boot_stage_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x90000
    and rsp, -16
    sub rsp, 32
    call enable_boot_cpu
    call boot_main
    add rsp, 32
.halt:
    hlt
    jmp .halt

boot_uefi_entry:
    mov rsp, 0x90000
    and rsp, -16
    sub rsp, 32
    call enable_boot_cpu
    call boot_main
    add rsp, 32
.uefi_halt:
    hlt
    jmp .uefi_halt
