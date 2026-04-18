#include <stddef.h>
#include <stdint.h>

/* LunaLoader Stage 1 for the UEFI boot path. */

#include "../include/luna_proto.h"
#include "uefi_loader_config.h"

#define EFIAPI __attribute__((ms_abi))

typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint64_t EFI_LBA;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t UINTN;

enum {
    AllocateAnyPages = 0,
    AllocateMaxAddress = 1,
    AllocateAddress = 2,
};

enum {
    EfiLoaderCode = 1,
    EfiLoaderData = 2,
};

#define EFI_SUCCESS 0
#define EFI_ERROR(status) (((status) & 0x8000000000000000ULL) != 0)

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} EFI_GUID;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *reset;
    EFI_STATUS(EFIAPI *output_string)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self, const CHAR16 *text);
};

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef EFI_STATUS(EFIAPI *efi_free_pool_fn_t)(void *buffer);
typedef EFI_STATUS(EFIAPI *efi_locate_device_path_fn_t)(EFI_GUID *protocol, void **device_path, EFI_HANDLE *device);
struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    void *raise_tpl;
    void *restore_tpl;
    EFI_STATUS(EFIAPI *allocate_pages)(uint32_t type, uint32_t memory_type, UINTN pages, EFI_PHYSICAL_ADDRESS *memory);
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    efi_free_pool_fn_t free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    EFI_STATUS(EFIAPI *handle_protocol)(EFI_HANDLE handle, EFI_GUID *protocol, void **interface_ptr);
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    efi_locate_device_path_fn_t locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    EFI_STATUS(EFIAPI *locate_handle_buffer)(uint32_t search_type, EFI_GUID *protocol, void *search_key, UINTN *handle_count, EFI_HANDLE **buffer);
};

typedef struct {
    EFI_TABLE_HEADER hdr;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
    EFI_HANDLE standard_error_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *std_err;
    void *runtime_services;
    EFI_BOOT_SERVICES *boot_services;
} EFI_SYSTEM_TABLE;

typedef struct {
    uint32_t media_id;
    uint8_t removable_media;
    uint8_t media_present;
    uint8_t logical_partition;
    uint8_t read_only;
    uint8_t write_caching;
    uint8_t block_size_pad[3];
    uint32_t block_size;
    uint32_t io_align;
    EFI_LBA last_block;
} EFI_BLOCK_IO_MEDIA;

typedef struct EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;
struct EFI_BLOCK_IO_PROTOCOL {
    uint64_t revision;
    EFI_BLOCK_IO_MEDIA *media;
    void *reset;
    EFI_STATUS(EFIAPI *read_blocks)(EFI_BLOCK_IO_PROTOCOL *self, uint32_t media_id, EFI_LBA lba, UINTN buffer_size, void *buffer);
    EFI_STATUS(EFIAPI *write_blocks)(EFI_BLOCK_IO_PROTOCOL *self, uint32_t media_id, EFI_LBA lba, UINTN buffer_size, void *buffer);
};

typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_STATUS(EFIAPI *open_volume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *self, EFI_FILE_PROTOCOL **root);
};

struct EFI_FILE_PROTOCOL {
    uint64_t revision;
    EFI_STATUS(EFIAPI *open)(EFI_FILE_PROTOCOL *self, EFI_FILE_PROTOCOL **new_handle, const CHAR16 *file_name, uint64_t open_mode, uint64_t attributes);
    EFI_STATUS(EFIAPI *close)(EFI_FILE_PROTOCOL *self);
    void *delete_fn;
    EFI_STATUS(EFIAPI *read)(EFI_FILE_PROTOCOL *self, UINTN *buffer_size, void *buffer);
    void *write;
    void *get_position;
    void *set_position;
};

typedef struct {
    uint32_t revision;
    EFI_HANDLE parent_handle;
    EFI_SYSTEM_TABLE *system_table;
    EFI_HANDLE device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    void *unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct {
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
} EFI_PIXEL_BITMASK;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    EFI_PIXEL_BITMASK pixel_information;
    uint32_t pixels_per_scan_line;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINTN size_of_info;
    EFI_PHYSICAL_ADDRESS frame_buffer_base;
    UINTN frame_buffer_size;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void *query_mode;
    void *set_mode;
    void *blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
};

