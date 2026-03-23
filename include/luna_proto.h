#ifndef LUNA_PROTO_H
#define LUNA_PROTO_H

#include <stdint.h>

#define LUNA_MANIFEST_MAGIC 0x414E554Cull
#define LUNA_PROTO_VERSION 0x00000250u

#define LUNA_DATA_OBJECT_CAPACITY 128u
#define LUNA_DATA_SLOT_BYTES 512u
#define LUNA_DATA_STORE_LBA 1024u
#define LUNA_DATA_META_SECTORS 23u
#define LUNA_DATA_BITMAP_SECTORS 1u
#define LUNA_DATA_SLOT_SECTORS 1u
#define LUNA_LIFECYCLE_CAPACITY 16u
#define LUNA_SYSTEM_CAPACITY 16u
#define LUNA_OBSERVE_LOG_CAPACITY 32u
#define LUNA_NETWORK_PACKET_BYTES 256u
#define LUNA_PACKAGE_CAPACITY 8u

enum luna_space_id {
    LUNA_SPACE_BOOT = 0,
    LUNA_SPACE_SYSTEM = 1,
    LUNA_SPACE_DATA = 2,
    LUNA_SPACE_SECURITY = 3,
    LUNA_SPACE_GRAPHICS = 4,
    LUNA_SPACE_DEVICE = 5,
    LUNA_SPACE_NETWORK = 6,
    LUNA_SPACE_USER = 7,
    LUNA_SPACE_TIME = 8,
    LUNA_SPACE_LIFECYCLE = 9,
    LUNA_SPACE_OBSERVABILITY = 10,
    LUNA_SPACE_AI = 11,
    LUNA_SPACE_PROGRAM = 12,
    LUNA_SPACE_PACKAGE = 13,
    LUNA_SPACE_UPDATE = 14,
    LUNA_SPACE_TEST = 16,
};

enum luna_gate_opcode {
    LUNA_GATE_REQUEST_CAP = 1,
    LUNA_GATE_LIST_CAPS = 2,
};

enum luna_gate_status {
    LUNA_GATE_OK = 0,
    LUNA_GATE_DENIED = 0xE002u,
    LUNA_GATE_UNKNOWN = 0xE001u,
};

enum luna_cap_domain {
    LUNA_CAP_DATA_SEED = 0xD201u,
    LUNA_CAP_DATA_POUR = 0xD202u,
    LUNA_CAP_DATA_DRAW = 0xD203u,
    LUNA_CAP_DATA_SHRED = 0xD204u,
    LUNA_CAP_DATA_GATHER = 0xD205u,
    LUNA_CAP_LIFE_WAKE = 0xC901u,
    LUNA_CAP_LIFE_READ = 0xC902u,
    LUNA_CAP_LIFE_SPAWN = 0xC903u,
    LUNA_CAP_SYSTEM_REGISTER = 0xB101u,
    LUNA_CAP_SYSTEM_QUERY = 0xB102u,
    LUNA_CAP_SYSTEM_ALLOCATE = 0xB103u,
    LUNA_CAP_SYSTEM_EVENT = 0xB104u,
    LUNA_CAP_PROGRAM_LOAD = 0xA201u,
    LUNA_CAP_PROGRAM_START = 0xA202u,
    LUNA_CAP_PROGRAM_STOP = 0xA203u,
    LUNA_CAP_USER_SHELL = 0xA701u,
    LUNA_CAP_TIME_PULSE = 0xA801u,
    LUNA_CAP_TIME_CHIME = 0xA802u,
    LUNA_CAP_DEVICE_LIST = 0xA501u,
    LUNA_CAP_DEVICE_READ = 0xA502u,
    LUNA_CAP_DEVICE_WRITE = 0xA503u,
    LUNA_CAP_OBSERVE_LOG = 0xAA01u,
    LUNA_CAP_OBSERVE_READ = 0xAA02u,
    LUNA_CAP_OBSERVE_STATS = 0xAA03u,
    LUNA_CAP_NETWORK_SEND = 0xA601u,
    LUNA_CAP_NETWORK_RECV = 0xA602u,
    LUNA_CAP_GRAPHICS_DRAW = 0xA401u,
    LUNA_CAP_PACKAGE_INSTALL = 0xAD01u,
    LUNA_CAP_PACKAGE_LIST = 0xAD02u,
    LUNA_CAP_UPDATE_CHECK = 0xAE01u,
    LUNA_CAP_UPDATE_APPLY = 0xAE02u,
    LUNA_CAP_AI_INFER = 0xAB01u,
    LUNA_CAP_AI_CREATE = 0xAB02u,
};

