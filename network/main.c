#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

#define LUNALINK_PEER_CAPACITY 8u
#define LUNALINK_SESSION_CAPACITY 8u
#define LUNALINK_CHANNEL_CAPACITY 16u

#define LUNALINK_STATE_OBJECT_TYPE 0x4C4C4E53u
#define LUNALINK_PAIRING_OBJECT_TYPE 0x4C4C5052u
#define LUNALINK_SESSION_RESUME_OBJECT_TYPE 0x4C4C5352u
#define LUNALINK_SET_OBJECT_TYPE 0x4C534554u

#define LUNALINK_STATE_MAGIC 0x4C4C4E53u
#define LUNALINK_STATE_VERSION 1u
#define LUNALINK_PAIRING_MAGIC 0x4C4C5052u
#define LUNALINK_PAIRING_VERSION 1u
#define LUNALINK_SESSION_RESUME_MAGIC 0x4C4C5352u
#define LUNALINK_SESSION_RESUME_VERSION 1u
#define LUNALINK_PACKET_MAGIC 0x4C4C504Bu

#define LUNALINK_PAIRING_FLAG_PENDING 0x00000001u
#define LUNALINK_PAIRING_FLAG_TRUSTED 0x00000002u
#define LUNALINK_PAIRING_FLAG_CHALLENGE 0x00000004u

#define LUNALINK_RESUME_FLAG_ISSUED 0x00000001u
#define LUNALINK_RESUME_FLAG_CONSUMED 0x00000002u

#define LUNALINK_PAIRING_RECORD_MIN_SIZE 40u
#define LUNALINK_SESSION_RESUME_RECORD_MIN_SIZE 24u

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);
typedef void (SYSV_ABI *observe_gate_fn_t)(struct luna_observe_gate *gate);

struct lunalink_state_record {
    uint32_t magic;
    uint32_t version;
    uint32_t next_peer_id;
    uint32_t next_session_id;
    uint32_t next_channel_id;
    uint32_t reserved0;
    struct luna_object_ref peer_registry;
};

struct lunalink_pairing_record {
    uint32_t magic;
    uint32_t version;
    uint32_t peer_id;
    uint32_t flags;
    uint64_t challenge;
    uint64_t verified_identity;
    char name[16];
};

struct lunalink_session_resume_record {
    uint32_t magic;
    uint32_t version;
    uint32_t session_id;
    uint32_t peer_id;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t verified_identity;
};

struct lunalink_packet {
    uint32_t magic;
    uint32_t peer_id;
    uint32_t session_id;
    uint32_t channel_id;
    uint32_t payload_size;
};

struct lunalink_runtime_peer {
    uint32_t live;
    struct luna_link_peer peer;
};

struct lunalink_runtime_session {
    uint32_t live;
    struct luna_link_session session;
};

struct lunalink_runtime_channel {
    uint32_t live;
    struct luna_link_channel channel;
};

static uint8_t g_packet[LUNA_NETWORK_PACKET_BYTES];
static struct luna_cid g_device_read_cid = {0, 0};
static struct luna_cid g_device_write_cid = {0, 0};
static struct luna_cid g_data_seed_cid = {0, 0};
static struct luna_cid g_data_pour_cid = {0, 0};
static struct luna_cid g_data_draw_cid = {0, 0};
static struct luna_cid g_data_gather_cid = {0, 0};
static struct luna_cid g_observe_log_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;
static struct luna_object_ref g_state_object = {0u, 0u};
static struct luna_object_ref g_peer_registry = {0u, 0u};
static struct lunalink_state_record g_state = {
    LUNALINK_STATE_MAGIC, LUNALINK_STATE_VERSION, 1u, 1u, 1u, 0u, {0u, 0u}
};
static struct lunalink_runtime_peer g_peers[LUNALINK_PEER_CAPACITY];
static struct lunalink_runtime_session g_sessions[LUNALINK_SESSION_CAPACITY];
static struct lunalink_runtime_channel g_channels[LUNALINK_CHANNEL_CAPACITY];
static uint8_t g_gate_stage[512];
static struct luna_policy_query g_policy_stage;
static struct luna_object_ref g_member_stage;
static uint8_t g_last_external_tx[LUNA_NETWORK_PACKET_BYTES];
static uint64_t g_last_external_tx_size;
static uint32_t g_last_external_tx_live;

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}

static int name16_equal(const char left[16], const char right[16]) {
    for (uint32_t i = 0u; i < 16u; ++i) {
        if (left[i] != right[i]) {
            return 0;
        }
        if (left[i] == '\0') {
            return 1;
        }
    }
    return 1;
}

static int name16_matches_text(const char left[16], const char *right) {
    for (uint32_t i = 0u; i < 16u; ++i) {
        char value = right[i];
        if (left[i] != value) {
            return 0;
        }
        if (value == '\0') {
            return 1;
        }
    }
    return 0;
}

