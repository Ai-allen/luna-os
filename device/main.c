#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

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

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static int allow_list(uint64_t low, uint64_t high) {
    return low == 0x65A10001ull && high == 0x65007001ull;
}

static int allow_read(uint64_t low, uint64_t high) {
    return low == 0x65A10002ull && high == 0x65007002ull;
}

static int allow_write(uint64_t low, uint64_t high) {
    return low == 0x65A10003ull && high == 0x65007003ull;
}

void SYSV_ABI device_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    serial_write("[DEVICE] lane ready\r\n");
}

void SYSV_ABI device_entry_gate(struct luna_device_gate *gate) {
    static const struct luna_device_record serial_lane = {
        1u, LUNA_DEVICE_KIND_SERIAL, 0u, 0u, "serial0"
    };

    gate->result_count = 0;
    gate->status = LUNA_DEVICE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_DEVICE_LIST) {
        if (!allow_list(gate->cid_low, gate->cid_high)) {
            return;
        }
        if (gate->buffer_size < sizeof(serial_lane)) {
            gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
            return;
        }
        copy_bytes((void *)(uintptr_t)gate->buffer_addr, &serial_lane, sizeof(serial_lane));
        gate->result_count = 1;
        gate->status = LUNA_DEVICE_OK;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_READ) {
        if (!allow_read(gate->cid_low, gate->cid_high) || gate->device_id != 1u) {
            return;
        }
        if ((inb(0x3FD) & 0x01) != 0 && gate->buffer_size != 0u) {
            ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = inb(0x3F8);
            gate->size = 1;
        } else {
            gate->size = 0;
        }
        gate->status = LUNA_DEVICE_OK;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_WRITE) {
        const uint8_t *src = (const uint8_t *)(uintptr_t)gate->buffer_addr;
        if (!allow_write(gate->cid_low, gate->cid_high) || gate->device_id != 1u) {
            return;
        }
        for (uint64_t i = 0; i < gate->size && i < gate->buffer_size; ++i) {
            serial_putc((char)src[i]);
        }
        gate->status = LUNA_DEVICE_OK;
    }
}