enum luna_data_opcode {
    LUNA_DATA_SEED_OBJECT = 1,
    LUNA_DATA_POUR_SPAN = 2,
    LUNA_DATA_DRAW_SPAN = 3,
    LUNA_DATA_SHRED_OBJECT = 4,
    LUNA_DATA_GATHER_SET = 5,
};

enum luna_data_status {
    LUNA_DATA_OK = 0,
    LUNA_DATA_ERR_INVALID_CAP = 0xD100u,
    LUNA_DATA_ERR_NOT_FOUND = 0xD101u,
    LUNA_DATA_ERR_NO_SPACE = 0xD102u,
    LUNA_DATA_ERR_RANGE = 0xD103u,
    LUNA_DATA_ERR_IO = 0xD104u,
    LUNA_DATA_ERR_METADATA = 0xD105u,
};

enum luna_lifecycle_opcode {
    LUNA_LIFE_WAKE_UNIT = 1,
    LUNA_LIFE_REST_UNIT = 2,
    LUNA_LIFE_READ_UNITS = 3,
    LUNA_LIFE_SPAWN_SPACE = 4,
};

enum luna_lifecycle_status {
    LUNA_LIFE_OK = 0,
    LUNA_LIFE_ERR_INVALID_CAP = 0xC100u,
    LUNA_LIFE_ERR_NOT_FOUND = 0xC101u,
    LUNA_LIFE_ERR_NO_ROOM = 0xC102u,
};

enum luna_system_opcode {
    LUNA_SYSTEM_REGISTER_SPACE = 1,
    LUNA_SYSTEM_QUERY_SPACE = 2,
    LUNA_SYSTEM_ALLOCATE_RESOURCE = 3,
    LUNA_SYSTEM_REPORT_EVENT = 4,
};

enum luna_system_status {
    LUNA_SYSTEM_OK = 0,
    LUNA_SYSTEM_ERR_INVALID_CAP = 0xB100u,
    LUNA_SYSTEM_ERR_NOT_FOUND = 0xB101u,
    LUNA_SYSTEM_ERR_NO_ROOM = 0xB102u,
};

enum luna_program_opcode {
    LUNA_PROGRAM_LOAD_APP = 1,
    LUNA_PROGRAM_START_APP = 2,
    LUNA_PROGRAM_STOP_APP = 3,
};

enum luna_program_status {
    LUNA_PROGRAM_OK = 0,
    LUNA_PROGRAM_ERR_INVALID_CAP = 0xA200u,
    LUNA_PROGRAM_ERR_NOT_FOUND = 0xA201u,
    LUNA_PROGRAM_ERR_BAD_TICKET = 0xA202u,
    LUNA_PROGRAM_ERR_BAD_PACKAGE = 0xA203u,
};

enum luna_time_opcode {
    LUNA_TIME_READ_PULSE = 1,
    LUNA_TIME_SET_CHIME = 2,
    LUNA_TIME_FOLD_CHIME = 3,
};

enum luna_time_status {
    LUNA_TIME_OK = 0,
    LUNA_TIME_ERR_INVALID_CAP = 0xA810u,
};

enum luna_device_opcode {
    LUNA_DEVICE_LIST = 1,
    LUNA_DEVICE_READ = 2,
    LUNA_DEVICE_WRITE = 3,
};

enum luna_device_status {
    LUNA_DEVICE_OK = 0,
    LUNA_DEVICE_ERR_INVALID_CAP = 0xA510u,
    LUNA_DEVICE_ERR_NOT_FOUND = 0xA511u,
};

enum luna_observe_opcode {
    LUNA_OBSERVE_LOG = 1,
    LUNA_OBSERVE_GET_LOGS = 2,
    LUNA_OBSERVE_GET_STATS = 3,
};

enum luna_observe_status {
    LUNA_OBSERVE_OK = 0,
    LUNA_OBSERVE_ERR_INVALID_CAP = 0xAA10u,
    LUNA_OBSERVE_ERR_NO_ROOM = 0xAA11u,
};

enum luna_network_opcode {
    LUNA_NETWORK_SEND_PACKET = 1,
    LUNA_NETWORK_RECV_PACKET = 2,
};

