#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

#define LUNA_PROGRAM_APP_MAGIC 0x50414E55u
#define LUNA_PROGRAM_APP_VERSION 1u
#define LUNA_PROGRAM_BUNDLE_MAGIC 0x4C554E42u
#define LUNA_PROGRAM_BUNDLE_VERSION_V2 2u
#define LUNA_PROGRAM_BUNDLE_SIGNATURE_SEED 0x9A11D00D55AA7711ull
#define LUNA_DATA_OBJECT_TYPE_SET 0x4C534554u
#define LUNA_DATA_OBJECT_TYPE_LUNA_APP 0x4C554E41u
#define LUNA_DATA_OBJECT_TYPE_PACKAGE_STATE 0x504B4753u
#define LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL 0x504B4749u
#define LUNA_DATA_OBJECT_TYPE_PACKAGE_INDEX 0x504B474Eu
#define LUNA_DATA_OBJECT_TYPE_TRUSTED_SIGNER 0x5452474Eu
#define LUNA_PACKAGE_STATE_MAGIC 0x504B4753u
#define LUNA_PACKAGE_STATE_VERSION 5u
#define LUNA_PACKAGE_STATE_VERSION_V4 4u
#define LUNA_PACKAGE_STATE_VERSION_LEGACY 1u
#define LUNA_PACKAGE_STATE_VERSION_V2 2u
#define LUNA_PACKAGE_STATE_VERSION_V3 3u
#define LUNA_PACKAGE_INSTALL_MAGIC 0x504B4749u
#define LUNA_PACKAGE_INSTALL_VERSION 1u
#define LUNA_PACKAGE_INDEX_MAGIC 0x504B474Eu
#define LUNA_PACKAGE_INDEX_VERSION 1u
#define LUNA_DATA_OBJECT_TYPE_UPDATE_TXN 0x55505458u
#define LUNA_DATA_OBJECT_TYPE_TRUSTED_SOURCE 0x54525354u
#define LUNA_PACKAGE_UPDATE_TXN_MAGIC 0x55505458u
#define LUNA_PACKAGE_UPDATE_TXN_VERSION 1u
#define LUNA_PACKAGE_TRUSTED_SOURCE_MAGIC 0x54525354u
#define LUNA_PACKAGE_TRUSTED_SOURCE_VERSION 1u
#define LUNA_PACKAGE_TRUSTED_SIGNER_MAGIC 0x5452474Eu
#define LUNA_PACKAGE_TRUSTED_SIGNER_VERSION 1u
#define LUNA_PACKAGE_UPDATE_KIND_INSTALL 1u
#define LUNA_PACKAGE_UPDATE_KIND_REPLACE 2u
#define LUNA_PACKAGE_UPDATE_KIND_REMOVE 3u
#define LUNA_SET_MAGIC 0x5345544Cu
#define LUNA_SET_VERSION 1u

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);

struct luna_app_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_offset;
    uint32_t capability_count;
    uint32_t reserved0;
    char name[16];
    uint64_t capability_keys[4];
};

struct luna_bundle_header {
    uint32_t magic;
    uint32_t version;
    uint64_t bundle_id;
    uint64_t source_id;
    uint64_t app_version;
    uint64_t integrity_check;
    uint64_t header_bytes;
    uint64_t entry_offset;
    uint64_t signer_id;
    uint64_t signature_check;
    uint32_t capability_count;
    uint32_t flags;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t sdk_major;
    uint16_t sdk_minor;
    uint32_t min_proto_version;
    uint32_t max_proto_version;
    char name[16];
    uint64_t capability_keys[4];
};

struct luna_app_metadata {
    char name[16];
    uint64_t bundle_id;
    uint64_t source_id;
    uint64_t app_version;
    uint64_t integrity_check;
    uint64_t header_bytes;
    uint64_t entry_offset;
    uint64_t signer_id;
    uint64_t signature_check;
    uint32_t capability_count;
    uint32_t flags;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t sdk_major;
    uint16_t sdk_minor;
    uint32_t min_proto_version;
    uint32_t max_proto_version;
    uint64_t capability_keys[4];
};

struct luna_package_state_record_v1 {
    uint32_t magic;
    uint32_t version;
    struct luna_object_ref installed_apps_set;
};

struct luna_package_state_record_v2 {
    uint32_t magic;
    uint32_t version;
    struct luna_object_ref installed_apps_set;
    struct luna_object_ref install_records_set;
    struct luna_object_ref app_index_set;
};

struct luna_package_state_record_v3 {
    uint32_t magic;
    uint32_t version;
    struct luna_object_ref installed_apps_set;
    struct luna_object_ref install_records_set;
    struct luna_object_ref app_index_set;
    struct luna_object_ref update_txn_object;
    struct luna_object_ref rollback_refs_set;
};

struct luna_package_state_record {
    uint32_t magic;
    uint32_t version;
    struct luna_object_ref installed_apps_set;
    struct luna_object_ref install_records_set;
    struct luna_object_ref app_index_set;
    struct luna_object_ref trusted_source_set;
    struct luna_object_ref trusted_signer_set;
    struct luna_object_ref update_txn_log_set;
    uint64_t next_txn_id;
    struct luna_object_ref update_txn_object;
    struct luna_object_ref rollback_refs_set;
};

struct luna_package_state_record_v4 {
    uint32_t magic;
    uint32_t version;
    struct luna_object_ref installed_apps_set;
    struct luna_object_ref install_records_set;
    struct luna_object_ref app_index_set;
    struct luna_object_ref trusted_source_set;
    struct luna_object_ref update_txn_log_set;
    uint64_t next_txn_id;
    struct luna_object_ref update_txn_object;
    struct luna_object_ref rollback_refs_set;
};

struct luna_package_install_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t app_version;
    struct luna_object_ref app_object;
    char name[16];
};

struct luna_package_index_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t app_version;
    struct luna_object_ref app_object;
    struct luna_object_ref install_object;
    char name[16];
};

struct luna_package_update_txn_record {
    uint32_t magic;
    uint32_t version;
    uint64_t txn_id;
    uint32_t state;
    uint32_t kind;
    uint64_t current_version;
    uint64_t target_version;
    struct luna_object_ref old_app_object;
    struct luna_object_ref old_install_object;
    struct luna_object_ref old_index_object;
    struct luna_object_ref new_app_object;
    struct luna_object_ref new_install_object;
    struct luna_object_ref new_index_object;
    char name[16];
};

struct luna_package_trusted_source_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t source_id;
    uint64_t signer_id;
};

struct luna_package_trusted_signer_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t signer_id;
};

struct luna_set_header {
    uint32_t magic;
    uint32_t version;
    uint32_t member_count;
    uint32_t flags;
};

struct builtin_package_spec {
    const char *base_name;
    const char *label;
    uint32_t icon_x;
    uint32_t icon_y;
    uint32_t flags;
    uint32_t preferred_x;
    uint32_t preferred_y;
    uint32_t preferred_width;
    uint32_t preferred_height;
    uint32_t window_role;
    uint64_t version;
};

struct luna_embedded_app {
    uint64_t base;
    uint64_t size;
};

struct runtime_package_entry {
    uint32_t active;
    uint32_t reserved0;
    struct luna_object_ref app_object;
    struct luna_app_metadata meta;
};

static const struct builtin_package_spec g_builtin_specs[] = {
    { "hello", "Settings", 3u, 20u, LUNA_PACKAGE_FLAG_PINNED, 40u, 14u, 17u, 7u, LUNA_WINDOW_ROLE_UTILITY, 1u },
    { "files", "Files", 3u, 4u, LUNA_PACKAGE_FLAG_PINNED | LUNA_PACKAGE_FLAG_STARTUP, 2u, 2u, 26u, 9u, LUNA_WINDOW_ROLE_DOCUMENT, 1u },
    { "notes", "Notes", 3u, 8u, LUNA_PACKAGE_FLAG_PINNED, 29u, 2u, 27u, 9u, LUNA_WINDOW_ROLE_DOCUMENT, 1u },
    { "guard", "Guard", 3u, 12u, LUNA_PACKAGE_FLAG_PINNED, 57u, 2u, 21u, 11u, LUNA_WINDOW_ROLE_UTILITY, 1u },
    { "console", "Console", 3u, 16u, LUNA_PACKAGE_FLAG_PINNED, 2u, 12u, 37u, 11u, LUNA_WINDOW_ROLE_CONSOLE, 1u },
};

static struct luna_package_record g_catalog[LUNA_PACKAGE_CAPACITY];
static struct runtime_package_entry g_runtime_packages[LUNA_PACKAGE_CAPACITY];
static uint8_t g_app_metadata_blob[16384];
static uint8_t g_package_blob_stage[2048];
static uint8_t g_set_blob[sizeof(struct luna_set_header) + (sizeof(struct luna_object_ref) * LUNA_DATA_OBJECT_CAPACITY)];
static struct luna_package_install_record g_install_record_stage;
static struct luna_package_index_record g_index_record_stage;
static struct luna_object_ref g_member_stage;
static struct luna_cid g_device_write_cid = {0, 0};
static struct luna_cid g_package_install_cid = {0, 0};
static struct luna_cid g_data_seed_cid = {0, 0};
static struct luna_cid g_data_pour_cid = {0, 0};
static struct luna_cid g_data_draw_cid = {0, 0};
static struct luna_cid g_data_shred_cid = {0, 0};
static struct luna_cid g_data_gather_cid = {0, 0};
static struct luna_cid g_data_query_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;
static struct luna_object_ref g_installed_apps_set = {0, 0};
static struct luna_object_ref g_install_records_set = {0, 0};
static struct luna_object_ref g_app_index_set = {0, 0};
static struct luna_object_ref g_trusted_source_set = {0, 0};
static struct luna_object_ref g_trusted_signer_set = {0, 0};
static struct luna_object_ref g_update_txn_log_set = {0, 0};
static uint64_t g_next_txn_id = 1u;
static struct luna_object_ref g_update_txn_object = {0, 0};
static struct luna_object_ref g_rollback_refs_set = {0, 0};
static struct luna_object_ref g_package_state_object = {0, 0};

static int ensure_package_sets(void);
static uint32_t query_catalog_rows(struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count);
static void fill_record_from_query_row(struct luna_package_record *record, const struct luna_query_row *row);
static int find_catalog_row_by_name(const char request_name[16], struct luna_query_row *out_row);
static int find_index_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_index_record *out_record
);
static int find_install_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_install_record *out_record
);
static int find_stale_app_object_by_name(const char request_name[16], struct luna_object_ref *out_ref);
static int find_stale_install_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_install_record *out_record
);
static int find_stale_index_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_index_record *out_record
);
static int set_contains_ref(struct luna_object_ref set_object, struct luna_object_ref target);
static int catalog_contains_name(const char request_name[16]);
static int catalog_has_meta_name(const char request_name[16]);
static int rewrite_set_add_member(struct luna_object_ref set_object, struct luna_object_ref member);
static int rebuild_installed_apps_set(void);
static int find_installed_app_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
);
static int find_any_app_object_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
);
static int find_runtime_package_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
);
static void upsert_runtime_package(const struct luna_app_metadata *meta, struct luna_object_ref app_object);
static void remove_runtime_package(const char request_name[16]);
static int audit_install_registration(
    const struct luna_app_metadata *meta,
    struct luna_object_ref app_object,
    struct luna_object_ref install_object,
    struct luna_object_ref index_object
);
static int purge_stale_package_name(const char request_name[16]);
static int audit_remove_registration(const char request_name[16]);
static int persist_package_state(void);
static int persist_update_txn(struct luna_package_update_txn_record *record);
static int signer_is_trusted(uint64_t signer_id);
static int source_is_trusted(uint64_t source_id, uint64_t signer_id);
static int read_app_metadata_trace(struct luna_object_ref object, struct luna_app_metadata *out_meta, int trace_failures);
static int read_app_metadata(struct luna_object_ref object, struct luna_app_metadata *out_meta);
static uint32_t build_installed_catalog(void);
static void device_write(const char *text);
static void *data_stage_buffer(uint64_t size);
static void append_u64_hex_fixed(char *out, uint64_t value, uint32_t digits);
static void device_write_hex_value(const char *prefix, uint64_t value);
static void device_write_ref_line(const char *prefix, struct luna_object_ref ref);
static uint32_t data_pour_span(struct luna_cid cid, struct luna_object_ref object, uint64_t offset, const void *buffer, uint64_t size);
static uint32_t data_draw_span(
    struct luna_cid cid,
    struct luna_object_ref object,
    uint64_t offset,
    void *buffer,
    uint64_t buffer_size,
    uint64_t *out_size,
    uint64_t *out_content_size
);
static uint32_t data_gather_set(
    struct luna_cid cid,
    struct luna_object_ref object,
    struct luna_object_ref *out_refs,
    uint64_t buffer_size,
    uint32_t *out_count
);

