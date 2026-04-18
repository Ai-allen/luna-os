#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *app_write_fn_t)(const struct luna_bootview *bootview, const char *text);
typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *observe_gate_fn_t)(struct luna_observe_gate *gate);

struct luna_guard_observe_payload {
    struct luna_query_request request;
    struct luna_query_row rows[4];
};

static struct luna_guard_observe_payload g_guard_observe_payload;

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

static const char *space_name(uint32_t space_id);
static const char *observe_level_name(uint32_t level);
static uint32_t query_observe_logs(struct luna_cid cid, struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count);

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 90u;
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

static uint32_t read_cap_count(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 91u;
    gate->opcode = LUNA_GATE_LIST_CAPS;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return 0u;
    }
    return gate->count;
}

static uint32_t read_seal_count(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 99u;
    gate->opcode = LUNA_GATE_LIST_SEALS;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return 0u;
    }
    return gate->count;
}

static __attribute__((unused)) const char *cap_name(uint64_t domain_key) {
    switch (domain_key) {
        case LUNA_CAP_DATA_SEED: return "data.seed";
        case LUNA_CAP_DATA_POUR: return "data.pour";
        case LUNA_CAP_DATA_DRAW: return "data.draw";
        case LUNA_CAP_DATA_GATHER: return "data.gather";
        case LUNA_CAP_LIFE_READ: return "life.read";
        case LUNA_CAP_DEVICE_LIST: return "device.list";
        case LUNA_CAP_DEVICE_READ: return "device.read";
        case LUNA_CAP_DEVICE_WRITE: return "device.write";
        case LUNA_CAP_OBSERVE_READ: return "observe.read";
        case LUNA_CAP_OBSERVE_STATS: return "observe.stats";
        case LUNA_CAP_PROGRAM_LOAD: return "program.load";
        case LUNA_CAP_PROGRAM_START: return "program.start";
        default: return "cap";
    }
}

static __attribute__((unused)) void preview_caps(const struct luna_bootview *bootview) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    volatile struct luna_cap_record *records =
        (volatile struct luna_cap_record *)(uintptr_t)manifest->list_buffer_base;
    char line[16];
    uint32_t shown = 0u;
    uint32_t i = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 98u;
    gate->opcode = LUNA_GATE_LIST_CAPS;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return;
    }

    while (i < gate->count && shown < 5u) {
        app_write(bootview, "[GUARD] cap ");
        app_write(bootview, cap_name(records[i].domain_key));
        app_write(bootview, " -> ");
        if (records[i].target_space != 0u && space_name(records[i].target_space) != 0) {
            app_write(bootview, space_name(records[i].target_space));
        } else {
            app_write(bootview, "ANY");
        }
        app_write(bootview, ":");
        append_u32(line, records[i].target_gate);
        app_write(bootview, line);
        app_write(bootview, " uses=");
        append_u32(line, records[i].uses_left);
        app_write(bootview, line);
        app_write(bootview, " ttl=");
        append_u32(line, records[i].ttl);
        app_write(bootview, line);
        app_write(bootview, "\r\n");
        shown += 1u;
        i += 1u;
    }

    shown = 0u;
    i = 0u;
    app_write(bootview, "[GUARD] revoke: ");
    while (i < gate->count && shown < 4u) {
        uint32_t seen = 0u;
        uint32_t j = 0u;
        while (j < i) {
            if (records[j].domain_key == records[i].domain_key) {
                seen = 1u;
                break;
            }
            j += 1u;
        }
        if (seen == 0u) {
            if (shown != 0u) {
                app_write(bootview, ", ");
            }
            app_write(bootview, cap_name(records[i].domain_key));
            shown += 1u;
        }
        i += 1u;
    }
    app_write(bootview, "\r\n");

    shown = 0u;
    i = 0u;
    while (i < gate->count && shown < 3u) {
        uint32_t seen = 0u;
        uint32_t j = 0u;
        while (j < i) {
            if (records[j].domain_key == records[i].domain_key) {
                seen = 1u;
                break;
            }
            j += 1u;
        }
        if (seen == 0u) {
            app_write(bootview, "[GUARD] command: revoke-cap ");
            app_write(bootview, cap_name(records[i].domain_key));
            app_write(bootview, "\r\n");
            shown += 1u;
        }
        i += 1u;
    }
}

static __attribute__((unused)) void preview_seals(const struct luna_bootview *bootview) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    volatile struct luna_seal_record *records =
        (volatile struct luna_seal_record *)(uintptr_t)manifest->list_buffer_base;
    char line[16];
    uint32_t shown = 0u;
    uint32_t i = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 100u;
    gate->opcode = LUNA_GATE_LIST_SEALS;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return;
    }

    while (i < gate->count && shown < 4u) {
        app_write(bootview, "[GUARD] seal -> ");
        if (records[i].target_space != 0u && space_name(records[i].target_space) != 0) {
            app_write(bootview, space_name(records[i].target_space));
        } else {
            app_write(bootview, "ANY");
        }
        app_write(bootview, ":");
        append_u32(line, records[i].target_gate);
        app_write(bootview, line);
        app_write(bootview, "\r\n");
        shown += 1u;
        i += 1u;
    }
}

