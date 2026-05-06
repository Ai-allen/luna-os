#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);

static struct luna_cid g_device_write_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;

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
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 90;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_AI;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 94;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = LUNA_SPACE_AI;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_AI;
    gate->target_gate = target_gate;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;
    while (text[size] != '\0') {
        size += 1u;
    }
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 93;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_AI;
    gate->actor_space = LUNA_SPACE_AI;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)manifest->device_gate_base);
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
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[AI] yuesi stub ready\r\n");
    }
}

void SYSV_ABI ai_entry_gate(struct luna_ai_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_AI_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_AI_INFER) {
        char *text = (char *)(uintptr_t)gate->buffer_addr;
        if (validate_capability(LUNA_CAP_AI_INFER, gate->cid_low, gate->cid_high, LUNA_AI_INFER) != LUNA_GATE_OK) {
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
            data_gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
            zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
            data_gate->sequence = 91;
            data_gate->opcode = LUNA_DATA_SEED_OBJECT;
            data_gate->cid_low = seed.low;
            data_gate->cid_high = seed.high;
            data_gate->object_type = 0x54454D50u;
            ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
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
        if (validate_capability(LUNA_CAP_AI_CREATE, gate->cid_low, gate->cid_high, LUNA_AI_CREATE_SPACE) != LUNA_GATE_OK) {
            return;
        }
        space_id = lookup_space_id(name);
        if (space_id == 0u || request_capability(LUNA_CAP_LIFE_WAKE, &wake) != LUNA_GATE_OK) {
            gate->status = LUNA_AI_ERR_NOT_UNDERSTOOD;
            return;
        }
        life_gate = (volatile struct luna_lifecycle_gate *)(uintptr_t)g_manifest->lifecycle_gate_base;
        zero_bytes((void *)(uintptr_t)g_manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
        life_gate->sequence = 92;
        life_gate->opcode = LUNA_LIFE_WAKE_UNIT;
        life_gate->space_id = space_id;
        life_gate->state = LUNA_UNIT_LIVE;
        life_gate->cid_low = wake.low;
        life_gate->cid_high = wake.high;
        ((lifecycle_gate_fn_t)(uintptr_t)g_manifest->lifecycle_gate_entry)((struct luna_lifecycle_gate *)(uintptr_t)g_manifest->lifecycle_gate_base);
        gate->ticket = life_gate->epoch;
        gate->status = life_gate->status == LUNA_LIFE_OK ? LUNA_AI_OK : LUNA_AI_ERR_NOT_UNDERSTOOD;
    }
}