typedef void (*boot_entry_fn_t)(void);

#define LUNA_UEFI_DISK_MAGIC 0x49464555u

struct luna_uefi_disk_handoff {
    uint32_t magic;
    uint32_t version;
    uint64_t block_io_protocol;
    uint64_t media_id;
    uint64_t read_blocks_entry;
    uint64_t write_blocks_entry;
    uint64_t diag_status;
    uint64_t diag_handle_count;
    uint64_t diag_full_disk_seen;
    uint64_t target_block_io_protocol;
    uint64_t target_media_id;
    uint64_t target_read_blocks_entry;
    uint64_t target_write_blocks_entry;
    uint64_t flags;
};

#define LUNA_UEFI_DISK_FLAG_SOURCE_REMOVABLE 0x1u
#define LUNA_UEFI_DISK_FLAG_SOURCE_READ_ONLY 0x2u
#define LUNA_UEFI_DISK_FLAG_SOURCE_LOGICAL_PARTITION 0x4u
#define LUNA_UEFI_DISK_FLAG_TARGET_PRESENT 0x8u
#define LUNA_UEFI_DISK_FLAG_TARGET_SEPARATE 0x10u

struct luna_uefi_graphics_handoff {
    uint32_t magic;
    uint32_t version;
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format;
};

static EFI_GUID kLoadedImageProtocolGuid = {
    0x5B1B31A1u, 0x9562u, 0x11D2u, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};
static EFI_GUID kBlockIoProtocolGuid = {
    0x964E5B21u, 0x6459u, 0x11D2u, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};
static EFI_GUID kSimpleFileSystemProtocolGuid = {
    0x964E5B22u, 0x6459u, 0x11D2u, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};
static EFI_GUID kGraphicsOutputProtocolGuid = {
    0x9042A9DEu, 0x23DCu, 0x4A38u, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A}
};

static const CHAR16 kStageFileName[] = {
    '\\', 'L', 'U', 'N', 'A', 'O', 'S', '.', 'I', 'M', 'G', 0
};
static const CHAR16 kStageIsoFileName[] = {
    '\\', 'L', 'U', 'N', 'A', 'O', 'S', '.', 'I', 'M', 'G', ';', '1', 0
};

#define LUNA_UEFI_GRAPHICS_MAGIC 0x46504755u
#define LUNA_RUNTIME_STORE_PROBE_LBA 0x4800u

