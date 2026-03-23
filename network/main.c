#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static uint8_t g_packet[LUNA_NETWORK_PACKET_BYTES];
static uint64_t g_packet_size = 0;
static uint64_t g_packet_epoch = 0;

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

static int allow_send(uint64_t low, uint64_t high) {
    return low == 0x66A10001ull && high == 0x66009001ull;
}

static int allow_recv(uint64_t low, uint64_t high) {
    return low == 0x66A10002ull && high == 0x66009002ull;
}

void SYSV_ABI network_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    serial_write("[NETWORK] loop ready\r\n");
}

void SYSV_ABI network_entry_gate(struct luna_network_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_NETWORK_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_NETWORK_SEND_PACKET) {
        if (!allow_send(gate->cid_low, gate->cid_high) || gate->size > gate->buffer_size || gate->size > sizeof(g_packet)) {
            gate->status = LUNA_NETWORK_ERR_RANGE;
            return;
        }
        copy_bytes(g_packet, (const void *)(uintptr_t)gate->buffer_addr, (size_t)gate->size);
        g_packet_size = gate->size;
        g_packet_epoch += 1u;
        gate->status = LUNA_NETWORK_OK;
        return;
    }
    if (gate->opcode == LUNA_NETWORK_RECV_PACKET) {
        if (!allow_recv(gate->cid_low, gate->cid_high)) {
            return;
        }
        if (g_packet_size == 0u) {
            gate->status = LUNA_NETWORK_ERR_EMPTY;
            return;
        }
        if (gate->buffer_size < g_packet_size) {
            gate->status = LUNA_NETWORK_ERR_RANGE;
            return;
        }
        copy_bytes((void *)(uintptr_t)gate->buffer_addr, g_packet, (size_t)g_packet_size);
        gate->size = g_packet_size;
        gate->result_count = 1u;
        gate->status = LUNA_NETWORK_OK;
        g_packet_size = 0u;
    }
}
