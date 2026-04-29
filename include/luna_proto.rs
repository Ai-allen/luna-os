pub const LUNA_FORMAL_SPACE_COUNT: usize = 15;

#[repr(C)]
pub struct LunaBootView {
    pub security_gate_base: u64,
    pub data_gate_base: u64,
    pub lifecycle_gate_base: u64,
    pub system_gate_base: u64,
    pub time_gate_base: u64,
    pub device_gate_base: u64,
    pub observe_gate_base: u64,
    pub network_gate_base: u64,
    pub graphics_gate_base: u64,
    pub ai_gate_base: u64,
    pub package_gate_base: u64,
    pub update_gate_base: u64,
    pub program_gate_base: u64,
    pub data_buffer_base: u64,
    pub data_buffer_size: u64,
    pub list_buffer_base: u64,
    pub list_buffer_size: u64,
    pub serial_port: u32,
    pub reserved0: u32,
    pub reserve_base: u64,
    pub reserve_size: u64,
    pub app_write_entry: u64,
    pub framebuffer_base: u64,
    pub framebuffer_size: u64,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_pixels_per_scanline: u32,
    pub framebuffer_pixel_format: u32,
    pub install_uuid_low: u64,
    pub install_uuid_high: u64,
    pub system_store_lba: u64,
    pub data_store_lba: u64,
    pub volume_state: u32,
    pub system_mode: u32,
    pub installer_target_flags: u64,
    pub installer_target_system_lba: u64,
    pub installer_target_data_lba: u64,
}

#[repr(C)]
pub struct LunaManifest {
    pub magic: u32,
    pub version: u32,
    pub security_base: u64,
    pub security_boot_entry: u64,
    pub security_gate_entry: u64,
    pub data_base: u64,
    pub data_boot_entry: u64,
    pub data_gate_entry: u64,
    pub lifecycle_base: u64,
    pub lifecycle_boot_entry: u64,
    pub lifecycle_gate_entry: u64,
    pub system_base: u64,
    pub system_boot_entry: u64,
    pub system_gate_entry: u64,
    pub time_base: u64,
    pub time_boot_entry: u64,
    pub time_gate_entry: u64,
    pub device_base: u64,
    pub device_boot_entry: u64,
    pub device_gate_entry: u64,
    pub observe_base: u64,
    pub observe_boot_entry: u64,
    pub observe_gate_entry: u64,
    pub network_base: u64,
    pub network_boot_entry: u64,
    pub network_gate_entry: u64,
    pub graphics_base: u64,
    pub graphics_boot_entry: u64,
    pub graphics_gate_entry: u64,
    pub ai_base: u64,
    pub ai_boot_entry: u64,
    pub ai_gate_entry: u64,
    pub package_base: u64,
    pub package_boot_entry: u64,
    pub package_gate_entry: u64,
    pub update_base: u64,
    pub update_boot_entry: u64,
    pub update_gate_entry: u64,
    pub program_base: u64,
    pub program_boot_entry: u64,
    pub program_gate_entry: u64,
    pub user_base: u64,
    pub user_boot_entry: u64,
    pub app_hello_base: u64,
    pub app_hello_size: u64,
    pub bootview_base: u64,
    pub security_gate_base: u64,
    pub data_gate_base: u64,
    pub lifecycle_gate_base: u64,
    pub system_gate_base: u64,
    pub time_gate_base: u64,
    pub device_gate_base: u64,
    pub observe_gate_base: u64,
    pub network_gate_base: u64,
    pub graphics_gate_base: u64,
    pub ai_gate_base: u64,
    pub package_gate_base: u64,
    pub update_gate_base: u64,
    pub program_gate_base: u64,
    pub data_buffer_base: u64,
    pub data_buffer_size: u64,
    pub list_buffer_base: u64,
    pub list_buffer_size: u64,
    pub reserve_base: u64,
    pub reserve_size: u64,
    pub data_store_lba: u64,
    pub data_object_capacity: u64,
    pub data_slot_sectors: u64,
    pub app_files_base: u64,
    pub app_files_size: u64,
    pub app_notes_base: u64,
    pub app_notes_size: u64,
    pub app_console_base: u64,
    pub app_console_size: u64,
    pub app_guard_base: u64,
    pub app_guard_size: u64,
    pub program_app_write_entry: u64,
    pub session_script_base: u64,
    pub session_script_size: u64,
    pub system_store_lba: u64,
    pub install_uuid_low: u64,
    pub install_uuid_high: u64,
}

