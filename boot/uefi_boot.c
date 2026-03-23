#include <stdint.h>

typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint16_t CHAR16;

#define EFI_SUCCESS 0

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self, const CHAR16 *text);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
};

typedef struct {
    char _pad0[44];
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
} EFI_SYSTEM_TABLE;

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table);
void *luna_uefi_reloc_anchor = (void *)&efi_main;

static EFI_STATUS write_line(EFI_SYSTEM_TABLE *table, const CHAR16 *text) {
    if (table == 0 || table->con_out == 0 || table->con_out->OutputString == 0) {
        return 1;
    }
    return table->con_out->OutputString(table->con_out, text);
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    (void)image_handle;
    if ((uintptr_t)luna_uefi_reloc_anchor == 0) {
        return 1;
    }
    write_line(system_table, L"LunaOS experimental UEFI path\r\n");
    write_line(system_table, L"UEFI boot stub ready\r\n");
    write_line(system_table, L"BIOS path remains the validated runtime path.\r\n");
    return EFI_SUCCESS;
}
