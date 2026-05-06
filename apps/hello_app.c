#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *app_write_fn_t)(const struct luna_bootview *bootview, const char *text);
typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);

static void app_write(const struct luna_bootview *bootview, const char *text) {
    uint64_t entry = 0u;
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;

    if (text == 0) {
        return;
    }
    if (bootview != 0) {
        entry = bootview->app_write_entry;
    }
    if (entry == 0u || entry == ~0ull) {
        entry = manifest->program_app_write_entry;
    }
    if (entry == 0u || entry == ~0ull) {
        return;
    }
    ((app_write_fn_t)(uintptr_t)entry)(bootview, text);
}

static void zero_bytes(void *ptr, uint64_t len) {
    uint8_t *out = (uint8_t *)ptr;
    uint64_t i = 0u;
    while (i < len) {
        out[i] = 0u;
        i += 1u;
    }
}

static void append_u32(char *out, uint32_t value) {
    char digits[10];
    uint32_t count = 0u;
    uint32_t cursor = 0u;

    if (value == 0u) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value != 0u && count < 10u) {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        count += 1u;
    }
    while (count != 0u) {
        count -= 1u;
        out[cursor] = digits[count];
        cursor += 1u;
    }
    out[cursor] = '\0';
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 103u;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t read_space_count(struct luna_cid cid) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 104u;
    gate->opcode = LUNA_LIFE_READ_UNITS;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );
    if (gate->status != LUNA_LIFE_OK) {
        return 0u;
    }
    return gate->result_count;
}

static uint32_t read_device_count(struct luna_cid cid) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 105u;
    gate->opcode = LUNA_DEVICE_LIST;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->actor_space = LUNA_SPACE_PROGRAM;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    if (gate->status != LUNA_DEVICE_OK) {
        return 0u;
    }
    return gate->result_count;
}

void SYSV_ABI hello_app_entry(const struct luna_bootview *bootview) {
    struct luna_cid life_cid = {0u, 0u};
    struct luna_cid device_cid = {0u, 0u};
    char line[24];

    app_write(bootview, "[SETTINGS] Luna control\r\n");
    app_write(bootview, "appearance daylight\r\n");
    app_write(bootview, "input desktop hotkeys enabled\r\n");
    app_write(bootview, "system entry ready\r\n");
    if (request_capability(LUNA_CAP_LIFE_READ, &life_cid) == LUNA_GATE_OK) {
        append_u32(line, read_space_count(life_cid));
        app_write(bootview, "services online ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
    }
    if (request_capability(LUNA_CAP_DEVICE_LIST, &device_cid) == LUNA_GATE_OK) {
        append_u32(line, read_device_count(device_cid));
        app_write(bootview, "devices ready ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
    }
    app_write(bootview, "window keys F6 F7 F8\r\n");
}
