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

static int allow_draw(uint64_t low, uint64_t high) {
    return low == 0x64A10001ull && high == 0x6400A001ull;
}

void SYSV_ABI graphics_entry_boot(const struct luna_bootview *bootview) {
    volatile uint16_t *vga = (volatile uint16_t *)(uintptr_t)0xB8000u;
    (void)bootview;
    vga[0] = (uint16_t)0x1F00u | (uint16_t)'L';
    serial_write("[GRAPHICS] console ready\r\n");
}

void SYSV_ABI graphics_entry_gate(struct luna_graphics_gate *gate) {
    volatile uint16_t *vga = (volatile uint16_t *)(uintptr_t)0xB8000u;
    gate->result_count = 0u;
    gate->status = LUNA_GRAPHICS_ERR_INVALID_CAP;
    if (gate->opcode != LUNA_GRAPHICS_DRAW_CHAR || !allow_draw(gate->cid_low, gate->cid_high)) {
        return;
    }
    if (gate->x >= 80u || gate->y >= 25u) {
        gate->status = LUNA_GRAPHICS_ERR_RANGE;
        return;
    }
    vga[gate->y * 80u + gate->x] = (uint16_t)((gate->attr & 0xFFu) << 8) | (uint16_t)(gate->glyph & 0xFFu);
    gate->status = LUNA_GRAPHICS_OK;
}
