#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_DATA_OBJECT_TYPE_PACKAGE_STATE 0x504B4753u
#define LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL 0x504B4749u
#define LUNA_PACKAGE_STATE_MAGIC 0x504B4753u
#define LUNA_PACKAGE_STATE_VERSION 5u
#define LUNA_PACKAGE_STATE_VERSION_V4 4u
#define LUNA_PACKAGE_INSTALL_MAGIC 0x504B4749u
#define LUNA_PACKAGE_INSTALL_VERSION 1u
#define LUNA_PACKAGE_UPDATE_TXN_MAGIC 0x55505458u
#define LUNA_PACKAGE_UPDATE_TXN_VERSION 1u
#define LUNA_PACKAGE_UPDATE_KIND_REPLACE 2u
#define LUNA_PROGRAM_BUNDLE_MAGIC 0x4C554E42u
#define LUNA_UPDATE_TARGET_NAME "sample"
#define LUNA_STORE_MAGIC 0x5346414Cu
#define LUNA_STORE_VERSION 3u
#define LUNA_NATIVE_RESERVED_PROFILE 6u
#define LUNA_NATIVE_RESERVED_INSTALL_LOW 7u
#define LUNA_NATIVE_RESERVED_INSTALL_HIGH 8u
#define LUNA_NATIVE_RESERVED_ACTIVATION 9u
#define LUNA_NATIVE_RESERVED_PEER_LBA 10u

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

static struct luna_cid g_device_write_cid = {0, 0};
static struct luna_cid g_device_read_cid = {0, 0};
static struct luna_cid g_data_seed_cid = {0, 0};
static struct luna_cid g_data_pour_cid = {0, 0};
static struct luna_cid g_data_draw_cid = {0, 0};
static struct luna_cid g_data_gather_cid = {0, 0};
static struct luna_cid g_data_shred_cid = {0, 0};
static struct luna_cid g_package_install_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;
static struct luna_bootview g_bootview = {0};
static uint8_t g_update_bundle_stage[16384];

static void device_write(const char *text);

#undef memcpy
#undef memset

void *memcpy(void *dest, const void *src, size_t len) {
    uint8_t *out = (uint8_t *)dest;
    const uint8_t *in = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        out[i] = in[i];
    }
    return dest;
}

void *memset(void *dest, int value, size_t len) {
    uint8_t *out = (uint8_t *)dest;
    for (size_t i = 0; i < len; ++i) {
        out[i] = (uint8_t)value;
    }
    return dest;
}

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
    uint64_t install_uuid_low;
    uint64_t install_uuid_high;
    uint64_t system_store_lba;
    uint64_t data_store_lba;
    uint64_t activation_state;
    char name[16];
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

static struct luna_object_ref g_package_state_object = {0u, 0u};
static struct luna_package_state_record g_package_state = {0};
static struct luna_object_ref g_latest_txn_ref = {0u, 0u};

static int load_package_state(void);
static int persist_package_state(void);
static int load_latest_update_txn(struct luna_package_update_txn_record *out);
static int persist_update_txn(const struct luna_package_update_txn_record *txn);

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)g_manifest->security_gate_base)[i] = 0;
    }
    gate->sequence = 43;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_UPDATE;
    gate->domain_key = domain_key;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static int names_equal(const char a[16], const char *b) {
    size_t i = 0u;
    while (i < 16u) {
        char rhs = b[i];
        if (a[i] != rhs) {
            return 0;
        }
        if (rhs == '\0') {
            return 1;
        }
        i += 1u;
    }
    return 1;
}

static void copy_name16(volatile char out[16], const char *text) {
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

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)g_manifest->security_gate_base)[i] = 0;
    }
    gate->sequence = 45;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = 0;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_UPDATE;
    gate->target_gate = target_gate;
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