static uint32_t read_space_count(struct luna_cid cid) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 92u;
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
    gate->sequence = 93u;
    gate->opcode = LUNA_DEVICE_LIST;
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

static const char *space_name(uint32_t space_id) {
    switch (space_id) {
        case LUNA_SPACE_BOOT: return "BOOT";
        case LUNA_SPACE_SYSTEM: return "SYSTEM";
        case LUNA_SPACE_DATA: return "DATA";
        case LUNA_SPACE_SECURITY: return "SECURITY";
        case LUNA_SPACE_GRAPHICS: return "GRAPHICS";
        case LUNA_SPACE_DEVICE: return "DEVICE";
        case LUNA_SPACE_NETWORK: return "NETWORK";
        case LUNA_SPACE_USER: return "USER";
        case LUNA_SPACE_TIME: return "TIME";
        case LUNA_SPACE_LIFECYCLE: return "LIFECYCLE";
        case LUNA_SPACE_OBSERVABILITY: return "OBSERVABILITY";
        case LUNA_SPACE_AI: return "AI";
        case LUNA_SPACE_PROGRAM: return "PROGRAM";
        case LUNA_SPACE_PACKAGE: return "PACKAGE";
        case LUNA_SPACE_UPDATE: return "UPDATE";
        default: return 0;
    }
}

static const char *observe_level_name(uint32_t level) {
    switch (level) {
        case 0u: return "trace";
        case 1u: return "info";
        case 2u: return "warn";
        case 3u: return "fault";
        default: return "level";
    }
}

static __attribute__((unused)) void preview_live_spaces(const struct luna_bootview *bootview, struct luna_cid cid) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
    volatile struct luna_unit_record *records =
        (volatile struct luna_unit_record *)(uintptr_t)manifest->list_buffer_base;
    uint32_t printed = 0u;
    uint32_t i = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 94u;
    gate->opcode = LUNA_LIFE_READ_UNITS;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );
    if (gate->status != LUNA_LIFE_OK) {
        return;
    }

    app_write(bootview, "[GUARD] spaces: ");
    while (i < gate->result_count && printed < 5u) {
        const char *name = space_name(records[i].space_id);
        if (name != 0 && records[i].state == LUNA_UNIT_LIVE) {
            if (printed != 0u) {
                app_write(bootview, ", ");
            }
            app_write(bootview, name);
            printed += 1u;
        }
        i += 1u;
    }
    app_write(bootview, "\r\n");
}

static __attribute__((unused)) void preview_device_lanes(const struct luna_bootview *bootview, struct luna_cid cid) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    volatile struct luna_device_record *records =
        (volatile struct luna_device_record *)(uintptr_t)manifest->list_buffer_base;
    uint32_t printed = 0u;
    uint32_t i = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 95u;
    gate->opcode = LUNA_DEVICE_LIST;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    if (gate->status != LUNA_DEVICE_OK) {
        return;
    }

    app_write(bootview, "[GUARD] lanes: ");
    while (i < gate->result_count && printed < 6u) {
        if (printed != 0u) {
            app_write(bootview, ", ");
        }
        app_write(bootview, (const char *)records[i].name);
        printed += 1u;
        i += 1u;
    }
    app_write(bootview, "\r\n");
}

