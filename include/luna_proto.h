#ifndef LUNA_PROTO_H
#define LUNA_PROTO_H

#include <stdint.h>
#include "luna_layout.h"

#define LUNA_MANIFEST_MAGIC 0x414E554Cull
#define LUNA_PROTO_VERSION 0x00000250u
#define LUNA_SESSION_SCRIPT_MAGIC 0x54504353u
#define LUNA_PROGRAM_BUNDLE_VERSION 2u
#define LUNA_PROGRAM_BUNDLE_VERSION_LEGACY 1u
#define LUNA_LA_ABI_MAJOR 1u
#define LUNA_LA_ABI_MINOR 0u
#define LUNA_SDK_MAJOR 1u
#define LUNA_SDK_MINOR 0u
#define LUNA_PROGRAM_BUNDLE_FLAG_DRIVER 0x00000001u

#define LUNA_DATA_OBJECT_CAPACITY 128u
#define LUNA_DATA_SLOT_BYTES 512u
#define LUNA_DATA_STORE_LBA 18432u
#define LUNA_DATA_META_SECTORS 25u
#define LUNA_DATA_BITMAP_SECTORS 1u
#define LUNA_DATA_SLOT_SECTORS 1u
#define LUNA_LIFECYCLE_CAPACITY 16u
#define LUNA_SYSTEM_CAPACITY 16u
#define LUNA_OBSERVE_LOG_CAPACITY 32u
#define LUNA_NETWORK_PACKET_BYTES 256u
#define LUNA_PACKAGE_CAPACITY 8u
#define LUNA_DESKTOP_ENTRY_CAPACITY 8u

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
};

#define LUNA_DIAG_TEST 16u

enum luna_gate_opcode {
    LUNA_GATE_REQUEST_CAP = 1,
    LUNA_GATE_LIST_CAPS = 2,
    LUNA_GATE_ISSUE_SEAL = 3,
    LUNA_GATE_CONSUME_SEAL = 4,
    LUNA_GATE_REVOKE_CAP = 5,
    LUNA_GATE_VALIDATE_CAP = 6,
    LUNA_GATE_LIST_SEALS = 7,
    LUNA_GATE_REVOKE_DOMAIN = 8,
    LUNA_GATE_POLICY_QUERY = 9,
    LUNA_GATE_POLICY_SYNC = 10,
    LUNA_GATE_AUTH_LOGIN = 11,
    LUNA_GATE_GOVERN = 12,
};

enum luna_gate_status {
    LUNA_GATE_OK = 0,
    LUNA_GATE_EXHAUSTED = 0xE003u,
    LUNA_GATE_DENIED = 0xE002u,
    LUNA_GATE_UNKNOWN = 0xE001u,
};

enum luna_policy_type {
    LUNA_POLICY_TYPE_SOURCE = 1,
    LUNA_POLICY_TYPE_PEER = 2,
    LUNA_POLICY_TYPE_UPDATE = 3,
    LUNA_POLICY_TYPE_SIGNER = 4,
};

enum luna_policy_state {
    LUNA_POLICY_STATE_ALLOW = 1,
    LUNA_POLICY_STATE_DENY = 2,
    LUNA_POLICY_STATE_REVOKE = 3,
};

enum luna_cap_kind {
    LUNA_CID_KIND_SESSION = 1,
    LUNA_CID_KIND_ROOT = 2,
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
    LUNA_CAP_USER_AUTH = 0xA702u,
    LUNA_CAP_TIME_PULSE = 0xA801u,
    LUNA_CAP_TIME_CHIME = 0xA802u,
    LUNA_CAP_DEVICE_LIST = 0xA501u,
    LUNA_CAP_DEVICE_READ = 0xA502u,
    LUNA_CAP_DEVICE_WRITE = 0xA503u,
    LUNA_CAP_DEVICE_PROBE = 0xA504u,
    LUNA_CAP_DEVICE_BIND = 0xA505u,
    LUNA_CAP_DEVICE_MMIO = 0xA506u,
    LUNA_CAP_DEVICE_DMA = 0xA507u,
    LUNA_CAP_DEVICE_IRQ = 0xA508u,
    LUNA_CAP_OBSERVE_LOG = 0xAA01u,
    LUNA_CAP_OBSERVE_READ = 0xAA02u,
    LUNA_CAP_OBSERVE_STATS = 0xAA03u,
    LUNA_CAP_NETWORK_SEND = 0xA601u,
    LUNA_CAP_NETWORK_RECV = 0xA602u,
    LUNA_CAP_NETWORK_PAIR = 0xA603u,
    LUNA_CAP_NETWORK_SESSION = 0xA604u,
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
    LUNA_DATA_STAT_STORE = 6,
    LUNA_DATA_VERIFY_STORE = 7,
    LUNA_DATA_SET_ADD_MEMBER = 8,
    LUNA_DATA_SET_REMOVE_MEMBER = 9,
};

