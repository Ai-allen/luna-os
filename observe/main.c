#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static struct luna_observe_record g_logs[LUNA_OBSERVE_LOG_CAPACITY];
static uint32_t g_log_count = 0;
static uint32_t g_log_head = 0;
static uint32_t g_log_dropped = 0;
static uint64_t g_stamp = 1;

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

static void log_record(uint32_t space_id, uint32_t level, const char *message) {
    struct luna_observe_record *entry = &g_logs[g_log_head];
    entry->stamp = g_stamp++;
    entry->space_id = space_id;
    entry->level = level;
    for (size_t i = 0; i < sizeof(entry->message); ++i) {
        entry->message[i] = 0;
        if (message[i] == '\0') {
            break;
        }
        entry->message[i] = message[i];
    }
    g_log_head = (g_log_head + 1u) % LUNA_OBSERVE_LOG_CAPACITY;
    if (g_log_count < LUNA_OBSERVE_LOG_CAPACITY) {
        g_log_count += 1u;
    } else {
        g_log_dropped += 1u;
    }
}

static int allow_log(uint64_t low, uint64_t high) {
    return low == 0x6AA10001ull && high == 0x6A008001ull;
}

static int allow_read(uint64_t low, uint64_t high) {
    return low == 0x6AA10002ull && high == 0x6A008002ull;
}

static int allow_stats(uint64_t low, uint64_t high) {
    return low == 0x6AA10003ull && high == 0x6A008003ull;
}

void SYSV_ABI observe_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    log_record(LUNA_SPACE_OBSERVABILITY, 1u, "observe online");
    serial_write("[OBSERVE] ribbon ready\r\n");
}

void SYSV_ABI observe_entry_gate(struct luna_observe_gate *gate) {
    gate->result_count = 0;
    gate->status = LUNA_OBSERVE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_OBSERVE_LOG) {
        if (!allow_log(gate->cid_low, gate->cid_high)) {
            return;
        }
        log_record(gate->space_id, gate->level, gate->message);
        gate->status = LUNA_OBSERVE_OK;
        return;
    }
    if (gate->opcode == LUNA_OBSERVE_GET_LOGS) {
        uint32_t count = 0;
        struct luna_observe_record *out = (struct luna_observe_record *)(uintptr_t)gate->buffer_addr;
        if (!allow_read(gate->cid_low, gate->cid_high)) {
            return;
        }
        while (count < g_log_count && (uint64_t)(count + 1u) * sizeof(*out) <= gate->buffer_size) {
            uint32_t index = (g_log_head + LUNA_OBSERVE_LOG_CAPACITY - g_log_count + count) % LUNA_OBSERVE_LOG_CAPACITY;
            copy_bytes(&out[count], &g_logs[index], sizeof(*out));
            count += 1u;
        }
        gate->result_count = count;
        gate->status = LUNA_OBSERVE_OK;
        return;
    }
    if (gate->opcode == LUNA_OBSERVE_GET_STATS) {
        struct luna_observe_stats *stats = (struct luna_observe_stats *)(uintptr_t)gate->buffer_addr;
        if (!allow_stats(gate->cid_low, gate->cid_high) || gate->buffer_size < sizeof(*stats)) {
            return;
        }
        stats->count = g_log_count;
        stats->dropped = g_log_dropped;
        stats->newest_level = g_log_count == 0u ? 0u : g_logs[(g_log_head + LUNA_OBSERVE_LOG_CAPACITY - 1u) % LUNA_OBSERVE_LOG_CAPACITY].level;
        stats->reserved0 = 0u;
        stats->newest_stamp = g_stamp - 1u;
        gate->result_count = 1u;
        gate->status = LUNA_OBSERVE_OK;
    }
}
