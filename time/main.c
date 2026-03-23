#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static uint64_t g_boot_tsc = 0;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint64_t rdtsc_now(void) {
    uint32_t lo;
    uint32_t hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
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

static int allow_read(uint64_t low, uint64_t high) {
    return low == 0x68A10001ull && high == 0x68006001ull;
}

static int allow_chime(uint64_t low, uint64_t high) {
    return low == 0x68A10002ull && high == 0x68006002ull;
}

static uint64_t pulse_ticks(void) {
    return (rdtsc_now() - g_boot_tsc) / 1000000ull;
}

void SYSV_ABI time_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    g_boot_tsc = rdtsc_now();
    serial_write("[TIME] pulse ready\r\n");
}

void SYSV_ABI time_entry_gate(struct luna_time_gate *gate) {
    gate->result_count = 0;
    gate->status = LUNA_TIME_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_TIME_READ_PULSE) {
        if (!allow_read(gate->cid_low, gate->cid_high)) {
            return;
        }
        gate->arg0 = pulse_ticks();
        gate->result_count = 1;
        gate->status = LUNA_TIME_OK;
        return;
    }
    if (gate->opcode == LUNA_TIME_SET_CHIME) {
        uint64_t target;
        if (!allow_chime(gate->cid_low, gate->cid_high)) {
            return;
        }
        target = pulse_ticks() + gate->arg0;
        while (pulse_ticks() < target) {
        }
        gate->status = LUNA_TIME_OK;
    }
}