static uint32_t count_set_members(struct luna_object_ref set_object) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (set_object.low == 0u && set_object.high == 0u) {
        return 0u;
    }
    if (data_gather_set(g_data_gather_cid, set_object, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0u;
    }
    return count;
}

static int set_contains_ref(struct luna_object_ref set_object, struct luna_object_ref target) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;

    if ((set_object.low == 0u && set_object.high == 0u) ||
        (target.low == 0u && target.high == 0u)) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, set_object, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (refs[i].low == target.low && refs[i].high == target.high) {
            return 1;
        }
    }
    return 0;
}

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

static size_t text_length16(const char text[16]) {
    size_t i = 0u;
    while (i < 16u && text[i] != '\0') {
        i += 1u;
    }
    return i;
}

static void copy_name16(char out[16], const char *text) {
    size_t i = 0u;
    while (i < 15u && text[i] != '\0') {
        out[i] = text[i];
        i += 1u;
    }
    while (i < 16u) {
        out[i] = '\0';
        i += 1u;
    }
}

static void copy_name_with_suffix(char out[16], const char base[16], const char *suffix) {
    size_t i = 0u;
    size_t j = 0u;
    while (i < 15u && base[i] != '\0') {
        out[i] = base[i];
        i += 1u;
    }
    while (i < 15u && suffix[j] != '\0') {
        out[i] = suffix[j];
        i += 1u;
        j += 1u;
    }
    while (i < 16u) {
        out[i] = '\0';
        i += 1u;
    }
}

static int app_name_matches(const char base_name[16], const char request_name[16]) {
    size_t base_len = text_length16(base_name);
    size_t request_len = text_length16(request_name);
    if (base_len == request_len) {
        for (size_t i = 0; i < base_len; ++i) {
            if (base_name[i] != request_name[i]) {
                return 0;
            }
        }
        return 1;
    }
    if (request_len == base_len + 5u) {
        for (size_t i = 0; i < base_len; ++i) {
            if (base_name[i] != request_name[i]) {
                return 0;
            }
        }
        return request_name[base_len] == '.' &&
               request_name[base_len + 1u] == 'l' &&
               request_name[base_len + 2u] == 'u' &&
               request_name[base_len + 3u] == 'n' &&
               request_name[base_len + 4u] == 'a';
    }
    if (request_len == base_len + 3u) {
        for (size_t i = 0; i < base_len; ++i) {
            if (base_name[i] != request_name[i]) {
                return 0;
            }
        }
        return request_name[base_len] == '.' &&
               request_name[base_len + 1u] == 'l' &&
               request_name[base_len + 2u] == 'a';
    }
    return 0;
}

static int catalog_contains_name(const char request_name[16]) {
    uint32_t count = build_installed_catalog();

    for (uint32_t i = 0u; i < count && i < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_catalog[i].installed == 0u) {
            continue;
        }
        if (app_name_matches(g_catalog[i].name, request_name) ||
            app_name_matches(request_name, g_catalog[i].name)) {
            return 1;
        }
    }
    return 0;
}

static int catalog_has_meta_name(const char request_name[16]) {
    for (uint32_t i = 0u; i < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_catalog[i].installed == 0u) {
            continue;
        }
        if (app_name_matches(request_name, g_catalog[i].name) ||
            app_name_matches(g_catalog[i].name, request_name)) {
            return 1;
        }
    }
    return 0;
}

static int rewrite_set_add_member(struct luna_object_ref set_object, struct luna_object_ref member) {
    struct luna_set_header *header = (struct luna_set_header *)g_set_blob;
    struct luna_object_ref *members = (struct luna_object_ref *)(g_set_blob + sizeof(struct luna_set_header));
    uint64_t size = 0u;
    uint64_t content_size = 0u;

    if ((set_object.low == 0u && set_object.high == 0u) ||
        (member.low == 0u && member.high == 0u)) {
        return 0;
    }
    zero_bytes(g_set_blob, sizeof(g_set_blob));
    if (data_draw_span(g_data_draw_cid, set_object, 0u, g_set_blob, sizeof(g_set_blob), &size, &content_size) != LUNA_DATA_OK ||
        size < sizeof(struct luna_set_header) ||
        header->magic != LUNA_SET_MAGIC ||
        header->version != LUNA_SET_VERSION ||
        header->member_count >= LUNA_DATA_OBJECT_CAPACITY) {
        return 0;
    }
    for (uint32_t i = 0u; i < header->member_count; ++i) {
        if (members[i].low == member.low && members[i].high == member.high) {
            return 1;
        }
    }
    members[header->member_count] = member;
    header->member_count += 1u;
    size = sizeof(struct luna_set_header) + ((uint64_t)header->member_count * sizeof(struct luna_object_ref));
    return data_pour_span(g_data_pour_cid, set_object, 0u, g_set_blob, size) == LUNA_DATA_OK;
}

static int find_installed_app_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;

    if ((g_installed_apps_set.low == 0u && g_installed_apps_set.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_installed_apps_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_app_metadata meta;
        zero_bytes(&meta, sizeof(meta));
        if (!read_app_metadata_trace(refs[i], &meta, 0) || !app_name_matches(meta.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_meta != 0) {
            *out_meta = meta;
        }
        return 1;
    }
    return 0;
}

static int find_any_app_object_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
) {
    if (find_runtime_package_by_name(request_name, out_ref, out_meta)) {
        device_write("[PACKAGE] resolve runtime\r\n");
        return 1;
    }
    if (find_installed_app_by_name(request_name, out_ref, out_meta)) {
        device_write("[PACKAGE] resolve installed-set\r\n");
        return 1;
    }
    if (!find_stale_app_object_by_name(request_name, out_ref)) {
        return 0;
    }
    if (out_meta != 0) {
        zero_bytes(out_meta, sizeof(*out_meta));
        if (!read_app_metadata_trace(*out_ref, out_meta, 0)) {
            return 0;
        }
    }
    device_write("[PACKAGE] resolve stale-app\r\n");
    return 1;
}

static int find_runtime_package_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_app_metadata *out_meta
) {
    for (uint32_t i = 0u; i < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_runtime_packages[i].active == 0u ||
            !app_name_matches(g_runtime_packages[i].meta.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = g_runtime_packages[i].app_object;
        }
        if (out_meta != 0) {
            *out_meta = g_runtime_packages[i].meta;
        }
        return 1;
    }
    return 0;
}

static void upsert_runtime_package(const struct luna_app_metadata *meta, struct luna_object_ref app_object) {
    uint32_t slot = LUNA_PACKAGE_CAPACITY;

    if (meta == 0 || (app_object.low == 0u && app_object.high == 0u)) {
        return;
    }
    for (uint32_t i = 0u; i < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_runtime_packages[i].active != 0u &&
            app_name_matches(g_runtime_packages[i].meta.name, meta->name)) {
            slot = i;
            break;
        }
        if (slot == LUNA_PACKAGE_CAPACITY && g_runtime_packages[i].active == 0u) {
            slot = i;
        }
    }
    if (slot >= LUNA_PACKAGE_CAPACITY) {
        return;
    }
    zero_bytes(&g_runtime_packages[slot], sizeof(g_runtime_packages[slot]));
    g_runtime_packages[slot].active = 1u;
    g_runtime_packages[slot].app_object = app_object;
    g_runtime_packages[slot].meta = *meta;
}

static void remove_runtime_package(const char request_name[16]) {
    for (uint32_t i = 0u; i < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_runtime_packages[i].active == 0u ||
            !app_name_matches(g_runtime_packages[i].meta.name, request_name)) {
            continue;
        }
        zero_bytes(&g_runtime_packages[i], sizeof(g_runtime_packages[i]));
    }
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 73;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_PACKAGE;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 75;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = 0;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_PACKAGE;
    gate->target_gate = target_gate;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static uint32_t security_policy_sync(uint32_t policy_type, uint64_t target_id, uint32_t state, uint32_t flags, uint64_t binding_id) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    struct luna_policy_query query;
    zero_bytes(&query, sizeof(query));
    query.policy_type = policy_type;
    query.state = state;
    query.flags = flags;
    query.target_id = target_id;
    query.binding_id = binding_id;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 76;
    gate->opcode = LUNA_GATE_POLICY_SYNC;
    gate->caller_space = LUNA_SPACE_PACKAGE;
    gate->buffer_addr = (uint64_t)(uintptr_t)&query;
    gate->buffer_size = sizeof(query);
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static uint32_t security_policy_query(uint32_t policy_type, uint64_t target_id, uint32_t *out_state, uint32_t *out_flags, uint64_t *out_binding_id) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    struct luna_policy_query query;
    zero_bytes(&query, sizeof(query));
    query.policy_type = policy_type;
    query.target_id = target_id;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 77;
    gate->opcode = LUNA_GATE_POLICY_QUERY;
    gate->caller_space = LUNA_SPACE_PACKAGE;
    gate->buffer_addr = (uint64_t)(uintptr_t)&query;
    gate->buffer_size = sizeof(query);
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    if (out_state != 0) {
        *out_state = query.state;
    }
    if (out_flags != 0) {
        *out_flags = query.flags;
    }
    if (out_binding_id != 0) {
        *out_binding_id = query.binding_id;
    }
    return gate->status;
}

static uint32_t security_trust_eval_bundle(const struct luna_app_metadata *meta, const void *blob, uint64_t blob_size, struct luna_trust_eval_request *out_request) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    struct luna_trust_eval_request request;

    if (meta == 0 || blob == 0 || blob_size == 0u) {
        return LUNA_GATE_DENIED;
    }
    zero_bytes(&request, sizeof(request));
    request.flags = meta->flags;
    request.abi_major = meta->abi_major;
    request.abi_minor = meta->abi_minor;
    request.sdk_major = meta->sdk_major;
    request.sdk_minor = meta->sdk_minor;
    request.bundle_id = meta->bundle_id;
    request.source_id = meta->source_id;
    request.signer_id = meta->signer_id;
    request.app_version = meta->app_version;
    request.integrity_check = meta->integrity_check;
    request.header_bytes = meta->header_bytes;
    request.entry_offset = meta->entry_offset;
    request.signature_check = meta->signature_check;
    request.blob_addr = (uint64_t)(uintptr_t)blob;
    request.blob_size = blob_size;
    request.capability_count = meta->capability_count;
    request.min_proto_version = meta->min_proto_version;
    request.max_proto_version = meta->max_proto_version;
    copy_bytes(request.name, meta->name, sizeof(request.name));
    copy_bytes(request.capability_keys, meta->capability_keys, sizeof(request.capability_keys));
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 78;
    gate->opcode = LUNA_GATE_TRUST_EVAL;
    gate->caller_space = LUNA_SPACE_PACKAGE;
    gate->domain_key = LUNA_CAP_PACKAGE_INSTALL;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    if (out_request != 0) {
        copy_bytes(out_request, &request, sizeof(request));
    }
    return gate->status;
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;
    if (text != 0 && text[0] == '[') {
        return;
    }
    while (text[size] != '\0') {
        size += 1u;
    }
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 74;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)manifest->device_gate_base);
}

