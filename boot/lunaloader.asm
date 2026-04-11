; LunaLoader Stage 1 for BIOS boot.
; Stage 0 loads this binary in low memory.
; LunaLoader reads the full LunaOS Stage 2 image into high memory,
; prepares long mode, and jumps to the Stage 2 entry point.

%ifndef STAGE_LBA
%define STAGE_LBA 2
%endif

%ifndef STAGE_SECTORS
%define STAGE_SECTORS 1
%endif

%ifndef STAGE_LOAD_ADDR
%define STAGE_LOAD_ADDR 0x200000
%endif

%ifndef STAGE_ENTRY_OFFSET
%define STAGE_ENTRY_OFFSET 0
%endif

%ifndef STAGE_PD_ENTRY_COUNT
%define STAGE_PD_ENTRY_COUNT 2
%endif

%define BOUNCE_BUFFER_ADDR 0x10000

[org 0x8000]
[bits 16]

lunaloader_stage1_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    mov [boot_drive], dl

    call serial_init
    mov si, msg_lunaloader_enter
    call serial_write

    in al, 0x92
    or al, 0x02
    out 0x92, al

    mov word [remaining_sectors], STAGE_SECTORS
    mov dword [dest_ptr], STAGE_LOAD_ADDR
    mov dword [dap_lba], STAGE_LBA
    mov dword [dap_lba + 4], 0

load_stage:
    cmp word [remaining_sectors], 0
    je stage_loaded

    mov ax, [remaining_sectors]
    cmp ax, 127
    jbe .chunk_ready
    mov ax, 127
.chunk_ready:
    mov [dap_count], ax
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc load_fail

    mov si, msg_lunaloader_read
    call serial_write
    movzx ecx, word [dap_count]
    shl ecx, 9
    mov [copy_bytes], ecx
    call copy_chunk_to_stage
    mov si, msg_lunaloader_copy
    call serial_write

    movzx eax, word [dap_count]
    sub word [remaining_sectors], ax
    shl eax, 9
    add dword [dest_ptr], eax

    movzx eax, word [dap_count]
    add dword [dap_lba], eax
    adc dword [dap_lba + 4], 0
    jmp load_stage

stage_loaded:
    mov si, msg_lunaloader_loaded
    call serial_write
    cli
    xor eax, eax
    mov edi, 0x1000
    mov ecx, (3 * 4096) / 4
    o32 rep stosd

    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0x00000000
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0x00000000

    mov edi, 0x3000
    mov eax, 0x00000083
    mov ecx, STAGE_PD_ENTRY_COUNT
.map_pages:
    mov dword [edi], eax
    mov dword [edi + 4], 0x00000000
    add eax, 0x00200000
    add edi, 8
    loop .map_pages

    lgdt [gdt_ptr]
    lidt [idt_ptr]

    mov eax, cr4
    or eax, 0x20
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

    jmp dword 0x18:long_mode_entry

load_fail:
    mov si, msg_lunaloader_fail
    call serial_write
    hlt
    jmp load_fail

serial_init:
    mov dx, 0x3F8 + 1
    xor al, al
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8 + 0
    mov al, 0x01
    out dx, al
    mov dx, 0x3F8 + 1
    xor al, al
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x03
    out dx, al
    mov dx, 0x3F8 + 2
    mov al, 0xC7
    out dx, al
    mov dx, 0x3F8 + 4
    mov al, 0x0B
    out dx, al
    ret

serial_write:
    lodsb
    test al, al
    jz .done
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, 0x3F8
    mov al, [si - 1]
    out dx, al
    jmp serial_write
.done:
    ret

copy_chunk_to_stage:
    cli
    lgdt [gdt_ptr]
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
    jmp 0x08:protected_copy_entry

protected_copy_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    o32 mov esi, BOUNCE_BUFFER_ADDR
    o32 mov edi, [dest_ptr]
    o32 mov ecx, [copy_bytes]
    o32 mov eax, ecx
    o32 shr ecx, 2
    a32 o32 rep movsd
    o32 mov ecx, eax
    o32 and ecx, 3
    a32 rep movsb
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax
    jmp 0x0000:protected_copy_return

[bits 64]
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov eax, STAGE_LOAD_ADDR + STAGE_ENTRY_OFFSET
    jmp rax

[bits 16]
protected_copy_return:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ret

 [bits 16]
align 8, db 0
gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

idt_ptr:
    dw 0
    dd 0

gdt:
    dq 0
    dq 0x00009A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00209A0000000000
gdt_end:

boot_drive:
    db 0
remaining_sectors:
    dw 0
dest_ptr:
    dd 0
copy_bytes:
    dd 0
msg_lunaloader_enter:
    db "[LunaLoader] Stage 1 online", 13, 10, 0
msg_lunaloader_read:
    db "[LunaLoader] Stage 1 read", 13, 10, 0
msg_lunaloader_copy:
    db "[LunaLoader] Stage 1 copy", 13, 10, 0
msg_lunaloader_loaded:
    db "[LunaLoader] Stage 2 loaded", 13, 10, 0
msg_lunaloader_fail:
    db "[LunaLoader] Stage 1 fail", 13, 10, 0

align 8, db 0
dap:
    db 0x10
    db 0x00
dap_count:
    dw 0
    dw 0x0000
    dw 0x1000
dap_lba:
    dq 0