enum luna_data_status {
    LUNA_DATA_OK = 0,
    LUNA_DATA_ERR_INVALID_CAP = 0xD100u,
    LUNA_DATA_ERR_NOT_FOUND = 0xD101u,
    LUNA_DATA_ERR_NO_SPACE = 0xD102u,
    LUNA_DATA_ERR_RANGE = 0xD103u,
    LUNA_DATA_ERR_IO = 0xD104u,
    LUNA_DATA_ERR_METADATA = 0xD105u,
    LUNA_DATA_ERR_DENIED = 0xD106u,
    LUNA_DATA_ERR_READONLY = 0xD107u,
    LUNA_DATA_ERR_RECOVERY = 0xD108u,
};

enum luna_volume_state {
    LUNA_VOLUME_HEALTHY = 1,
    LUNA_VOLUME_DEGRADED = 2,
    LUNA_VOLUME_RECOVERY_REQUIRED = 3,
    LUNA_VOLUME_FATAL_INCOMPATIBLE = 4,
    LUNA_VOLUME_FATAL_UNRECOVERABLE = 5,
};

enum luna_system_mode {
    LUNA_MODE_NORMAL = 1,
    LUNA_MODE_READONLY = 2,
    LUNA_MODE_RECOVERY = 3,
    LUNA_MODE_FATAL = 4,
    LUNA_MODE_INSTALL = 5,
};

enum luna_native_profile {
    LUNA_NATIVE_PROFILE_SYSTEM = 0x5359534Cu,
    LUNA_NATIVE_PROFILE_DATA = 0x5441444Cu,
};

enum luna_activation_state {
    LUNA_ACTIVATION_EMPTY = 0,
    LUNA_ACTIVATION_PROVISIONED = 1,
    LUNA_ACTIVATION_COMMITTED = 2,
    LUNA_ACTIVATION_ACTIVE = 3,
    LUNA_ACTIVATION_RECOVERY = 4,
};

enum luna_govern_action {
    LUNA_GOVERN_MOUNT = 1,
    LUNA_GOVERN_WRITE = 2,
    LUNA_GOVERN_COMMIT = 3,
    LUNA_GOVERN_REPLAY = 4,
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
    LUNA_SYSTEM_QUERY_SPACES = 5,
    LUNA_SYSTEM_QUERY_EVENTS = 6,
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
    LUNA_PROGRAM_BIND_VIEW = 4,
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
    LUNA_DEVICE_CENSUS = 4,
    LUNA_DEVICE_PCI_SCAN = 5,
    LUNA_DEVICE_QUERY = 6,
    LUNA_DEVICE_INPUT_READ = 7,
    LUNA_DEVICE_BLOCK_READ = 8,
    LUNA_DEVICE_BLOCK_WRITE = 9,
    LUNA_DEVICE_DISPLAY_PRESENT = 10,
};

enum luna_device_status {
    LUNA_DEVICE_OK = 0,
    LUNA_DEVICE_ERR_INVALID_CAP = 0xA510u,
    LUNA_DEVICE_ERR_NOT_FOUND = 0xA511u,
};

enum luna_lane_class {
    LUNA_LANE_CLASS_STREAM = 1,
    LUNA_LANE_CLASS_BLOCK = 2,
    LUNA_LANE_CLASS_SCANOUT = 3,
    LUNA_LANE_CLASS_CLOCK = 4,
    LUNA_LANE_CLASS_INPUT = 5,
    LUNA_LANE_CLASS_LINK = 6,
};