static void append_u64_hex_fixed(char *out, uint64_t value, uint32_t digits) {
    static const char table[] = "0123456789ABCDEF";
    uint32_t index = 0u;
    while (index < digits) {
        uint32_t shift = (digits - 1u - index) * 4u;
        out[index] = table[(value >> shift) & 0xFu];
        index += 1u;
    }
    out[digits] = '\0';
}

static void device_write_ref_line(const char *prefix, struct luna_object_ref ref) {
    char low_hex[17];
    char high_hex[17];
    if (prefix != 0 && prefix[0] == '[') {
        return;
    }
    append_u64_hex_fixed(low_hex, ref.low, 16u);
    append_u64_hex_fixed(high_hex, ref.high, 16u);
    device_write(prefix);
    device_write(" low=0x");
    device_write(low_hex);
    device_write(" high=0x");
    device_write(high_hex);
    device_write("\r\n");
}

static void device_write_hex_value(const char *prefix, uint64_t value) {
    char hex[17];
    if (prefix != 0 && prefix[0] == '[') {
        return;
    }
    append_u64_hex_fixed(hex, value, 16u);
    device_write(prefix);
    device_write("0x");
    device_write(hex);
    device_write("\r\n");
}

static uint32_t data_seed_object(struct luna_cid cid, uint32_t object_type, uint32_t object_flags, struct luna_object_ref *out) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    char stage_hex[17];
    char index_hex[17];
    char slot_hex[17];
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    device_write_hex_value("[PACKAGE] seed type=", object_type);
    gate->sequence = 90;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_type = object_type;
    gate->object_flags = object_flags;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    out->low = gate->object_low;
    out->high = gate->object_high;
    device_write_hex_value("[PACKAGE] seed status=", gate->status);
    if (gate->status != LUNA_DATA_OK) {
        append_u64_hex_fixed(stage_hex, gate->result_count, 16u);
        append_u64_hex_fixed(index_hex, gate->content_size, 16u);
        append_u64_hex_fixed(slot_hex, gate->created_at, 16u);
        device_write("[PACKAGE] seed fail stage=0x");
        device_write(stage_hex);
        device_write(" index=0x");
        device_write(index_hex);
        device_write(" slot=0x");
        device_write(slot_hex);
        device_write("\r\n");
        device_write_ref_line("[PACKAGE] seed fail write cid", *out);
    }
    if (gate->status == LUNA_DATA_OK) {
        device_write_ref_line("[PACKAGE] seed object", *out);
    }
    return gate->status;
}

static uint32_t data_pour_span(struct luna_cid cid, struct luna_object_ref object, uint64_t offset, const void *buffer, uint64_t size) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 91;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = offset;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
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
    void *stage = data_stage_buffer(buffer_size);
    void *target = stage != 0 ? stage : buffer;
    zero_bytes(target, (size_t)buffer_size);
    if (target != buffer) {
        zero_bytes(buffer, (size_t)buffer_size);
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
    *out_size = gate->size;
    if (out_content_size != 0) {
        *out_content_size = gate->content_size;
    }
    if (gate->status == LUNA_DATA_OK && target != buffer && gate->size != 0u) {
        copy_bytes(buffer, target, (size_t)gate->size);
    }
    return gate->status;
}

static uint32_t data_shred_object(struct luna_cid cid, struct luna_object_ref object) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 93;
    gate->opcode = LUNA_DATA_SHRED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_gather_set(struct luna_cid cid, struct luna_object_ref object, struct luna_object_ref *out_refs, uint64_t buffer_size, uint32_t *out_count) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = data_stage_buffer(buffer_size);
    void *target = stage != 0 ? stage : out_refs;
    zero_bytes(target, (size_t)buffer_size);
    if (target != out_refs) {
        zero_bytes(out_refs, (size_t)buffer_size);
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
    *out_count = gate->result_count;
    if (gate->status == LUNA_DATA_OK && target != out_refs && gate->result_count != 0u) {
        uint64_t copy_size = (uint64_t)gate->result_count * sizeof(struct luna_object_ref);
        if (copy_size > buffer_size) {
            copy_size = buffer_size;
        }
        copy_bytes(out_refs, target, (size_t)copy_size);
    }
    return gate->status;
}

static uint32_t data_set_add_member(struct luna_cid cid, struct luna_object_ref set_object, struct luna_object_ref member) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = data_stage_buffer(sizeof(g_member_stage));
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    g_member_stage = member;
    if (stage != 0) {
        copy_bytes(stage, &g_member_stage, sizeof(g_member_stage));
    }
    gate->sequence = 95;
    gate->opcode = LUNA_DATA_SET_ADD_MEMBER;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_object.low;
    gate->object_high = set_object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)(stage != 0 ? stage : &g_member_stage);
    gate->buffer_size = sizeof(g_member_stage);
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_set_remove_member(struct luna_cid cid, struct luna_object_ref set_object, struct luna_object_ref member) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = data_stage_buffer(sizeof(g_member_stage));
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    g_member_stage = member;
    if (stage != 0) {
        copy_bytes(stage, &g_member_stage, sizeof(g_member_stage));
    }
    gate->sequence = 96;
    gate->opcode = LUNA_DATA_SET_REMOVE_MEMBER;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_object.low;
    gate->object_high = set_object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)(stage != 0 ? stage : &g_member_stage);
    gate->buffer_size = sizeof(g_member_stage);
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static void *data_stage_buffer(uint64_t size) {
    if (g_manifest == 0 || g_manifest->data_buffer_base == 0u || g_manifest->data_buffer_size < size) {
        return 0;
    }
    return (void *)(uintptr_t)g_manifest->data_buffer_base;
}

static void install_builtin(
    uint32_t slot,
    const char *name,
    const char *label,
    uint32_t icon_x,
    uint32_t icon_y,
    uint32_t flags,
    uint32_t preferred_x,
    uint32_t preferred_y,
    uint32_t preferred_width,
    uint32_t preferred_height,
    uint32_t window_role,
    uint64_t version
) {
    if (slot >= LUNA_PACKAGE_CAPACITY) {
        return;
    }
    zero_bytes(&g_catalog[slot], sizeof(g_catalog[slot]));
    copy_name16(g_catalog[slot].name, name);
    copy_name16(g_catalog[slot].label, label);
    g_catalog[slot].installed = 1u;
    g_catalog[slot].icon_x = icon_x;
    g_catalog[slot].icon_y = icon_y;
    g_catalog[slot].flags = flags;
    g_catalog[slot].preferred_x = preferred_x;
    g_catalog[slot].preferred_y = preferred_y;
    g_catalog[slot].preferred_width = preferred_width;
    g_catalog[slot].preferred_height = preferred_height;
    g_catalog[slot].window_role = window_role;
    g_catalog[slot].reserved0 = 0u;
    g_catalog[slot].version = version;
}

static void fill_builtin_catalog(void) {
    zero_bytes(g_catalog, sizeof(g_catalog));
    install_builtin(0u, "hello.luna", "Settings", 3u, 20u, LUNA_PACKAGE_FLAG_PINNED, 40u, 14u, 17u, 7u, LUNA_WINDOW_ROLE_UTILITY, 1u);
    install_builtin(1u, "files.luna", "Files", 3u, 4u, LUNA_PACKAGE_FLAG_PINNED | LUNA_PACKAGE_FLAG_STARTUP, 2u, 2u, 26u, 9u, LUNA_WINDOW_ROLE_DOCUMENT, 1u);
    install_builtin(2u, "notes.luna", "Notes", 3u, 8u, LUNA_PACKAGE_FLAG_PINNED, 29u, 2u, 27u, 9u, LUNA_WINDOW_ROLE_DOCUMENT, 1u);
    install_builtin(3u, "guard.luna", "Guard", 3u, 12u, LUNA_PACKAGE_FLAG_PINNED, 57u, 2u, 21u, 11u, LUNA_WINDOW_ROLE_UTILITY, 1u);
    install_builtin(4u, "console.luna", "Console", 3u, 16u, LUNA_PACKAGE_FLAG_PINNED, 2u, 12u, 37u, 11u, LUNA_WINDOW_ROLE_CONSOLE, 1u);
}

static const struct builtin_package_spec *find_builtin_spec(const char name[16]) {
    for (size_t i = 0u; i < sizeof(g_builtin_specs) / sizeof(g_builtin_specs[0]); ++i) {
        if (app_name_matches(g_builtin_specs[i].base_name, name)) {
            return &g_builtin_specs[i];
        }
    }
    return 0;
}

static uint64_t fold_bytes(uint64_t seed, const uint8_t *bytes, uint64_t size) {
    for (uint64_t i = 0u; i < size; ++i) {
        seed ^= (uint64_t)bytes[i];
        seed = (seed << 5) | (seed >> 59);
        seed *= 0x100000001B3ull;
    }
    return seed;
}

static uint64_t compute_bundle_integrity(const void *blob, uint64_t blob_size, uint64_t header_bytes) {
    const uint8_t *bytes = (const uint8_t *)blob;
    if (blob == 0 || header_bytes > blob_size) {
        return 0u;
    }
    return fold_bytes(0xCBF29CE484222325ull, bytes + header_bytes, blob_size - header_bytes);
}

static uint64_t compute_bundle_signature(const struct luna_app_metadata *meta, const void *blob, uint64_t blob_size) {
    const uint8_t *payload = (const uint8_t *)blob + meta->header_bytes;
    uint64_t payload_size = blob_size - meta->header_bytes;
    uint8_t stage[128];
    uint64_t seed = LUNA_PROGRAM_BUNDLE_SIGNATURE_SEED ^ meta->signer_id ^ (meta->source_id << 1);

    zero_bytes(stage, sizeof(stage));
    copy_bytes(stage, meta->name, sizeof(meta->name));
    seed = fold_bytes(seed, stage, sizeof(meta->name));

    zero_bytes(stage, sizeof(stage));
    copy_bytes(stage + 0u, &meta->bundle_id, sizeof(meta->bundle_id));
    copy_bytes(stage + 8u, &meta->source_id, sizeof(meta->source_id));
    copy_bytes(stage + 16u, &meta->app_version, sizeof(meta->app_version));
    copy_bytes(stage + 24u, &meta->capability_count, sizeof(meta->capability_count));
    copy_bytes(stage + 28u, &meta->flags, sizeof(meta->flags));
    copy_bytes(stage + 32u, &meta->abi_major, sizeof(meta->abi_major));
    copy_bytes(stage + 34u, &meta->abi_minor, sizeof(meta->abi_minor));
    copy_bytes(stage + 36u, &meta->sdk_major, sizeof(meta->sdk_major));
    copy_bytes(stage + 38u, &meta->sdk_minor, sizeof(meta->sdk_minor));
    copy_bytes(stage + 40u, &meta->min_proto_version, sizeof(meta->min_proto_version));
    copy_bytes(stage + 44u, &meta->max_proto_version, sizeof(meta->max_proto_version));
    copy_bytes(stage + 48u, &meta->signer_id, sizeof(meta->signer_id));
    copy_bytes(stage + 56u, meta->capability_keys, sizeof(meta->capability_keys));
    seed = fold_bytes(seed, stage, 56u + sizeof(meta->capability_keys));
    return fold_bytes(seed, payload, payload_size);
}

