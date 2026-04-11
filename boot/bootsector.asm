; LunaOS Stage 0 bootsector.
; Its only job is to load LunaLoader Stage 1 from disk and transfer control.

%ifndef LUNALOADER_SECTORS
%define LUNALOADER_SECTORS 1
%endif

%define LUNALOADER_SEGMENT 0x0800
%define LUNALOADER_OFFSET  0x0000
%define LUNALOADER_ENTRY   0x8000

[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [stage0_boot_drive], dl

    mov word [dap_count], LUNALOADER_SECTORS
    mov word [dap_offset], LUNALOADER_OFFSET
    mov word [dap_segment], LUNALOADER_SEGMENT
    mov dword [dap_lba], 1
    mov dword [dap_lba + 4], 0

    mov si, dap
    mov ah, 0x42
    int 0x13
    jc load_fail

    mov dl, [stage0_boot_drive]
    jmp 0x0000:LUNALOADER_ENTRY

load_fail:
    hlt
    jmp load_fail

align 8, db 0
dap:
    db 0x10
    db 0x00
dap_count:
    dw LUNALOADER_SECTORS
dap_offset:
    dw LUNALOADER_OFFSET
dap_segment:
    dw LUNALOADER_SEGMENT
dap_lba:
    dq 1
stage0_boot_drive:
    db 0

times 510 - ($ - $$) db 0
dw 0xAA55