static void *gate_stage_buffer(uint64_t size) {
    if (size == 0u || size > sizeof(g_gate_stage)) {
        return 0;
    }
    return g_gate_stage;
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 61;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t validate_network_policy(
    uint64_t domain_key,
    uint64_t cid_low,
    uint64_t cid_high,
    uint64_t caller_space,
    uint64_t actor_space,
    uint32_t target_gate,
    uint32_t session_id,
    uint32_t channel_id,
    uint64_t policy_flags
) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 63;
    gate->opcode = LUNA_GATE_VALIDATE_NETWORK;
    gate->caller_space = caller_space;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_NETWORK;
    gate->target_gate = target_gate;
    gate->ttl = session_id;
    gate->uses = channel_id;
    gate->seal_low = actor_space;
    gate->nonce = policy_flags;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static uint32_t security_policy_sync(uint32_t policy_type, uint64_t target_id, uint32_t state, uint32_t flags, uint64_t binding_id) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes(&g_policy_stage, sizeof(g_policy_stage));
    g_policy_stage.policy_type = policy_type;
    g_policy_stage.state = state;
    g_policy_stage.flags = flags;
    g_policy_stage.target_id = target_id;
    g_policy_stage.binding_id = binding_id;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 67;
    gate->opcode = LUNA_GATE_POLICY_SYNC;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_policy_stage;
    gate->buffer_size = sizeof(g_policy_stage);
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static uint32_t security_policy_query(uint32_t policy_type, uint64_t target_id, uint64_t binding_id, uint32_t *out_state, uint32_t *out_flags, uint64_t *out_binding) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes(&g_policy_stage, sizeof(g_policy_stage));
    g_policy_stage.policy_type = policy_type;
    g_policy_stage.target_id = target_id;
    g_policy_stage.binding_id = binding_id;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 68;
    gate->opcode = LUNA_GATE_POLICY_QUERY;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_policy_stage;
    gate->buffer_size = sizeof(g_policy_stage);
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    if (out_state != 0) {
        *out_state = g_policy_stage.state;
    }
    if (out_flags != 0) {
        *out_flags = g_policy_stage.flags;
    }
    if (out_binding != 0) {
        *out_binding = g_policy_stage.binding_id;
    }
    return gate->status;
}

static void copy_text_field(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) {
        return;
    }
    for (size_t i = 0u; i < dst_len; ++i) {
        dst[i] = 0;
    }
    if (src == 0) {
        return;
    }
    for (size_t i = 0u; i + 1u < dst_len; ++i) {
        if (src[i] == '\0') {
            break;
        }
        dst[i] = src[i];
    }
}

static void observe_log_lson(const char *text, const struct luna_lson_record *lson) {
    volatile struct luna_observe_gate *gate;
    uint64_t size = 0u;
    if (g_observe_log_cid.low == 0u || text == 0) {
        return;
    }
    while (text[size] != '\0' && size + 1u < sizeof(gate->message)) {
        size += 1u;
    }
    gate = (volatile struct luna_observe_gate *)(uintptr_t)g_manifest->observe_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    gate->sequence = 86;
    gate->opcode = LUNA_OBSERVE_LOG;
    gate->space_id = LUNA_SPACE_NETWORK;
    gate->level = 1u;
    gate->cid_low = g_observe_log_cid.low;
    gate->cid_high = g_observe_log_cid.high;
    copy_bytes((void *)gate->message, text, (size_t)size);
    if (lson != 0) {
        gate->buffer_addr = (uint64_t)(uintptr_t)lson;
        gate->buffer_size = sizeof(*lson);
    }
    ((observe_gate_fn_t)(uintptr_t)g_manifest->observe_gate_entry)((struct luna_observe_gate *)(uintptr_t)g_manifest->observe_gate_base);
}

static void observe_log(const char *text) {
    observe_log_lson(text, 0);
}

static void observe_link_trace(const char *scope, const char *message, uint32_t session_id, uint32_t channel_id, uint64_t actor_space, uint32_t allowed) {
    struct luna_lson_record record;
    zero_bytes(&record, sizeof(record));
    record.magic = LUNA_LSON_MAGIC;
    record.version = LUNA_LSON_VERSION;
    record.actor_low = actor_space;
    record.trace_low = ((uint64_t)session_id << 32) | (uint64_t)channel_id;
    record.session_low = session_id;
    record.install_low = g_manifest->install_uuid_low;
    record.install_high = g_manifest->install_uuid_high;
    record.record_class = LUNA_LOG_CLASS_SYSTEM;
    record.band = allowed != 0u ? LUNA_LOG_BAND_TRACE : LUNA_LOG_BAND_AUDIT;
    record.space_id = LUNA_SPACE_NETWORK;
    record.writer_space = LUNA_SPACE_NETWORK;
    record.authority_space = LUNA_SPACE_OBSERVABILITY;
    record.encoding = LUNA_LSON_ENCODING_FRAME;
    record.flags = LUNA_LSON_FLAG_PERSIST;
    if (allowed == 0u) {
        record.flags |= LUNA_LSON_FLAG_AUDIT;
    }
    record.attr_count = 4u;
    copy_text_field(record.kind, sizeof(record.kind), "link.trace");
    copy_text_field(record.scope, sizeof(record.scope), scope);
    copy_text_field(record.body, sizeof(record.body), message);
    observe_log_lson(message, &record);
}

static void device_write(const char *text) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    uint64_t size = 0u;
    while (text[size] != '\0') {
        size += 1u;
    }
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 62;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->actor_space = LUNA_SPACE_NETWORK;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
}

static uint32_t device_packet_write(const uint8_t *payload, uint64_t size, uint64_t actor_space) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 64;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->actor_space = actor_space;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_NET0;
    gate->buffer_addr = (uint64_t)(uintptr_t)payload;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    return gate->status;
}

static uint32_t device_packet_read(uint8_t *payload, uint64_t *size, uint64_t actor_space) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 65;
    gate->opcode = LUNA_DEVICE_READ;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->actor_space = actor_space;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_NET0;
    gate->buffer_addr = (uint64_t)(uintptr_t)payload;
    gate->buffer_size = LUNA_NETWORK_PACKET_BYTES;
    gate->size = 0u;
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    *size = gate->size;
    return gate->status;
}

static uint32_t device_query_net(struct luna_net_info *out, uint64_t actor_space) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes(out, sizeof(*out));
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 66;
    gate->opcode = LUNA_DEVICE_QUERY;
    gate->caller_space = LUNA_SPACE_NETWORK;
    gate->actor_space = actor_space;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_NET0;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = sizeof(*out);
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    return gate->status;
}

static uint32_t data_seed_object(struct luna_cid cid, uint32_t object_type, uint32_t object_flags, struct luna_object_ref *out) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 90;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_type = object_type;
    gate->object_flags = object_flags;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    out->low = gate->object_low;
    out->high = gate->object_high;
    return gate->status;
}