static int govern_storage_state(uint32_t target_state) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    struct luna_governance_query query;
    zero_bytes(&query, sizeof(query));
    query.action = LUNA_GOVERN_MOUNT;
    query.result_state = target_state;
    query.mode = g_bootview.system_mode;
    query.install_low = g_bootview.install_uuid_low;
    query.install_high = g_bootview.install_uuid_high;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 47;
    gate->opcode = LUNA_GATE_GOVERN;
    gate->caller_space = LUNA_SPACE_UPDATE;
    gate->buffer_addr = (uint64_t)(uintptr_t)&query;
    gate->buffer_size = sizeof(query);
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    g_bootview.volume_state = query.result_state;
    g_bootview.system_mode = query.mode;
    return gate->status == LUNA_GATE_OK &&
           query.result_state != LUNA_VOLUME_FATAL_INCOMPATIBLE &&
           query.result_state != LUNA_VOLUME_FATAL_UNRECOVERABLE;
}

static void route_storage_failure(const char *reason) {
    device_write(reason);
    (void)govern_storage_state(LUNA_VOLUME_RECOVERY_REQUIRED);
    if (g_bootview.system_mode == LUNA_MODE_RECOVERY) {
        device_write("audit update.apply storage=recovery\r\n");
    } else if (g_bootview.system_mode == LUNA_MODE_READONLY) {
        device_write("audit update.apply storage=readonly\r\n");
    }
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;
    while (text[size] != '\0') {
        size += 1u;
    }
    for (size_t i = 0; i < sizeof(struct luna_device_gate); ++i) {
        ((volatile uint8_t *)(uintptr_t)manifest->device_gate_base)[i] = 0;
    }
    gate->sequence = 44;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)manifest->device_gate_base);
}

static uint32_t device_read_sector(uint32_t lba, void *buffer) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 44;
    gate->opcode = LUNA_DEVICE_BLOCK_READ;
    gate->device_id = LUNA_DEVICE_ID_DISK0;
    gate->flags = lba;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = 512u;
    gate->size = 0u;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    return gate->status;
}

static uint32_t device_write_sector(uint32_t lba, const void *buffer) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 45;
    gate->opcode = LUNA_DEVICE_BLOCK_WRITE;
    gate->device_id = LUNA_DEVICE_ID_DISK0;
    gate->flags = lba;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = 512u;
    gate->size = 512u;
    ((void (SYSV_ABI *)(struct luna_device_gate *))(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    return gate->status;
}

static int load_store_super(uint64_t lba, struct luna_store_superblock *out_super) {
    zero_bytes(out_super, sizeof(*out_super));
    return device_read_sector((uint32_t)lba, out_super) == LUNA_DEVICE_OK;
}

static int save_store_super(uint64_t lba, const struct luna_store_superblock *super) {
    return device_write_sector((uint32_t)lba, super) == LUNA_DEVICE_OK;
}

static int stores_match_contract(struct luna_store_superblock *system_super, struct luna_store_superblock *data_super) {
    return system_super->magic == LUNA_STORE_MAGIC &&
           system_super->version == LUNA_STORE_VERSION &&
           system_super->reserved[LUNA_NATIVE_RESERVED_PROFILE] == LUNA_NATIVE_PROFILE_SYSTEM &&
           system_super->reserved[LUNA_NATIVE_RESERVED_INSTALL_LOW] == g_bootview.install_uuid_low &&
           system_super->reserved[LUNA_NATIVE_RESERVED_INSTALL_HIGH] == g_bootview.install_uuid_high &&
           system_super->reserved[LUNA_NATIVE_RESERVED_PEER_LBA] == g_bootview.data_store_lba &&
           data_super->magic == LUNA_STORE_MAGIC &&
           data_super->version == LUNA_STORE_VERSION &&
           data_super->reserved[LUNA_NATIVE_RESERVED_PROFILE] == LUNA_NATIVE_PROFILE_DATA &&
           data_super->reserved[LUNA_NATIVE_RESERVED_INSTALL_LOW] == g_bootview.install_uuid_low &&
           data_super->reserved[LUNA_NATIVE_RESERVED_INSTALL_HIGH] == g_bootview.install_uuid_high &&
           data_super->reserved[LUNA_NATIVE_RESERVED_PEER_LBA] == g_bootview.system_store_lba;
}

static int load_install_contract(struct luna_store_superblock *system_super, struct luna_store_superblock *data_super) {
    if (!load_store_super(g_bootview.system_store_lba, system_super) ||
        !load_store_super(g_bootview.data_store_lba, data_super)) {
        return 0;
    }
    return stores_match_contract(system_super, data_super);
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
    zero_bytes(buffer, (size_t)buffer_size);
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 46;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = offset;
    gate->size = buffer_size;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = buffer_size;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    *out_size = gate->size;
    if (out_content_size != 0) {
        *out_content_size = gate->content_size;
    }
    return gate->status;
}

static uint32_t data_seed_object(struct luna_cid cid, uint32_t object_type, uint64_t flags, struct luna_object_ref *out_ref) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 46;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_type = object_type;
    gate->object_flags = (uint32_t)flags;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    out_ref->low = gate->object_low;
    out_ref->high = gate->object_high;
    return gate->status;
}