enum luna_network_status {
    LUNA_NETWORK_OK = 0,
    LUNA_NETWORK_ERR_INVALID_CAP = 0xA610u,
    LUNA_NETWORK_ERR_EMPTY = 0xA611u,
    LUNA_NETWORK_ERR_RANGE = 0xA612u,
};

enum luna_graphics_opcode {
    LUNA_GRAPHICS_DRAW_CHAR = 1,
};

enum luna_graphics_status {
    LUNA_GRAPHICS_OK = 0,
    LUNA_GRAPHICS_ERR_INVALID_CAP = 0xA410u,
    LUNA_GRAPHICS_ERR_RANGE = 0xA411u,
};

enum luna_package_opcode {
    LUNA_PACKAGE_INSTALL = 1,
    LUNA_PACKAGE_LIST = 2,
};

enum luna_package_status {
    LUNA_PACKAGE_OK = 0,
    LUNA_PACKAGE_ERR_INVALID_CAP = 0xAD10u,
    LUNA_PACKAGE_ERR_NOT_FOUND = 0xAD11u,
    LUNA_PACKAGE_ERR_NO_ROOM = 0xAD12u,
};

enum luna_update_opcode {
    LUNA_UPDATE_CHECK = 1,
    LUNA_UPDATE_APPLY = 2,
};

enum luna_update_status {
    LUNA_UPDATE_OK = 0,
    LUNA_UPDATE_ERR_INVALID_CAP = 0xAE10u,
};

enum luna_ai_opcode {
    LUNA_AI_INFER = 1,
    LUNA_AI_CREATE_SPACE = 2,
};

enum luna_ai_status {
    LUNA_AI_OK = 0,
    LUNA_AI_ERR_INVALID_CAP = 0xAB10u,
    LUNA_AI_ERR_NOT_UNDERSTOOD = 0xAB11u,
};

enum luna_unit_state {
    LUNA_UNIT_BOOTING = 1,
    LUNA_UNIT_LIVE = 2,
    LUNA_UNIT_REST = 3,
    LUNA_UNIT_FOLDED = 4,
};

enum luna_system_state {
    LUNA_SYSTEM_STATE_VOID = 0,
    LUNA_SYSTEM_STATE_ACTIVE = 1,
    LUNA_SYSTEM_STATE_PAUSED = 2,
    LUNA_SYSTEM_STATE_DEGRADED = 3,
};

enum luna_resource_type {
    LUNA_RESOURCE_MEMORY = 1,
    LUNA_RESOURCE_TIME = 2,
};

enum luna_device_kind {
    LUNA_DEVICE_KIND_SERIAL = 1,
};

struct luna_cid {
    uint64_t low;
    uint64_t high;
};

struct luna_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint64_t caller_space;
    uint64_t domain_key;
    uint64_t cid_low;
    uint64_t cid_high;
    uint32_t status;
    uint32_t count;
};

struct luna_bootview {
    uint64_t security_gate_base;
    uint64_t data_gate_base;
    uint64_t lifecycle_gate_base;
    uint64_t system_gate_base;
    uint64_t time_gate_base;
    uint64_t device_gate_base;
    uint64_t observe_gate_base;
    uint64_t network_gate_base;
    uint64_t graphics_gate_base;
    uint64_t ai_gate_base;
    uint64_t package_gate_base;
    uint64_t update_gate_base;
    uint64_t program_gate_base;
    uint64_t data_buffer_base;
    uint64_t data_buffer_size;
    uint64_t list_buffer_base;
    uint64_t list_buffer_size;
    uint32_t serial_port;
    uint32_t reserved0;
    uint64_t reserve_base;
    uint64_t reserve_size;
};

struct luna_object_ref {
    uint64_t low;
    uint64_t high;
};

struct luna_data_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t object_type;
    uint32_t object_flags;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t object_low;
    uint64_t object_high;
    uint64_t offset;
    uint64_t size;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    uint64_t created_at;
    uint64_t content_size;
};

struct luna_unit_record {
    uint32_t space_id;
    uint32_t state;
    uint64_t epoch;
    uint64_t flags;
};

struct luna_lifecycle_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t space_id;
    uint32_t state;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t epoch;
    uint64_t flags;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    uint64_t entry_addr;
};

struct luna_system_record {
    uint32_t space_id;
    uint32_t state;
    uint32_t resource_memory;
    uint32_t resource_time;
    uint64_t last_event;
    char name[16];
};

