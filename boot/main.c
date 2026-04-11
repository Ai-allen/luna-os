#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"


#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *space_boot_fn_t)(const struct luna_bootview *view);
typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *system_gate_fn_t)(struct luna_system_gate *gate);

#define LUNA_UEFI_GRAPHICS_MAGIC 0x46504755u
#define LUNA_NATIVE_RESERVED_PROFILE 6u
#define LUNA_NATIVE_RESERVED_INSTALL_LOW 7u
#define LUNA_NATIVE_RESERVED_INSTALL_HIGH 8u
#define LUNA_NATIVE_RESERVED_ACTIVATION 9u
#define LUNA_NATIVE_RESERVED_PEER_LBA 10u

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

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void disable_interrupts(void) {
    __asm__ volatile ("cli" : : : "memory");
}

static void mask_legacy_pic(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

static void enable_fpu_sse(void) {
    uint64_t cr0;
    uint64_t cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ull << 2);
    cr0 |= (1ull << 1);
    cr0 |= (1ull << 5);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    __asm__ volatile ("clts" : : : "memory");

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ull << 9);
    cr4 |= (1ull << 10);
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    __asm__ volatile ("fninit" : : : "memory");
}

static void serial_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x03);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

static void serial_putc(char value) {
    while ((inb(0x3FD) & 0x20) == 0) {
    }
    outb(0x3F8, (uint8_t)value);
}

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static struct luna_cid g_device_read_cid = {0, 0};

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 1;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_BOOT;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t device_read_sector(uint32_t lba, void *buffer) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 3;
    gate->opcode = LUNA_DEVICE_BLOCK_READ;
    gate->device_id = LUNA_DEVICE_ID_DISK0;
    gate->flags = lba;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = 512;
    gate->size = 0;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->status;
}

static void apply_storage_gate(volatile struct luna_bootview *bootview, uint32_t state, uint32_t mode) {
    bootview->volume_state = state;
    bootview->system_mode = mode;
}

static void validate_native_pair(volatile struct luna_bootview *bootview) {
    struct luna_store_superblock system_super;
    struct luna_store_superblock data_super;

    zero_bytes(&system_super, sizeof(system_super));
    zero_bytes(&data_super, sizeof(data_super));

    if (device_read_sector((uint32_t)bootview->system_store_lba, &system_super) != LUNA_DEVICE_OK) {
        apply_storage_gate(bootview, LUNA_VOLUME_FATAL_UNRECOVERABLE, LUNA_MODE_FATAL);
        serial_write("[BOOT] lsys read fail\r\n");
        return;
    }
    if (device_read_sector((uint32_t)bootview->data_store_lba, &data_super) != LUNA_DEVICE_OK) {
        apply_storage_gate(bootview, LUNA_VOLUME_RECOVERY_REQUIRED, LUNA_MODE_RECOVERY);
        serial_write("[BOOT] ldat read fail\r\n");
        return;
    }

    if (system_super.magic != 0x5346414Cu || system_super.version != 3u ||
        system_super.reserved[LUNA_NATIVE_RESERVED_PROFILE] != LUNA_NATIVE_PROFILE_SYSTEM ||
        system_super.reserved[LUNA_NATIVE_RESERVED_INSTALL_LOW] != bootview->install_uuid_low ||
        system_super.reserved[LUNA_NATIVE_RESERVED_INSTALL_HIGH] != bootview->install_uuid_high ||
        system_super.reserved[LUNA_NATIVE_RESERVED_PEER_LBA] != bootview->data_store_lba) {
        apply_storage_gate(bootview, LUNA_VOLUME_FATAL_INCOMPATIBLE, LUNA_MODE_FATAL);
        serial_write("[BOOT] lsys contract fail\r\n");
        return;
    }

    if (data_super.magic != 0x5346414Cu || data_super.version != 3u ||
        data_super.reserved[LUNA_NATIVE_RESERVED_PROFILE] != LUNA_NATIVE_PROFILE_DATA ||
        data_super.reserved[LUNA_NATIVE_RESERVED_INSTALL_LOW] != bootview->install_uuid_low ||
        data_super.reserved[LUNA_NATIVE_RESERVED_INSTALL_HIGH] != bootview->install_uuid_high ||
        data_super.reserved[LUNA_NATIVE_RESERVED_PEER_LBA] != bootview->system_store_lba) {
        apply_storage_gate(bootview, LUNA_VOLUME_RECOVERY_REQUIRED, LUNA_MODE_RECOVERY);
        serial_write("[BOOT] ldat contract fail\r\n");
        return;
    }

    if (system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] == LUNA_ACTIVATION_RECOVERY) {
        apply_storage_gate(bootview, LUNA_VOLUME_RECOVERY_REQUIRED, LUNA_MODE_RECOVERY);
        serial_write("[BOOT] activation recovery\r\n");
        return;
    }
    if (system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] != LUNA_ACTIVATION_ACTIVE &&
        system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] != LUNA_ACTIVATION_COMMITTED) {
        apply_storage_gate(bootview, LUNA_VOLUME_FATAL_INCOMPATIBLE, LUNA_MODE_FATAL);
        serial_write("[BOOT] activation invalid\r\n");
    }
}

static uint32_t list_capabilities(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 2;
    gate->opcode = LUNA_GATE_LIST_CAPS;
    gate->caller_space = LUNA_SPACE_BOOT;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    return gate->status == LUNA_GATE_OK && gate->count >= 33 ? LUNA_GATE_OK : gate->status;
}