static uint32_t read_observe_count(struct luna_cid cid, struct luna_observe_stats *out_stats) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_observe_gate *gate =
        (volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    gate->sequence = 96u;
    gate->opcode = LUNA_OBSERVE_GET_STATS;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = sizeof(struct luna_observe_stats);
    ((observe_gate_fn_t)(uintptr_t)manifest->observe_gate_entry)(
        (struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base
    );
    if (gate->status != LUNA_OBSERVE_OK) {
        return 0u;
    }
    *out_stats = *(volatile struct luna_observe_stats *)(uintptr_t)manifest->list_buffer_base;
    return 1u;
}

static uint32_t query_observe_logs(struct luna_cid cid, struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_observe_gate *gate =
        (volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base;
    volatile struct luna_query_request *request = &g_guard_observe_payload.request;
    volatile struct luna_query_row *rows = g_guard_observe_payload.rows;
    uint32_t copied = 0u;

    if (out_count != 0) {
        *out_count = 0u;
    }
    if (out_rows == 0 || capacity == 0u) {
        return 0u;
    }
    if (capacity > (sizeof(g_guard_observe_payload.rows) / sizeof(g_guard_observe_payload.rows[0]))) {
        capacity = (uint32_t)(sizeof(g_guard_observe_payload.rows) / sizeof(g_guard_observe_payload.rows[0]));
    }
    if (capacity == 0u) {
        return 0u;
    }

    zero_bytes(&g_guard_observe_payload, sizeof(g_guard_observe_payload));
    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    request->target = LUNA_QUERY_TARGET_OBSERVE_LOGS;
    request->projection_flags =
        LUNA_QUERY_PROJECT_MESSAGE |
        LUNA_QUERY_PROJECT_STATE |
        LUNA_QUERY_PROJECT_OWNER |
        LUNA_QUERY_PROJECT_CREATED;
    request->sort_mode = LUNA_QUERY_SORT_STAMP_DESC;
    request->limit = capacity;
    gate->sequence = 97u;
    gate->opcode = LUNA_OBSERVE_QUERY;
    gate->space_id = LUNA_SPACE_PROGRAM;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_guard_observe_payload;
    gate->buffer_size = sizeof(struct luna_query_request) + ((uint64_t)capacity * sizeof(struct luna_query_row));
    ((observe_gate_fn_t)(uintptr_t)manifest->observe_gate_entry)(
        (struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base
    );
    if (gate->status != LUNA_OBSERVE_OK) {
        return 0u;
    }

    copied = gate->result_count;
    if (copied > capacity) {
        copied = capacity;
    }
    for (uint32_t i = 0u; i < copied; ++i) {
        out_rows[i] = rows[i];
    }
    if (out_count != 0) {
        *out_count = copied;
    }
    return 1u;
}

static void preview_observe_logs(const struct luna_bootview *bootview, struct luna_cid cid) {
    struct luna_query_row rows[4];
    char line[16];
    uint32_t shown = 0u;
    uint32_t count = 0u;
    uint32_t i = 0u;

    zero_bytes(rows, sizeof(rows));
    if (query_observe_logs(cid, rows, 4u, &count) == 0u || count == 0u) {
        return;
    }

    app_write(bootview, "[GUARD] observe query=lasql\r\n");
    while (i < count && shown < 2u) {
        app_write(bootview, "[GUARD] obs ");
        if (space_name((uint32_t)rows[i].owner_low) != 0) {
            app_write(bootview, space_name((uint32_t)rows[i].owner_low));
        } else {
            append_u32(line, (uint32_t)rows[i].owner_low);
            app_write(bootview, line);
        }
        app_write(bootview, " [");
        app_write(bootview, observe_level_name(rows[i].state));
        app_write(bootview, "]");
        app_write(bootview, ": ");
        app_write(bootview, (const char *)rows[i].message);
        app_write(bootview, "\r\n");
        shown += 1u;
        i += 1u;
    }
}

void SYSV_ABI guard_app_entry(const struct luna_bootview *bootview) {
    struct luna_cid life_read_cid = {0u, 0u};
    struct luna_cid device_list_cid = {0u, 0u};
    struct luna_cid observe_stats_cid = {0u, 0u};
    struct luna_cid observe_read_cid = {0u, 0u};
    struct luna_observe_stats observe_stats;
    char line[48];

    app_write(bootview, "security center\r\n");

    zero_bytes(line, sizeof(line));
    append_u32(line, read_cap_count());
    app_write(bootview, "active cids: ");
    app_write(bootview, line);
    app_write(bootview, "\r\n");

    zero_bytes(line, sizeof(line));
    append_u32(line, read_seal_count());
    app_write(bootview, "live seals: ");
    app_write(bootview, line);
    app_write(bootview, "\r\n");

    if (request_capability(LUNA_CAP_LIFE_READ, &life_read_cid) == LUNA_GATE_OK) {
        zero_bytes(line, sizeof(line));
        append_u32(line, read_space_count(life_read_cid));
        app_write(bootview, "live spaces: ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
    }

    if (request_capability(LUNA_CAP_DEVICE_LIST, &device_list_cid) == LUNA_GATE_OK) {
        zero_bytes(line, sizeof(line));
        append_u32(line, read_device_count(device_list_cid));
        app_write(bootview, "device lanes: ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
    }

    if (request_capability(LUNA_CAP_OBSERVE_STATS, &observe_stats_cid) == LUNA_GATE_OK &&
        read_observe_count(observe_stats_cid, &observe_stats) != 0u) {
        zero_bytes(line, sizeof(line));
        append_u32(line, observe_stats.count);
        app_write(bootview, "observe entries: ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");

        app_write(bootview, "observe level: ");
        app_write(bootview, observe_level_name(observe_stats.newest_level));
        app_write(bootview, "\r\n");
    }

    if (request_capability(LUNA_CAP_OBSERVE_READ, &observe_read_cid) == LUNA_GATE_OK) {
        preview_observe_logs(bootview, observe_read_cid);
    }

    app_write(bootview, "action: revoke-cap\r\n");
}