enum luna_lane_driver_family {
    LUNA_LANE_DRIVER_NONE = 0,
    LUNA_LANE_DRIVER_UART16550 = 1,
    LUNA_LANE_DRIVER_ATA_PIO = 2,
    LUNA_LANE_DRIVER_UEFI_BLOCK_IO = 3,
    LUNA_LANE_DRIVER_VGA_TEXT = 4,
    LUNA_LANE_DRIVER_RDTSC = 5,
    LUNA_LANE_DRIVER_PS2_KEYBOARD = 6,
    LUNA_LANE_DRIVER_SOFT_LOOP = 7,
    LUNA_LANE_DRIVER_PS2_MOUSE = 8,
    LUNA_LANE_DRIVER_PIT_TSC = 9,
    LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER = 10,
    LUNA_LANE_DRIVER_E1000 = 11,
    LUNA_LANE_DRIVER_PCI_IDE = 12,
    LUNA_LANE_DRIVER_E1000E = 13,
    LUNA_LANE_DRIVER_AHCI = 14,
    LUNA_LANE_DRIVER_QEMU_STD_VGA = 15,
    LUNA_LANE_DRIVER_I8042_KEYBOARD = 16,
    LUNA_LANE_DRIVER_I8042_MOUSE = 17,
    LUNA_LANE_DRIVER_PIIX_UART = 18,
    LUNA_LANE_DRIVER_ICH9_UART = 19,
    LUNA_LANE_DRIVER_VIRTIO_KEYBOARD = 20,
};

enum luna_driver_stage {
    LUNA_DRIVER_STAGE_PROBE = 1,
    LUNA_DRIVER_STAGE_APPROVE = 2,
    LUNA_DRIVER_STAGE_BIND = 3,
    LUNA_DRIVER_STAGE_FAIL = 4,
    LUNA_DRIVER_STAGE_ROLLBACK = 5,
};

enum luna_driver_class {
    LUNA_DRIVER_CLASS_STORAGE_BOOT = 1,
    LUNA_DRIVER_CLASS_DISPLAY_MIN = 2,
    LUNA_DRIVER_CLASS_INPUT_MIN = 3,
    LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY = 4,
    LUNA_DRIVER_CLASS_NETWORK_BASELINE = 5,
};

enum luna_driver_failure_class {
    LUNA_DRIVER_FAIL_NONE = 0,
    LUNA_DRIVER_FAIL_FATAL = 1,
    LUNA_DRIVER_FAIL_DEGRADED = 2,
    LUNA_DRIVER_FAIL_OBSERVER_ONLY = 3,
};

enum luna_driver_runtime_class {
    LUNA_DRIVER_RUNTIME_SYSTEM = 1,
    LUNA_DRIVER_RUNTIME_THIRD_PARTY = 2,
};

enum luna_driver_lifecycle_flag {
    LUNA_DRIVER_LIFE_STORAGE_FATAL = 1u << 0,
    LUNA_DRIVER_LIFE_DISPLAY_DEGRADED = 1u << 1,
    LUNA_DRIVER_LIFE_INPUT_DEGRADED = 1u << 2,
    LUNA_DRIVER_LIFE_PLATFORM_READY = 1u << 3,
    LUNA_DRIVER_LIFE_NETWORK_DEGRADED = 1u << 4,
};

enum luna_lane_state_flag {
    LUNA_LANE_FLAG_PRESENT = 1u << 0,
    LUNA_LANE_FLAG_READY = 1u << 1,
    LUNA_LANE_FLAG_BOOT = 1u << 2,
    LUNA_LANE_FLAG_FALLBACK = 1u << 3,
    LUNA_LANE_FLAG_POLLING = 1u << 4,
};

#define LUNA_DATA_OBJECT_TYPE_DRIVER_BIND 0x44525642u

struct luna_driver_bind_record {
    uint32_t magic;
    uint32_t version;
    uint32_t driver_class;
    uint32_t stage;
    uint32_t runtime_class;
    uint32_t failure_class;
    uint32_t device_id;
    uint32_t lane_class;
    uint32_t driver_family;
    uint32_t state_flags;
    uint64_t bound_at;
    uint64_t writer_space;
    uint64_t authority_space;
    char lane_name[16];
    char driver_name[16];
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
    LUNA_NETWORK_PAIR_PEER = 3,
    LUNA_NETWORK_OPEN_SESSION = 4,
    LUNA_NETWORK_OPEN_CHANNEL = 5,
    LUNA_NETWORK_SEND_CHANNEL = 6,
    LUNA_NETWORK_RECV_CHANNEL = 7,
    LUNA_NETWORK_GET_INFO = 8,
};