static uint32_t data_pour_span(struct luna_cid cid, struct luna_object_ref object, const void *buffer, uint64_t size) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 47;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = 0u;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = size;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_gather_set(struct luna_cid cid, struct luna_object_ref object, struct luna_object_ref *out_refs, uint64_t buffer_size, uint32_t *out_count) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes(out_refs, (size_t)buffer_size);
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 48;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)out_refs;
    gate->buffer_size = buffer_size;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    *out_count = gate->result_count;
    return gate->status;
}

static uint32_t data_set_add_member(struct luna_cid cid, struct luna_object_ref set_object, struct luna_object_ref member) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 49;
    gate->opcode = LUNA_DATA_SET_ADD_MEMBER;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_object.low;
    gate->object_high = set_object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&member;
    gate->buffer_size = sizeof(member);
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_set_remove_member(struct luna_cid cid, struct luna_object_ref set_object, struct luna_object_ref member) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 50;
    gate->opcode = LUNA_DATA_SET_REMOVE_MEMBER;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_object.low;
    gate->object_high = set_object.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&member;
    gate->buffer_size = sizeof(member);
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static uint32_t data_shred_object(struct luna_cid cid, struct luna_object_ref object) {
    volatile struct luna_data_gate *gate = (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 51;
    gate->opcode = LUNA_DATA_SHRED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)g_manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base);
    return gate->status;
}

static int find_install_record_by_name(const char *name, struct luna_package_install_record *out_record) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    if ((g_package_state.install_records_set.low == 0u && g_package_state.install_records_set.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_package_state.install_records_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
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
        if (size < sizeof(record) ||
            record.magic != LUNA_PACKAGE_INSTALL_MAGIC ||
            record.version != LUNA_PACKAGE_INSTALL_VERSION ||
            !names_equal(record.name, name)) {
            continue;
        }
        if (out_record != 0) {
            *out_record = record;
        }
        return 1;
    }
    return 0;
}