struct luna_system_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t space_id;
    uint32_t resource_type;
    uint32_t amount;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t ticket;
    uint64_t event_word;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    char name[16];
};

struct luna_program_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t ticket;
    uint64_t app_base;
    uint64_t app_size;
    uint64_t entry_addr;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    char name[16];
};

struct luna_time_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t ticket;
    uint64_t buffer_addr;
    uint64_t buffer_size;
};

struct luna_device_record {
    uint32_t device_id;
    uint32_t device_kind;
    uint32_t flags;
    uint32_t reserved0;
    char name[16];
};

struct luna_device_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t device_id;
    uint32_t result_count;
    uint32_t flags;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t size;
    uint64_t buffer_addr;
    uint64_t buffer_size;
};

struct luna_observe_record {
    uint64_t stamp;
    uint32_t space_id;
    uint32_t level;
    char message[32];
};

struct luna_observe_stats {
    uint32_t count;
    uint32_t dropped;
    uint32_t newest_level;
    uint32_t reserved0;
    uint64_t newest_stamp;
};

struct luna_observe_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t space_id;
    uint32_t level;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    char message[32];
};

struct luna_network_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t size;
    uint64_t flags;
    uint64_t buffer_addr;
    uint64_t buffer_size;
};

struct luna_graphics_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint32_t x;
    uint32_t y;
    uint32_t glyph;
    uint32_t attr;
};

struct luna_package_record {
    char name[16];
    uint32_t installed;
    uint32_t reserved0;
    uint64_t version;
};

struct luna_package_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t ticket;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    char name[16];
};

struct luna_update_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t current_version;
    uint64_t target_version;
    uint64_t flags;
};

struct luna_ai_gate {
    uint32_t sequence;
    uint32_t opcode;
    uint32_t status;
    uint32_t result_count;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t ticket;
    uint64_t buffer_addr;
    uint64_t buffer_size;
    uint64_t arg0;
};

struct luna_manifest {
    uint32_t magic;
    uint32_t version;
    uint64_t security_base;
    uint64_t security_boot_entry;
    uint64_t security_gate_entry;
    uint64_t data_base;
    uint64_t data_boot_entry;
    uint64_t data_gate_entry;
    uint64_t lifecycle_base;
    uint64_t lifecycle_boot_entry;
    uint64_t lifecycle_gate_entry;
    uint64_t system_base;
    uint64_t system_boot_entry;
    uint64_t system_gate_entry;
    uint64_t time_base;
    uint64_t time_boot_entry;
    uint64_t time_gate_entry;
    uint64_t device_base;
    uint64_t device_boot_entry;
    uint64_t device_gate_entry;
    uint64_t observe_base;
    uint64_t observe_boot_entry;
    uint64_t observe_gate_entry;
    uint64_t network_base;
    uint64_t network_boot_entry;
    uint64_t network_gate_entry;
    uint64_t graphics_base;
    uint64_t graphics_boot_entry;
    uint64_t graphics_gate_entry;
    uint64_t ai_base;
    uint64_t ai_boot_entry;
    uint64_t ai_gate_entry;
    uint64_t package_base;
    uint64_t package_boot_entry;
    uint64_t package_gate_entry;
    uint64_t update_base;
    uint64_t update_boot_entry;
    uint64_t update_gate_entry;
    uint64_t program_base;
    uint64_t program_boot_entry;
    uint64_t program_gate_entry;
    uint64_t user_base;
    uint64_t user_boot_entry;
    uint64_t test_base;
    uint64_t test_boot_entry;
    uint64_t app_hello_base;
    uint64_t app_hello_size;
    uint64_t security_gate_base;
    uint64_t data_gate_base;
    uint64_t lifecycle_gate_base;
    uint64_t system_gate_base;
    uint64_t time_gate_base;
    uint64_t device_gate_base;
    uint64_t observe_gate_base;
    uint64_t network_gate_base;
    uint64_t graphics_gate_base;
    uint64_t ai_gate_base;
    uint64_t package_gate_base;
    uint64_t update_gate_base;
    uint64_t program_gate_base;
    uint64_t data_buffer_base;
    uint64_t data_buffer_size;
    uint64_t list_buffer_base;
    uint64_t list_buffer_size;
    uint64_t reserve_base;
    uint64_t reserve_size;
    uint64_t data_store_lba;
    uint64_t data_object_capacity;
    uint64_t data_slot_sectors;
};

#endif