static int parse_luna_metadata(const void *blob, uint64_t blob_size, struct luna_app_metadata *out) {
    const struct luna_bundle_header *bundle = (const struct luna_bundle_header *)blob;
    const struct luna_app_header *legacy = (const struct luna_app_header *)blob;
    struct luna_app_metadata meta;
    if (blob == 0) {
        return 0;
    }
    zero_bytes(&meta, sizeof(meta));
    if (blob_size >= sizeof(struct luna_bundle_header) &&
        bundle->magic == LUNA_PROGRAM_BUNDLE_MAGIC &&
        bundle->version == LUNA_PROGRAM_BUNDLE_VERSION_V2) {
        if (bundle->name[0] == '\0' ||
            bundle->header_bytes < sizeof(struct luna_bundle_header) ||
            bundle->entry_offset < bundle->header_bytes ||
            bundle->entry_offset >= blob_size) {
            return 0;
        }
        copy_bytes(meta.name, bundle->name, sizeof(meta.name));
        meta.bundle_id = bundle->bundle_id;
        meta.source_id = bundle->source_id;
        meta.app_version = bundle->app_version;
        meta.integrity_check = bundle->integrity_check;
        meta.header_bytes = bundle->header_bytes;
        meta.entry_offset = bundle->entry_offset;
        meta.signer_id = bundle->signer_id;
        meta.signature_check = bundle->signature_check;
        meta.capability_count = bundle->capability_count;
        meta.flags = bundle->flags;
        meta.abi_major = bundle->abi_major;
        meta.abi_minor = bundle->abi_minor;
        meta.sdk_major = bundle->sdk_major;
        meta.sdk_minor = bundle->sdk_minor;
        meta.min_proto_version = bundle->min_proto_version;
        meta.max_proto_version = bundle->max_proto_version;
        copy_bytes(meta.capability_keys, bundle->capability_keys, sizeof(meta.capability_keys));
        if (meta.integrity_check != compute_bundle_integrity(blob, blob_size, meta.header_bytes) ||
            meta.signature_check != compute_bundle_signature(&meta, blob, blob_size)) {
            return 0;
        }
    } else {
        if (blob_size < sizeof(struct luna_app_header) ||
            legacy->magic != LUNA_PROGRAM_APP_MAGIC ||
            legacy->version != LUNA_PROGRAM_APP_VERSION ||
            legacy->name[0] == '\0' ||
            legacy->entry_offset < sizeof(struct luna_app_header) ||
            legacy->entry_offset >= blob_size) {
            return 0;
        }
        copy_bytes(meta.name, legacy->name, sizeof(meta.name));
        meta.bundle_id = 0u;
        meta.source_id = 0u;
        meta.app_version = 1u;
        meta.integrity_check = 0u;
        meta.header_bytes = sizeof(struct luna_app_header);
        meta.entry_offset = legacy->entry_offset;
        meta.signer_id = 0u;
        meta.signature_check = 0u;
        meta.capability_count = legacy->capability_count;
        meta.flags = 0u;
        meta.abi_major = 0u;
        meta.abi_minor = 0u;
        meta.sdk_major = 0u;
        meta.sdk_minor = 0u;
        meta.min_proto_version = 0u;
        meta.max_proto_version = 0u;
        copy_bytes(meta.capability_keys, legacy->capability_keys, sizeof(meta.capability_keys));
    }
    if (out != 0) {
        copy_bytes(out, &meta, sizeof(meta));
    }
    return 1;
}

static uint32_t count_installed_luna_objects(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    uint32_t installed = 0u;
    if (g_installed_apps_set.low == 0u && g_installed_apps_set.high == 0u) {
        return 0u;
    }
    if (data_gather_set(g_data_gather_cid, g_installed_apps_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0u;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_app_metadata meta;
        zero_bytes(&meta, sizeof(meta));
        if (!read_app_metadata_trace(refs[i], &meta, 0)) {
            continue;
        }
        installed += 1u;
    }
    return installed;
}

static void fill_record_from_meta(struct luna_package_record *record, const struct luna_app_metadata *meta, uint32_t install_flags) {
    const struct builtin_package_spec *builtin = find_builtin_spec(meta->name);
    zero_bytes(record, sizeof(*record));
    copy_name_with_suffix(record->name, meta->name, ".luna");
    if (builtin != 0) {
        copy_name16(record->label, builtin->label);
        record->icon_x = builtin->icon_x;
        record->icon_y = builtin->icon_y;
        record->flags = builtin->flags | install_flags;
        record->preferred_x = builtin->preferred_x;
        record->preferred_y = builtin->preferred_y;
        record->preferred_width = builtin->preferred_width;
        record->preferred_height = builtin->preferred_height;
        record->window_role = builtin->window_role;
    } else {
        copy_name16(record->label, meta->name);
    }
    record->installed = 1u;
    record->version = meta->app_version;
}

static uint32_t query_catalog_rows(struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    uint8_t *payload = g_app_metadata_blob;
    struct luna_query_request request;
    uint32_t count = 0u;
    uint64_t payload_size = 0u;

    if (out_count != 0) {
        *out_count = 0u;
    }
    if (out_rows == 0 || capacity == 0u || g_data_query_cid.low == 0u || manifest->data_gate_base == 0u) {
        return LUNA_DATA_ERR_RANGE;
    }
    if (capacity > LUNA_PACKAGE_CAPACITY) {
        capacity = LUNA_PACKAGE_CAPACITY;
    }
    payload_size = sizeof(request) + ((uint64_t)capacity * sizeof(struct luna_query_row));
    if (payload_size > sizeof(g_app_metadata_blob)) {
        return LUNA_DATA_ERR_RANGE;
    }

    zero_bytes(&request, sizeof(request));
    request.target = LUNA_QUERY_TARGET_PACKAGE_CATALOG;
    request.filter_flags = LUNA_QUERY_FILTER_NAMESPACE;
    request.projection_flags =
        LUNA_QUERY_PROJECT_NAME |
        LUNA_QUERY_PROJECT_LABEL |
        LUNA_QUERY_PROJECT_REF |
        LUNA_QUERY_PROJECT_TYPE |
        LUNA_QUERY_PROJECT_STATE |
        LUNA_QUERY_PROJECT_VERSION;
    request.sort_mode = LUNA_QUERY_SORT_NAME_ASC;
    request.limit = capacity;
    request.namespace_id = LUNA_QUERY_NAMESPACE_PACKAGE;

    zero_bytes(payload, (size_t)payload_size);
    copy_bytes(payload, &request, sizeof(request));
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 92u;
    gate->opcode = LUNA_DATA_QUERY;
    gate->cid_low = g_data_query_cid.low;
    gate->cid_high = g_data_query_cid.high;
    gate->writer_space = LUNA_SPACE_PACKAGE;
    gate->buffer_addr = (uint64_t)(uintptr_t)payload;
    gate->buffer_size = payload_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return gate->status;
    }

    count = gate->result_count;
    if (count > capacity) {
        count = capacity;
    }
    copy_bytes(out_rows, payload + sizeof(request), (size_t)count * sizeof(struct luna_query_row));
    if (out_count != 0) {
        *out_count = count;
    }
    return LUNA_DATA_OK;
}

static void fill_record_from_query_row(struct luna_package_record *record, const struct luna_query_row *row) {
    const struct builtin_package_spec *builtin = find_builtin_spec(row->name);
    zero_bytes(record, sizeof(*record));
    copy_name_with_suffix(record->name, row->name, ".luna");
    if (builtin != 0) {
        copy_name16(record->label, builtin->label);
        record->icon_x = builtin->icon_x;
        record->icon_y = builtin->icon_y;
        record->flags = builtin->flags | row->flags;
        record->preferred_x = builtin->preferred_x;
        record->preferred_y = builtin->preferred_y;
        record->preferred_width = builtin->preferred_width;
        record->preferred_height = builtin->preferred_height;
        record->window_role = builtin->window_role;
    } else {
        if (row->label[0] != '\0') {
            copy_name16(record->label, row->label);
        } else {
            copy_name16(record->label, row->name);
        }
        record->flags = row->flags;
    }
    record->installed = 1u;
    record->version = row->version;
}

static int find_catalog_row_by_name(const char request_name[16], struct luna_query_row *out_row) {
    struct luna_query_row rows[LUNA_PACKAGE_CAPACITY];
    uint32_t count = 0u;
    if (query_catalog_rows(rows, LUNA_PACKAGE_CAPACITY, &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (!app_name_matches(rows[i].name, request_name) &&
            !app_name_matches(request_name, rows[i].name)) {
            continue;
        }
        if (out_row != 0) {
            *out_row = rows[i];
        }
        return 1;
    }
    return 0;
}

static int load_package_state(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    uint64_t best_high = 0u;
    int found = 0;
    if (g_data_gather_cid.low == 0u || g_data_draw_cid.low == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0, 0}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_state_record state;
        struct luna_package_state_record_v1 legacy;
        struct luna_package_state_record_v2 legacy_v2;
        struct luna_package_state_record_v3 legacy_v3;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&state, sizeof(state));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &state, sizeof(state), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (state.magic != LUNA_PACKAGE_STATE_MAGIC) {
            continue;
        }
        if (state.version == LUNA_PACKAGE_STATE_VERSION && size >= sizeof(state)) {
            if (found && refs[i].high < best_high) {
                continue;
            }
            g_package_state_object = refs[i];
            g_installed_apps_set = state.installed_apps_set;
            g_install_records_set = state.install_records_set;
            g_app_index_set = state.app_index_set;
            g_trusted_source_set = state.trusted_source_set;
            g_trusted_signer_set = state.trusted_signer_set;
            g_update_txn_log_set = state.update_txn_log_set;
            g_next_txn_id = state.next_txn_id == 0u ? 1u : state.next_txn_id;
            g_update_txn_object = state.update_txn_object;
            g_rollback_refs_set = state.rollback_refs_set;
            best_high = refs[i].high;
            found = 1;
            continue;
        }
        if (state.version == LUNA_PACKAGE_STATE_VERSION_V4 && size >= sizeof(struct luna_package_state_record_v4)) {
            struct luna_package_state_record_v4 legacy_v4;
            if (found && refs[i].high < best_high) {
                continue;
            }
            copy_bytes(&legacy_v4, &state, sizeof(legacy_v4));
            g_package_state_object = refs[i];
            g_installed_apps_set = legacy_v4.installed_apps_set;
            g_install_records_set = legacy_v4.install_records_set;
            g_app_index_set = legacy_v4.app_index_set;
            g_trusted_source_set = legacy_v4.trusted_source_set;
            g_trusted_signer_set = (struct luna_object_ref){0u, 0u};
            g_update_txn_log_set = legacy_v4.update_txn_log_set;
            g_next_txn_id = legacy_v4.next_txn_id == 0u ? 1u : legacy_v4.next_txn_id;
            g_update_txn_object = legacy_v4.update_txn_object;
            g_rollback_refs_set = legacy_v4.rollback_refs_set;
            best_high = refs[i].high;
            found = 1;
            continue;
        }
        if (state.version == LUNA_PACKAGE_STATE_VERSION_V3 && size >= sizeof(legacy_v3)) {
            if (found && refs[i].high < best_high) {
                continue;
            }
            copy_bytes(&legacy_v3, &state, sizeof(legacy_v3));
            g_package_state_object = refs[i];
            g_installed_apps_set = legacy_v3.installed_apps_set;
            g_install_records_set = legacy_v3.install_records_set;
            g_app_index_set = legacy_v3.app_index_set;
            g_trusted_source_set = (struct luna_object_ref){0u, 0u};
            g_trusted_signer_set = (struct luna_object_ref){0u, 0u};
            g_update_txn_log_set = (struct luna_object_ref){0u, 0u};
            g_next_txn_id = 1u;
            g_update_txn_object = legacy_v3.update_txn_object;
            g_rollback_refs_set = legacy_v3.rollback_refs_set;
            best_high = refs[i].high;
            found = 1;
            continue;
        }
        if (state.version == LUNA_PACKAGE_STATE_VERSION_V2 && size >= sizeof(legacy_v2)) {
            if (found && refs[i].high < best_high) {
                continue;
            }
            copy_bytes(&legacy_v2, &state, sizeof(legacy_v2));
            g_package_state_object = refs[i];
            g_installed_apps_set = legacy_v2.installed_apps_set;
            g_install_records_set = legacy_v2.install_records_set;
            g_app_index_set = legacy_v2.app_index_set;
            g_trusted_source_set = (struct luna_object_ref){0u, 0u};
            g_trusted_signer_set = (struct luna_object_ref){0u, 0u};
            g_update_txn_log_set = (struct luna_object_ref){0u, 0u};
            g_next_txn_id = 1u;
            g_update_txn_object = (struct luna_object_ref){0u, 0u};
            g_rollback_refs_set = (struct luna_object_ref){0u, 0u};
            best_high = refs[i].high;
            found = 1;
            continue;
        }
        if (state.version != LUNA_PACKAGE_STATE_VERSION_LEGACY || size < sizeof(legacy)) {
            continue;
        }
        if (found && refs[i].high < best_high) {
            continue;
        }
        copy_bytes(&legacy, &state, sizeof(legacy));
        g_package_state_object = refs[i];
        g_installed_apps_set = legacy.installed_apps_set;
        g_install_records_set = (struct luna_object_ref){0u, 0u};
        g_app_index_set = (struct luna_object_ref){0u, 0u};
        g_trusted_source_set = (struct luna_object_ref){0u, 0u};
        g_trusted_signer_set = (struct luna_object_ref){0u, 0u};
        g_update_txn_log_set = (struct luna_object_ref){0u, 0u};
        g_next_txn_id = 1u;
        g_update_txn_object = (struct luna_object_ref){0u, 0u};
        g_rollback_refs_set = (struct luna_object_ref){0u, 0u};
        best_high = refs[i].high;
        found = 1;
    }
    return found;
}