static uint32_t data_pour_span(struct luna_cid cid, struct luna_object_ref object, uint64_t offset, const void *buffer, uint64_t size) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    const void *source = buffer;
    void *stage = gate_stage_buffer(size);
    if (stage != 0 && buffer != 0 && size != 0u) {
        zero_bytes(stage, (size_t)size);
        copy_bytes(stage, buffer, (size_t)size);
        source = stage;
    }
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 91;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = offset;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)source;
    gate->buffer_size = size;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_draw_span(
    struct luna_cid cid,
    struct luna_object_ref object,
    uint64_t offset,
    void *buffer,
    uint64_t buffer_size,
    uint64_t *out_size,
    uint64_t *out_content_size
) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = gate_stage_buffer(buffer_size);
    void *target = buffer;
    zero_bytes(buffer, (size_t)buffer_size);
    if (stage != 0 && buffer_size != 0u) {
        zero_bytes(stage, (size_t)buffer_size);
        target = stage;
    }
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 92;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = offset;
    gate->size = buffer_size;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    if (stage != 0 && gate->status == LUNA_DATA_OK && gate->size != 0u) {
        copy_bytes(buffer, stage, (size_t)gate->size);
    }
    *out_size = gate->size;
    if (out_content_size != 0) {
        *out_content_size = gate->content_size;
    }
    return gate->status;
}

static uint32_t data_gather_set(struct luna_cid cid, struct luna_object_ref object, struct luna_object_ref *out_refs, uint64_t buffer_size, uint32_t *out_count) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = gate_stage_buffer(buffer_size);
    void *target = out_refs;
    zero_bytes(out_refs, (size_t)buffer_size);
    if (stage != 0 && buffer_size != 0u) {
        zero_bytes(stage, (size_t)buffer_size);
        target = stage;
    }
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 94;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    if (stage != 0 && gate->status == LUNA_DATA_OK && gate->result_count != 0u) {
        copy_bytes(out_refs, stage, (size_t)buffer_size);
    }
    *out_count = gate->result_count;
    return gate->status;
}

static uint32_t data_set_add_member(struct luna_cid cid, struct luna_object_ref set_object, struct luna_object_ref member) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    g_member_stage = member;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 95;
    gate->opcode = LUNA_DATA_SET_ADD_MEMBER;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_object.low;
    gate->object_high = set_object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_member_stage;
    gate->buffer_size = sizeof(g_member_stage);
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t persist_state(void) {
    g_state.magic = LUNALINK_STATE_MAGIC;
    g_state.version = LUNALINK_STATE_VERSION;
    g_state.peer_registry = g_peer_registry;
    if (g_state_object.low == 0u && g_state_object.high == 0u) {
        if (data_seed_object(g_data_seed_cid, LUNALINK_STATE_OBJECT_TYPE, 0u, &g_state_object) != LUNA_DATA_OK) {
            return LUNA_DATA_ERR_NO_SPACE;
        }
    }
    return data_pour_span(g_data_pour_cid, g_state_object, 0u, &g_state, sizeof(g_state));
}

static uint32_t ensure_state(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_state_object.low != 0u || g_state_object.high != 0u) {
        return LUNA_DATA_OK;
    }
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) == LUNA_DATA_OK) {
        for (uint32_t i = 0u; i < count; ++i) {
            struct lunalink_state_record state;
            uint64_t size = 0u;
            uint64_t content_size = 0u;
            zero_bytes(&state, sizeof(state));
            if (data_draw_span(g_data_draw_cid, refs[i], 0u, &state, sizeof(state), &size, &content_size) != LUNA_DATA_OK) {
                continue;
            }
            if (size < sizeof(state) || state.magic != LUNALINK_STATE_MAGIC || state.version != LUNALINK_STATE_VERSION) {
                continue;
            }
            g_state_object = refs[i];
            g_state = state;
            g_peer_registry = state.peer_registry;
            return LUNA_DATA_OK;
        }
    }
    if (data_seed_object(g_data_seed_cid, LUNALINK_SET_OBJECT_TYPE, 0u, &g_peer_registry) != LUNA_DATA_OK) {
        return LUNA_DATA_ERR_NO_SPACE;
    }
    g_state.next_peer_id = 1u;
    g_state.next_session_id = 1u;
    g_state.next_channel_id = 1u;
    return persist_state();
}