static void zero_bytes(void *ptr, uint64_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (uint64_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static void patch_manifest(EFI_PHYSICAL_ADDRESS scratch_alloc_base) {
    uint8_t *manifest = (uint8_t *)(uintptr_t)LUNA_MANIFEST_ADDR;
    for (uint32_t i = 0; i < LUNA_SCRATCH_FIELD_COUNT; ++i) {
        uint64_t *field = (uint64_t *)(void *)(manifest + luna_scratch_field_offsets[i]);
        *field = *field - LUNA_SCRATCH_ORIG_BASE + scratch_alloc_base;
    }
}

static struct luna_uefi_disk_handoff *disk_handoff(EFI_PHYSICAL_ADDRESS scratch_alloc_base) {
    return (struct luna_uefi_disk_handoff *)(uintptr_t)(
        scratch_alloc_base + LUNA_BOOTVIEW_UEFI_DISK_HANDOFF_OFFSET
    );
}

static void init_disk_handoff(EFI_PHYSICAL_ADDRESS scratch_alloc_base) {
    struct luna_uefi_disk_handoff *handoff = disk_handoff(scratch_alloc_base);
    zero_bytes(handoff, sizeof(*handoff));
    handoff->magic = LUNA_UEFI_DISK_MAGIC;
    handoff->version = 2;
}

static void write_source_media_flags(EFI_PHYSICAL_ADDRESS scratch_alloc_base, EFI_BLOCK_IO_PROTOCOL *block_io) {
    struct luna_uefi_disk_handoff *handoff = disk_handoff(scratch_alloc_base);
    handoff->flags &= ~(uint64_t)(
        LUNA_UEFI_DISK_FLAG_SOURCE_REMOVABLE |
        LUNA_UEFI_DISK_FLAG_SOURCE_READ_ONLY |
        LUNA_UEFI_DISK_FLAG_SOURCE_LOGICAL_PARTITION
    );
    if (block_io == NULL || block_io->media == NULL) {
        return;
    }
    if (block_io->media->removable_media != 0) {
        handoff->flags |= LUNA_UEFI_DISK_FLAG_SOURCE_REMOVABLE;
    }
    if (block_io->media->read_only != 0) {
        handoff->flags |= LUNA_UEFI_DISK_FLAG_SOURCE_READ_ONLY;
    }
    if (block_io->media->logical_partition != 0) {
        handoff->flags |= LUNA_UEFI_DISK_FLAG_SOURCE_LOGICAL_PARTITION;
    }
}

static void write_boot_disk_handoff(EFI_PHYSICAL_ADDRESS scratch_alloc_base, EFI_BLOCK_IO_PROTOCOL *block_io) {
    struct luna_uefi_disk_handoff *handoff =
        disk_handoff(scratch_alloc_base);
    handoff->block_io_protocol = (uint64_t)(uintptr_t)block_io;
    handoff->media_id = block_io->media != NULL ? block_io->media->media_id : 0;
    handoff->read_blocks_entry = (uint64_t)(uintptr_t)block_io->read_blocks;
    handoff->write_blocks_entry = (uint64_t)(uintptr_t)block_io->write_blocks;
}

static void write_target_disk_handoff(
    EFI_PHYSICAL_ADDRESS scratch_alloc_base,
    EFI_BLOCK_IO_PROTOCOL *block_io,
    EFI_BLOCK_IO_PROTOCOL *boot_block_io
) {
    struct luna_uefi_disk_handoff *handoff =
        disk_handoff(scratch_alloc_base);
    handoff->target_block_io_protocol = (uint64_t)(uintptr_t)block_io;
    handoff->target_media_id = block_io->media != NULL ? block_io->media->media_id : 0;
    handoff->target_read_blocks_entry = (uint64_t)(uintptr_t)block_io->read_blocks;
    handoff->target_write_blocks_entry = (uint64_t)(uintptr_t)block_io->write_blocks;
    handoff->flags |= LUNA_UEFI_DISK_FLAG_TARGET_PRESENT;
    if (block_io != boot_block_io) {
        handoff->flags |= LUNA_UEFI_DISK_FLAG_TARGET_SEPARATE;
    }
    handoff->diag_status = 9u;
}

static void write_graphics_handoff(EFI_PHYSICAL_ADDRESS scratch_alloc_base, EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    struct luna_uefi_graphics_handoff *handoff =
        (struct luna_uefi_graphics_handoff *)(uintptr_t)(
            scratch_alloc_base + LUNA_BOOTVIEW_UEFI_GRAPHICS_HANDOFF_OFFSET
        );

    if (gop == NULL || gop->mode == NULL || gop->mode->info == NULL) {
        return;
    }

    handoff->magic = LUNA_UEFI_GRAPHICS_MAGIC;
    handoff->version = 1;
    handoff->framebuffer_base = gop->mode->frame_buffer_base;
    handoff->framebuffer_size = gop->mode->frame_buffer_size;
    handoff->width = gop->mode->info->horizontal_resolution;
    handoff->height = gop->mode->info->vertical_resolution;
    handoff->pixels_per_scanline = gop->mode->info->pixels_per_scan_line;
    handoff->pixel_format = gop->mode->info->pixel_format;
}

static EFI_STATUS print_message(EFI_SYSTEM_TABLE *st, const CHAR16 *text) {
    if (st == NULL || st->con_out == NULL || st->con_out->output_string == NULL) {
        return 1;
    }
    return st->con_out->output_string(st->con_out, text);
}

static void print_stage(EFI_SYSTEM_TABLE *st, const CHAR16 *text) {
    (void)print_message(st, text);
}

static void print_hex64(EFI_SYSTEM_TABLE *st, uint64_t value) {
    static const CHAR16 digits[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    CHAR16 buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (uint32_t index = 0; index < 16; ++index) {
        uint32_t shift = (15u - index) * 4u;
        buffer[2 + index] = digits[(value >> shift) & 0xFu];
    }
    buffer[18] = 0;
    (void)print_message(st, buffer);
}

static void print_manifest_diag(EFI_SYSTEM_TABLE *st, const CHAR16 *label) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    print_stage(st, label);
    print_stage(st, L" bootview=");
    print_hex64(st, manifest->bootview_base);
    print_stage(st, L" system=");
    print_hex64(st, manifest->system_store_lba);
    print_stage(st, L" data=");
    print_hex64(st, manifest->data_store_lba);
    print_stage(st, L" install_low=");
    print_hex64(st, manifest->install_uuid_low);
    print_stage(st, L" install_high=");
    print_hex64(st, manifest->install_uuid_high);
    print_stage(st, L"\r\n");
}

static void print_disk_diag(EFI_SYSTEM_TABLE *st, EFI_PHYSICAL_ADDRESS scratch_alloc_base) {
    struct luna_uefi_disk_handoff *handoff = disk_handoff(scratch_alloc_base);
    print_stage(st, L"LunaLoader UEFI Stage 1 handoff-state scratch=");
    print_hex64(st, scratch_alloc_base);
    print_stage(st, L" disk=");
    print_hex64(st, (uint64_t)(uintptr_t)handoff);
    print_stage(st, L" magic=");
    print_hex64(st, handoff->magic);
    print_stage(st, L" version=");
    print_hex64(st, handoff->version);
    print_stage(st, L" status=");
    print_hex64(st, handoff->diag_status);
    print_stage(st, L" flags=");
    print_hex64(st, handoff->flags);
    print_stage(st, L"\r\n");
}

static EFI_BOOT_SERVICES *resolve_boot_services(EFI_SYSTEM_TABLE *system_table) {
    static const uint32_t candidate_offsets[] = {0x60u, 0x68u, 0x70u, 0x78u};
    EFI_BOOT_SERVICES *boot_services = system_table->boot_services;
    if (boot_services != NULL && boot_services->allocate_pages != NULL && boot_services->handle_protocol != NULL) {
        return boot_services;
    }
    for (size_t i = 0; i < sizeof(candidate_offsets) / sizeof(candidate_offsets[0]); ++i) {
        EFI_BOOT_SERVICES *candidate =
            *(EFI_BOOT_SERVICES **)(void *)((uint8_t *)(void *)system_table + candidate_offsets[i]);
        if (candidate != NULL && candidate->allocate_pages != NULL && candidate->handle_protocol != NULL) {
            return candidate;
        }
    }
    return NULL;
}

static EFI_STATUS load_stage_via_file(
    EFI_BOOT_SERVICES *boot_services,
    EFI_HANDLE device_handle,
    EFI_PHYSICAL_ADDRESS load_addr
) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS status;
    UINTN bytes_to_read = LUNA_STAGE_BYTES;

    status = boot_services->handle_protocol(device_handle, &kSimpleFileSystemProtocolGuid, (void **)&fs);
    if (EFI_ERROR(status) || fs == NULL || fs->open_volume == NULL) {
        return status;
    }
    status = fs->open_volume(fs, &root);
    if (EFI_ERROR(status) || root == NULL || root->open == NULL) {
        return status;
    }
    status = root->open(root, &file, kStageFileName, 1, 0);
    if (EFI_ERROR(status) || file == NULL || file->read == NULL) {
        file = NULL;
        status = root->open(root, &file, kStageIsoFileName, 1, 0);
    }
    if (EFI_ERROR(status) || file == NULL || file->read == NULL) {
        if (root->close != NULL) {
            (void)root->close(root);
        }
        return status;
    }
    status = file->read(file, &bytes_to_read, (void *)(uintptr_t)load_addr);
    if (file->close != NULL) {
        (void)file->close(file);
    }
    if (root->close != NULL) {
        (void)root->close(root);
    }
    if (EFI_ERROR(status) || bytes_to_read != LUNA_STAGE_BYTES) {
        return EFI_ERROR(status) ? status : 5;
    }
    return EFI_SUCCESS;
}

static int is_runtime_block_io_candidate(EFI_BLOCK_IO_PROTOCOL *block_io, int require_full_disk) {
    if (block_io == NULL
        || block_io->media == NULL
        || block_io->read_blocks == NULL
        || block_io->write_blocks == NULL
        || block_io->media->media_present == 0
        || block_io->media->read_only != 0
        || block_io->media->block_size != 512u) {
        return 0;
    }
    if (require_full_disk && block_io->media->logical_partition != 0) {
        return 0;
    }
    return 1;
}

static int is_boot_block_io_candidate(EFI_BLOCK_IO_PROTOCOL *block_io, int require_full_disk) {
    if (block_io == NULL
        || block_io->media == NULL
        || block_io->read_blocks == NULL
        || block_io->media->media_present == 0
        || block_io->media->block_size != 512u) {
        return 0;
    }
    if (require_full_disk && block_io->media->logical_partition != 0) {
        return 0;
    }
    return 1;
}

static int probe_boot_block_io(EFI_BLOCK_IO_PROTOCOL *block_io, EFI_PHYSICAL_ADDRESS scratch_alloc_base) {
    void *buffer = (void *)(uintptr_t)(scratch_alloc_base + 0x400u);
    if (!is_boot_block_io_candidate(block_io, 0)) {
        return 0;
    }
    return block_io->read_blocks(
        block_io,
        block_io->media->media_id,
        LUNA_RUNTIME_STORE_PROBE_LBA,
        512u,
        buffer
    ) == EFI_SUCCESS;
}

static EFI_BLOCK_IO_PROTOCOL *resolve_source_block_io(
    EFI_BOOT_SERVICES *boot_services,
    EFI_HANDLE preferred_handle,
    void *preferred_file_path
) {
    EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
    EFI_STATUS status;
    EFI_HANDLE resolved_handle = NULL;
    void *device_path = preferred_file_path;

    status = boot_services->handle_protocol(preferred_handle, &kBlockIoProtocolGuid, (void **)&block_io);
    if (!EFI_ERROR(status) && block_io != NULL) {
        return block_io;
    }

    if (boot_services->locate_device_path == NULL || device_path == NULL) {
        return NULL;
    }
    status = boot_services->locate_device_path(&kBlockIoProtocolGuid, &device_path, &resolved_handle);
    if (EFI_ERROR(status) || resolved_handle == NULL) {
        return NULL;
    }
    block_io = NULL;
    status = boot_services->handle_protocol(resolved_handle, &kBlockIoProtocolGuid, (void **)&block_io);
    if (EFI_ERROR(status) || block_io == NULL) {
        return NULL;
    }
    return block_io;
}

static EFI_BLOCK_IO_PROTOCOL *select_boot_block_io(
    EFI_BOOT_SERVICES *boot_services,
    EFI_HANDLE preferred_handle,
    void *preferred_file_path,
    EFI_PHYSICAL_ADDRESS scratch_alloc_base,
    int allow_full_disk_scan
) {
    EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status;
    EFI_HANDLE resolved_handle = NULL;
    void *device_path = preferred_file_path;

    status = boot_services->handle_protocol(preferred_handle, &kBlockIoProtocolGuid, (void **)&block_io);
    if (!EFI_ERROR(status)
        && is_boot_block_io_candidate(block_io, 1)
        && probe_boot_block_io(block_io, scratch_alloc_base)) {
        return block_io;
    }

    if (boot_services->locate_device_path != NULL && device_path != NULL) {
        status = boot_services->locate_device_path(&kBlockIoProtocolGuid, &device_path, &resolved_handle);
        if (!EFI_ERROR(status) && resolved_handle != NULL) {
            block_io = NULL;
            status = boot_services->handle_protocol(resolved_handle, &kBlockIoProtocolGuid, (void **)&block_io);
            if (!EFI_ERROR(status)
                && is_boot_block_io_candidate(block_io, 1)
                && probe_boot_block_io(block_io, scratch_alloc_base)) {
                return block_io;
            }
        }
    }

    if (allow_full_disk_scan && boot_services->locate_handle_buffer != NULL) {
        status = boot_services->locate_handle_buffer(2u, &kBlockIoProtocolGuid, NULL, &handle_count, &handles);
        if (!EFI_ERROR(status) && handles != NULL) {
            for (UINTN i = 0; i < handle_count; ++i) {
                block_io = NULL;
                status = boot_services->handle_protocol(handles[i], &kBlockIoProtocolGuid, (void **)&block_io);
                if (!EFI_ERROR(status)
                    && is_boot_block_io_candidate(block_io, 1)
                    && probe_boot_block_io(block_io, scratch_alloc_base)) {
                    if (boot_services->free_pool != NULL) {
                        (void)boot_services->free_pool(handles);
                    }
                    return block_io;
                }
            }
            if (boot_services->free_pool != NULL) {
                (void)boot_services->free_pool(handles);
            }
        }
    }
    return NULL;
}

static int probe_runtime_block_io(
    EFI_BLOCK_IO_PROTOCOL *block_io,
    EFI_PHYSICAL_ADDRESS scratch_alloc_base,
    int require_write
) {
    void *buffer = (void *)(uintptr_t)(scratch_alloc_base + 0x400u);
    if (!is_runtime_block_io_candidate(block_io, 0)) {
        return 0;
    }
    if (block_io->read_blocks(
        block_io,
        block_io->media->media_id,
        LUNA_RUNTIME_STORE_PROBE_LBA,
        512u,
        buffer
    ) != EFI_SUCCESS) {
        return 0;
    }
    if (require_write) {
        return block_io->write_blocks(
            block_io,
            block_io->media->media_id,
            LUNA_RUNTIME_STORE_PROBE_LBA,
            512u,
            buffer
        ) == EFI_SUCCESS;
    }
    /*
     * VMware's installed-boot UEFI SCSI path can reject a pre-exit write probe
     * on the full-disk handle even though the same Block I/O handle remains the
     * only viable runtime path for LSYS/LDAT reads after handoff. Preserve that
     * path for normal boot, but let installer-media target selection require a
     * successful write-capable handle.
     */
    (void)block_io->write_blocks(
        block_io,
        block_io->media->media_id,
        LUNA_RUNTIME_STORE_PROBE_LBA,
        512u,
        buffer
    );
    return 1;
}

static EFI_BLOCK_IO_PROTOCOL *select_runtime_block_io(
    EFI_BOOT_SERVICES *boot_services,
    EFI_HANDLE preferred_handle,
    void *preferred_file_path,
    EFI_PHYSICAL_ADDRESS scratch_alloc_base,
    int require_write
) {
    EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status;
    EFI_HANDLE resolved_handle = NULL;
    void *device_path = preferred_file_path;

    status = boot_services->handle_protocol(preferred_handle, &kBlockIoProtocolGuid, (void **)&block_io);
    if (!EFI_ERROR(status)) {
        if (is_runtime_block_io_candidate(block_io, 1)) {
            if (probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
                return block_io;
            }
            disk_handoff(scratch_alloc_base)->diag_status = 3u;
        } else {
            disk_handoff(scratch_alloc_base)->diag_status = 2u;
        }
    } else {
        disk_handoff(scratch_alloc_base)->diag_status = 1u;
    }

    if (boot_services->locate_device_path != NULL && device_path != NULL) {
        status = boot_services->locate_device_path(&kBlockIoProtocolGuid, &device_path, &resolved_handle);
        if (!EFI_ERROR(status) && resolved_handle != NULL) {
            block_io = NULL;
            status = boot_services->handle_protocol(resolved_handle, &kBlockIoProtocolGuid, (void **)&block_io);
            if (!EFI_ERROR(status)
                && is_runtime_block_io_candidate(block_io, 0)
                && probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
                disk_handoff(scratch_alloc_base)->diag_status = 10u;
                return block_io;
            }
            disk_handoff(scratch_alloc_base)->diag_status = 8u;
        }
    }

    if (boot_services->locate_handle_buffer == NULL) {
        if (is_runtime_block_io_candidate(block_io, 0)
            && probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
            return block_io;
        }
        disk_handoff(scratch_alloc_base)->diag_status = 4u;
        return NULL;
    }
    status = boot_services->locate_handle_buffer(2u, &kBlockIoProtocolGuid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handles == NULL) {
        if (is_runtime_block_io_candidate(block_io, 0)
            && probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
            return block_io;
        }
        disk_handoff(scratch_alloc_base)->diag_status = 5u;
        return NULL;
    }
    disk_handoff(scratch_alloc_base)->diag_handle_count = handle_count;
    for (UINTN i = 0; i < handle_count; ++i) {
        block_io = NULL;
        status = boot_services->handle_protocol(handles[i], &kBlockIoProtocolGuid, (void **)&block_io);
        if (!EFI_ERROR(status)
            && is_runtime_block_io_candidate(block_io, 1)) {
            disk_handoff(scratch_alloc_base)->diag_full_disk_seen += 1u;
            if (probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
                if (boot_services->free_pool != NULL) {
                    (void)boot_services->free_pool(handles);
                }
                return block_io;
            }
            disk_handoff(scratch_alloc_base)->diag_status = 6u;
        }
    }
    for (UINTN i = 0; i < handle_count; ++i) {
        block_io = NULL;
        status = boot_services->handle_protocol(handles[i], &kBlockIoProtocolGuid, (void **)&block_io);
        if (!EFI_ERROR(status)
            && is_runtime_block_io_candidate(block_io, 0)
            && probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
            if (boot_services->free_pool != NULL) {
                (void)boot_services->free_pool(handles);
            }
            return block_io;
        }
    }
    if (boot_services->free_pool != NULL) {
        (void)boot_services->free_pool(handles);
    }
    if (is_runtime_block_io_candidate(block_io, 0)
        && probe_runtime_block_io(block_io, scratch_alloc_base, require_write)) {
        return block_io;
    }
    if (disk_handoff(scratch_alloc_base)->diag_status == 0u) {
        disk_handoff(scratch_alloc_base)->diag_status = 7u;
    }
    return NULL;
}

static EFI_GRAPHICS_OUTPUT_PROTOCOL *locate_gop(EFI_BOOT_SERVICES *boot_services, EFI_SYSTEM_TABLE *system_table) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status;

    if (boot_services == NULL) {
        return NULL;
    }

    if (system_table != NULL && system_table->console_out_handle != NULL) {
        status = boot_services->handle_protocol(system_table->console_out_handle, &kGraphicsOutputProtocolGuid, (void **)&gop);
        if (!EFI_ERROR(status) && gop != NULL && gop->mode != NULL && gop->mode->info != NULL) {
            return gop;
        }
    }

    if (boot_services->locate_handle_buffer == NULL) {
        return NULL;
    }

    status = boot_services->locate_handle_buffer(2u, &kGraphicsOutputProtocolGuid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handles == NULL || handle_count == 0u) {
        return NULL;
    }

    for (UINTN i = 0; i < handle_count; ++i) {
        gop = NULL;
        status = boot_services->handle_protocol(handles[i], &kGraphicsOutputProtocolGuid, (void **)&gop);
        if (!EFI_ERROR(status) && gop != NULL && gop->mode != NULL && gop->mode->info != NULL) {
            if (boot_services->free_pool != NULL) {
                (void)boot_services->free_pool(handles);
            }
            return gop;
        }
    }

    if (boot_services->free_pool != NULL) {
        (void)boot_services->free_pool(handles);
    }
    return NULL;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    EFI_STATUS status;
    EFI_BOOT_SERVICES *boot_services;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_BLOCK_IO_PROTOCOL *source_block_io = NULL;
    EFI_BLOCK_IO_PROTOCOL *boot_block_io = NULL;
    EFI_BLOCK_IO_PROTOCOL *runtime_block_io = NULL;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_PHYSICAL_ADDRESS load_addr = LUNA_STAGE_LOAD_ADDR;
    EFI_PHYSICAL_ADDRESS scratch_alloc_base = 0;
    uint64_t manifest_magic;
    int installer_boot = 0;

    print_stage(system_table, L"LunaLoader UEFI Stage 1 handoff\r\n");

    if (system_table == NULL) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-system-table\r\n");
        return 1;
    }
    boot_services = resolve_boot_services(system_table);
    if (boot_services == NULL) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-boot-services\r\n");
        return 1;
    }
    print_stage(system_table, L"LunaLoader UEFI Stage 1 boot-services ok\r\n");

    status = boot_services->allocate_pages(AllocateAddress, EfiLoaderCode, LUNA_STAGE_PAGES, &load_addr);
    if (EFI_ERROR(status) || load_addr != LUNA_STAGE_LOAD_ADDR) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-stage-alloc\r\n");
        return status;
    }
    print_stage(system_table, L"LunaLoader UEFI Stage 1 stage-alloc ok\r\n");

    status = boot_services->allocate_pages(AllocateAnyPages, EfiLoaderData, LUNA_SCRATCH_PAGES, &scratch_alloc_base);
    if (EFI_ERROR(status) || scratch_alloc_base == 0) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-scratch-alloc\r\n");
        return status;
    }
    zero_bytes((void *)(uintptr_t)scratch_alloc_base, LUNA_SCRATCH_PAGES * 4096ULL);
    print_stage(system_table, L"LunaLoader UEFI Stage 1 scratch-alloc ok\r\n");

    status = boot_services->handle_protocol(image_handle, &kLoadedImageProtocolGuid, (void **)&loaded_image);
    if (EFI_ERROR(status) || loaded_image == NULL) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-loaded-image\r\n");
        return status;
    }
    print_stage(system_table, L"LunaLoader UEFI Stage 1 loaded-image ok\r\n");

    status = load_stage_via_file(boot_services, loaded_image->device_handle, load_addr);
    if (EFI_ERROR(status)) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-stage-load\r\n");
        return status;
    }
    print_stage(system_table, L"LunaLoader UEFI Stage 1 stage-load ok\r\n");
    print_manifest_diag(system_table, L"LunaLoader UEFI Stage 1 manifest-pre");

    manifest_magic = *(volatile uint32_t *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (manifest_magic != 0x414E554Cu) {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 fail-manifest\r\n");
        return 5;
    }
    print_stage(system_table, L"LunaLoader UEFI Stage 1 manifest ok\r\n");

    init_disk_handoff(scratch_alloc_base);
    source_block_io = resolve_source_block_io(
        boot_services,
        loaded_image->device_handle,
        loaded_image->file_path
    );
    if (source_block_io != NULL) {
        write_source_media_flags(scratch_alloc_base, source_block_io);
        if (source_block_io->media != NULL
            && (source_block_io->media->removable_media != 0
                || source_block_io->media->read_only != 0)) {
            installer_boot = 1;
        }
    }
    boot_block_io = select_boot_block_io(
        boot_services,
        loaded_image->device_handle,
        loaded_image->file_path,
        scratch_alloc_base,
        installer_boot == 0
    );
    if (boot_block_io != NULL) {
        write_boot_disk_handoff(scratch_alloc_base, boot_block_io);
        if (boot_block_io->media != NULL
            && (boot_block_io->media->removable_media != 0
                || boot_block_io->media->read_only != 0)) {
            installer_boot = 1;
        }
    }
    runtime_block_io = select_runtime_block_io(
        boot_services,
        loaded_image->device_handle,
        loaded_image->file_path,
        scratch_alloc_base,
        installer_boot
    );
    if (runtime_block_io != NULL) {
        write_target_disk_handoff(scratch_alloc_base, runtime_block_io, boot_block_io);
        print_stage(system_table, L"LunaLoader UEFI Stage 1 block-io ready\r\n");
    } else {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 block-io missing\r\n");
    }
    gop = locate_gop(boot_services, system_table);
    if (gop != NULL) {
        write_graphics_handoff(scratch_alloc_base, gop);
        print_stage(system_table, L"LunaLoader UEFI Stage 1 gop ready\r\n");
    } else {
        print_stage(system_table, L"LunaLoader UEFI Stage 1 gop missing\r\n");
    }
    print_disk_diag(system_table, scratch_alloc_base);
    patch_manifest(scratch_alloc_base);
    print_manifest_diag(system_table, L"LunaLoader UEFI Stage 1 manifest-post");
    print_stage(system_table, L"LunaLoader UEFI Stage 1 jump stage\r\n");
    ((boot_entry_fn_t)(uintptr_t)(LUNA_STAGE_LOAD_ADDR + LUNA_BOOT_ENTRY_OFFSET))();
    return EFI_SUCCESS;
}