static __attribute__((unused)) int persist_package_state(void) {
    struct luna_package_state_record state;

    if (g_data_seed_cid.low == 0u || g_data_pour_cid.low == 0u) {
        return 0;
    }
    if ((g_package_state_object.low == 0u && g_package_state_object.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_PACKAGE_STATE, 0u, &g_package_state_object) != LUNA_DATA_OK) {
        return 0;
    }

    zero_bytes(&state, sizeof(state));
    state.magic = LUNA_PACKAGE_STATE_MAGIC;
    state.version = LUNA_PACKAGE_STATE_VERSION;
    state.installed_apps_set = g_installed_apps_set;
    state.install_records_set = g_install_records_set;
    state.app_index_set = g_app_index_set;
    state.trusted_source_set = g_trusted_source_set;
    state.trusted_signer_set = g_trusted_signer_set;
    state.update_txn_log_set = g_update_txn_log_set;
    state.next_txn_id = g_next_txn_id == 0u ? 1u : g_next_txn_id;
    state.update_txn_object = g_update_txn_object;
    state.rollback_refs_set = g_rollback_refs_set;
    return data_pour_span(g_data_pour_cid, g_package_state_object, 0u, &state, sizeof(state)) == LUNA_DATA_OK;
}

static int ensure_package_sets(void) {
    struct luna_package_trusted_source_record trusted_source;
    struct luna_package_trusted_signer_record trusted_signer;
    struct luna_object_ref trusted_ref = {0u, 0u};
    struct luna_object_ref signer_ref = {0u, 0u};
    if (g_data_seed_cid.low == 0u || g_data_pour_cid.low == 0u) {
        return 0;
    }
    if ((g_installed_apps_set.low == 0u && g_installed_apps_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_installed_apps_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_install_records_set.low == 0u && g_install_records_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_install_records_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_app_index_set.low == 0u && g_app_index_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_app_index_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_trusted_source_set.low == 0u && g_trusted_source_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_trusted_source_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_trusted_signer_set.low == 0u && g_trusted_signer_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_trusted_signer_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_update_txn_log_set.low == 0u && g_update_txn_log_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_update_txn_log_set) != LUNA_DATA_OK) {
        return 0;
    }
    if ((g_rollback_refs_set.low == 0u && g_rollback_refs_set.high == 0u) &&
        data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &g_rollback_refs_set) != LUNA_DATA_OK) {
        return 0;
    }
    if (!signer_is_trusted(0u)) {
        zero_bytes(&trusted_signer, sizeof(trusted_signer));
        trusted_signer.magic = LUNA_PACKAGE_TRUSTED_SIGNER_MAGIC;
        trusted_signer.version = LUNA_PACKAGE_TRUSTED_SIGNER_VERSION;
        trusted_signer.signer_id = 0u;
        if (data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_TRUSTED_SIGNER, 0u, &signer_ref) != LUNA_DATA_OK ||
            data_pour_span(g_data_pour_cid, signer_ref, 0u, &trusted_signer, sizeof(trusted_signer)) != LUNA_DATA_OK ||
            data_set_add_member(g_data_pour_cid, g_trusted_signer_set, signer_ref) != LUNA_DATA_OK) {
            return 0;
        }
    }
    if (!source_is_trusted(0u, 0u)) {
        zero_bytes(&trusted_source, sizeof(trusted_source));
        trusted_source.magic = LUNA_PACKAGE_TRUSTED_SOURCE_MAGIC;
        trusted_source.version = LUNA_PACKAGE_TRUSTED_SOURCE_VERSION;
        trusted_source.source_id = 0u;
        trusted_source.signer_id = 0u;
        if (data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_TRUSTED_SOURCE, 0u, &trusted_ref) != LUNA_DATA_OK ||
            data_pour_span(g_data_pour_cid, trusted_ref, 0u, &trusted_source, sizeof(trusted_source)) != LUNA_DATA_OK ||
            data_set_add_member(g_data_pour_cid, g_trusted_source_set, trusted_ref) != LUNA_DATA_OK) {
            return 0;
        }
    }
    return 1;
}

static __attribute__((unused)) int persist_update_txn(struct luna_package_update_txn_record *record) {
    if (record == 0 || !ensure_package_sets()) {
        return 1;
    }
    if (record->txn_id == 0u) {
        record->txn_id = g_next_txn_id++;
    }
    {
        uint32_t policy_state = LUNA_POLICY_STATE_DENY;
        if (record->state == LUNA_UPDATE_TXN_STATE_ACTIVATED) {
            policy_state = LUNA_POLICY_STATE_ALLOW;
        } else if (record->state == LUNA_UPDATE_TXN_STATE_ROLLED_BACK) {
            policy_state = LUNA_POLICY_STATE_REVOKE;
        }
        if (security_policy_sync(LUNA_POLICY_TYPE_UPDATE, record->txn_id, policy_state, record->state, 0u) != LUNA_GATE_OK) {
            device_write("[PACKAGE] txn policy stale\r\n");
        }
    }
    return 1;
}

static int signer_is_trusted(uint64_t signer_id) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (signer_id == 0u) {
        return 1;
    }
    if (g_trusted_signer_set.low == 0u && g_trusted_signer_set.high == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, g_trusted_signer_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_trusted_signer_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) ||
            record.magic != LUNA_PACKAGE_TRUSTED_SIGNER_MAGIC ||
            record.version != LUNA_PACKAGE_TRUSTED_SIGNER_VERSION) {
            continue;
        }
        if (record.signer_id == signer_id) {
            return 1;
        }
    }
    return 0;
}

static int source_is_trusted(uint64_t source_id, uint64_t signer_id) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (source_id == 0u && signer_id == 0u) {
        return 1;
    }
    if (g_trusted_source_set.low == 0u && g_trusted_source_set.high == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, g_trusted_source_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_trusted_source_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) ||
            record.magic != LUNA_PACKAGE_TRUSTED_SOURCE_MAGIC ||
            record.version != LUNA_PACKAGE_TRUSTED_SOURCE_VERSION) {
            continue;
        }
        if (record.source_id == source_id && record.signer_id == signer_id) {
            return 1;
        }
    }
    return 0;
}

static int sync_signer_policies(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_trusted_signer_set.low == 0u && g_trusted_signer_set.high == 0u) {
        return 1;
    }
    if (data_gather_set(g_data_gather_cid, g_trusted_signer_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_trusted_signer_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK ||
            size < sizeof(record) ||
            record.magic != LUNA_PACKAGE_TRUSTED_SIGNER_MAGIC ||
            record.version != LUNA_PACKAGE_TRUSTED_SIGNER_VERSION) {
            continue;
        }
        if (security_policy_sync(LUNA_POLICY_TYPE_SIGNER, record.signer_id, LUNA_POLICY_STATE_ALLOW, record.flags, 0u) != LUNA_GATE_OK) {
            return 0;
        }
    }
    return 1;
}

static int sync_source_policies(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_trusted_source_set.low == 0u && g_trusted_source_set.high == 0u) {
        return 1;
    }
    if (data_gather_set(g_data_gather_cid, g_trusted_source_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_trusted_source_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK ||
            size < sizeof(record) ||
            record.magic != LUNA_PACKAGE_TRUSTED_SOURCE_MAGIC ||
            record.version != LUNA_PACKAGE_TRUSTED_SOURCE_VERSION) {
            continue;
        }
        if (security_policy_sync(LUNA_POLICY_TYPE_SOURCE, record.source_id, LUNA_POLICY_STATE_ALLOW, record.flags, record.signer_id) != LUNA_GATE_OK) {
            return 0;
        }
    }
    return 1;
}

static int clear_rollback_refs(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_rollback_refs_set.low == 0u && g_rollback_refs_set.high == 0u) {
        return 1;
    }
    if (data_gather_set(g_data_gather_cid, g_rollback_refs_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (data_set_remove_member(g_data_pour_cid, g_rollback_refs_set, refs[i]) != LUNA_DATA_OK) {
            return 0;
        }
    }
    return 1;
}

static __attribute__((unused)) int stage_rollback_refs(
    struct luna_object_ref old_app,
    struct luna_object_ref old_install,
    struct luna_object_ref old_index
) {
    if (!clear_rollback_refs()) {
        return 0;
    }
    if ((old_app.low != 0u || old_app.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_rollback_refs_set, old_app) != LUNA_DATA_OK) {
        return 0;
    }
    if ((old_install.low != 0u || old_install.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_rollback_refs_set, old_install) != LUNA_DATA_OK) {
        return 0;
    }
    if ((old_index.low != 0u || old_index.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_rollback_refs_set, old_index) != LUNA_DATA_OK) {
        return 0;
    }
    return 1;
}

static int find_index_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_index_record *out_record
) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_app_index_set.low == 0u && g_app_index_set.high == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, g_app_index_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_index_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) || record.magic != LUNA_PACKAGE_INDEX_MAGIC || record.version != LUNA_PACKAGE_INDEX_VERSION) {
            continue;
        }
        if (!app_name_matches(record.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1;
    }
    return 0;
}

static int find_install_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_install_record *out_record
) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_install_records_set.low == 0u && g_install_records_set.high == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, g_install_records_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_install_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) || record.magic != LUNA_PACKAGE_INSTALL_MAGIC || record.version != LUNA_PACKAGE_INSTALL_VERSION) {
            continue;
        }
        if (!app_name_matches(record.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1;
    }
    return 0;
}

static int read_app_metadata_trace(struct luna_object_ref object, struct luna_app_metadata *out_meta, int trace_failures) {
    uint8_t header_buf[sizeof(struct luna_bundle_header)] = {0};
    uint64_t size = 0u;
    uint64_t content_size = 0u;
    if (data_draw_span(g_data_draw_cid, object, 0u, header_buf, sizeof(header_buf), &size, &content_size) != LUNA_DATA_OK) {
        if (trace_failures) {
            device_write("[PACKAGE] meta head draw fail\r\n");
        }
        return 0;
    }
    if (content_size > sizeof(g_app_metadata_blob)) {
        if (trace_failures) {
            device_write("[PACKAGE] meta too large\r\n");
        }
        return 0;
    }
    zero_bytes(g_app_metadata_blob, sizeof(g_app_metadata_blob));
    if (data_draw_span(g_data_draw_cid, object, 0u, g_app_metadata_blob, content_size, &size, &content_size) != LUNA_DATA_OK) {
        if (trace_failures) {
            device_write("[PACKAGE] meta full draw fail\r\n");
        }
        return 0;
    }
    if (!parse_luna_metadata(g_app_metadata_blob, content_size, out_meta)) {
        if (trace_failures) {
            device_write("[PACKAGE] meta parse fail\r\n");
        }
        return 0;
    }
    return 1;
}

