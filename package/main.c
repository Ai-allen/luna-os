#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_MANIFEST_ADDR 0x39000ull

static struct luna_package_record g_catalog[LUNA_PACKAGE_CAPACITY];

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
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

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static int allow_install(uint64_t low, uint64_t high) {
    return low == 0x6DA10001ull && high == 0x6D00B001ull;
}

static int allow_list(uint64_t low, uint64_t high) {
    return low == 0x6DA10002ull && high == 0x6D00B002ull;
}

static int text_equal16(const char lhs[16], const char *rhs) {
    for (size_t i = 0; i < 16; ++i) {
        char r = rhs[i];
        if (lhs[i] != r) {
            return 0;
        }
        if (r == '\0') {
            return 1;
        }
    }
    return 1;
}

void SYSV_ABI package_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    zero_bytes(g_catalog, sizeof(g_catalog));
    serial_write("[PACKAGE] catalog ready\r\n");
}

void SYSV_ABI package_entry_gate(struct luna_package_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_PACKAGE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_PACKAGE_INSTALL) {
        if (!allow_install(gate->cid_low, gate->cid_high)) {
            return;
        }
        if (!text_equal16(gate->name, "hello")) {
            gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
            return;
        }
        copy_bytes(g_catalog[0].name, "hello", 6u);
        g_catalog[0].installed = 1u;
        g_catalog[0].version = 1u;
        gate->ticket = 1u;
        gate->status = LUNA_PACKAGE_OK;
        return;
    }
    if (gate->opcode == LUNA_PACKAGE_LIST) {
        struct luna_package_record *out = (struct luna_package_record *)(uintptr_t)gate->buffer_addr;
        if (!allow_list(gate->cid_low, gate->cid_high)) {
            return;
        }
        if (gate->buffer_size < sizeof(g_catalog[0])) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        copy_bytes(out, &g_catalog[0], sizeof(g_catalog[0]));
        gate->result_count = g_catalog[0].installed;
        gate->status = LUNA_PACKAGE_OK;
    }
}