void SYSV_ABI boot_main(void) {
    static const char msg_boot[] = "[BOOT] dawn online\r\n";
    static const char msg_sec_enter[] = "[BOOT] security enter\r\n";
    static const char msg_sec_return[] = "[BOOT] security return\r\n";
    static const char msg_bootview_enter[] = "[BOOT] bootview enter\r\n";
    static const char msg_bootview_done[] = "[BOOT] bootview done\r\n";
    static const char msg_fb_done[] = "[BOOT] fb done\r\n";
    static const char msg_req_enter[] = "[BOOT] request enter\r\n";
    static const char msg_request_ok[] = "[BOOT] capability request ok\r\n";
    static const char msg_request_fail[] = "[BOOT] capability request fail\r\n";
    static const char msg_device_ok[] = "[BOOT] device read cap ok\r\n";
    static const char msg_device_fail[] = "[BOOT] device read cap fail\r\n";
    static const char msg_roster_enter[] = "[BOOT] roster enter\r\n";
    static const char msg_roster_ok[] = "[BOOT] capability roster ok\r\n";
    static const char msg_roster_fail[] = "[BOOT] capability roster fail\r\n";
    static const char msg_life_enter[] = "[BOOT] lifecycle enter\r\n";
    static const char msg_life_return[] = "[BOOT] lifecycle return\r\n";
    static const char msg_system_enter[] = "[BOOT] system enter\r\n";
    static const char msg_system_return[] = "[BOOT] system return\r\n";

    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_bootview *bootview =
        (volatile struct luna_bootview *)(uintptr_t)manifest->bootview_base;
    volatile struct luna_uefi_graphics_handoff *graphics_handoff =
        (volatile struct luna_uefi_graphics_handoff *)(uintptr_t)(manifest->bootview_base + 0x180u);
    struct luna_cid life_spawn = {0, 0};

    serial_init();
    disable_interrupts();
    mask_legacy_pic();
    enable_fpu_sse();
    serial_write(msg_boot);

    serial_write(msg_bootview_enter);
    bootview->security_gate_base = manifest->security_gate_base;
    bootview->data_gate_base = manifest->data_gate_base;
    bootview->lifecycle_gate_base = manifest->lifecycle_gate_base;
    bootview->system_gate_base = manifest->system_gate_base;
    bootview->time_gate_base = manifest->time_gate_base;
    bootview->device_gate_base = manifest->device_gate_base;
    bootview->observe_gate_base = manifest->observe_gate_base;
    bootview->network_gate_base = manifest->network_gate_base;
    bootview->graphics_gate_base = manifest->graphics_gate_base;
    bootview->ai_gate_base = manifest->ai_gate_base;
    bootview->package_gate_base = manifest->package_gate_base;
    bootview->update_gate_base = manifest->update_gate_base;
    bootview->program_gate_base = manifest->program_gate_base;
    bootview->data_buffer_base = manifest->data_buffer_base;
    bootview->data_buffer_size = manifest->data_buffer_size;
    bootview->list_buffer_base = manifest->list_buffer_base;
    bootview->list_buffer_size = manifest->list_buffer_size;
    bootview->serial_port = 0x3F8u;
    bootview->reserved0 = 0;
    bootview->reserve_base = manifest->reserve_base;
    bootview->reserve_size = manifest->reserve_size;
    bootview->app_write_entry = manifest->program_app_write_entry;
    bootview->framebuffer_base = 0;
    bootview->framebuffer_size = 0;
    bootview->framebuffer_width = 0;
    bootview->framebuffer_height = 0;
    bootview->framebuffer_pixels_per_scanline = 0;
    bootview->framebuffer_pixel_format = 0;
    bootview->install_uuid_low = manifest->install_uuid_low;
    bootview->install_uuid_high = manifest->install_uuid_high;
    bootview->system_store_lba = manifest->system_store_lba;
    bootview->data_store_lba = manifest->data_store_lba;
    bootview->volume_state = LUNA_VOLUME_HEALTHY;
    bootview->system_mode = LUNA_MODE_NORMAL;
    serial_write(msg_bootview_done);

    if (graphics_handoff->magic == LUNA_UEFI_GRAPHICS_MAGIC) {
        bootview->framebuffer_base = graphics_handoff->framebuffer_base;
        bootview->framebuffer_size = graphics_handoff->framebuffer_size;
        bootview->framebuffer_width = graphics_handoff->width;
        bootview->framebuffer_height = graphics_handoff->height;
        bootview->framebuffer_pixels_per_scanline = graphics_handoff->pixels_per_scanline;
        bootview->framebuffer_pixel_format = graphics_handoff->pixel_format;
    }
    serial_write(msg_fb_done);

    serial_write(msg_sec_enter);
    ((space_boot_fn_t)(uintptr_t)manifest->security_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)manifest->bootview_base
    );
    serial_write(msg_sec_return);

    if (request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid) == LUNA_GATE_OK) {
        serial_write(msg_device_ok);
        validate_native_pair(bootview);
    } else {
        serial_write(msg_device_fail);
        apply_storage_gate(bootview, LUNA_VOLUME_RECOVERY_REQUIRED, LUNA_MODE_RECOVERY);
    }

    serial_write(msg_req_enter);
    if (request_capability(LUNA_CAP_LIFE_SPAWN, &life_spawn) == LUNA_GATE_OK) {
        serial_write(msg_request_ok);
    } else {
        serial_write(msg_request_fail);
    }

    serial_write(msg_roster_enter);
    if (list_capabilities() == LUNA_GATE_OK) {
        serial_write(msg_roster_ok);
    } else {
        serial_write(msg_roster_fail);
    }

    serial_write(msg_life_enter);
    ((space_boot_fn_t)(uintptr_t)manifest->lifecycle_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)manifest->bootview_base
    );
    serial_write(msg_life_return);

    serial_write(msg_system_enter);
    ((space_boot_fn_t)(uintptr_t)manifest->system_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)manifest->bootview_base
    );
    serial_write(msg_system_return);
}
