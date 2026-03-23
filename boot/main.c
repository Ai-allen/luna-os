#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define LUNA_MANIFEST_ADDR 0x39000ull
#define LUNA_BOOTVIEW_ADDR 0x42000ull

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *space_boot_fn_t)(const struct luna_bootview *view);
typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *system_gate_fn_t)(struct luna_system_gate *gate);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
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
    static const char msg_request_ok[] = "[BOOT] capability request ok\r\n";
    static const char msg_request_fail[] = "[BOOT] capability request fail\r\n";
    static const char msg_roster_ok[] = "[BOOT] capability roster ok\r\n";
    static const char msg_roster_fail[] = "[BOOT] capability roster fail\r\n";

    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_bootview *bootview =
        (volatile struct luna_bootview *)(uintptr_t)LUNA_BOOTVIEW_ADDR;
    struct luna_cid life_spawn = {0, 0};

    serial_init();
    serial_write(msg_boot);

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

    ((space_boot_fn_t)(uintptr_t)manifest->security_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)LUNA_BOOTVIEW_ADDR
    );

    if (request_capability(LUNA_CAP_LIFE_SPAWN, &life_spawn) == LUNA_GATE_OK) {
        serial_write(msg_request_ok);
    } else {
        serial_write(msg_request_fail);
    }

    if (list_capabilities() == LUNA_GATE_OK) {
        serial_write(msg_roster_ok);
    } else {
        serial_write(msg_roster_fail);
    }

    ((space_boot_fn_t)(uintptr_t)manifest->data_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)LUNA_BOOTVIEW_ADDR
    );

    ((space_boot_fn_t)(uintptr_t)manifest->lifecycle_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)LUNA_BOOTVIEW_ADDR
    );

    ((space_boot_fn_t)(uintptr_t)manifest->system_boot_entry)(
        (const struct luna_bootview *)(uintptr_t)LUNA_BOOTVIEW_ADDR
    );
}
