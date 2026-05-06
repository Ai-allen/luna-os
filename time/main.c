#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static uint64_t g_boot_pulse = 0;
static struct luna_cid g_device_read_cid = {0, 0};
static struct luna_cid g_device_write_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;

    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)g_manifest->security_gate_base)[i] = 0;
    }
    gate->sequence = 80;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_TIME;
    gate->domain_key = domain_key;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)g_manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)g_manifest->security_gate_base)[i] = 0;
    }
    gate->sequence = 82;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = LUNA_SPACE_TIME;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_TIME;
    gate->target_gate = target_gate;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)g_manifest->security_gate_base
    );
    return gate->status;
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;

    while (text[size] != '\0') {
        size += 1u;
    }
    for (size_t i = 0; i < sizeof(struct luna_device_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)manifest->device_gate_base)[i] = 0;
    }
    gate->sequence = 81;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_TIME;
    gate->actor_space = LUNA_SPACE_TIME;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
}

static uint64_t device_clock_now(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t value = 0;

    for (size_t i = 0; i < sizeof(struct luna_device_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)manifest->device_gate_base)[i] = 0;
    }
    gate->sequence = 83;
    gate->opcode = LUNA_DEVICE_READ;
    gate->caller_space = LUNA_SPACE_TIME;
    gate->actor_space = LUNA_SPACE_TIME;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_CLOCK0;
    gate->buffer_addr = (uint64_t)(uintptr_t)&value;
    gate->buffer_size = sizeof(value);
    gate->size = 0;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->status == LUNA_DEVICE_OK ? value : 0;
}

static uint64_t pulse_ticks(void) {
    uint64_t now = device_clock_now();
    if (now <= g_boot_pulse) {
        return 0;
    }
    return now - g_boot_pulse;
}

void SYSV_ABI time_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid) == LUNA_GATE_OK) {
        g_boot_pulse = device_clock_now();
    }
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[TIME] pulse ready\r\n");
    }
}

void SYSV_ABI time_entry_gate(struct luna_time_gate *gate) {
    gate->result_count = 0;
    gate->status = LUNA_TIME_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_TIME_READ_PULSE) {
        if (validate_capability(LUNA_CAP_TIME_PULSE, gate->cid_low, gate->cid_high, LUNA_TIME_READ_PULSE) != LUNA_GATE_OK) {
            return;
        }
        gate->arg0 = pulse_ticks();
        gate->result_count = 1;
        gate->status = LUNA_TIME_OK;
        return;
    }
    if (gate->opcode == LUNA_TIME_SET_CHIME) {
        uint64_t target;
        if (validate_capability(LUNA_CAP_TIME_CHIME, gate->cid_low, gate->cid_high, LUNA_TIME_SET_CHIME) != LUNA_GATE_OK) {
            return;
        }
        target = pulse_ticks() + gate->arg0;
        while (pulse_ticks() < target) {
        }
        gate->status = LUNA_TIME_OK;
    }
}
