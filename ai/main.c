#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_MANIFEST_ADDR 0x39000ull

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);

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

static int text_equal(const char *lhs, const char *rhs) {
    while (*lhs != '\0' || *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }
    return 1;
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 90;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_AI;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static void write_text(uint64_t buffer_addr, uint64_t buffer_size, const char *text) {
    char *out = (char *)(uintptr_t)buffer_addr;
    size_t i = 0;
    if (buffer_size == 0u) {
        return;
    }
    while (i + 1u < buffer_size && text[i] != '\0') {
        out[i] = text[i];
        i += 1u;
    }
    out[i] = '\0';
}

static int allow_infer(uint64_t low, uint64_t high) {
    return low == 0x6BA10001ull && high == 0x6B00D001ull;
}

static int allow_create(uint64_t low, uint64_t high) {
    return low == 0x6BA10002ull && high == 0x6B00D002ull;
}

static uint32_t lookup_space_id(const char *name) {
    if (text_equal(name, "PROGRAM")) return LUNA_SPACE_PROGRAM;
    if (text_equal(name, "USER")) return LUNA_SPACE_USER;
    if (text_equal(name, "NETWORK")) return LUNA_SPACE_NETWORK;
    if (text_equal(name, "GRAPHICS")) return LUNA_SPACE_GRAPHICS;
    if (text_equal(name, "AI")) return LUNA_SPACE_AI;
    return 0u;
}

void SYSV_ABI ai_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    serial_write("[AI] yuesi stub ready\r\n");
}

void SYSV_ABI ai_entry_gate(struct luna_ai_gate *gate) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    gate->result_count = 0u;
    gate->status = LUNA_AI_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_AI_INFER) {
        char *text = (char *)(uintptr_t)gate->buffer_addr;
        if (!allow_infer(gate->cid_low, gate->cid_high)) {
            return;
        }
        if (text_equal(text, "list spaces")) {
            write_text(gate->buffer_addr, gate->buffer_size, "BOOT SECURITY DATA LIFECYCLE SYSTEM TIME DEVICE OBSERVE NETWORK GRAPHICS PACKAGE UPDATE AI PROGRAM USER");
            gate->result_count = 1u;
            gate->status = LUNA_AI_OK;
            return;
        }
        if (text_equal(text, "create temporary storage")) {
            struct luna_cid seed = {0, 0};
            volatile struct luna_data_gate *data_gate;
            if (request_capability(LUNA_CAP_DATA_SEED, &seed) != LUNA_GATE_OK) {
                gate->status = LUNA_AI_ERR_NOT_UNDERSTOOD;
                return;
            }
            data_gate = (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
            zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
            data_gate->sequence = 91;
            data_gate->opcode = LUNA_DATA_SEED_OBJECT;
            data_gate->cid_low = seed.low;
            data_gate->cid_high = seed.high;
            data_gate->object_type = 0x54454D50u;
            ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)manifest->data_gate_base);
            if (data_gate->status != LUNA_DATA_OK) {
                gate->status = LUNA_AI_ERR_NOT_UNDERSTOOD;
                return;
            }
            write_text(gate->buffer_addr, gate->buffer_size, "temporary storage seeded");
            gate->result_count = 1u;
            gate->status = LUNA_AI_OK;
            return;
        }
        write_text(gate->buffer_addr, gate->buffer_size, "rule not understood");
        gate->status = LUNA_AI_ERR_NOT_UNDERSTOOD;
        return;
    }
    if (gate->opcode == LUNA_AI_CREATE_SPACE) {
        struct luna_cid wake = {0, 0};
        volatile struct luna_lifecycle_gate *life_gate;
        uint32_t space_id;
        char *name = (char *)(uintptr_t)gate->buffer_addr;
        if (!allow_create(gate->cid_low, gate->cid_high)) {
            return;
        }
        space_id = lookup_space_id(name);
        if (space_id == 0u || request_capability(LUNA_CAP_LIFE_WAKE, &wake) != LUNA_GATE_OK) {
            gate->status = LUNA_AI_ERR_NOT_UNDERSTOOD;
            return;
        }
        life_gate = (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
        zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
        life_gate->sequence = 92;
        life_gate->opcode = LUNA_LIFE_WAKE_UNIT;
        life_gate->space_id = space_id;
        life_gate->state = LUNA_UNIT_LIVE;
        life_gate->cid_low = wake.low;
        life_gate->cid_high = wake.high;
        ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)((struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base);
        gate->ticket = life_gate->epoch;
        gate->status = life_gate->status == LUNA_LIFE_OK ? LUNA_AI_OK : LUNA_AI_ERR_NOT_UNDERSTOOD;
    }
}