static int package_install_blob(const void *buffer, uint64_t size) {
    volatile struct luna_package_gate *gate = (volatile struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 54;
    gate->opcode = LUNA_PACKAGE_INSTALL;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = size;
    ((void (SYSV_ABI *)(struct luna_package_gate *))(uintptr_t)g_manifest->package_gate_entry)((struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base);
    return gate->status == LUNA_PACKAGE_OK;
}

static int package_remove_name(const char *name) {
    volatile struct luna_package_gate *gate = (volatile struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 55;
    gate->opcode = LUNA_PACKAGE_REMOVE;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    copy_name16(gate->name, name);
    ((void (SYSV_ABI *)(struct luna_package_gate *))(uintptr_t)g_manifest->package_gate_entry)((struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base);
    return gate->status == LUNA_PACKAGE_OK;
}

static uint32_t security_trust_eval_bundle(struct luna_bundle_header *bundle, void *blob, uint64_t blob_size, struct luna_trust_eval_request *out_request) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    struct luna_trust_eval_request request;

    if (bundle == 0 || blob == 0 || blob_size == 0u) {
        return LUNA_GATE_DENIED;
    }

    zero_bytes(&request, sizeof(request));
    request.flags = bundle->flags;
    request.abi_major = bundle->abi_major;
    request.abi_minor = bundle->abi_minor;
    request.sdk_major = bundle->sdk_major;
    request.sdk_minor = bundle->sdk_minor;
    request.bundle_id = bundle->bundle_id;
    request.source_id = bundle->source_id;
    request.signer_id = bundle->signer_id;
    request.app_version = bundle->app_version;
    request.integrity_check = bundle->integrity_check;
    request.header_bytes = bundle->header_bytes;
    request.entry_offset = bundle->entry_offset;
    request.signature_check = bundle->signature_check;
    request.blob_addr = (uint64_t)(uintptr_t)blob;
    request.blob_size = blob_size;
    request.capability_count = bundle->capability_count;
    request.min_proto_version = bundle->min_proto_version;
    request.max_proto_version = bundle->max_proto_version;
    memcpy(request.name, bundle->name, sizeof(request.name));
    memcpy(request.capability_keys, bundle->capability_keys, sizeof(request.capability_keys));

    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 56;
    gate->opcode = LUNA_GATE_TRUST_EVAL;
    gate->caller_space = LUNA_SPACE_UPDATE;
    gate->domain_key = LUNA_CAP_PACKAGE_INSTALL;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    if (out_request != 0) {
        memcpy(out_request, &request, sizeof(request));
    }
    return gate->status;
}

static int current_target_install(struct luna_package_install_record *out_record) {
    return load_package_state() && find_install_record_by_name(LUNA_UPDATE_TARGET_NAME, out_record);
}

static int prepare_candidate_bundle(
    struct luna_package_install_record *out_current,
    void **out_buffer,
    uint64_t *out_size,
    uint64_t *out_target_version
) {
    struct luna_package_install_record current;
    struct luna_bundle_header *bundle = 0;
    struct luna_trust_eval_request trust;
    uint64_t size = 0u;
    uint64_t content_size = 0u;
    void *buffer = g_update_bundle_stage;

    if (!current_target_install(&current) ||
        sizeof(g_update_bundle_stage) < sizeof(struct luna_bundle_header)) {
        return 0;
    }
    if (data_draw_span(g_data_draw_cid, current.app_object, 0u, buffer, sizeof(g_update_bundle_stage), &size, &content_size) != LUNA_DATA_OK ||
        content_size < sizeof(struct luna_bundle_header) ||
        content_size > sizeof(g_update_bundle_stage)) {
        return 0;
    }
    bundle = (struct luna_bundle_header *)buffer;
    if (bundle->magic != LUNA_PROGRAM_BUNDLE_MAGIC || bundle->version != LUNA_PROGRAM_BUNDLE_VERSION) {
        return 0;
    }
    if (bundle->header_bytes < sizeof(struct luna_bundle_header) ||
        bundle->entry_offset < bundle->header_bytes ||
        bundle->entry_offset >= content_size) {
        return 0;
    }
    if (!names_equal(bundle->name, LUNA_UPDATE_TARGET_NAME)) {
        return 0;
    }
    bundle->app_version = current.app_version + 1u;
    if (security_trust_eval_bundle(bundle, buffer, content_size, &trust) != LUNA_GATE_OK) {
        return 0;
    }
    bundle->integrity_check = trust.result_integrity;
    bundle->signature_check = trust.result_signature;
    if (out_current != 0) {
        *out_current = current;
    }
    if (out_buffer != 0) {
        *out_buffer = buffer;
    }
    if (out_size != 0) {
        *out_size = content_size;
    }
    if (out_target_version != 0) {
        *out_target_version = bundle->app_version;
    }
    return 1;
}

static int persist_new_txn(struct luna_package_update_txn_record *txn) {
    struct luna_object_ref object = {0u, 0u};
    if (!load_package_state()) {
        return 0;
    }
    if (txn->txn_id == 0u) {
        struct luna_package_update_txn_record latest;
        txn->txn_id = g_package_state.next_txn_id == 0u ? 1u : g_package_state.next_txn_id;
        if (load_latest_update_txn(&latest) && latest.txn_id >= txn->txn_id) {
            txn->txn_id = latest.txn_id + 1u;
        }
        g_package_state.next_txn_id = txn->txn_id + 1u;
    }
    txn->install_uuid_low = g_bootview.install_uuid_low;
    txn->install_uuid_high = g_bootview.install_uuid_high;
    txn->system_store_lba = g_bootview.system_store_lba;
    txn->data_store_lba = g_bootview.data_store_lba;
    if (data_seed_object(g_data_seed_cid, LUNA_PACKAGE_UPDATE_TXN_MAGIC, 0u, &object) != LUNA_DATA_OK ||
        data_pour_span(g_data_pour_cid, object, txn, sizeof(*txn)) != LUNA_DATA_OK ||
        data_set_add_member(g_data_pour_cid, g_package_state.update_txn_log_set, object) != LUNA_DATA_OK) {
        return 0;
    }
    g_latest_txn_ref = object;
    g_package_state.update_txn_object = object;
    return persist_package_state();
}

static int mark_committed_txn_active(void) {
    struct luna_package_update_txn_record txn;
    struct luna_package_install_record current;
    struct luna_store_superblock system_super;
    struct luna_store_superblock data_super;
    if (!load_package_state() || !load_latest_update_txn(&txn)) {
        return 0;
    }
    if (txn.state != LUNA_UPDATE_TXN_STATE_COMMITTED) {
        return 1;
    }
    if (!load_install_contract(&system_super, &data_super)) {
        route_storage_failure("audit update.apply denied reason=contract\r\n");
        txn.state = LUNA_UPDATE_TXN_STATE_FAILED;
        return persist_update_txn(&txn);
    }
    if (!find_install_record_by_name(txn.name, &current)) {
        txn.state = LUNA_UPDATE_TXN_STATE_FAILED;
        return persist_update_txn(&txn);
    }
    if (current.app_version == txn.target_version &&
        system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] == LUNA_ACTIVATION_COMMITTED) {
        system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] = LUNA_ACTIVATION_ACTIVE;
        if (!save_store_super(g_bootview.system_store_lba, &system_super)) {
            route_storage_failure("audit update.apply denied reason=activation-write\r\n");
            txn.state = LUNA_UPDATE_TXN_STATE_FAILED;
            return persist_update_txn(&txn);
        }
        txn.state = LUNA_UPDATE_TXN_STATE_ACTIVATED;
        txn.current_version = current.app_version;
        txn.activation_state = LUNA_ACTIVATION_ACTIVE;
        g_package_state.update_txn_object = g_latest_txn_ref;
        if (!persist_update_txn(&txn)) {
            route_storage_failure("audit update.apply denied reason=txn-log\r\n");
            return 0;
        }
        device_write("audit update.apply activation=LSYS\r\n");
        device_write("audit update.apply persisted=DATA authority=UPDATE\r\n");
        return 1;
    }
    device_write("audit update.apply denied reason=activation-mismatch\r\n");
    txn.state = LUNA_UPDATE_TXN_STATE_FAILED;
    return persist_update_txn(&txn);
}

static int load_package_state(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    uint64_t best_high = 0u;
    int found = 0;
    if (data_gather_set(g_data_gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_state_record state;
        struct luna_package_state_record_v4 legacy_v4;
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
            g_package_state = state;
            best_high = refs[i].high;
            found = 1;
            continue;
        }
        if (state.version != LUNA_PACKAGE_STATE_VERSION_V4 || size < sizeof(legacy_v4)) {
            continue;
        }
        if (found && refs[i].high < best_high) {
            continue;
        }
        memcpy(&legacy_v4, &state, sizeof(legacy_v4));
        zero_bytes(&g_package_state, sizeof(g_package_state));
        g_package_state.magic = legacy_v4.magic;
        g_package_state.version = LUNA_PACKAGE_STATE_VERSION;
        g_package_state.installed_apps_set = legacy_v4.installed_apps_set;
        g_package_state.install_records_set = legacy_v4.install_records_set;
        g_package_state.app_index_set = legacy_v4.app_index_set;
        g_package_state.trusted_source_set = legacy_v4.trusted_source_set;
        g_package_state.trusted_signer_set = (struct luna_object_ref){0u, 0u};
        g_package_state.update_txn_log_set = legacy_v4.update_txn_log_set;
        g_package_state.next_txn_id = legacy_v4.next_txn_id;
        g_package_state.update_txn_object = legacy_v4.update_txn_object;
        g_package_state.rollback_refs_set = legacy_v4.rollback_refs_set;
        g_package_state_object = refs[i];
        best_high = refs[i].high;
        found = 1;
    }
    return found;
}

static int persist_package_state(void) {
    return data_pour_span(g_data_pour_cid, g_package_state_object, &g_package_state, sizeof(g_package_state)) == LUNA_DATA_OK;
}

static int load_latest_update_txn(struct luna_package_update_txn_record *out) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;
    uint64_t best_txn_id = 0u;
    g_latest_txn_ref = (struct luna_object_ref){0u, 0u};
    zero_bytes(out, sizeof(*out));
    if ((g_package_state.update_txn_log_set.low == 0u && g_package_state.update_txn_log_set.high == 0u) ||
        data_gather_set(g_data_gather_cid, g_package_state.update_txn_log_set, refs, sizeof(refs), &count) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        struct luna_package_update_txn_record candidate;
        uint64_t size = 0u;
        uint64_t content_size = 0u;
        zero_bytes(&candidate, sizeof(candidate));
        if (data_draw_span(g_data_draw_cid, refs[i], 0u, &candidate, sizeof(candidate), &size, &content_size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(candidate) ||
            candidate.magic != LUNA_PACKAGE_UPDATE_TXN_MAGIC ||
            candidate.version != LUNA_PACKAGE_UPDATE_TXN_VERSION ||
            candidate.txn_id == 0u) {
            continue;
        }
        if (candidate.txn_id < best_txn_id) {
            continue;
        }
        best_txn_id = candidate.txn_id;
        *out = candidate;
        g_latest_txn_ref = refs[i];
    }
    return best_txn_id != 0u;
}

static int persist_update_txn(const struct luna_package_update_txn_record *txn) {
    if ((g_latest_txn_ref.low == 0u && g_latest_txn_ref.high == 0u)) {
        return 0;
    }
    return data_pour_span(g_data_pour_cid, g_latest_txn_ref, txn, sizeof(*txn)) == LUNA_DATA_OK;
}

static int rollback_txn(struct luna_package_update_txn_record *txn, uint32_t final_state) {
    struct luna_store_superblock system_super;
    struct luna_store_superblock data_super;
    if ((txn->new_app_object.low != 0u || txn->new_app_object.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_package_state.installed_apps_set, txn->new_app_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->new_install_object.low != 0u || txn->new_install_object.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_package_state.install_records_set, txn->new_install_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->new_index_object.low != 0u || txn->new_index_object.high != 0u) &&
        data_set_remove_member(g_data_pour_cid, g_package_state.app_index_set, txn->new_index_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->old_app_object.low != 0u || txn->old_app_object.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_package_state.installed_apps_set, txn->old_app_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->old_install_object.low != 0u || txn->old_install_object.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_package_state.install_records_set, txn->old_install_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->old_index_object.low != 0u || txn->old_index_object.high != 0u) &&
        data_set_add_member(g_data_pour_cid, g_package_state.app_index_set, txn->old_index_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->new_app_object.low != 0u || txn->new_app_object.high != 0u) &&
        data_shred_object(g_data_shred_cid, txn->new_app_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->new_install_object.low != 0u || txn->new_install_object.high != 0u) &&
        data_shred_object(g_data_shred_cid, txn->new_install_object) != LUNA_DATA_OK) {
        return 0;
    }
    if ((txn->new_index_object.low != 0u || txn->new_index_object.high != 0u) &&
        data_shred_object(g_data_shred_cid, txn->new_index_object) != LUNA_DATA_OK) {
        return 0;
    }
    if (load_install_contract(&system_super, &data_super)) {
        system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] = LUNA_ACTIVATION_ACTIVE;
        if (!save_store_super(g_bootview.system_store_lba, &system_super)) {
            return 0;
        }
    }
    txn->state = final_state;
    txn->activation_state = LUNA_ACTIVATION_ACTIVE;
    g_package_state.update_txn_object = g_latest_txn_ref;
    return persist_package_state() && persist_update_txn(txn);
}