#[repr(C)]
pub struct LunaGovernanceQuery {
    pub action: u32,
    pub result_state: u32,
    pub writer_space: u32,
    pub authority_space: u32,
    pub mode: u32,
    pub object_type: u32,
    pub object_flags: u32,
    pub reserved0: u32,
    pub domain_key: u64,
    pub cid_low: u64,
    pub cid_high: u64,
    pub object_low: u64,
    pub object_high: u64,
    pub install_low: u64,
    pub install_high: u64,
}

#[repr(C)]
pub struct LunaStoreSuperblock {
    pub magic: u32,
    pub version: u32,
    pub object_capacity: u32,
    pub slot_sectors: u32,
    pub store_base_lba: u64,
    pub data_start_lba: u64,
    pub next_nonce: u64,
    pub format_count: u64,
    pub mount_count: u64,
    pub reserved: [u64; 55],
}

pub const LUNA_GATE_GOVERN: u32 = 12;
pub const LUNA_GATE_QUERY_GOVERN: u32 = 17;
pub const LUNA_DATA_ERR_DENIED: u32 = 0xD106;
pub const LUNA_DATA_ERR_READONLY: u32 = 0xD107;
pub const LUNA_DATA_ERR_RECOVERY: u32 = 0xD108;
pub const LUNA_VOLUME_HEALTHY: u32 = 1;
pub const LUNA_VOLUME_DEGRADED: u32 = 2;
pub const LUNA_VOLUME_RECOVERY_REQUIRED: u32 = 3;
pub const LUNA_VOLUME_FATAL_INCOMPATIBLE: u32 = 4;
pub const LUNA_VOLUME_FATAL_UNRECOVERABLE: u32 = 5;
pub const LUNA_NATIVE_PROFILE_SYSTEM: u32 = 0x5359_534C;
pub const LUNA_NATIVE_PROFILE_DATA: u32 = 0x5441_444C;
pub const LUNA_ACTIVATION_EMPTY: u32 = 0;
pub const LUNA_ACTIVATION_PROVISIONED: u32 = 1;
pub const LUNA_ACTIVATION_COMMITTED: u32 = 2;
pub const LUNA_ACTIVATION_ACTIVE: u32 = 3;
pub const LUNA_ACTIVATION_RECOVERY: u32 = 4;
pub const LUNA_MODE_NORMAL: u32 = 1;
pub const LUNA_MODE_READONLY: u32 = 2;
pub const LUNA_MODE_RECOVERY: u32 = 3;
pub const LUNA_MODE_FATAL: u32 = 4;
pub const LUNA_MODE_INSTALL: u32 = 5;
pub const LUNA_GOVERN_MOUNT: u32 = 1;
pub const LUNA_GOVERN_WRITE: u32 = 2;
pub const LUNA_GOVERN_COMMIT: u32 = 3;
pub const LUNA_GOVERN_REPLAY: u32 = 4;
pub const LUNA_CAP_DATA_QUERY: u64 = 0xD206;
pub const LUNA_DATA_QUERY: u32 = 10;
pub const LUNA_OBSERVE_QUERY: u32 = 4;
pub const LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL: u32 = 0x504B_4749;
pub const LUNA_DATA_OBJECT_TYPE_PACKAGE_INDEX: u32 = 0x504B_474E;
pub const LUNA_QUERY_TARGET_PACKAGE_CATALOG: u32 = 1;
pub const LUNA_QUERY_TARGET_USER_FILES: u32 = 2;
pub const LUNA_QUERY_TARGET_OBSERVE_LOGS: u32 = 3;
pub const LUNA_QUERY_FILTER_NAMESPACE: u32 = 1u32 << 0;
pub const LUNA_QUERY_FILTER_OWNER: u32 = 1u32 << 1;
pub const LUNA_QUERY_FILTER_TYPE: u32 = 1u32 << 2;
pub const LUNA_QUERY_FILTER_STATE: u32 = 1u32 << 3;
pub const LUNA_QUERY_PROJECT_NAME: u32 = 1u32 << 0;
pub const LUNA_QUERY_PROJECT_LABEL: u32 = 1u32 << 1;
pub const LUNA_QUERY_PROJECT_REF: u32 = 1u32 << 2;
pub const LUNA_QUERY_PROJECT_TYPE: u32 = 1u32 << 3;
pub const LUNA_QUERY_PROJECT_STATE: u32 = 1u32 << 4;
pub const LUNA_QUERY_PROJECT_OWNER: u32 = 1u32 << 5;
pub const LUNA_QUERY_PROJECT_MESSAGE: u32 = 1u32 << 6;
pub const LUNA_QUERY_PROJECT_VERSION: u32 = 1u32 << 7;
pub const LUNA_QUERY_PROJECT_CREATED: u32 = 1u32 << 8;
pub const LUNA_QUERY_PROJECT_UPDATED: u32 = 1u32 << 9;
pub const LUNA_QUERY_SORT_NONE: u32 = 0;
pub const LUNA_QUERY_SORT_NAME_ASC: u32 = 1;
pub const LUNA_QUERY_SORT_CREATED_DESC: u32 = 2;
pub const LUNA_QUERY_SORT_UPDATED_DESC: u32 = 3;
pub const LUNA_QUERY_SORT_STAMP_DESC: u32 = 4;
pub const LUNA_QUERY_NAMESPACE_PACKAGE: u32 = 1;
pub const LUNA_QUERY_NAMESPACE_USER: u32 = 2;
pub const LUNA_QUERY_NAMESPACE_OBSERVE: u32 = 3;
pub const LUNA_QUERY_STATE_ANY: u32 = 0;
pub const LUNA_QUERY_STATE_ACTIVE: u32 = 1;
pub const LUNA_QUERY_STATE_ARCHIVED: u32 = 2;
pub const LUNA_QUERY_FLAG_AUDIT_REQUIRED: u32 = 1u32 << 0;
pub const LUNA_QUERY_FLAG_REDACTED: u32 = 1u32 << 1;
pub const LUNA_LSON_MAGIC: u32 = 0x4C53_4F4E;
pub const LUNA_LSON_VERSION: u32 = 0x0001_0000;
pub const LUNA_LSON_BODY_BYTES: usize = 48;
pub const LUNA_LSON_ATTR_TEXT_BYTES: usize = 16;
pub const LUNA_LOG_CLASS_BOOT: u32 = 1;
pub const LUNA_LOG_CLASS_LIFECYCLE: u32 = 2;
pub const LUNA_LOG_CLASS_SYSTEM: u32 = 3;
pub const LUNA_LOG_CLASS_DEVICE: u32 = 4;
pub const LUNA_LOG_CLASS_DATA: u32 = 5;
pub const LUNA_LOG_CLASS_SECURITY: u32 = 6;
pub const LUNA_LOG_CLASS_CRYPTO: u32 = 7;
pub const LUNA_LOG_CLASS_PACKAGE: u32 = 8;
pub const LUNA_LOG_CLASS_INSTALL: u32 = 9;
pub const LUNA_LOG_CLASS_UPDATE: u32 = 10;
pub const LUNA_LOG_CLASS_RECOVERY: u32 = 11;
pub const LUNA_LOG_CLASS_USER: u32 = 12;
pub const LUNA_LOG_CLASS_QUERY: u32 = 13;
pub const LUNA_LOG_BAND_TRACE: u32 = 1;
pub const LUNA_LOG_BAND_DEBUG: u32 = 2;
pub const LUNA_LOG_BAND_INFO: u32 = 3;
pub const LUNA_LOG_BAND_NOTICE: u32 = 4;
pub const LUNA_LOG_BAND_WARN: u32 = 5;
pub const LUNA_LOG_BAND_ERROR: u32 = 6;
pub const LUNA_LOG_BAND_FATAL: u32 = 7;
pub const LUNA_LOG_BAND_AUDIT: u32 = 8;
pub const LUNA_LSON_ENCODING_TEXT: u32 = 1;
pub const LUNA_LSON_ENCODING_FRAME: u32 = 2;
pub const LUNA_LSON_FLAG_PERSIST: u32 = 1u32 << 0;
pub const LUNA_LSON_FLAG_AUDIT: u32 = 1u32 << 1;
pub const LUNA_LSON_FLAG_REDACTED: u32 = 1u32 << 2;
pub const LUNA_LSON_FLAG_DIGEST_ONLY: u32 = 1u32 << 3;
pub const LUNA_LSON_FLAG_COMPRESSED: u32 = 1u32 << 4;
pub const LUNA_LSON_ATTR_TAG: u32 = 1;
pub const LUNA_LSON_ATTR_U64: u32 = 2;
pub const LUNA_LSON_ATTR_I64: u32 = 3;
pub const LUNA_LSON_ATTR_HEX: u32 = 4;
pub const LUNA_LSON_ATTR_BOOL: u32 = 5;
pub const LUNA_LSON_ATTR_TEXT: u32 = 6;
pub const LUNA_LSON_ATTR_ID128: u32 = 7;
pub const LUNA_LSON_KEY_NAMESPACE: u32 = 1;
pub const LUNA_LSON_KEY_OWNER: u32 = 2;
pub const LUNA_LSON_KEY_OBJECT: u32 = 3;
pub const LUNA_LSON_KEY_TARGET: u32 = 4;
pub const LUNA_LSON_KEY_TYPE: u32 = 5;
pub const LUNA_LSON_KEY_STATE: u32 = 6;
pub const LUNA_LSON_KEY_LBA: u32 = 7;
pub const LUNA_LSON_KEY_DEVICE_ID: u32 = 8;
pub const LUNA_LSON_KEY_DRIVER_FAMILY: u32 = 9;
pub const LUNA_LSON_KEY_QUERY_TARGET: u32 = 10;
pub const LUNA_LSON_KEY_POLICY_RESULT: u32 = 11;
pub const LUNA_LSON_KEY_STATUS: u32 = 12;
pub const LUNA_LSON_KEY_SOURCE: u32 = 13;
pub const LUNA_LSON_KEY_SIGNER: u32 = 14;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaQueryRequest {
    pub target: u32,
    pub filter_flags: u32,
    pub projection_flags: u32,
    pub sort_mode: u32,
    pub limit: u32,
    pub namespace_id: u32,
    pub object_type: u32,
    pub state: u32,
    pub owner_low: u64,
    pub owner_high: u64,
    pub scope_low: u64,
    pub scope_high: u64,
    pub result_flags: u32,
    pub reserved0: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaQueryRow {
    pub object_low: u64,
    pub object_high: u64,
    pub owner_low: u64,
    pub owner_high: u64,
    pub created_at: u64,
    pub updated_at: u64,
    pub version: u64,
    pub namespace_id: u32,
    pub object_type: u32,
    pub state: u32,
    pub flags: u32,
    pub name: [u8; 16],
    pub label: [u8; 16],
    pub message: [u8; 32],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaLsonAttr {
    pub key: u32,
    pub value_type: u32,
    pub value_low: u64,
    pub value_high: u64,
    pub text: [u8; LUNA_LSON_ATTR_TEXT_BYTES],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaLsonRecord {
    pub magic: u32,
    pub version: u32,
    pub tick: u64,
    pub actor_low: u64,
    pub actor_high: u64,
    pub trace_low: u64,
    pub trace_high: u64,
    pub session_low: u64,
    pub session_high: u64,
    pub install_low: u64,
    pub install_high: u64,
    pub record_class: u32,
    pub band: u32,
    pub space_id: u32,
    pub writer_space: u32,
    pub authority_space: u32,
    pub encoding: u32,
    pub flags: u32,
    pub device_id: u32,
    pub driver_family: u32,
    pub attr_count: u32,
    pub reserved0: u32,
    pub kind: [u8; 24],
    pub scope: [u8; 16],
    pub body: [u8; LUNA_LSON_BODY_BYTES],
}

pub const LUNA_GRAPHICS_MOVE_WINDOW: u32 = 5;
pub const LUNA_GRAPHICS_RENDER_DESKTOP: u32 = 6;
pub const LUNA_GRAPHICS_QUERY_WINDOW: u32 = 7;
pub const LUNA_CAP_NETWORK_PAIR: u64 = 0xA603;
pub const LUNA_CAP_NETWORK_SESSION: u64 = 0xA604;
pub const LUNA_NETWORK_PAIR_PEER: u32 = 3;
pub const LUNA_NETWORK_OPEN_SESSION: u32 = 4;
pub const LUNA_NETWORK_OPEN_CHANNEL: u32 = 5;
pub const LUNA_NETWORK_SEND_CHANNEL: u32 = 6;
pub const LUNA_NETWORK_RECV_CHANNEL: u32 = 7;
pub const LUNA_NETWORK_GET_INFO: u32 = 8;
pub const LUNA_DEVICE_CENSUS: u32 = 4;
pub const LUNA_DEVICE_PCI_SCAN: u32 = 5;
pub const LUNA_DEVICE_QUERY: u32 = 6;
pub const LUNA_DEVICE_INPUT_READ: u32 = 7;
pub const LUNA_DEVICE_BLOCK_READ: u32 = 8;
pub const LUNA_DEVICE_BLOCK_WRITE: u32 = 9;
pub const LUNA_DEVICE_DISPLAY_PRESENT: u32 = 10;
pub const LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET: u32 = 11;
pub const LUNA_INSTALL_TARGET_PRESENT: u64 = 0x1;
pub const LUNA_INSTALL_TARGET_BOUND: u64 = 0x2;
pub const LUNA_PROGRAM_BUNDLE_FLAG_DRIVER: u32 = 0x0000_0001;
pub const LUNA_DEVICE_ID_SERIAL0: u32 = 1;
pub const LUNA_DEVICE_ID_DISK0: u32 = 2;
pub const LUNA_DEVICE_ID_DISPLAY0: u32 = 3;
pub const LUNA_DEVICE_ID_CLOCK0: u32 = 4;
pub const LUNA_DEVICE_ID_INPUT0: u32 = 5;
pub const LUNA_DEVICE_ID_NET0: u32 = 6;
pub const LUNA_DEVICE_ID_POINTER0: u32 = 7;
pub const LUNA_DEVICE_ID_DISK1: u32 = 8;

pub const LUNA_LANE_CLASS_STREAM: u32 = 1;
pub const LUNA_LANE_CLASS_BLOCK: u32 = 2;
pub const LUNA_LANE_CLASS_SCANOUT: u32 = 3;
pub const LUNA_LANE_CLASS_CLOCK: u32 = 4;
pub const LUNA_LANE_CLASS_INPUT: u32 = 5;
pub const LUNA_LANE_CLASS_LINK: u32 = 6;

pub const LUNA_LANE_DRIVER_NONE: u32 = 0;
pub const LUNA_LANE_DRIVER_UART16550: u32 = 1;
pub const LUNA_LANE_DRIVER_ATA_PIO: u32 = 2;
pub const LUNA_LANE_DRIVER_UEFI_BLOCK_IO: u32 = 3;
pub const LUNA_LANE_DRIVER_VGA_TEXT: u32 = 4;
pub const LUNA_LANE_DRIVER_RDTSC: u32 = 5;
pub const LUNA_LANE_DRIVER_PS2_KEYBOARD: u32 = 6;
pub const LUNA_LANE_DRIVER_SOFT_LOOP: u32 = 7;
pub const LUNA_LANE_DRIVER_PS2_MOUSE: u32 = 8;
pub const LUNA_LANE_DRIVER_PIT_TSC: u32 = 9;
pub const LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER: u32 = 10;
pub const LUNA_LANE_DRIVER_E1000: u32 = 11;
pub const LUNA_LANE_DRIVER_PCI_IDE: u32 = 12;
pub const LUNA_LANE_DRIVER_E1000E: u32 = 13;
pub const LUNA_LANE_DRIVER_AHCI: u32 = 14;
pub const LUNA_LANE_DRIVER_QEMU_STD_VGA: u32 = 15;
pub const LUNA_LANE_DRIVER_I8042_KEYBOARD: u32 = 16;
pub const LUNA_LANE_DRIVER_I8042_MOUSE: u32 = 17;
pub const LUNA_LANE_DRIVER_PIIX_UART: u32 = 18;
pub const LUNA_LANE_DRIVER_ICH9_UART: u32 = 19;
pub const LUNA_LANE_DRIVER_VIRTIO_KEYBOARD: u32 = 20;

pub const LUNA_LANE_FLAG_PRESENT: u32 = 1u32 << 0;
pub const LUNA_LANE_FLAG_READY: u32 = 1u32 << 1;
pub const LUNA_LANE_FLAG_BOOT: u32 = 1u32 << 2;
pub const LUNA_LANE_FLAG_FALLBACK: u32 = 1u32 << 3;
pub const LUNA_LANE_FLAG_POLLING: u32 = 1u32 << 4;

pub const LUNA_CAP_DEVICE_PROBE: u64 = 0xA504;
pub const LUNA_CAP_DEVICE_BIND: u64 = 0xA505;
pub const LUNA_CAP_DEVICE_MMIO: u64 = 0xA506;
pub const LUNA_CAP_DEVICE_DMA: u64 = 0xA507;
pub const LUNA_CAP_DEVICE_IRQ: u64 = 0xA508;

pub const LUNA_DRIVER_STAGE_PROBE: u32 = 1;
pub const LUNA_DRIVER_STAGE_APPROVE: u32 = 2;
pub const LUNA_DRIVER_STAGE_BIND: u32 = 3;
pub const LUNA_DRIVER_STAGE_FAIL: u32 = 4;
pub const LUNA_DRIVER_STAGE_ROLLBACK: u32 = 5;

pub const LUNA_DRIVER_CLASS_STORAGE_BOOT: u32 = 1;
pub const LUNA_DRIVER_CLASS_DISPLAY_MIN: u32 = 2;
pub const LUNA_DRIVER_CLASS_INPUT_MIN: u32 = 3;
pub const LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY: u32 = 4;
pub const LUNA_DRIVER_CLASS_NETWORK_BASELINE: u32 = 5;

pub const LUNA_DRIVER_FAIL_NONE: u32 = 0;
pub const LUNA_DRIVER_FAIL_FATAL: u32 = 1;
pub const LUNA_DRIVER_FAIL_DEGRADED: u32 = 2;
pub const LUNA_DRIVER_FAIL_OBSERVER_ONLY: u32 = 3;

pub const LUNA_DRIVER_RUNTIME_SYSTEM: u32 = 1;
pub const LUNA_DRIVER_RUNTIME_THIRD_PARTY: u32 = 2;

pub const LUNA_DRIVER_LIFE_STORAGE_FATAL: u64 = 1u64 << 0;
pub const LUNA_DRIVER_LIFE_DISPLAY_DEGRADED: u64 = 1u64 << 1;
pub const LUNA_DRIVER_LIFE_INPUT_DEGRADED: u64 = 1u64 << 2;
pub const LUNA_DRIVER_LIFE_PLATFORM_READY: u64 = 1u64 << 3;
pub const LUNA_DRIVER_LIFE_NETWORK_DEGRADED: u64 = 1u64 << 4;

pub const LUNA_DATA_OBJECT_TYPE_DRIVER_BIND: u32 = 0x4452_5642;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaDriverBindRecord {
    pub magic: u32,
    pub version: u32,
    pub driver_class: u32,
    pub stage: u32,
    pub runtime_class: u32,
    pub failure_class: u32,
    pub device_id: u32,
    pub lane_class: u32,
    pub driver_family: u32,
    pub state_flags: u32,
    pub bound_at: u64,
    pub writer_space: u64,
    pub authority_space: u64,
    pub lane_name: [u8; 16],
    pub driver_name: [u8; 16],
}

#[repr(C)]
pub struct LunaLaneRecord {
    pub device_id: u32,
    pub device_kind: u32,
    pub lane_class: u32,
    pub driver_family: u32,
    pub state_flags: u32,
    pub reserved0: u32,
    pub lane_name: [u8; 16],
    pub driver_name: [u8; 16],
}

#[repr(C)]
pub struct LunaPciRecord {
    pub vendor_id: u16,
    pub device_id: u16,
    pub bus: u8,
    pub slot: u8,
    pub function: u8,
    pub class_code: u8,
    pub subclass: u8,
    pub prog_if: u8,
    pub header_type: u8,
    pub reserved0: [u8; 3],
}

#[repr(C)]
pub struct LunaDisplayInfo {
    pub framebuffer_base: u64,
    pub framebuffer_size: u64,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_pixels_per_scanline: u32,
    pub framebuffer_pixel_format: u32,
    pub text_columns: u32,
    pub text_rows: u32,
    pub state_flags: u32,
    pub driver_family: u32,
}

#[repr(C)]
pub struct LunaPointerEvent {
    pub dx: i32,
    pub dy: i32,
    pub buttons: u32,
    pub flags: u32,
}

#[repr(C)]
pub struct LunaNetInfo {
    pub version: u32,
    pub driver_family: u32,
    pub state_flags: u32,
    pub ctrl: u32,
    pub vendor_id: u16,
    pub device_id: u16,
    pub rx_head: u16,
    pub rx_tail: u16,
    pub tx_head: u16,
    pub tx_tail: u16,
    pub hw_rx_head: u16,
    pub hw_rx_tail: u16,
    pub hw_tx_head: u16,
    pub hw_tx_tail: u16,
    pub mmio_base: u64,
    pub tx_packets: u64,
    pub rx_packets: u64,
    pub rctl: u32,
    pub tctl: u32,
    pub status: u32,
    pub reserved0: u32,
    pub mac: [u8; 6],
    pub reserved1: [u8; 2],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaLinkPeer {
    pub peer_id: u32,
    pub flags: u32,
    pub pairing_low: u64,
    pub pairing_high: u64,
    pub name: [u8; 16],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaLinkSession {
    pub session_id: u32,
    pub peer_id: u32,
    pub flags: u32,
    pub reserved0: u32,
    pub resume_low: u64,
    pub resume_high: u64,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct LunaLinkChannel {
    pub channel_id: u32,
    pub session_id: u32,
    pub kind: u32,
    pub transfer_class: u32,
}
