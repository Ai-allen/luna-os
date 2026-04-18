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

%assign STAGE_TOTAL_BYTES (STAGE_SECTORS * 512)
%assign STAGE_TOTAL_CHUNKS ((STAGE_SECTORS + 126) / 127)

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
    mov word [total_chunk_count], STAGE_TOTAL_CHUNKS
    mov word [chunk_index], 0
    mov dword [loaded_bytes], 0
    mov dword [total_stage_bytes], STAGE_TOTAL_BYTES
    mov dword [dest_ptr], STAGE_LOAD_ADDR
    mov dword [dap_lba], STAGE_LBA
    mov dword [dap_lba + 4], 0
    call log_stage_plan

load_stage:
    cmp word [remaining_sectors], 0
    je stage_loaded

    mov ax, [remaining_sectors]
    cmp ax, 127
    jbe .chunk_ready
    mov ax, 127
.chunk_ready:
    mov [dap_count], ax
    mov byte [current_chunk_final], 0
    cmp word [remaining_sectors], 127
    ja .not_final
    mov byte [current_chunk_final], 1
.not_final:
    movzx ecx, word [dap_count]
    shl ecx, 9
    mov [copy_bytes], ecx
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc load_fail

    inc word [chunk_index]
    call log_stage_read_progress
    call copy_chunk_to_stage
    call log_stage_copy_progress

    movzx eax, word [dap_count]
    sub word [remaining_sectors], ax
    shl eax, 9
    add dword [loaded_bytes], eax
    add dword [dest_ptr], eax

    movzx eax, word [dap_count]
    add dword [dap_lba], eax
    adc dword [dap_lba + 4], 0
    jmp load_stage

stage_loaded:
    call log_stage_loaded
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
    call serial_write_char
    jmp serial_write
.done:
    ret

serial_write_char:
    push bx
    push dx
    mov bl, al
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, 0x3F8
    mov al, bl
    out dx, al
    pop dx
    pop bx
    ret

serial_write_hex_nibble:
    and al, 0x0F
    cmp al, 10
    jb .digit
    add al, 'A' - 10
    jmp serial_write_char
.digit:
    add al, '0'
    jmp serial_write_char

serial_write_hex16:
    push ax
    push cx
    mov cx, 4
.loop:
    rol ax, 4
    push ax
    and al, 0x0F
    call serial_write_hex_nibble
    pop ax
    loop .loop
    pop cx
    pop ax
    ret

serial_write_hex32:
    o32 push eax
    push cx
    mov cx, 8
.loop:
    o32 rol eax, 4
    o32 push eax
    and al, 0x0F
    call serial_write_hex_nibble
    o32 pop eax
    loop .loop
    pop cx
    o32 pop eax
    ret

serial_write_bool:
    cmp byte [current_chunk_final], 0
    je .no
    mov si, msg_bool_yes
    jmp serial_write
.no:
    mov si, msg_bool_no
    jmp serial_write

log_stage_chunk_progress_core:
    push ax
    push si
    mov ax, [chunk_index]
    call serial_write_hex16
    mov si, msg_chunk_sep
    call serial_write
    mov ax, [total_chunk_count]
    call serial_write_hex16
    mov si, msg_chunk_offset
    call serial_write
    mov eax, [loaded_bytes]
    call serial_write_hex32
    mov si, msg_chunk_size
    call serial_write
    mov eax, [copy_bytes]
    call serial_write_hex32
    mov si, msg_chunk_total
    call serial_write
    mov eax, [total_stage_bytes]
    call serial_write_hex32
    mov si, msg_chunk_final
    call serial_write
    call serial_write_bool
    pop si
    pop ax
    ret

log_stage_plan:
    mov si, msg_lunaloader_plan
    call serial_write
    mov ax, [total_chunk_count]
    call serial_write_hex16
    mov si, msg_plan_bytes
    call serial_write
    mov eax, [total_stage_bytes]
    call serial_write_hex32
    mov si, msg_plan_entry
    call serial_write
    mov eax, STAGE_LOAD_ADDR + STAGE_ENTRY_OFFSET
    call serial_write_hex32
    mov si, msg_newline
    jmp serial_write

log_stage_read_progress:
    mov si, msg_lunaloader_read
    call serial_write
    call log_stage_chunk_progress_core
    mov si, msg_chunk_lba
    call serial_write
    mov eax, [dap_lba]
    call serial_write_hex32
    mov si, msg_newline
    jmp serial_write

log_stage_copy_progress:
    mov si, msg_lunaloader_copy
    call serial_write
    call log_stage_chunk_progress_core
    mov si, msg_chunk_dest
    call serial_write
    mov eax, [dest_ptr]
    call serial_write_hex32
    mov si, msg_newline
    jmp serial_write

log_stage_loaded:
    mov si, msg_lunaloader_loaded
    call serial_write
    mov ax, [chunk_index]
    call serial_write_hex16
    mov si, msg_chunk_sep
    call serial_write
    mov ax, [total_chunk_count]
    call serial_write_hex16
    mov si, msg_loaded_bytes
    call serial_write
    mov eax, [loaded_bytes]
    call serial_write_hex32
    mov si, msg_plan_entry
    call serial_write
    mov eax, STAGE_LOAD_ADDR + STAGE_ENTRY_OFFSET
    call serial_write_hex32
    mov si, msg_newline
    jmp serial_write

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
total_chunk_count:
    dw 0
chunk_index:
    dw 0
current_chunk_final:
    db 0
    db 0
loaded_bytes:
    dd 0
total_stage_bytes:
    dd 0
dest_ptr:
    dd 0
copy_bytes:
    dd 0
msg_lunaloader_enter:
    db "[LunaLoader] Stage 1 online", 13, 10, 0
msg_lunaloader_plan:
    db "[LunaLoader] Stage 1 plan chunks=", 0
msg_lunaloader_read:
    db "[LunaLoader] Stage 1 read chunk=", 0
msg_lunaloader_copy:
    db "[LunaLoader] Stage 1 copy chunk=", 0
msg_lunaloader_loaded:
    db "[LunaLoader] Stage 2 loaded ok chunks=", 0
msg_lunaloader_fail:
    db "[LunaLoader] Stage 1 fail", 13, 10, 0
msg_chunk_sep:
    db "/", 0
msg_chunk_offset:
    db " offset=0x", 0
msg_chunk_size:
    db " size=0x", 0
msg_chunk_total:
    db " total=0x", 0
msg_chunk_final:
    db " final=", 0
msg_chunk_lba:
    db " lba=0x", 0
msg_chunk_dest:
    db " dest=0x", 0
msg_plan_bytes:
    db " total_bytes=0x", 0
msg_plan_entry:
    db " entry=0x", 0
msg_loaded_bytes:
    db " total_bytes=0x", 0
msg_bool_yes:
    db "yes", 0
msg_bool_no:
    db "no", 0
msg_newline:
    db 13, 10, 0

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