static int recover_incomplete_txn(void) {
    struct luna_package_update_txn_record txn;
    if (!load_package_state() || !load_latest_update_txn(&txn)) {
        return 1;
    }
    g_package_state.update_txn_object = g_latest_txn_ref;
    if (txn.state == LUNA_UPDATE_TXN_STATE_COMMITTED) {
        return mark_committed_txn_active();
    }
    if (txn.state != LUNA_UPDATE_TXN_STATE_STAGED) {
        return 1;
    }
    return rollback_txn(&txn, LUNA_UPDATE_TXN_STATE_FAILED);
}

static int rollback_last_update(void) {
    struct luna_package_update_txn_record txn;
    if (!load_package_state() || !load_latest_update_txn(&txn)) {
        return 0;
    }
    if (txn.state != LUNA_UPDATE_TXN_STATE_ACTIVATED && txn.state != LUNA_UPDATE_TXN_STATE_FAILED) {
        return 0;
    }
    g_package_state.update_txn_object = g_latest_txn_ref;
    return rollback_txn(&txn, LUNA_UPDATE_TXN_STATE_ROLLED_BACK);
}

void SYSV_ABI update_entry_boot(const struct luna_bootview *bootview) {
    g_bootview = *bootview;
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[UPDATE] wave ready\r\n");
    } else {
        serial_write("[UPDATE] cap fail\r\n");
    }
    (void)request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid);
    (void)request_capability(LUNA_CAP_DATA_SEED, &g_data_seed_cid);
    (void)request_capability(LUNA_CAP_DATA_POUR, &g_data_pour_cid);
    (void)request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid);
    (void)request_capability(LUNA_CAP_DATA_GATHER, &g_data_gather_cid);
    (void)request_capability(LUNA_CAP_DATA_SHRED, &g_data_shred_cid);
    (void)request_capability(LUNA_CAP_PACKAGE_INSTALL, &g_package_install_cid);
    (void)recover_incomplete_txn();
    device_write("audit recovery.contract checked\r\n");
}