enum luna_network_status {
    LUNA_NETWORK_OK = 0,
    LUNA_NETWORK_ERR_INVALID_CAP = 0xA610u,
    LUNA_NETWORK_ERR_EMPTY = 0xA611u,
    LUNA_NETWORK_ERR_RANGE = 0xA612u,
    LUNA_NETWORK_ERR_NOT_FOUND = 0xA613u,
    LUNA_NETWORK_ERR_BAD_STATE = 0xA614u,
};

enum luna_link_channel_kind {
    LUNA_LINK_CHANNEL_KIND_MESSAGE = 1,
    LUNA_LINK_CHANNEL_KIND_BULK = 2,
};

enum luna_link_transfer_class {
    LUNA_LINK_TRANSFER_CLASS_MESSAGE = 1,
    LUNA_LINK_TRANSFER_CLASS_BULK = 2,
};

enum luna_graphics_opcode {
    LUNA_GRAPHICS_DRAW_CHAR = 1,
    LUNA_GRAPHICS_CREATE_WINDOW = 2,
    LUNA_GRAPHICS_SET_ACTIVE_WINDOW = 3,
    LUNA_GRAPHICS_CLOSE_WINDOW = 4,
    LUNA_GRAPHICS_MOVE_WINDOW = 5,
    LUNA_GRAPHICS_RENDER_DESKTOP = 6,
    LUNA_GRAPHICS_QUERY_WINDOW = 7,
};

enum luna_graphics_status {
    LUNA_GRAPHICS_OK = 0,
    LUNA_GRAPHICS_ERR_INVALID_CAP = 0xA410u,
    LUNA_GRAPHICS_ERR_RANGE = 0xA411u,
    LUNA_GRAPHICS_ERR_NO_ROOM = 0xA412u,
    LUNA_GRAPHICS_ERR_NOT_FOUND = 0xA413u,
};

enum luna_package_opcode {
    LUNA_PACKAGE_INSTALL = 1,
    LUNA_PACKAGE_LIST = 2,
    LUNA_PACKAGE_REMOVE = 3,
    LUNA_PACKAGE_RESOLVE = 4,
};

enum luna_package_status {
    LUNA_PACKAGE_OK = 0,
    LUNA_PACKAGE_ERR_INVALID_CAP = 0xAD10u,
    LUNA_PACKAGE_ERR_NOT_FOUND = 0xAD11u,
    LUNA_PACKAGE_ERR_NO_ROOM = 0xAD12u,
};

enum luna_package_flag {
    LUNA_PACKAGE_FLAG_PINNED = 1u << 0,
    LUNA_PACKAGE_FLAG_STARTUP = 1u << 1,
};

enum luna_window_role {
    LUNA_WINDOW_ROLE_DOCUMENT = 1,
    LUNA_WINDOW_ROLE_UTILITY = 2,
    LUNA_WINDOW_ROLE_CONSOLE = 3,
};

enum luna_update_opcode {
    LUNA_UPDATE_CHECK = 1,
    LUNA_UPDATE_APPLY = 2,
};

enum luna_update_status {
    LUNA_UPDATE_OK = 0,
    LUNA_UPDATE_ERR_INVALID_CAP = 0xAE10u,
};

enum luna_update_flag {
    LUNA_UPDATE_FLAG_NONE = 0u,
    LUNA_UPDATE_FLAG_ROLLBACK = 1u << 0,
};

enum luna_update_txn_state {
    LUNA_UPDATE_TXN_STATE_EMPTY = 0,
    LUNA_UPDATE_TXN_STATE_STAGED = 1,
    LUNA_UPDATE_TXN_STATE_COMMITTED = 2,
    LUNA_UPDATE_TXN_STATE_ACTIVATED = 3,
    LUNA_UPDATE_TXN_STATE_ROLLED_BACK = 4,
    LUNA_UPDATE_TXN_STATE_FAILED = 5,
};

#define LUNA_DATA_OBJECT_TYPE_HOST_RECORD 0x4C485354u
#define LUNA_DATA_OBJECT_TYPE_USER_RECORD 0x4C555352u
#define LUNA_DATA_OBJECT_TYPE_USER_SECRET 0x4C555345u
#define LUNA_DATA_OBJECT_TYPE_USER_HOME_ROOT 0x4C55484Du
#define LUNA_DATA_OBJECT_TYPE_USER_PROFILE 0x4C555046u