static int read_app_metadata(struct luna_object_ref object, struct luna_app_metadata *out_meta) {
    return read_app_metadata_trace(object, out_meta, 1);
}

static int find_stale_app_object_by_name(const char request_name[16], struct luna_object_ref *out_ref) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_data_gather_cid.low == 0u || g_data_draw_cid.low == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_app_metadata meta;
        if (!read_app_metadata_trace(refs[i], &meta, 0) || !app_name_matches(meta.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        return 1;
    }
    return 0;
}

static int find_stale_install_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_install_record *out_record
) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_data_gather_cid.low == 0u || g_data_draw_cid.low == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_install_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) || record.magic != LUNA_PACKAGE_INSTALL_MAGIC || record.version != LUNA_PACKAGE_INSTALL_VERSION) {
            continue;
        }
        if (!app_name_matches(record.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1;
    }
    return 0;
}

static int find_stale_index_record_by_name(
    const char request_name[16],
    struct luna_object_ref *out_ref,
    struct luna_package_index_record *out_record
) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (g_data_gather_cid.low == 0u || g_data_draw_cid.low == 0u) {
        return 0;
    }
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_index_record record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&record, sizeof(record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &record, sizeof(record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(record) || record.magic != LUNA_PACKAGE_INDEX_MAGIC || record.version != LUNA_PACKAGE_INDEX_VERSION) {
            continue;
        }
        if (!app_name_matches(record.name, request_name)) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1;
    }
    return 0;
}

static int write_install_record(
    const struct luna_app_metadata *meta,
    struct luna_object_ref app_object,
    struct luna_object_ref *out_record_ref
) {
    struct luna_package_install_record *record = &g_install_record_stage;
    struct luna_object_ref object = {0u, 0u};
    zero_bytes(record, sizeof(*record));
    record->magic = LUNA_PACKAGE_INSTALL_MAGIC;
    record->version = LUNA_PACKAGE_INSTALL_VERSION;
    record->flags = 0u;
    record->app_version = meta->app_version;
    record->app_object = app_object;
    copy_bytes(record->name, meta->name, sizeof(record->name));
    if (out_record_ref != 0) {
        object = *out_record_ref;
    }
    if ((object.low == 0u && object.high == 0u) &&
        !find_stale_install_record_by_name(meta->name, &object, 0)) {
        device_write("[PACKAGE] install record seed begin\r\n");
        uint32_t status = data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL, 0u, &object);
        if (status != LUNA_DATA_OK) {
            if (status == LUNA_DATA_ERR_NO_SPACE) {
                device_write("[PACKAGE] install record seed no-space\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PACKAGE] install record seed io\r\n");
            } else {
                device_write("[PACKAGE] install record seed fail\r\n");
            }
            return 0;
        }
        device_write_ref_line("[PACKAGE] install record object", object);
    }
    if (data_pour_span(g_data_pour_cid, object, 0u, record, sizeof(*record)) != LUNA_DATA_OK) {
        device_write("[PACKAGE] install record pour fail\r\n");
        return 0;
    }
    if (data_set_add_member(g_data_pour_cid, g_install_records_set, object) != LUNA_DATA_OK) {
        device_write("[PACKAGE] install record set fail\r\n");
        return 0;
    }
    if (out_record_ref != 0) {
        *out_record_ref = object;
    }
    return 1;
}

static int write_index_record(
    const struct luna_app_metadata *meta,
    struct luna_object_ref app_object,
    struct luna_object_ref install_object,
    struct luna_object_ref *out_record_ref
) {
    struct luna_package_index_record *record = &g_index_record_stage;
    struct luna_object_ref object = {0u, 0u};
    zero_bytes(record, sizeof(*record));
    record->magic = LUNA_PACKAGE_INDEX_MAGIC;
    record->version = LUNA_PACKAGE_INDEX_VERSION;
    record->flags = 0u;
    record->app_version = meta->app_version;
    record->app_object = app_object;
    record->install_object = install_object;
    copy_bytes(record->name, meta->name, sizeof(record->name));
    if (out_record_ref != 0) {
        object = *out_record_ref;
    }
    if ((object.low == 0u && object.high == 0u) &&
        !find_stale_index_record_by_name(meta->name, &object, 0)) {
        device_write("[PACKAGE] index record seed begin\r\n");
        uint32_t status = data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_PACKAGE_INDEX, 0u, &object);
        if (status != LUNA_DATA_OK) {
            if (status == LUNA_DATA_ERR_NO_SPACE) {
                device_write("[PACKAGE] index record seed no-space\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PACKAGE] index record seed io\r\n");
            } else {
                device_write("[PACKAGE] index record seed fail\r\n");
            }
            return 0;
        }
        device_write_ref_line("[PACKAGE] index record object", object);
    }
    if (data_pour_span(g_data_pour_cid, object, 0u, record, sizeof(*record)) != LUNA_DATA_OK) {
        device_write("[PACKAGE] index record pour fail\r\n");
        return 0;
    }
    if (data_set_add_member(g_data_pour_cid, g_app_index_set, object) != LUNA_DATA_OK) {
        device_write("[PACKAGE] index record set fail\r\n");
        return 0;
    }
    if (out_record_ref != 0) {
        *out_record_ref = object;
    }
    return 1;
}

static int audit_install_registration(
    const struct luna_app_metadata *meta,
    struct luna_object_ref app_object,
    struct luna_object_ref install_object,
    struct luna_object_ref index_object
) {
    struct luna_app_metadata stored_meta;
    struct luna_package_install_record install_record;
    struct luna_package_index_record index_record;
    struct luna_object_ref install_ref = {0u, 0u};
    struct luna_object_ref index_ref = {0u, 0u};

    if (meta == 0) {
        return 0;
    }
    zero_bytes(&stored_meta, sizeof(stored_meta));
    zero_bytes(&install_record, sizeof(install_record));
    zero_bytes(&index_record, sizeof(index_record));

    if (!read_app_metadata(app_object, &stored_meta)) {
        device_write("[PACKAGE] install data missing\r\n");
        return 0;
    }
    device_write("[PACKAGE] install data ok\r\n");
    if (!set_contains_ref(g_installed_apps_set, app_object)) {
        device_write("[PACKAGE] install installed set missing\r\n");
        return 0;
    }
    device_write("[PACKAGE] install installed set ok\r\n");
    if (install_object.low == 0u && install_object.high == 0u) {
        device_write("[PACKAGE] install record fallback\r\n");
    } else {
        if (!find_install_record_by_name(meta->name, &install_ref, &install_record) ||
            install_ref.low != install_object.low ||
            install_ref.high != install_object.high ||
            install_record.app_object.low != app_object.low ||
            install_record.app_object.high != app_object.high) {
            device_write("[PACKAGE] install record missing\r\n");
            return 0;
        }
        device_write("[PACKAGE] install record ok\r\n");
    }
    if (index_object.low == 0u && index_object.high == 0u) {
        device_write("[PACKAGE] install index fallback\r\n");
    } else {
        if (!find_index_record_by_name(meta->name, &index_ref, &index_record) ||
            index_ref.low != index_object.low ||
            index_ref.high != index_object.high ||
            index_record.app_object.low != app_object.low ||
            index_record.app_object.high != app_object.high ||
            index_record.install_object.low != install_object.low ||
            index_record.install_object.high != install_object.high) {
            device_write("[PACKAGE] install index missing\r\n");
            return 0;
        }
        device_write("[PACKAGE] install index ok\r\n");
    }
    if (!catalog_contains_name(meta->name)) {
        device_write("[PACKAGE] install catalog missing\r\n");
        return 0;
    }
    device_write("[PACKAGE] install catalog ok\r\n");
    return 1;
}

static int rebuild_installed_apps_set(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    struct luna_object_ref new_set = {0u, 0u};
    uint32_t count = 0u;

    if (!ensure_package_sets()) {
        device_write("[PACKAGE] installed set rebuild ensure fail\r\n");
        return 0;
    }
    if (data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_SET, 0u, &new_set) != LUNA_DATA_OK) {
        device_write("[PACKAGE] installed set rebuild seed fail\r\n");
        return 0;
    }
    if ((g_install_records_set.low == 0u && g_install_records_set.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_install_records_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        g_installed_apps_set = new_set;
        return 1;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_install_record install_record;
        uint64_t size = 0u;
        uint64_t content_size = 0u;

        zero_bytes(&install_record, sizeof(install_record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &install_record, sizeof(install_record), &size, &content_size) != LUNA_DATA_OK ||
            size < sizeof(install_record) ||
            install_record.magic != LUNA_PACKAGE_INSTALL_MAGIC ||
            install_record.version != LUNA_PACKAGE_INSTALL_VERSION) {
            continue;
        }
        if (install_record.app_object.low == 0u || install_record.app_object.high == 0u) {
            continue;
        }
        if (data_set_add_member(g_data_pour_cid, new_set, install_record.app_object) != LUNA_DATA_OK) {
            device_write("[PACKAGE] installed set rebuild add fail\r\n");
            return 0;
        }
    }
    g_installed_apps_set = new_set;
    device_write("[PACKAGE] installed set rebuild\r\n");
    return 1;
}

static int purge_stale_package_name(const char request_name[16]) {
    struct luna_object_ref ref = {0u, 0u};

    while (find_stale_app_object_by_name(request_name, &ref)) {
        (void)data_set_remove_member(g_data_pour_cid, g_installed_apps_set, ref);
        if (data_shred_object(g_data_shred_cid, ref) != LUNA_DATA_OK) {
            device_write("[PACKAGE] purge app fail\r\n");
            return 0;
        }
        zero_bytes(&ref, sizeof(ref));
    }
    while (find_stale_install_record_by_name(request_name, &ref, 0)) {
        (void)data_set_remove_member(g_data_pour_cid, g_install_records_set, ref);
        if (data_shred_object(g_data_shred_cid, ref) != LUNA_DATA_OK) {
            device_write("[PACKAGE] purge install fail\r\n");
            return 0;
        }
        zero_bytes(&ref, sizeof(ref));
    }
    while (find_stale_index_record_by_name(request_name, &ref, 0)) {
        (void)data_set_remove_member(g_data_pour_cid, g_app_index_set, ref);
        if (data_shred_object(g_data_shred_cid, ref) != LUNA_DATA_OK) {
            device_write("[PACKAGE] purge index fail\r\n");
            return 0;
        }
        zero_bytes(&ref, sizeof(ref));
    }
    return 1;
}

static int audit_remove_registration(const char request_name[16]) {
    if (find_stale_app_object_by_name(request_name, 0)) {
        device_write("[PACKAGE] remove data present\r\n");
        return 0;
    }
    device_write("[PACKAGE] remove data clear\r\n");
    if (find_install_record_by_name(request_name, 0, 0) || find_stale_install_record_by_name(request_name, 0, 0)) {
        device_write("[PACKAGE] remove install present\r\n");
        return 0;
    }
    device_write("[PACKAGE] remove installed set clear\r\n");
    if (find_index_record_by_name(request_name, 0, 0) || find_stale_index_record_by_name(request_name, 0, 0)) {
        device_write("[PACKAGE] remove index present\r\n");
        return 0;
    }
    device_write("[PACKAGE] remove index clear\r\n");
    if (catalog_contains_name(request_name)) {
        device_write("[PACKAGE] remove catalog present\r\n");
        return 0;
    }
    device_write("[PACKAGE] remove catalog clear\r\n");
    return 1;
}

