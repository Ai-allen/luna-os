%ifndef STAGE_SECTORS
%define STAGE_SECTORS 1
%endif

[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    in al, 0x92
    or al, 0x02
    out 0x92, al

    mov bx, STAGE_SECTORS
    mov word [dap_count], 0
    mov word [dap_offset], 0
    mov word [dap_segment], 0x1000
    mov dword [dap_lba], 1
    mov dword [dap_lba + 4], 0

load_stage:
    cmp bx, 0
    je stage_loaded
    mov ax, bx
    cmp ax, 127
    jbe .chunk_ready
    mov ax, 127
.chunk_ready:
    mov [dap_count], ax
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc load_fail

    mov ax, [dap_count]
    sub bx, ax
    add word [dap_lba], ax
    adc word [dap_lba + 2], 0
    adc word [dap_lba + 4], 0
    adc word [dap_lba + 6], 0

    mov cl, 5
    shl ax, cl
    add word [dap_segment], ax
    jmp load_stage

stage_loaded:

    xor ax, ax
    mov es, ax
    mov di, 0x1000
    mov cx, 0x1800
    rep stosw

    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0x0000
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0x0000
    mov dword [0x3000], 0x0083
    mov dword [0x3004], 0x0000

    lgdt [gdt_ptr]
    lidt [idt_ptr]

    mov eax, cr4
    or eax, 0x620
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x00000100
    wrmsr

    mov eax, 0x1000
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    jmp dword 0x08:0x10000

load_fail:
    hlt
    jmp load_fail

align 8, db 0
gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

idt_ptr:
    dw 0
    dd 0

gdt:
    dq 0
    dq 0x00209A0000000000
    dq 0x0000920000000000
gdt_end:

dap:
    db 0x10
    db 0x00
dap_count:
    dw STAGE_SECTORS
dap_offset:
    dw 0x0000
dap_segment:
    dw 0x1000
dap_lba:
    dq 0x0000000000000001

times 510 - ($ - $$) db 0
dw 0xAA55