#define LUNA_HOST_RECORD_MAGIC 0x484F5354u
#define LUNA_USER_RECORD_MAGIC 0x55534552u
#define LUNA_USER_SECRET_MAGIC 0x55534543u
#define LUNA_USER_HOME_MAGIC 0x484F4D45u
#define LUNA_USER_PROFILE_MAGIC 0x50524F46u
#define LUNA_USER_RECORD_VERSION 1u

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
    LUNA_SYSTEM_STATE_BOOTING = 4,
};

enum luna_resource_type {
    LUNA_RESOURCE_MEMORY = 1,
    LUNA_RESOURCE_TIME = 2,
};

enum luna_device_kind {
    LUNA_DEVICE_KIND_SERIAL = 1,
    LUNA_DEVICE_KIND_DISK = 2,
    LUNA_DEVICE_KIND_DISPLAY = 3,
    LUNA_DEVICE_KIND_CLOCK = 4,
    LUNA_DEVICE_KIND_INPUT = 5,
    LUNA_DEVICE_KIND_NETWORK = 6,
};

enum luna_device_id {
    LUNA_DEVICE_ID_SERIAL0 = 1,
    LUNA_DEVICE_ID_DISK0 = 2,
    LUNA_DEVICE_ID_DISPLAY0 = 3,
    LUNA_DEVICE_ID_CLOCK0 = 4,
    LUNA_DEVICE_ID_INPUT0 = 5,
    LUNA_DEVICE_ID_NET0 = 6,
    LUNA_DEVICE_ID_POINTER0 = 7,
};

struct luna_display_cell {
    uint32_t x;
    uint32_t y;
    uint32_t glyph;
    uint32_t attr;
};

struct luna_display_info {
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pixels_per_scanline;
    uint32_t framebuffer_pixel_format;
    uint32_t text_columns;
    uint32_t text_rows;
    uint32_t state_flags;
    uint32_t driver_family;
};

struct luna_pointer_event {
    int32_t dx;
    int32_t dy;
    uint32_t buttons;
    uint32_t flags;
};

struct luna_net_info {
    uint32_t version;
    uint32_t driver_family;
    uint32_t state_flags;
    uint32_t ctrl;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t tx_head;
    uint16_t tx_tail;
    uint16_t hw_rx_head;
    uint16_t hw_rx_tail;
    uint16_t hw_tx_head;
    uint16_t hw_tx_tail;
    uint64_t mmio_base;
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint32_t rctl;
    uint32_t tctl;
    uint32_t status;
    uint32_t reserved0;
    uint8_t mac[6];
    uint8_t reserved1[2];
};

struct luna_link_peer {
    uint32_t peer_id;
    uint32_t flags;
    uint64_t pairing_low;
    uint64_t pairing_high;
    uint64_t challenge;
    uint64_t proof;
    char name[16];
};

struct luna_link_session {
    uint32_t session_id;
    uint32_t peer_id;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t resume_low;
    uint64_t resume_high;
};

struct luna_link_channel {
    uint32_t channel_id;
    uint32_t session_id;
    uint32_t kind;
    uint32_t transfer_class;
};

struct luna_link_pair_request {
    char name[16];
    uint32_t flags;
    uint32_t reserved0;
    uint64_t challenge;
    uint64_t response;
};

struct luna_link_session_request {
    uint32_t peer_id;
    uint32_t reserved0;
};

struct luna_link_channel_request {
    uint32_t session_id;
    uint32_t kind;
    uint32_t transfer_class;
    uint32_t reserved0;
};

struct luna_link_send_request {
    uint32_t channel_id;
    uint32_t reserved0;
    uint64_t payload_addr;
    uint64_t payload_size;
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
    uint32_t target_space;
    uint32_t target_gate;
    uint32_t ttl;
    uint32_t uses;
    uint64_t seal_low;
    uint64_t seal_high;
    uint64_t nonce;
    uint64_t buffer_addr;
    uint64_t buffer_size;
};

struct luna_governance_query {
    uint32_t action;
    uint32_t result_state;
    uint32_t writer_space;
    uint32_t authority_space;
    uint32_t mode;
    uint32_t object_type;
    uint32_t object_flags;
    uint32_t reserved0;
    uint64_t domain_key;
    uint64_t cid_low;
    uint64_t cid_high;
    uint64_t object_low;
    uint64_t object_high;
    uint64_t install_low;
    uint64_t install_high;
};