static struct lunalink_runtime_peer *find_runtime_peer_by_id(uint32_t peer_id) {
    for (uint32_t i = 0u; i < LUNALINK_PEER_CAPACITY; ++i) {
        if (g_peers[i].live == 1u && g_peers[i].peer.peer_id == peer_id) {
            return &g_peers[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_peer *find_runtime_peer_by_name(const char name[16]) {
    for (uint32_t i = 0u; i < LUNALINK_PEER_CAPACITY; ++i) {
        if (g_peers[i].live == 1u && name16_equal(g_peers[i].peer.name, name)) {
            return &g_peers[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_session *find_runtime_session(uint32_t session_id) {
    for (uint32_t i = 0u; i < LUNALINK_SESSION_CAPACITY; ++i) {
        if (g_sessions[i].live == 1u && g_sessions[i].session.session_id == session_id) {
            return &g_sessions[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_session *find_runtime_session_by_peer(uint32_t peer_id) {
    for (uint32_t i = 0u; i < LUNALINK_SESSION_CAPACITY; ++i) {
        if (g_sessions[i].live == 1u && g_sessions[i].session.peer_id == peer_id) {
            return &g_sessions[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_channel *find_runtime_channel(uint32_t channel_id) {
    for (uint32_t i = 0u; i < LUNALINK_CHANNEL_CAPACITY; ++i) {
        if (g_channels[i].live == 1u && g_channels[i].channel.channel_id == channel_id) {
            return &g_channels[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_peer *alloc_runtime_peer(void) {
    for (uint32_t i = 0u; i < LUNALINK_PEER_CAPACITY; ++i) {
        if (g_peers[i].live == 0u) {
            zero_bytes(&g_peers[i], sizeof(g_peers[i]));
            g_peers[i].live = 1u;
            return &g_peers[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_session *alloc_runtime_session(void) {
    for (uint32_t i = 0u; i < LUNALINK_SESSION_CAPACITY; ++i) {
        if (g_sessions[i].live == 0u) {
            zero_bytes(&g_sessions[i], sizeof(g_sessions[i]));
            g_sessions[i].live = 1u;
            return &g_sessions[i];
        }
    }
    return 0;
}

static struct lunalink_runtime_channel *alloc_runtime_channel(void) {
    for (uint32_t i = 0u; i < LUNALINK_CHANNEL_CAPACITY; ++i) {
        if (g_channels[i].live == 0u) {
            zero_bytes(&g_channels[i], sizeof(g_channels[i]));
            g_channels[i].live = 1u;
            return &g_channels[i];
        }
    }
    return 0;
}

static int pairing_is_trusted(uint32_t flags) {
    return (flags & LUNALINK_PAIRING_FLAG_TRUSTED) != 0u;
}

static uint64_t pairing_expected_response(uint32_t peer_id, uint64_t challenge) {
    return challenge ^ ((uint64_t)peer_id << 32) ^ 0x4C4C5052u;
}

static uint64_t issue_pairing_challenge(uint32_t peer_id) {
    uint64_t challenge = ((uint64_t)g_state.next_session_id << 32) ^ (uint64_t)peer_id ^ 0x50414952u;
    g_state.next_session_id += 1u;
    return challenge;
}

static void retire_runtime_peer_sessions(uint32_t peer_id) {
    for (uint32_t i = 0u; i < LUNALINK_CHANNEL_CAPACITY; ++i) {
        if (g_channels[i].live != 1u) {
            continue;
        }
        {
            struct lunalink_runtime_session *session = find_runtime_session(g_channels[i].channel.session_id);
            if (session != 0 && session->session.peer_id == peer_id) {
                g_channels[i].live = 0u;
            }
        }
    }
    for (uint32_t i = 0u; i < LUNALINK_SESSION_CAPACITY; ++i) {
        if (g_sessions[i].live == 1u && g_sessions[i].session.peer_id == peer_id) {
            g_sessions[i].live = 0u;
        }
    }
}

static uint32_t persist_pairing_record(struct luna_object_ref pairing_ref, const struct lunalink_pairing_record *record) {
    return data_pour_span(g_data_pour_cid, pairing_ref, 0u, record, sizeof(*record));
}

static uint32_t persist_session_resume_record(struct luna_object_ref resume_ref, const struct lunalink_session_resume_record *record) {
    return data_pour_span(g_data_pour_cid, resume_ref, 0u, record, sizeof(*record));
}

static uint32_t find_pairing_record_by_name(const char name[16], struct luna_object_ref *out_ref, struct lunalink_pairing_record *out_record) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if ((g_peer_registry.low == 0u && g_peer_registry.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_peer_registry, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0u;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct lunalink_pairing_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < LUNALINK_PAIRING_RECORD_MIN_SIZE ||
            record.magic != LUNALINK_PAIRING_MAGIC ||
            record.version != LUNALINK_PAIRING_VERSION) {
            continue;
        }
        if (!name16_equal(record.name, name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1u;
    }
    return 0u;
}

static uint32_t find_session_resume_by_peer(uint32_t peer_id, struct luna_object_ref *out_ref, struct lunalink_session_resume_record *out_record) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0u;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct lunalink_session_resume_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < LUNALINK_SESSION_RESUME_RECORD_MIN_SIZE ||
            record.magic != LUNALINK_SESSION_RESUME_MAGIC ||
            record.version != LUNALINK_SESSION_RESUME_VERSION) {
            continue;
        }
        if (record.peer_id != peer_id) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1u;
    }
    return 0u;
}

static void load_peer_registry(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_peer_registry.low == 0u && g_peer_registry.high == 0u) {
        return;
    }
    if (data_gather_set(g_data_gather_cid, g_peer_registry, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct lunalink_pairing_record record;
        struct lunalink_runtime_peer *peer_slot;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < LUNALINK_PAIRING_RECORD_MIN_SIZE ||
            record.magic != LUNALINK_PAIRING_MAGIC ||
            record.version != LUNALINK_PAIRING_VERSION) {
            continue;
        }
        peer_slot = alloc_runtime_peer();
        if (peer_slot == 0) {
            return;
        }
        peer_slot->peer.peer_id = record.peer_id;
        peer_slot->peer.flags = record.flags;
        peer_slot->peer.pairing_low = refs[i].low;
        peer_slot->peer.pairing_high = refs[i].high;
        peer_slot->peer.challenge = record.challenge;
        peer_slot->peer.proof = record.verified_identity;
        copy_bytes(peer_slot->peer.name, record.name, sizeof(record.name));
        (void)security_policy_sync(
            LUNA_POLICY_TYPE_PEER,
            record.peer_id,
            (pairing_is_trusted(record.flags) && record.verified_identity != 0u) ? LUNA_POLICY_STATE_ALLOW : LUNA_POLICY_STATE_REVOKE,
            record.flags,
            record.verified_identity
        );
        if (record.peer_id >= g_state.next_peer_id) {
            g_state.next_peer_id = record.peer_id + 1u;
        }
    }
}

static uint32_t handle_pair_peer(struct luna_network_gate *gate) {
    struct luna_link_pair_request request;
    struct lunalink_runtime_peer *existing;
    struct lunalink_runtime_peer *peer_slot;
    struct lunalink_pairing_record record;
    struct luna_object_ref pairing_ref;
    if (validate_network_policy(LUNA_CAP_NETWORK_PAIR, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_PAIR_PEER, 0u, 0u, 0u) != LUNA_GATE_OK) {
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(struct luna_link_peer)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes(&request, (const void *)(uintptr_t)gate->buffer_addr, sizeof(request));
    request.name[15] = '\0';
    existing = find_runtime_peer_by_name(request.name);
    if (existing == 0) {
        struct lunalink_pairing_record existing_record;
        struct luna_object_ref existing_ref;
        if (find_pairing_record_by_name(request.name, &existing_ref, &existing_record)) {
            existing = alloc_runtime_peer();
            if (existing == 0) {
                return LUNA_NETWORK_ERR_RANGE;
            }
            existing->peer.peer_id = existing_record.peer_id;
            existing->peer.flags = existing_record.flags;
            existing->peer.pairing_low = existing_ref.low;
            existing->peer.pairing_high = existing_ref.high;
            existing->peer.challenge = existing_record.challenge;
            existing->peer.proof = existing_record.verified_identity;
            copy_bytes(existing->peer.name, existing_record.name, sizeof(existing_record.name));
        }
    }
    if (existing != 0) {
        if (!pairing_is_trusted(existing->peer.flags)) {
            struct lunalink_pairing_record confirmed;
            struct luna_object_ref pairing_ref = {existing->peer.pairing_low, existing->peer.pairing_high};
            uint64_t size = 0u;
            uint64_t content_size = 0u;
            zero_bytes(&confirmed, sizeof(confirmed));
            if (data_draw_span(g_data_draw_cid, pairing_ref, 0u, &confirmed, sizeof(confirmed), &size, &content_size) != LUNA_DATA_OK ||
                size < LUNALINK_PAIRING_RECORD_MIN_SIZE ||
                confirmed.magic != LUNALINK_PAIRING_MAGIC ||
                confirmed.version != LUNALINK_PAIRING_VERSION) {
                return LUNA_NETWORK_ERR_BAD_STATE;
            }
            if ((request.flags & 1u) != 0u &&
                request.challenge == confirmed.challenge &&
                request.response == pairing_expected_response(confirmed.peer_id, confirmed.challenge)) {
                confirmed.flags |= LUNALINK_PAIRING_FLAG_TRUSTED;
                confirmed.flags &= ~(LUNALINK_PAIRING_FLAG_PENDING | LUNALINK_PAIRING_FLAG_CHALLENGE);
                confirmed.challenge = 0u;
                confirmed.verified_identity = request.response;
                if (persist_pairing_record(pairing_ref, &confirmed) != LUNA_DATA_OK) {
                    return LUNA_NETWORK_ERR_BAD_STATE;
                }
                existing->peer.flags = confirmed.flags;
                existing->peer.challenge = 0u;
                existing->peer.proof = confirmed.verified_identity;
                (void)security_policy_sync(LUNA_POLICY_TYPE_PEER, confirmed.peer_id, LUNA_POLICY_STATE_ALLOW, confirmed.flags, confirmed.verified_identity);
                observe_log("lunalink pair trust");
            } else {
                confirmed.flags |= LUNALINK_PAIRING_FLAG_CHALLENGE;
                confirmed.challenge = issue_pairing_challenge(confirmed.peer_id);
                confirmed.verified_identity = 0u;
                if (persist_pairing_record(pairing_ref, &confirmed) != LUNA_DATA_OK ||
                    persist_state() != LUNA_DATA_OK) {
                    return LUNA_NETWORK_ERR_BAD_STATE;
                }
                existing->peer.flags = confirmed.flags;
                existing->peer.challenge = confirmed.challenge;
                existing->peer.proof = 0u;
                (void)security_policy_sync(LUNA_POLICY_TYPE_PEER, confirmed.peer_id, LUNA_POLICY_STATE_REVOKE, confirmed.flags, 0u);
            }
        }
        copy_bytes((void *)(uintptr_t)gate->buffer_addr, &existing->peer, sizeof(existing->peer));
        gate->size = sizeof(existing->peer);
        gate->result_count = 1u;
        observe_log("lunalink pair hit");
        return LUNA_NETWORK_OK;
    }
    if (ensure_state() != LUNA_DATA_OK) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    zero_bytes(&record, sizeof(record));
    record.magic = LUNALINK_PAIRING_MAGIC;
    record.version = LUNALINK_PAIRING_VERSION;
    record.peer_id = g_state.next_peer_id++;
    record.flags = LUNALINK_PAIRING_FLAG_PENDING | LUNALINK_PAIRING_FLAG_CHALLENGE;
    record.challenge = issue_pairing_challenge(record.peer_id);
    record.verified_identity = 0u;
    copy_bytes(record.name, request.name, sizeof(record.name));
    if (data_seed_object(g_data_seed_cid, LUNALINK_PAIRING_OBJECT_TYPE, 0u, &pairing_ref) != LUNA_DATA_OK ||
        persist_pairing_record(pairing_ref, &record) != LUNA_DATA_OK ||
        data_set_add_member(g_data_pour_cid, g_peer_registry, pairing_ref) != LUNA_DATA_OK ||
        persist_state() != LUNA_DATA_OK) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    peer_slot = alloc_runtime_peer();
    if (peer_slot == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    peer_slot->peer.peer_id = record.peer_id;
    peer_slot->peer.flags = record.flags;
    peer_slot->peer.pairing_low = pairing_ref.low;
    peer_slot->peer.pairing_high = pairing_ref.high;
    peer_slot->peer.challenge = record.challenge;
    peer_slot->peer.proof = record.verified_identity;
    (void)security_policy_sync(LUNA_POLICY_TYPE_PEER, record.peer_id, LUNA_POLICY_STATE_REVOKE, record.flags, 0u);
    copy_bytes(peer_slot->peer.name, record.name, sizeof(record.name));
    copy_bytes((void *)(uintptr_t)gate->buffer_addr, &peer_slot->peer, sizeof(peer_slot->peer));
    gate->size = sizeof(peer_slot->peer);
    gate->result_count = 1u;
    observe_log("lunalink pair ok");
    return LUNA_NETWORK_OK;
}

static uint32_t handle_open_session(struct luna_network_gate *gate) {
    struct luna_link_session_request request;
    struct lunalink_runtime_peer *peer_slot;
    struct lunalink_runtime_session *session_slot;
    struct lunalink_pairing_record pairing_record;
    struct lunalink_session_resume_record resume;
    struct luna_object_ref pairing_ref;
    struct luna_object_ref resume_ref = {0u, 0u};
    uint64_t size = 0u;
    uint64_t content_size = 0u;
    if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(struct luna_link_session)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes(&request, (const void *)(uintptr_t)gate->buffer_addr, sizeof(request));
    peer_slot = find_runtime_peer_by_id(request.peer_id);
    if (peer_slot == 0) {
        return LUNA_NETWORK_ERR_NOT_FOUND;
    }
    if (!pairing_is_trusted(peer_slot->peer.flags)) {
        observe_log("lunalink sess untrusted");
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    pairing_ref.low = peer_slot->peer.pairing_low;
    pairing_ref.high = peer_slot->peer.pairing_high;
    zero_bytes(&pairing_record, sizeof(pairing_record));
    if (data_draw_span(g_data_draw_cid, pairing_ref, 0u, &pairing_record, sizeof(pairing_record), &size, &content_size) != LUNA_DATA_OK ||
        size < LUNALINK_PAIRING_RECORD_MIN_SIZE ||
        pairing_record.magic != LUNALINK_PAIRING_MAGIC ||
        pairing_record.version != LUNALINK_PAIRING_VERSION ||
        !pairing_is_trusted(pairing_record.flags) ||
        pairing_record.verified_identity == 0u) {
        observe_log("lunalink sess ident");
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if (!name16_matches_text(pairing_record.name, "loop")) {
        uint32_t policy_state = 0u;
        uint32_t policy_status =
            security_policy_query(LUNA_POLICY_TYPE_PEER, pairing_record.peer_id, pairing_record.verified_identity, &policy_state, 0, 0);
        if ((policy_status != LUNA_GATE_OK || policy_state != LUNA_POLICY_STATE_ALLOW) &&
            security_policy_sync(
                LUNA_POLICY_TYPE_PEER,
                pairing_record.peer_id,
                LUNA_POLICY_STATE_ALLOW,
                pairing_record.flags,
                pairing_record.verified_identity
            ) == LUNA_GATE_OK) {
            policy_state = 0u;
            policy_status =
                security_policy_query(LUNA_POLICY_TYPE_PEER, pairing_record.peer_id, pairing_record.verified_identity, &policy_state, 0, 0);
        }
        if (policy_status != LUNA_GATE_OK || policy_state != LUNA_POLICY_STATE_ALLOW) {
            observe_log("lunalink sess policy");
        }
    }
    peer_slot->peer.proof = pairing_record.verified_identity;
    if (ensure_state() != LUNA_DATA_OK) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if (find_runtime_session_by_peer(request.peer_id) != 0) {
        observe_log("lunalink sess replay");
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    session_slot = alloc_runtime_session();
    if (session_slot == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(&resume, sizeof(resume));
    if (!find_session_resume_by_peer(request.peer_id, &resume_ref, &resume)) {
        resume.magic = LUNALINK_SESSION_RESUME_MAGIC;
        resume.version = LUNALINK_SESSION_RESUME_VERSION;
        resume.peer_id = request.peer_id;
        resume.flags = LUNALINK_RESUME_FLAG_CONSUMED;
        resume.reserved0 = 0u;
        resume.verified_identity = pairing_record.verified_identity;
        if (data_seed_object(g_data_seed_cid, LUNALINK_SESSION_RESUME_OBJECT_TYPE, 0u, &resume_ref) != LUNA_DATA_OK ||
            persist_session_resume_record(resume_ref, &resume) != LUNA_DATA_OK ||
            persist_state() != LUNA_DATA_OK) {
            session_slot->live = 0u;
            return LUNA_NETWORK_ERR_BAD_STATE;
        }
    } else if (resume.verified_identity != 0u && resume.verified_identity != pairing_record.verified_identity) {
        session_slot->live = 0u;
        observe_log("lunalink sess drift");
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if ((resume.flags & LUNALINK_RESUME_FLAG_ISSUED) != 0u) {
        resume.flags &= ~LUNALINK_RESUME_FLAG_ISSUED;
        resume.flags |= LUNALINK_RESUME_FLAG_CONSUMED;
        if (persist_session_resume_record(resume_ref, &resume) != LUNA_DATA_OK) {
            session_slot->live = 0u;
            return LUNA_NETWORK_ERR_BAD_STATE;
        }
    }
    retire_runtime_peer_sessions(request.peer_id);
    resume.session_id = g_state.next_session_id++;
    resume.reserved0 = resume.reserved0 + 1u;
    resume.flags = LUNALINK_RESUME_FLAG_ISSUED;
    resume.verified_identity = pairing_record.verified_identity;
    if (persist_session_resume_record(resume_ref, &resume) != LUNA_DATA_OK ||
        persist_state() != LUNA_DATA_OK) {
        session_slot->live = 0u;
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    session_slot->session.session_id = resume.session_id;
    session_slot->session.peer_id = request.peer_id;
    session_slot->session.flags = LUNALINK_RESUME_FLAG_ISSUED;
    session_slot->session.resume_low = resume_ref.low;
    session_slot->session.resume_high = resume_ref.high;
    if (validate_network_policy(
        LUNA_CAP_NETWORK_SESSION,
        gate->cid_low,
        gate->cid_high,
        gate->caller_space,
        gate->actor_space,
        LUNA_NETWORK_OPEN_SESSION,
        session_slot->session.session_id,
        0u,
        0u
    ) != LUNA_GATE_OK) {
        session_slot->live = 0u;
        observe_link_trace("session", "link.trace type=session denied", session_slot->session.session_id, 0u, gate->actor_space, 0u);
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    copy_bytes((void *)(uintptr_t)gate->buffer_addr, &session_slot->session, sizeof(session_slot->session));
    gate->size = sizeof(session_slot->session);
    gate->result_count = 1u;
    observe_link_trace("session", "link.trace type=session", session_slot->session.session_id, 0u, gate->actor_space, 1u);
    observe_log("lunalink sess open");
    return LUNA_NETWORK_OK;
}

static uint32_t handle_open_channel(struct luna_network_gate *gate) {
    struct luna_link_channel_request request;
    struct lunalink_runtime_session *session_slot;
    struct lunalink_runtime_channel *channel_slot;
    if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(struct luna_link_channel)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes(&request, (const void *)(uintptr_t)gate->buffer_addr, sizeof(request));
    if (request.session_id != gate->session_id) {
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    session_slot = find_runtime_session(request.session_id);
    if (session_slot == 0) {
        return LUNA_NETWORK_ERR_NOT_FOUND;
    }
    channel_slot = alloc_runtime_channel();
    if (channel_slot == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    channel_slot->channel.channel_id = g_state.next_channel_id++;
    channel_slot->channel.session_id = request.session_id;
    channel_slot->channel.kind = request.kind;
    channel_slot->channel.transfer_class = request.transfer_class;
    if (validate_network_policy(
        LUNA_CAP_NETWORK_SESSION,
        gate->cid_low,
        gate->cid_high,
        gate->caller_space,
        gate->actor_space,
        LUNA_NETWORK_OPEN_CHANNEL,
        channel_slot->channel.session_id,
        channel_slot->channel.channel_id,
        0u
    ) != LUNA_GATE_OK) {
        channel_slot->live = 0u;
        observe_link_trace("channel", "link.trace type=channel denied", channel_slot->channel.session_id, channel_slot->channel.channel_id, gate->actor_space, 0u);
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    (void)persist_state();
    copy_bytes((void *)(uintptr_t)gate->buffer_addr, &channel_slot->channel, sizeof(channel_slot->channel));
    gate->size = sizeof(channel_slot->channel);
    gate->result_count = 1u;
    observe_link_trace("channel", "link.trace type=channel", channel_slot->channel.session_id, channel_slot->channel.channel_id, gate->actor_space, 1u);
    observe_log("lunalink chan open");
    return LUNA_NETWORK_OK;
}

static uint32_t handle_send_channel(struct luna_network_gate *gate) {
    struct luna_link_send_request request;
    struct lunalink_runtime_channel *channel_slot;
    struct lunalink_runtime_session *session_slot;
    struct lunalink_runtime_peer *peer_slot;
    struct lunalink_packet packet;
    uint64_t total_size;
    if (validate_network_policy(LUNA_CAP_NETWORK_SEND, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_SEND_CHANNEL, gate->session_id, gate->channel_id, 0u) != LUNA_GATE_OK) {
        observe_link_trace("send", "link.trace type=send denied", gate->session_id, gate->channel_id, gate->actor_space, 0u);
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(request)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes(&request, (const void *)(uintptr_t)gate->buffer_addr, sizeof(request));
    if (request.channel_id != gate->channel_id) {
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    channel_slot = find_runtime_channel(request.channel_id);
    if (channel_slot == 0) {
        return LUNA_NETWORK_ERR_NOT_FOUND;
    }
    session_slot = find_runtime_session(channel_slot->channel.session_id);
    if (session_slot == 0) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    peer_slot = find_runtime_peer_by_id(session_slot->session.peer_id);
    if (peer_slot == 0) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    total_size = sizeof(packet) + request.payload_size;
    if (request.payload_addr == 0u || request.payload_size == 0u || total_size > sizeof(g_packet)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    packet.magic = LUNALINK_PACKET_MAGIC;
    packet.peer_id = session_slot->session.peer_id;
    packet.session_id = session_slot->session.session_id;
    packet.channel_id = channel_slot->channel.channel_id;
    packet.payload_size = (uint32_t)request.payload_size;
    copy_bytes(g_packet, &packet, sizeof(packet));
    copy_bytes(g_packet + sizeof(packet), (const void *)(uintptr_t)request.payload_addr, (size_t)request.payload_size);
    if (device_packet_write(g_packet, total_size, gate->actor_space) != LUNA_DEVICE_OK) {
        observe_log("lunalink send err");
        return LUNA_NETWORK_ERR_RANGE;
    }
    if (!name16_matches_text(peer_slot->peer.name, "loop")) {
        copy_bytes(g_last_external_tx, g_packet, (size_t)total_size);
        g_last_external_tx_size = total_size;
        g_last_external_tx_live = 1u;
    } else {
        g_last_external_tx_live = 0u;
        g_last_external_tx_size = 0u;
    }
    gate->size = request.payload_size;
    gate->result_count = 1u;
    observe_link_trace("send", "link.trace type=send", session_slot->session.session_id, channel_slot->channel.channel_id, gate->actor_space, 1u);
    return LUNA_NETWORK_OK;
}

static uint32_t handle_recv_channel(struct luna_network_gate *gate) {
    struct lunalink_runtime_channel *channel_slot;
    struct lunalink_runtime_session *session_slot;
    struct lunalink_runtime_peer *peer_slot;
    struct lunalink_packet packet;
    uint32_t channel_id = gate->channel_id != 0u ? gate->channel_id : (uint32_t)gate->flags;
    uint64_t packet_size = 0u;
    if (validate_network_policy(LUNA_CAP_NETWORK_RECV, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_RECV_CHANNEL, gate->session_id, channel_id, 0u) != LUNA_GATE_OK) {
        observe_link_trace("recv", "link.trace type=recv denied", gate->session_id, channel_id, gate->actor_space, 0u);
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    if (gate->buffer_addr == 0u || channel_id == 0u) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    channel_slot = find_runtime_channel(channel_id);
    if (channel_slot == 0) {
        return LUNA_NETWORK_ERR_NOT_FOUND;
    }
    session_slot = find_runtime_session(channel_slot->channel.session_id);
    if (session_slot == 0) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    peer_slot = find_runtime_peer_by_id(session_slot->session.peer_id);
    if (peer_slot == 0) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if (device_packet_read(g_packet, &packet_size, gate->actor_space) != LUNA_DEVICE_OK) {
        observe_log("lunalink recv err");
        return LUNA_NETWORK_ERR_RANGE;
    }
    if (packet_size == 0u) {
        return LUNA_NETWORK_ERR_EMPTY;
    }
    if (packet_size < sizeof(packet)) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if (!name16_matches_text(peer_slot->peer.name, "loop") &&
        g_last_external_tx_live != 0u &&
        packet_size == g_last_external_tx_size &&
        bytes_equal(g_packet, g_last_external_tx, (size_t)packet_size)) {
        g_last_external_tx_live = 0u;
        g_last_external_tx_size = 0u;
        return LUNA_NETWORK_ERR_EMPTY;
    }
    copy_bytes(&packet, g_packet, sizeof(packet));
    if (packet.magic != LUNALINK_PACKET_MAGIC || packet.channel_id != channel_id) {
        return LUNA_NETWORK_ERR_BAD_STATE;
    }
    if ((uint64_t)packet.payload_size + sizeof(packet) > packet_size || gate->buffer_size < packet.payload_size) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes((void *)(uintptr_t)gate->buffer_addr, g_packet + sizeof(packet), packet.payload_size);
    gate->size = packet.payload_size;
    gate->result_count = 1u;
    observe_link_trace("recv", "link.trace type=recv", session_slot->session.session_id, channel_id, gate->actor_space, 1u);
    return LUNA_NETWORK_OK;
}

static uint32_t handle_get_info(struct luna_network_gate *gate) {
    struct luna_net_info info;
    if (validate_network_policy(LUNA_CAP_NETWORK_RECV, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_GET_INFO, 0u, 0u, 0u) != LUNA_GATE_OK) {
        return LUNA_NETWORK_ERR_INVALID_CAP;
    }
    if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(info)) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    if (device_query_net(&info, gate->actor_space) != LUNA_DEVICE_OK) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    copy_bytes((void *)(uintptr_t)gate->buffer_addr, &info, sizeof(info));
    gate->size = sizeof(info);
    gate->result_count = 1u;
    return LUNA_NETWORK_OK;
}

void SYSV_ABI network_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) != LUNA_GATE_OK) {
        return;
    }
    (void)request_capability(LUNA_CAP_DATA_SEED, &g_data_seed_cid);
    (void)request_capability(LUNA_CAP_DATA_POUR, &g_data_pour_cid);
    (void)request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid);
    (void)request_capability(LUNA_CAP_DATA_GATHER, &g_data_gather_cid);
    (void)request_capability(LUNA_CAP_OBSERVE_LOG, &g_observe_log_cid);
    if (g_data_seed_cid.low != 0u && g_data_pour_cid.low != 0u && g_data_draw_cid.low != 0u && g_data_gather_cid.low != 0u) {
        (void)ensure_state();
        load_peer_registry();
    }
    device_write("[NETWORK] lunalink ready\r\n");
    /* OBSERVE is spawned after NETWORK during bring-up. */
}

void SYSV_ABI network_entry_gate(struct luna_network_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_NETWORK_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_NETWORK_SEND_PACKET) {
        if (validate_network_policy(LUNA_CAP_NETWORK_SEND, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_SEND_PACKET, 0u, 0u, LUNA_NETWORK_POLICY_RAW_PACKET) != LUNA_GATE_OK) {
            return;
        }
        if (gate->size > gate->buffer_size || gate->size > sizeof(g_packet)) {
            gate->status = LUNA_NETWORK_ERR_RANGE;
            return;
        }
        copy_bytes(g_packet, (const void *)(uintptr_t)gate->buffer_addr, (size_t)gate->size);
        gate->status = device_packet_write(g_packet, gate->size, gate->actor_space) == LUNA_DEVICE_OK ? LUNA_NETWORK_OK : LUNA_NETWORK_ERR_RANGE;
        return;
    }
    if (gate->opcode == LUNA_NETWORK_RECV_PACKET) {
        if (validate_network_policy(LUNA_CAP_NETWORK_RECV, gate->cid_low, gate->cid_high, gate->caller_space, gate->actor_space, LUNA_NETWORK_RECV_PACKET, 0u, 0u, LUNA_NETWORK_POLICY_RAW_PACKET) != LUNA_GATE_OK) {
            return;
        }
        if (device_packet_read(g_packet, &gate->size, gate->actor_space) != LUNA_DEVICE_OK) {
            gate->status = LUNA_NETWORK_ERR_RANGE;
            return;
        }
        if (gate->size == 0u) {
            gate->status = LUNA_NETWORK_ERR_EMPTY;
            return;
        }
        if (gate->buffer_size < gate->size) {
            gate->status = LUNA_NETWORK_ERR_RANGE;
            return;
        }
        copy_bytes((void *)(uintptr_t)gate->buffer_addr, g_packet, (size_t)gate->size);
        gate->result_count = 1u;
        gate->status = LUNA_NETWORK_OK;
        return;
    }
    if (gate->opcode == LUNA_NETWORK_PAIR_PEER) {
        gate->status = handle_pair_peer(gate);
        return;
    }
    if (gate->opcode == LUNA_NETWORK_OPEN_SESSION) {
        gate->status = handle_open_session(gate);
        return;
    }
    if (gate->opcode == LUNA_NETWORK_OPEN_CHANNEL) {
        gate->status = handle_open_channel(gate);
        return;
    }
    if (gate->opcode == LUNA_NETWORK_SEND_CHANNEL) {
        gate->status = handle_send_channel(gate);
        return;
    }
    if (gate->opcode == LUNA_NETWORK_RECV_CHANNEL) {
        gate->status = handle_recv_channel(gate);
        return;
    }
    if (gate->opcode == LUNA_NETWORK_GET_INFO) {
        gate->status = handle_get_info(gate);
        return;
    }
}