void SYSV_ABI update_entry_gate(struct luna_update_gate *gate) {
    gate->result_count = 0u;
    gate->status = LUNA_UPDATE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_UPDATE_CHECK) {
        struct luna_package_update_txn_record txn;
        struct luna_package_install_record current;
        uint64_t target_version = 0u;
        (void)recover_incomplete_txn();
        if (validate_capability(LUNA_CAP_UPDATE_CHECK, gate->cid_low, gate->cid_high, LUNA_UPDATE_CHECK) != LUNA_GATE_OK) {
            return;
        }
        if (load_package_state() && load_latest_update_txn(&txn)) {
            gate->current_version = txn.current_version;
            gate->target_version = txn.target_version;
            gate->flags = txn.state;
        } else if (prepare_candidate_bundle(&current, 0, 0, &target_version)) {
            gate->current_version = current.app_version;
            gate->target_version = target_version;
            gate->flags = LUNA_UPDATE_TXN_STATE_STAGED;
        } else {
            gate->current_version = 0u;
            gate->target_version = 0u;
            gate->flags = LUNA_UPDATE_TXN_STATE_EMPTY;
        }
        gate->result_count = 1u;
        gate->status = LUNA_UPDATE_OK;
        return;
    }
    if (gate->opcode == LUNA_UPDATE_APPLY) {
        struct luna_package_install_record current;
        struct luna_package_update_txn_record txn;
        struct luna_store_superblock system_super;
        struct luna_store_superblock data_super;
        void *bundle = 0;
        uint64_t bundle_size = 0u;
        uint64_t target_version = 0u;
        if (validate_capability(LUNA_CAP_UPDATE_APPLY, gate->cid_low, gate->cid_high, LUNA_UPDATE_APPLY) != LUNA_GATE_OK) {
            return;
        }
        device_write("audit update.apply start\r\n");
        if (!load_install_contract(&system_super, &data_super)) {
            route_storage_failure("audit update.apply denied reason=contract\r\n");
            gate->status = LUNA_UPDATE_ERR_INVALID_CAP;
            return;
        }
        if ((gate->flags & LUNA_UPDATE_FLAG_ROLLBACK) != 0u) {
            if (!rollback_last_update()) {
                device_write("audit recovery.denied reason=rollback\r\n");
                gate->status = LUNA_UPDATE_ERR_INVALID_CAP;
                return;
            }
            device_write("audit recovery.persisted=DATA authority=UPDATE\r\n");
        }
        if (!prepare_candidate_bundle(&current, &bundle, &bundle_size, &target_version) ||
            target_version <= current.app_version) {
            gate->current_version = 0u;
            gate->target_version = 0u;
            gate->flags = LUNA_UPDATE_TXN_STATE_EMPTY;
            gate->status = LUNA_UPDATE_OK;
            return;
        }
        if (!package_remove_name(LUNA_UPDATE_TARGET_NAME) || !package_install_blob(bundle, bundle_size)) {
            route_storage_failure("audit update.apply denied reason=package-chain\r\n");
            gate->current_version = current.app_version;
            gate->target_version = target_version;
            gate->flags = LUNA_UPDATE_TXN_STATE_FAILED;
            gate->status = LUNA_UPDATE_OK;
            return;
        }
        zero_bytes(&txn, sizeof(txn));
        txn.magic = LUNA_PACKAGE_UPDATE_TXN_MAGIC;
        txn.version = LUNA_PACKAGE_UPDATE_TXN_VERSION;
        txn.state = LUNA_UPDATE_TXN_STATE_COMMITTED;
        txn.kind = LUNA_PACKAGE_UPDATE_KIND_REPLACE;
        txn.current_version = current.app_version;
        txn.target_version = target_version;
        txn.old_app_object = current.app_object;
        txn.activation_state = LUNA_ACTIVATION_COMMITTED;
        copy_name16(txn.name, LUNA_UPDATE_TARGET_NAME);
        system_super.reserved[LUNA_NATIVE_RESERVED_ACTIVATION] = LUNA_ACTIVATION_COMMITTED;
        if (!save_store_super(g_bootview.system_store_lba, &system_super)) {
            route_storage_failure("audit update.apply denied reason=lsys-commit\r\n");
            gate->current_version = current.app_version;
            gate->target_version = target_version;
            gate->flags = LUNA_UPDATE_TXN_STATE_FAILED;
            gate->status = LUNA_UPDATE_OK;
            return;
        }
        if (!persist_new_txn(&txn)) {
            struct luna_package_update_txn_record committed_txn;
            if (load_package_state() &&
                load_latest_update_txn(&committed_txn) &&
                committed_txn.state == LUNA_UPDATE_TXN_STATE_COMMITTED &&
                committed_txn.current_version == current.app_version &&
                committed_txn.target_version == target_version) {
                g_package_state.update_txn_object = g_latest_txn_ref;
                (void)persist_package_state();
                device_write("audit update.apply approved=SECURITY\r\n");
                device_write("audit update.apply activation=COMMITTED\r\n");
                gate->current_version = committed_txn.current_version;
                gate->target_version = committed_txn.target_version;
                gate->flags = committed_txn.state;
                gate->status = LUNA_UPDATE_OK;
                return;
            }
            route_storage_failure("audit update.apply denied reason=txn-log\r\n");
            gate->current_version = current.app_version;
            gate->target_version = target_version;
            gate->flags = LUNA_UPDATE_TXN_STATE_FAILED;
            gate->status = LUNA_UPDATE_OK;
            return;
        }
        device_write("audit update.apply approved=SECURITY\r\n");
        device_write("audit update.apply activation=COMMITTED\r\n");
        gate->current_version = txn.current_version;
        gate->target_version = txn.target_version;
        gate->flags = txn.state;
        gate->status = LUNA_UPDATE_OK;
    }
}
