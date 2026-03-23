#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static uint64_t g_current_version = 0x20u;
static uint64_t g_target_version = 0x21u;
static uint64_t g_flags = 0u;

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

static int allow_check(uint64_t low, uint64_t high) {
    return low == 0x6EA10001ull && high == 0x6E00C001ull;
}

static int allow_apply(uint64_t low, uint64_t high) {
    return low == 0x6EA10002ull && high == 0x6E00C002ull;
}

void SYSV_ABI update_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    serial_write("[UPDATE] wave ready\r\n");
}

void SYSV_ABI update_entry_gate(struct luna_update_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_UPDATE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_UPDATE_CHECK) {
        if (!allow_check(gate->cid_low, gate->cid_high)) {
            return;
        }
        gate->current_version = g_current_version;
        gate->target_version = g_target_version;
        gate->flags = g_flags;
        gate->result_count = 1u;
        gate->status = LUNA_UPDATE_OK;
        return;
    }
    if (gate->opcode == LUNA_UPDATE_APPLY) {
        if (!allow_apply(gate->cid_low, gate->cid_high)) {
            return;
        }
        g_flags = 1u;
        gate->current_version = g_current_version;
        gate->target_version = g_target_version;
        gate->flags = g_flags;
        gate->status = LUNA_UPDATE_OK;
    }
}