static int register_package_blob(const void *blob, uint64_t blob_size) {
    struct luna_app_metadata meta;
    const void *stable_blob_input = blob;
    struct luna_object_ref app_object = {0u, 0u};
    struct luna_object_ref install_object = {0u, 0u};
    struct luna_object_ref index_object = {0u, 0u};
    struct luna_object_ref old_app = {0u, 0u};
    struct luna_object_ref old_install = {0u, 0u};
    struct luna_object_ref old_index = {0u, 0u};
    if (g_device_write_cid.low != 0u || g_device_write_cid.high != 0u) {
        device_write("[PACKAGE] blob parse\r\n");
    }
    if (blob != 0 && blob_size != 0u && blob_size <= sizeof(g_package_blob_stage)) {
        copy_bytes(g_package_blob_stage, blob, (size_t)blob_size);
        stable_blob_input = g_package_blob_stage;
    }
    if (!parse_luna_metadata(stable_blob_input, blob_size, &meta) || !ensure_package_sets()) {
        return 0;
    }
    device_write("audit package.install start\r\n");
    device_write("[PACKAGE] blob policy\r\n");
    if (meta.min_proto_version != 0u &&
        (meta.min_proto_version > LUNA_PROTO_VERSION ||
         (meta.max_proto_version != 0u && meta.max_proto_version < LUNA_PROTO_VERSION))) {
        device_write("audit package.install denied reason=proto-range\r\n");
        return 0;
    }
    {
        struct luna_trust_eval_request trust;
        zero_bytes(&trust, sizeof(trust));
        if (security_trust_eval_bundle(&meta, stable_blob_input, blob_size, &trust) != LUNA_GATE_OK) {
            switch (trust.result_state) {
                case LUNA_TRUST_RESULT_PROTO:
                    device_write("audit package.install denied reason=proto-range\r\n");
                    break;
                case LUNA_TRUST_RESULT_INTEGRITY:
                    device_write("audit package.install denied reason=bundle-integrity\r\n");
                    break;
                case LUNA_TRUST_RESULT_SIGNATURE:
                    device_write("audit package.install denied reason=signature\r\n");
                    break;
                case LUNA_TRUST_RESULT_SIGNER:
                    device_write("audit package.install denied reason=signer-policy\r\n");
                    break;
                case LUNA_TRUST_RESULT_SOURCE:
                    device_write("audit package.install denied reason=source-policy\r\n");
                    break;
                case LUNA_TRUST_RESULT_BINDING:
                    device_write("audit package.install denied reason=source-signer-binding\r\n");
                    break;
                default:
                    device_write("audit package.install denied reason=security-trust\r\n");
                    break;
            }
            return 0;
        }
    }
    device_write("audit package.install approved=SECURITY\r\n");
    {
        struct luna_package_index_record index_record;
        struct luna_object_ref index_ref = {0u, 0u};
        if (find_index_record_by_name(meta.name, &index_ref, &index_record)) {
            old_app = index_record.app_object;
            old_install = index_record.install_object;
            old_index = index_ref;
        }
    }
    {
        struct luna_package_install_record install_record;
        struct luna_object_ref install_ref = {0u, 0u};
        if (find_install_record_by_name(meta.name, &install_ref, &install_record)) {
            if (old_install.low == 0u && old_install.high == 0u) {
                old_app = install_record.app_object;
                old_install = install_ref;
            }
        }
    }
    if (old_app.low == 0u && old_app.high == 0u) {
        (void)find_stale_app_object_by_name(meta.name, &old_app);
    }
    if (old_install.low == 0u && old_install.high == 0u) {
        (void)find_stale_install_record_by_name(meta.name, &old_install, 0);
    }
    if (old_index.low == 0u && old_index.high == 0u) {
        (void)find_stale_index_record_by_name(meta.name, &old_index, 0);
    }
    device_write("[PACKAGE] blob name=");
    device_write((const char *)meta.name);
    device_write("\r\n");
    device_write_hex_value("[PACKAGE] blob size=", blob_size);
    if ((old_app.low != 0u || old_app.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_installed_apps_set, old_app) != LUNA_DATA_OK) {
        return 0;
    }
    if ((old_install.low != 0u || old_install.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_install_records_set, old_install) != LUNA_DATA_OK) {
        return 0;
    }
    if ((old_index.low != 0u || old_index.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_app_index_set, old_index) != LUNA_DATA_OK) {
        return 0;
    }
    device_write("[PACKAGE] blob seed\r\n");
    app_object = old_app;
    if (app_object.low == 0u && app_object.high == 0u) {
        uint32_t status = data_seed_object(g_data_seed_cid, LUNA_DATA_OBJECT_TYPE_LUNA_APP, 0u, &app_object);
        if (status != LUNA_DATA_OK) {
            if (status == LUNA_DATA_ERR_NO_SPACE) {
                device_write("[PACKAGE] blob app seed no-space\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PACKAGE] blob app seed io\r\n");
            } else {
                device_write("[PACKAGE] blob app seed fail\r\n");
            }
            return 0;
        }
    }
    {
        const void *stable_blob = stable_blob_input;
        void *stage = data_stage_buffer(blob_size);
        if (stable_blob_input != 0 && blob_size != 0u && stage != 0) {
            copy_bytes(stage, stable_blob_input, (size_t)blob_size);
            stable_blob = stage;
        } else if (stable_blob_input != 0 && blob_size != 0u && blob_size <= sizeof(g_package_blob_stage)) {
            copy_bytes(g_package_blob_stage, stable_blob_input, (size_t)blob_size);
            stable_blob = g_package_blob_stage;
        }
        uint32_t status = data_pour_span(g_data_pour_cid, app_object, 0u, stable_blob, blob_size);
        if (status != LUNA_DATA_OK) {
            if (status == LUNA_DATA_ERR_NO_SPACE) {
                device_write("[PACKAGE] blob app pour no-space\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PACKAGE] blob app pour io\r\n");
            } else {
                device_write("[PACKAGE] blob app pour fail\r\n");
            }
            return 0;
        }
    }
    if (data_set_add_member(g_data_pour_cid, g_installed_apps_set, app_object) != LUNA_DATA_OK) {
        if ((!rewrite_set_add_member(g_installed_apps_set, app_object) &&
             (!rebuild_installed_apps_set() ||
              data_set_add_member(g_data_pour_cid, g_installed_apps_set, app_object) != LUNA_DATA_OK)) &&
            !rewrite_set_add_member(g_installed_apps_set, app_object)) {
            device_write("[PACKAGE] blob installed add fail\r\n");
            return 0;
        }
    }
    install_object = old_install;
    if (!write_install_record(&meta, app_object, &install_object)) {
        install_object.low = 0u;
        install_object.high = 0u;
        device_write("[PACKAGE] blob install fallback\r\n");
    }
    index_object = old_index;
    if (!write_index_record(&meta, app_object, install_object, &index_object)) {
        index_object.low = 0u;
        index_object.high = 0u;
        device_write("[PACKAGE] blob index fallback\r\n");
    }
    if (!audit_install_registration(&meta, app_object, install_object, index_object)) {
        device_write("[PACKAGE] blob audit fail\r\n");
        device_write("audit package.install denied reason=audit-registration\r\n");
        return 0;
    }
    upsert_runtime_package(&meta, app_object);
    device_write("audit package.install persisted=DATA authority=PACKAGE\r\n");
    device_write("[PACKAGE] blob commit\r\n");
    return 1;
}

static int rebuild_package_records(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if (!ensure_package_sets()) {
        return 0;
    }
    if (count_installed_luna_objects() == 0u) {
        return 1;
    }
    if (data_gather_set(g_data_gather_cid, g_installed_apps_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_app_metadata meta;
        if (!read_app_metadata_trace(refs[i], &meta, 0)) {
            continue;
        }
        if (find_index_record_by_name(meta.name, 0, 0)) {
            continue;
        }
        {
            struct luna_object_ref install_object = {0u, 0u};
            struct luna_object_ref index_object = {0u, 0u};
            if (!write_install_record(&meta, refs[i], &install_object) ||
                !write_index_record(&meta, refs[i], install_object, &index_object)) {
                return 0;
            }
        }
    }
    return 1;
}

static int bootstrap_installed_apps_from_embedded(void) {
    struct luna_embedded_app apps[] = {
        { g_manifest->app_guard_base, g_manifest->app_guard_size },
        { g_manifest->app_hello_base, g_manifest->app_hello_size },
        { g_manifest->app_files_base, g_manifest->app_files_size },
        { g_manifest->app_notes_base, g_manifest->app_notes_size },
        { g_manifest->app_console_base, g_manifest->app_console_size },
    };
    if (!ensure_package_sets()) {
        return 0;
    }
    if (count_installed_luna_objects() != 0u) {
        return rebuild_package_records();
    }
    for (size_t i = 0u; i < sizeof(apps) / sizeof(apps[0]); ++i) {
        if (apps[i].base == 0u || apps[i].size == 0u) {
            continue;
        }
        if (!register_package_blob((const void *)(uintptr_t)apps[i].base, apps[i].size)) {
            return 0;
        }
    }
    return 1;
}

static uint32_t build_installed_catalog(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    uint32_t copied = 0u;

    fill_builtin_catalog();
    copied = 5u;

    if ((g_install_records_set.low == 0u && g_install_records_set.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_install_records_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        fill_builtin_catalog();
        return 5u;
    }
    for (uint32_t i = 0u; i < count && copied < LUNA_PACKAGE_CAPACITY; ++i) {
        struct luna_package_install_record install_record;
        struct luna_app_metadata meta;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&install_record, sizeof(install_record));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &install_record, sizeof(install_record), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(install_record) ||
            install_record.magic != LUNA_PACKAGE_INSTALL_MAGIC ||
            install_record.version != LUNA_PACKAGE_INSTALL_VERSION ||
            !read_app_metadata_trace(install_record.app_object, &meta, 0) ||
            catalog_has_meta_name(meta.name)) {
            continue;
        }
        fill_record_from_meta(&g_catalog[copied], &meta, install_record.flags);
        copied += 1u;
    }
    if (copied == 0u && count_installed_luna_objects() != 0u && rebuild_package_records()) {
        device_write("[PACKAGE] catalog rebuild\r\n");
        if (data_gather_set(g_data_gather_cid, g_install_records_set, refs, sizeof(refs), &count) == LUNA_DATA_OK) {
            for (uint32_t i = 0u; i < count && copied < LUNA_PACKAGE_CAPACITY; ++i) {
                struct luna_package_install_record install_record;
                struct luna_app_metadata meta;
                uint64_t size = 0u;
                uint64_t content_size = 0u;
                zero_bytes(&install_record, sizeof(install_record));
                if (data_draw_span(g_data_draw_cid, refs[i], 0u, &install_record, sizeof(install_record), &size, &content_size) != LUNA_DATA_OK) {
                    continue;
                }
                if (size < sizeof(install_record) ||
                    install_record.magic != LUNA_PACKAGE_INSTALL_MAGIC ||
                    install_record.version != LUNA_PACKAGE_INSTALL_VERSION ||
                    !read_app_metadata_trace(install_record.app_object, &meta, 0)) {
                    continue;
                }
                fill_record_from_meta(&g_catalog[copied], &meta, install_record.flags);
                copied += 1u;
            }
        }
    }
    if ((g_installed_apps_set.low != 0u || g_installed_apps_set.high != 0u) &&
        data_gather_set(g_data_gather_cid, g_installed_apps_set, refs, sizeof(refs), &count) == LUNA_DATA_OK) {
        for (uint32_t i = 0u; i < count && copied < LUNA_PACKAGE_CAPACITY; ++i) {
            struct luna_app_metadata meta;
            zero_bytes(&meta, sizeof(meta));
            if (!read_app_metadata_trace(refs[i], &meta, 0) || catalog_has_meta_name(meta.name)) {
                continue;
            }
            fill_record_from_meta(&g_catalog[copied], &meta, 0u);
            copied += 1u;
        }
    }
    for (uint32_t i = 0u; i < LUNA_PACKAGE_CAPACITY && copied < LUNA_PACKAGE_CAPACITY; ++i) {
        if (g_runtime_packages[i].active == 0u || catalog_has_meta_name(g_runtime_packages[i].meta.name)) {
            continue;
        }
        fill_record_from_meta(&g_catalog[copied], &g_runtime_packages[i].meta, 0u);
        copied += 1u;
    }
    return copied;
}