struct luna_policy_query {
    uint32_t policy_type;
    uint32_t state;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t target_id;
    uint64_t binding_id;
};

struct luna_user_auth_request {
    uint64_t secret_low;
    uint64_t secret_high;
    char username[16];
    char password[32];
    uint32_t flags;
    uint32_t reserved0;
};

struct luna_cap_record {
    uint32_t holder_space;
    uint32_t target_space;
    uint32_t target_gate;
    uint32_t uses_left;
    uint32_t ttl;
    uint32_t reserved0;
    uint64_t domain_key;
    uint64_t cid_low;
    uint64_t cid_high;
};

struct luna_seal_record {
    uint32_t holder_space;
    uint32_t target_space;
    uint32_t target_gate;
    uint32_t reserved0;
    uint64_t seal_low;
    uint64_t seal_high;
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
    uint64_t app_write_entry;
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pixels_per_scanline;
    uint32_t framebuffer_pixel_format;
    uint64_t install_uuid_low;
    uint64_t install_uuid_high;
    uint64_t system_store_lba;
    uint64_t data_store_lba;
    uint32_t volume_state;
    uint32_t system_mode;
};

struct luna_object_ref {
    uint64_t low;
    uint64_t high;
};

struct luna_store_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t object_capacity;
    uint32_t slot_sectors;
    uint64_t store_base_lba;
    uint64_t data_start_lba;
    uint64_t next_nonce;
    uint64_t format_count;
    uint64_t mount_count;
    uint64_t reserved[55];
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
    uint32_t writer_space;
    uint32_t authority_space;
    uint32_t volume_state;
    uint32_t volume_mode;
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

struct luna_system_event_record {
    uint64_t sequence;
    uint32_t space_id;
    uint32_t state;
    uint64_t event_word;
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
    uint32_t window_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
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

struct luna_lane_record {
    uint32_t device_id;
    uint32_t device_kind;
    uint32_t lane_class;
    uint32_t driver_family;
    uint32_t state_flags;
    uint32_t reserved0;
    char lane_name[16];
    char driver_name[16];
};

struct luna_pci_record {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t reserved0[3];
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
    uint32_t window_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t glyph;
    uint32_t attr;
    uint64_t buffer_addr;
    uint64_t buffer_size;
};

struct luna_desktop_entry {
    char name[16];
    char label[16];
    uint32_t icon_x;
    uint32_t icon_y;
    uint32_t flags;
    uint32_t preferred_x;
    uint32_t preferred_y;
    uint32_t preferred_width;
    uint32_t preferred_height;
    uint32_t window_role;
    uint32_t reserved0;
};

struct luna_desktop_shell_state {
    uint32_t version;
    uint32_t entry_count;
    uint32_t desktop_attr;
    uint32_t chrome_attr;
    uint32_t panel_attr;
    uint32_t accent_attr;
    uint32_t last_key;
    uint32_t last_pointer_buttons;
    uint32_t key_events;
    uint32_t pointer_events;
    struct luna_desktop_entry entries[LUNA_DESKTOP_ENTRY_CAPACITY];
};

struct luna_package_record {
    char name[16];
    char label[16];
    uint32_t installed;
    uint32_t icon_x;
    uint32_t icon_y;
    uint32_t flags;
    uint32_t preferred_x;
    uint32_t preferred_y;
    uint32_t preferred_width;
    uint32_t preferred_height;
    uint32_t window_role;
    uint32_t reserved0;
    uint64_t version;
};

struct luna_package_resolve_record {
    char name[16];
    uint32_t flags;
    uint32_t reserved0;
    uint64_t version;
    struct luna_object_ref app_object;
    struct luna_object_ref install_object;
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
    uint64_t diag_base;
    uint64_t diag_boot_entry;
    uint64_t app_hello_base;
    uint64_t app_hello_size;
    uint64_t bootview_base;
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
    uint64_t app_files_base;
    uint64_t app_files_size;
    uint64_t app_notes_base;
    uint64_t app_notes_size;
    uint64_t app_console_base;
    uint64_t app_console_size;
    uint64_t app_guard_base;
    uint64_t app_guard_size;
    uint64_t program_app_write_entry;
    uint64_t session_script_base;
    uint64_t session_script_size;
    uint64_t system_store_lba;
    uint64_t install_uuid_low;
    uint64_t install_uuid_high;
};

#endif
