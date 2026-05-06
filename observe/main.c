#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

static struct luna_observe_record g_logs[LUNA_OBSERVE_LOG_CAPACITY];
static uint32_t g_log_count = 0;
static uint32_t g_log_head = 0;
static uint32_t g_log_dropped = 0;
static uint64_t g_stamp = 1;
static struct luna_cid g_device_write_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;

static uint32_t validate_capability_for(uint64_t domain_key,
                                        uint64_t cid_low,
                                        uint64_t cid_high,
                                        uint32_t target_gate,
                                        uint64_t caller_space);

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 101;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_OBSERVABILITY;
    gate->domain_key = domain_key;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    return validate_capability_for(domain_key, cid_low, cid_high, target_gate, 0u);
}

static uint32_t validate_capability_for(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate, uint64_t caller_space) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 103;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = caller_space;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_OBSERVABILITY;
    gate->target_gate = target_gate;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static uint32_t security_query_govern(uint64_t caller_space, struct luna_query_request *request) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 104;
    gate->opcode = LUNA_GATE_QUERY_GOVERN;
    gate->caller_space = caller_space;
    gate->buffer_addr = (uint64_t)(uintptr_t)request;
    gate->buffer_size = sizeof(*request);
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
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
    gate->sequence = 102;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_OBSERVABILITY;
    gate->actor_space = LUNA_SPACE_OBSERVABILITY;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)manifest->device_gate_base);
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

void SYSV_ABI observe_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    log_record(LUNA_SPACE_OBSERVABILITY, 1u, "observe online");
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[OBSERVE] ribbon ready\r\n");
    }
}

void SYSV_ABI observe_entry_gate(struct luna_observe_gate *gate) {
    gate->result_count = 0;
    gate->status = LUNA_OBSERVE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_OBSERVE_LOG) {
        if (validate_capability(LUNA_CAP_OBSERVE_LOG, gate->cid_low, gate->cid_high, LUNA_OBSERVE_LOG) != LUNA_GATE_OK) {
            return;
        }
        log_record(gate->space_id, gate->level, gate->message);
        gate->status = LUNA_OBSERVE_OK;
        return;
    }
    if (gate->opcode == LUNA_OBSERVE_GET_LOGS) {
        uint32_t count = 0;
        struct luna_observe_record *out = (struct luna_observe_record *)(uintptr_t)gate->buffer_addr;
        if (validate_capability(LUNA_CAP_OBSERVE_READ, gate->cid_low, gate->cid_high, LUNA_OBSERVE_GET_LOGS) != LUNA_GATE_OK) {
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
        if (validate_capability(LUNA_CAP_OBSERVE_STATS, gate->cid_low, gate->cid_high, LUNA_OBSERVE_GET_STATS) != LUNA_GATE_OK ||
            gate->buffer_size < sizeof(*stats)) {
            return;
        }
        stats->count = g_log_count;
        stats->dropped = g_log_dropped;
        stats->newest_level = g_log_count == 0u ? 0u : g_logs[(g_log_head + LUNA_OBSERVE_LOG_CAPACITY - 1u) % LUNA_OBSERVE_LOG_CAPACITY].level;
        stats->reserved0 = 0u;
        stats->newest_stamp = g_stamp - 1u;
        gate->result_count = 1u;
        gate->status = LUNA_OBSERVE_OK;
        return;
    }
    if (gate->opcode == LUNA_OBSERVE_QUERY) {
        struct luna_query_request *request = (struct luna_query_request *)(uintptr_t)gate->buffer_addr;
        struct luna_query_row *out;
        uint32_t copied = 0u;
        uint32_t available = 0u;
        uint32_t limit = 0u;
        if (gate->buffer_size < sizeof(*request) ||
            validate_capability_for(LUNA_CAP_OBSERVE_READ, gate->cid_low, gate->cid_high, LUNA_OBSERVE_QUERY, gate->space_id) != LUNA_GATE_OK ||
            security_query_govern(gate->space_id, request) != LUNA_GATE_OK ||
            request->target != LUNA_QUERY_TARGET_OBSERVE_LOGS) {
            return;
        }
        out = (struct luna_query_row *)(uintptr_t)(gate->buffer_addr + sizeof(*request));
        gate->buffer_size -= sizeof(*request);
        available = (uint32_t)(gate->buffer_size / sizeof(*out));
        limit = request->limit == 0u ? available : request->limit;
        for (uint32_t i = 0u; i < g_log_count && copied < available && copied < limit; ++i) {
            uint32_t index = (g_log_head + LUNA_OBSERVE_LOG_CAPACITY - 1u - i) % LUNA_OBSERVE_LOG_CAPACITY;
            struct luna_query_row row;
            zero_bytes(&row, sizeof(row));
            row.owner_low = g_logs[index].space_id;
            row.created_at = g_logs[index].stamp;
            row.updated_at = g_logs[index].stamp;
            row.namespace_id = LUNA_QUERY_NAMESPACE_OBSERVE;
            row.object_type = 0u;
            row.state = g_logs[index].level;
            copy_bytes(row.message, g_logs[index].message, sizeof(row.message));
            copy_bytes(&out[copied], &row, sizeof(row));
            copied += 1u;
        }
        log_record(LUNA_SPACE_OBSERVABILITY, 1u, "audit lasql.query logs");
        gate->result_count = copied;
        gate->status = LUNA_OBSERVE_OK;
    }
}