void SYSV_ABI package_entry_boot(const struct luna_bootview *bootview) {
    uint32_t installed_raw = 0u;
    uint32_t installed_valid = 0u;
    uint32_t install_record_raw = 0u;
    uint32_t catalog_count = 0u;
    int installer_mode = bootview != 0 && bootview->system_mode == LUNA_MODE_INSTALL;
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    fill_builtin_catalog();
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[PACKAGE] catalog ready\r\n");
    }
    (void)request_capability(LUNA_CAP_PACKAGE_INSTALL, &g_package_install_cid);
    (void)request_capability(LUNA_CAP_DATA_SEED, &g_data_seed_cid);
    (void)request_capability(LUNA_CAP_DATA_POUR, &g_data_pour_cid);
    (void)request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid);
    (void)request_capability(LUNA_CAP_DATA_SHRED, &g_data_shred_cid);
    (void)request_capability(LUNA_CAP_DATA_GATHER, &g_data_gather_cid);
    (void)request_capability(LUNA_CAP_DATA_QUERY, &g_data_query_cid);
    if (installer_mode) {
        device_write("[PACKAGE] installer isolation active\r\n");
        device_write("[PACKAGE] bootstrap skipped install-mode\r\n");
        device_write("[PACKAGE] catalog builtin-only\r\n");
        device_write("[PACKAGE] boot done\r\n");
        return;
    }
    device_write("[PACKAGE] state load\r\n");
    (void)load_package_state();
    device_write("[PACKAGE] bootstrap\r\n");
    (void)bootstrap_installed_apps_from_embedded();
    (void)rebuild_package_records();
    installed_raw = count_set_members(g_installed_apps_set);
    installed_valid = count_installed_luna_objects();
    install_record_raw = count_set_members(g_install_records_set);
    catalog_count = build_installed_catalog();
    if (installed_raw != 0u && installed_valid == 0u) {
        device_write("[PACKAGE] installed unreadable\r\n");
    }
    if (install_record_raw != 0u && catalog_count == 0u) {
        device_write("[PACKAGE] install records unreadable\r\n");
    }
    if (installed_raw == 0u) {
        device_write("[PACKAGE] installed set empty\r\n");
    }
    if (install_record_raw == 0u) {
        device_write("[PACKAGE] install set empty\r\n");
    }
    device_write("[PACKAGE] policy sync\r\n");
    (void)sync_signer_policies();
    (void)sync_source_policies();
    device_write("[PACKAGE] boot done\r\n");
}

void SYSV_ABI package_entry_gate(struct luna_package_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_PACKAGE_ERR_INVALID_CAP;

    if (gate->opcode == LUNA_PACKAGE_INSTALL) {
        if (validate_capability(LUNA_CAP_PACKAGE_INSTALL, gate->cid_low, gate->cid_high, LUNA_PACKAGE_INSTALL) != LUNA_GATE_OK) {
            return;
        }
        if (!register_package_blob((const void *)(uintptr_t)gate->buffer_addr, gate->buffer_size)) {
            gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
            return;
        }
        (void)persist_package_state();
        gate->ticket = 1u;
        gate->status = LUNA_PACKAGE_OK;
        return;
    }

    if (gate->opcode == LUNA_PACKAGE_REMOVE) {
        struct luna_object_ref installed_ref = {0u, 0u};
        struct luna_app_metadata installed_meta;
        struct luna_package_index_record index_record;
        struct luna_object_ref index_ref = {0u, 0u};
        if (validate_capability(LUNA_CAP_PACKAGE_INSTALL, gate->cid_low, gate->cid_high, LUNA_PACKAGE_REMOVE) != LUNA_GATE_OK) {
            return;
        }
        device_write("audit package.remove start\r\n");
        if (!find_index_record_by_name(gate->name, &index_ref, &index_record)) {
            if (!rebuild_package_records() || !find_index_record_by_name(gate->name, &index_ref, &index_record)) {
                zero_bytes(&installed_meta, sizeof(installed_meta));
                if (!find_any_app_object_by_name(gate->name, &installed_ref, &installed_meta)) {
                    device_write("audit package.remove denied reason=not-found\r\n");
                    gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
                    return;
                }
                if (set_contains_ref(g_installed_apps_set, installed_ref) &&
                    data_set_remove_member(g_data_pour_cid, g_installed_apps_set, installed_ref) != LUNA_DATA_OK) {
                    gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
                    return;
                }
                if (data_shred_object(g_data_shred_cid, installed_ref) != LUNA_DATA_OK) {
                    device_write("[PACKAGE] remove app stale\r\n");
                }
                remove_runtime_package(installed_meta.name);
                if (!purge_stale_package_name(installed_meta.name) || !audit_remove_registration(installed_meta.name)) {
                    device_write("audit package.remove denied reason=audit-remove\r\n");
                    gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
                    return;
                }
                (void)persist_package_state();
                device_write("audit package.remove approved=SECURITY\r\n");
                device_write("audit package.remove persisted=DATA authority=PACKAGE\r\n");
                gate->ticket = 1u;
                gate->status = LUNA_PACKAGE_OK;
                return;
            }
        }
        if ((index_record.app_object.low != 0u || index_record.app_object.high != 0u) &&
            data_set_remove_member(g_data_pour_cid, g_installed_apps_set, index_record.app_object) != LUNA_DATA_OK) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        if ((index_record.install_object.low != 0u || index_record.install_object.high != 0u) &&
            data_set_remove_member(g_data_pour_cid, g_install_records_set, index_record.install_object) != LUNA_DATA_OK) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        if ((index_ref.low != 0u || index_ref.high != 0u) &&
            data_set_remove_member(g_data_pour_cid, g_app_index_set, index_ref) != LUNA_DATA_OK) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        if ((index_record.app_object.low != 0u || index_record.app_object.high != 0u) &&
            data_shred_object(g_data_shred_cid, index_record.app_object) != LUNA_DATA_OK) {
            device_write("[PACKAGE] remove app stale\r\n");
        }
        if ((index_record.install_object.low != 0u || index_record.install_object.high != 0u) &&
            data_shred_object(g_data_shred_cid, index_record.install_object) != LUNA_DATA_OK) {
            device_write("[PACKAGE] remove install stale\r\n");
        }
        if ((index_ref.low != 0u || index_ref.high != 0u) &&
            data_shred_object(g_data_shred_cid, index_ref) != LUNA_DATA_OK) {
            device_write("[PACKAGE] remove index stale\r\n");
        }
        remove_runtime_package(gate->name);
        if (!purge_stale_package_name(gate->name) || !audit_remove_registration(gate->name)) {
            device_write("audit package.remove denied reason=audit-remove\r\n");
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        (void)persist_package_state();
        device_write("audit package.remove approved=SECURITY\r\n");
        device_write("audit package.remove persisted=DATA authority=PACKAGE\r\n");
        gate->ticket = 1u;
        gate->status = LUNA_PACKAGE_OK;
        return;
    }

    if (gate->opcode == LUNA_PACKAGE_LIST) {
        struct luna_package_record *out = (struct luna_package_record *)(uintptr_t)gate->buffer_addr;
        struct luna_query_row rows[LUNA_PACKAGE_CAPACITY];
        uint32_t copied = 0u;
        uint32_t available = 0u;
        if (validate_capability(LUNA_CAP_PACKAGE_LIST, gate->cid_low, gate->cid_high, LUNA_PACKAGE_LIST) != LUNA_GATE_OK) {
            return;
        }
        if (gate->buffer_size < sizeof(g_catalog[0])) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        if (query_catalog_rows(rows, LUNA_PACKAGE_CAPACITY, &available) != LUNA_DATA_OK) {
            gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
            return;
        }
        zero_bytes(g_catalog, sizeof(g_catalog));
        for (uint32_t i = 0u; i < available; ++i) {
            fill_record_from_query_row(&g_catalog[copied], &rows[i]);
            if (((uint64_t)(copied + 1u) * sizeof(g_catalog[0])) > gate->buffer_size) {
                break;
            }
            copy_bytes(&out[copied], &g_catalog[i], sizeof(g_catalog[0]));
            copied += 1u;
        }
        device_write("audit package.query source=lasql kind=list\r\n");
        gate->result_count = copied;
        gate->status = LUNA_PACKAGE_OK;
        return;
    }

    if (gate->opcode == LUNA_PACKAGE_RESOLVE) {
        struct luna_object_ref installed_ref = {0u, 0u};
        struct luna_app_metadata installed_meta;
        struct luna_query_row catalog_row;
        struct luna_package_index_record index_record;
        struct luna_package_install_record install_record;
        struct luna_object_ref install_ref = {0u, 0u};
        struct luna_package_resolve_record *out = (struct luna_package_resolve_record *)(uintptr_t)gate->buffer_addr;
        if (validate_capability(LUNA_CAP_PACKAGE_LIST, gate->cid_low, gate->cid_high, LUNA_PACKAGE_RESOLVE) != LUNA_GATE_OK) {
            return;
        }
        if (gate->buffer_addr == 0u || gate->buffer_size < sizeof(*out)) {
            gate->status = LUNA_PACKAGE_ERR_NO_ROOM;
            return;
        }
        zero_bytes(&catalog_row, sizeof(catalog_row));
        if (find_catalog_row_by_name(gate->name, &catalog_row)) {
            if (!find_install_record_by_name(catalog_row.name, &install_ref, &install_record)) {
                gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
                return;
            }
            zero_bytes(out, sizeof(*out));
            copy_bytes(out->name, catalog_row.name, sizeof(out->name));
            out->flags = catalog_row.flags;
            out->version = catalog_row.version;
            out->app_object = install_record.app_object;
            out->install_object = install_ref;
            gate->result_count = 1u;
            gate->status = LUNA_PACKAGE_OK;
            device_write("audit package.query source=lasql kind=resolve\r\n");
            device_write_ref_line("[PACKAGE] resolve app", out->app_object);
            return;
        }
        if (!find_index_record_by_name(gate->name, 0, &index_record)) {
            if (!rebuild_package_records() || !find_index_record_by_name(gate->name, 0, &index_record)) {
                zero_bytes(&installed_meta, sizeof(installed_meta));
                if (!find_any_app_object_by_name(gate->name, &installed_ref, &installed_meta)) {
                    if (count_set_members(g_installed_apps_set) == 0u) {
                        device_write("[PACKAGE] resolve installed-empty\r\n");
                    } else {
                        device_write("[PACKAGE] resolve installed-miss\r\n");
                    }
                    device_write("[PACKAGE] resolve miss\r\n");
                    gate->status = LUNA_PACKAGE_ERR_NOT_FOUND;
                    return;
                }
                zero_bytes(out, sizeof(*out));
                copy_bytes(out->name, installed_meta.name, sizeof(out->name));
                out->version = installed_meta.app_version;
                out->app_object = installed_ref;
                out->install_object = (struct luna_object_ref){0u, 0u};
                gate->result_count = 1u;
                gate->status = LUNA_PACKAGE_OK;
                device_write_ref_line("[PACKAGE] resolve app", installed_ref);
                device_write("[PACKAGE] resolve installed\r\n");
                return;
            }
            device_write("[PACKAGE] resolve rebuild\r\n");
        }
        zero_bytes(out, sizeof(*out));
        copy_bytes(out->name, index_record.name, sizeof(out->name));
        out->flags = index_record.flags;
        out->version = index_record.app_version;
        out->app_object = index_record.app_object;
        out->install_object = index_record.install_object;
        gate->result_count = 1u;
        gate->status = LUNA_PACKAGE_OK;
        device_write_ref_line("[PACKAGE] resolve app", index_record.app_object);
    }
}
