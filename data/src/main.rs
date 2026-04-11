#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::mem::size_of;
use core::panic::PanicInfo;
use core::ptr::{addr_of, addr_of_mut, copy_nonoverlapping, read_volatile, write_bytes, write_volatile};

include!("../../include/luna_layout.rs");
include!("../../include/luna_proto.rs");
const STORE_MAGIC: u32 = 0x5346_414C;
const STORE_VERSION: u32 = 3;
const OBJECT_CAPACITY: usize = 128;
const SLOT_SECTORS: u32 = 1;
const SLOT_BYTES: u64 = 512;
const SLOT_BYTES_USIZE: usize = 512;
const RECORD_SECTORS: u32 = 24;
const MAX_OBJECT_BYTES: usize = OBJECT_CAPACITY * SLOT_BYTES_USIZE;

const OBJECT_FLAG_LARGE: u32 = 0x0000_0001;
const OBJECT_TYPE_SET: u32 = 0x4C53_4554;
const OBJECT_TYPE_NAMESPACE: u32 = 0x4C4E_5350;

const SET_MAGIC: u32 = 0x5345_544C;
const SET_VERSION: u32 = 1;
const LARGE_MAGIC: u32 = 0x4C41_5247;
const LARGE_VERSION: u32 = 1;
const LARGE_MAX_EXTENTS: usize = 19;

const GATE_REQUEST_CAP: u32 = 1;
const GATE_VALIDATE_CAP: u32 = 6;
const SPACE_DATA: u64 = 2;
const CAP_DEVICE_READ: u64 = 0xA502;
const CAP_DEVICE_WRITE: u64 = 0xA503;

const CAP_DATA_SEED: u64 = 0xD201;
const CAP_DATA_POUR: u64 = 0xD202;
const CAP_DATA_DRAW: u64 = 0xD203;
const CAP_DATA_SHRED: u64 = 0xD204;
const CAP_DATA_GATHER: u64 = 0xD205;

const OP_SEED: u32 = 1;
const OP_POUR: u32 = 2;
const OP_DRAW: u32 = 3;
const OP_SHRED: u32 = 4;
const OP_GATHER: u32 = 5;
const OP_STAT: u32 = 6;
const OP_VERIFY: u32 = 7;
const OP_SET_ADD: u32 = 8;
const OP_SET_REMOVE: u32 = 9;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xD100;
const STATUS_NOT_FOUND: u32 = 0xD101;
const STATUS_NO_SPACE: u32 = 0xD102;
const STATUS_RANGE: u32 = 0xD103;
const STATUS_IO: u32 = 0xD104;
const STATUS_METADATA: u32 = 0xD105;

const STORE_STATE_CLEAN: u64 = 0x434C_4541_4E00_0001;
const STORE_STATE_DIRTY: u64 = 0x4449_5254_5900_0001;
const LAST_REPAIR_NONE: u64 = 0;
const LAST_REPAIR_REPLAY: u64 = 1;
const LAST_REPAIR_METADATA: u64 = 2;
const LAST_REPAIR_SCRUB: u64 = 3;
const LAST_REPAIR_REJECT: u64 = 4;
const LAST_REPAIR_LAYOUT_OBJECT_CAPACITY: u64 = 0x1001;
const LAST_REPAIR_LAYOUT_SLOT_SECTORS: u64 = 0x1002;
const LAST_REPAIR_LAYOUT_STORE_BASE: u64 = 0x1003;
const LAST_REPAIR_LAYOUT_DATA_START: u64 = 0x1004;
const LAST_REPAIR_LAYOUT_PROFILE: u64 = 0x1005;
const LAST_REPAIR_LAYOUT_INSTALL_LOW: u64 = 0x1006;
const LAST_REPAIR_LAYOUT_INSTALL_HIGH: u64 = 0x1007;
const LAST_REPAIR_LAYOUT_PEER_LBA: u64 = 0x1008;
const LAST_REPAIR_LAYOUT_CHECKSUM: u64 = 0x1009;
const VERIFY_FLAG_DIRTY: u32 = 1;
const VERIFY_FLAG_LAYOUT_MISMATCH: u32 = 2;
const VERIFY_FLAG_CHECKSUM_MISMATCH: u32 = 4;
const VERIFY_FLAG_INVALID_RECORDS: u32 = 8;
const STATUS_DENIED: u32 = 0xD106;
const STATUS_READONLY: u32 = 0xD107;
const STATUS_RECOVERY: u32 = 0xD108;
const GOVERN_OP: u32 = 12;
const VOLUME_HEALTHY: u32 = 1;
const VOLUME_DEGRADED: u32 = 2;
const VOLUME_RECOVERY_REQUIRED: u32 = 3;
const VOLUME_FATAL_INCOMPATIBLE: u32 = 4;
const VOLUME_FATAL_UNRECOVERABLE: u32 = 5;
const MODE_NORMAL: u32 = 1;
const MODE_READONLY: u32 = 2;
const MODE_RECOVERY: u32 = 3;
const MODE_FATAL: u32 = 4;
const MODE_INSTALL: u32 = 5;
const GOVERN_MOUNT: u32 = 1;
const GOVERN_WRITE: u32 = 2;
const GOVERN_COMMIT: u32 = 3;
const GOVERN_REPLAY: u32 = 4;
const SPACE_SECURITY: u32 = 3;
const PROFILE_SYSTEM: u64 = 0x5359_534C;
const PROFILE_DATA: u64 = 0x5441_444C;
const ACTIVATION_PROVISIONED: u64 = 1;

const TXN_MAGIC: u32 = 0x4C54_584E;
const TXN_VERSION: u32 = 1;
const TXN_STATE_EMPTY: u32 = 0;
const TXN_STATE_PREPARED: u32 = 1;
const TXN_STATE_COMMITTED: u32 = 2;
const TXN_STATE_APPLIED: u32 = 3;
const TXN_KIND_OBJECT_UPDATE: u32 = 1;
const TXN_KIND_SET_UPDATE: u32 = 2;
const TXN_KIND_NAMESPACE_UPDATE: u32 = 3;

#[repr(C)]
pub struct LunaDataGate {
    sequence: u32,
    opcode: u32,
    status: u32,
    object_type: u32,
    object_flags: u32,
    result_count: u32,
    cid_low: u64,
    cid_high: u64,
    object_low: u64,
    object_high: u64,
    offset: u64,
    size: u64,
    buffer_addr: u64,
    buffer_size: u64,
    created_at: u64,
    content_size: u64,
    writer_space: u32,
    authority_space: u32,
    volume_state: u32,
    volume_mode: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct ObjectRef {
    low: u64,
    high: u64,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct StoreSuperblock {
    magic: u32,
    version: u32,
    object_capacity: u32,
    slot_sectors: u32,
    store_base_lba: u64,
    data_start_lba: u64,
    next_nonce: u64,
    format_count: u64,
    mount_count: u64,
    reserved: [u64; 55],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct ObjectRecord {
    live: u32,
    object_type: u32,
    flags: u32,
    slot_index: u32,
    size: u64,
    created_at: u64,
    modified_at: u64,
    object_low: u64,
    object_high: u64,
    reserved: [u64; 4],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct SetHeader {
    magic: u32,
    version: u32,
    member_count: u32,
    flags: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct LargeExtent {
    start_slot: u32,
    slot_count: u32,
    logical_offset: u64,
    byte_length: u64,
}

#[repr(C)]
#[derive(Copy, Clone)]
struct LargeObjectHeader {
    magic: u32,
    version: u32,
    extent_count: u32,
    flags: u32,
    total_size: u64,
    reserved: [u64; 3],
    extents: [LargeExtent; LARGE_MAX_EXTENTS],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct TxnRecord {
    magic: u32,
    version: u32,
    state: u32,
    kind: u32,
    object_index: u32,
    has_aux: u32,
    writer_space: u32,
    authority_space: u32,
    volume_state: u32,
    volume_mode: u32,
    object_low: u64,
    object_high: u64,
    old_record: ObjectRecord,
    new_record: ObjectRecord,
}

const SET_BUFFER_BYTES: usize = size_of::<SetHeader>() + (OBJECT_CAPACITY * size_of::<ObjectRef>());

const EMPTY_SUPERBLOCK: StoreSuperblock = StoreSuperblock {
    magic: 0,
    version: 0,
    object_capacity: 0,
    slot_sectors: 0,
    store_base_lba: 0,
    data_start_lba: 0,
    next_nonce: 0,
    format_count: 0,
    mount_count: 0,
    reserved: [0; 55],
};

const EMPTY_RECORD: ObjectRecord = ObjectRecord {
    live: 0,
    object_type: 0,
    flags: 0,
    slot_index: 0,
    size: 0,
    created_at: 0,
    modified_at: 0,
    object_low: 0,
    object_high: 0,
    reserved: [0; 4],
};

static mut SUPERBLOCK: StoreSuperblock = EMPTY_SUPERBLOCK;
static mut OBJECTS: [ObjectRecord; OBJECT_CAPACITY] = [EMPTY_RECORD; OBJECT_CAPACITY];
static mut SECTOR_BUFFER: [u8; 512] = [0; 512];
static mut SLOT_BITMAP: [u8; OBJECT_CAPACITY] = [0; OBJECT_CAPACITY];
static mut SLOT_LIST: [u32; OBJECT_CAPACITY] = [0; OBJECT_CAPACITY];
static mut LAST_MUTATION_DEBUG: u64 = 0;
static mut LAST_DRAW_DEBUG: u64 = 0;
static mut LAST_DRAW_VALUE0: u64 = 0;
static mut LAST_DRAW_VALUE1: u64 = 0;
static mut SET_BUFFER: [u8; SET_BUFFER_BYTES] = [0; SET_BUFFER_BYTES];
static mut DEVICE_READ_LOW: u64 = 0;
static mut DEVICE_READ_HIGH: u64 = 0;
static mut DEVICE_WRITE_LOW: u64 = 0;
static mut DEVICE_WRITE_HIGH: u64 = 0;

const EMPTY_SET_HEADER: SetHeader = SetHeader {
    magic: SET_MAGIC,
    version: SET_VERSION,
    member_count: 0,
    flags: 0,
};

const EMPTY_EXTENT: LargeExtent = LargeExtent {
    start_slot: 0,
    slot_count: 0,
    logical_offset: 0,
    byte_length: 0,
};

const EMPTY_LARGE_HEADER: LargeObjectHeader = LargeObjectHeader {
    magic: LARGE_MAGIC,
    version: LARGE_VERSION,
    extent_count: 0,
    flags: 0,
    total_size: 0,
    reserved: [0; 3],
    extents: [EMPTY_EXTENT; LARGE_MAX_EXTENTS],
};

const EMPTY_TXN: TxnRecord = TxnRecord {
    magic: 0,
    version: 0,
    state: TXN_STATE_EMPTY,
    kind: 0,
    object_index: 0,
    has_aux: 0,
    writer_space: 0,
    authority_space: 0,
    volume_state: 0,
    volume_mode: 0,
    object_low: 0,
    object_high: 0,
    old_record: EMPTY_RECORD,
    new_record: EMPTY_RECORD,
};

#[repr(C)]
struct LunaGate {
    sequence: u32,
    opcode: u32,
    caller_space: u64,
    domain_key: u64,
    cid_low: u64,
    cid_high: u64,
    status: u32,
    count: u32,
    target_space: u32,
    target_gate: u32,
    ttl: u32,
    uses: u32,
    seal_low: u64,
    seal_high: u64,
    nonce: u64,
    buffer_addr: u64,
    buffer_size: u64,
}

#[repr(C)]
struct LunaDeviceGate {
    sequence: u32,
    opcode: u32,
    status: u32,
    device_id: u32,
    result_count: u32,
    flags: u32,
    cid_low: u64,
    cid_high: u64,
    size: u64,
    buffer_addr: u64,
    buffer_size: u64,
}

unsafe fn object_ptr(index: usize) -> *mut ObjectRecord {
    unsafe { (addr_of_mut!(OBJECTS) as *mut ObjectRecord).add(index) }
}

fn manifest() -> &'static LunaManifest {
    unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) }
}

fn request_cap(domain_key: u64, out_low: &mut u64, out_high: &mut u64) -> bool {
    let manifest = manifest();
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe { write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>()); }
    gate.sequence = 40;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = SPACE_DATA;
    gate.domain_key = domain_key;
    gate.target_space = 0;
    gate.target_gate = 0;
    gate.ttl = 0;
    gate.uses = 0;
    gate.seal_low = 0;
    gate.seal_high = 0;
    gate.nonce = 0;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == 0
}

fn validate_cap(domain_key: u64, cid_low: u64, cid_high: u64, target_gate: u32) -> bool {
    let manifest = manifest();
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe { write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>()); }
    gate.sequence = 41;
    gate.opcode = GATE_VALIDATE_CAP;
    gate.caller_space = 0;
    gate.domain_key = domain_key;
    gate.cid_low = cid_low;
    gate.cid_high = cid_high;
    gate.target_space = SPACE_DATA as u32;
    gate.target_gate = target_gate;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    gate.status == 0
}

fn bootview() -> &'static mut LunaBootView {
    unsafe { &mut *(manifest().bootview_base as *mut LunaBootView) }
}

fn store_base_lba() -> u32 {
    bootview().data_store_lba as u32
}

fn peer_system_store_lba() -> u64 {
    bootview().system_store_lba
}

fn data_start_lba() -> u32 {
    store_base_lba() + 1 + RECORD_SECTORS
}

fn txn_log_lba() -> u32 {
    data_start_lba() - 2
}

fn txn_aux_lba() -> u32 {
    data_start_lba() - 1
}

fn governance_query(action: u32, domain_key: u64, cid_low: u64, cid_high: u64, object_low: u64, object_high: u64, object_type: u32, object_flags: u32, authority_hint: u32) -> Option<LunaGovernanceQuery> {
    let manifest = manifest();
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    let mut query = LunaGovernanceQuery {
        action,
        result_state: unsafe { SUPERBLOCK.reserved[4] as u32 },
        writer_space: 0,
        authority_space: authority_hint,
        mode: unsafe { SUPERBLOCK.reserved[5] as u32 },
        object_type,
        object_flags,
        reserved0: 0,
        domain_key,
        cid_low,
        cid_high,
        object_low,
        object_high,
        install_low: manifest.install_uuid_low,
        install_high: manifest.install_uuid_high,
    };
    unsafe { write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>()); }
    gate.sequence = 42;
    gate.opcode = GOVERN_OP;
    gate.caller_space = SPACE_DATA;
    gate.buffer_addr = (&mut query as *mut LunaGovernanceQuery) as u64;
    gate.buffer_size = size_of::<LunaGovernanceQuery>() as u64;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    if gate.status == 0 { Some(query) } else { None }
}

fn set_volume_runtime(state: u32, mode: u32) {
    unsafe {
        SUPERBLOCK.reserved[4] = state as u64;
        SUPERBLOCK.reserved[5] = mode as u64;
    }
    let view = bootview();
    view.volume_state = state;
    view.system_mode = mode;
}

fn governance_mount(state: u32) -> bool {
    let manifest = manifest();
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    let mut query = LunaGovernanceQuery {
        action: GOVERN_MOUNT,
        result_state: state,
        writer_space: 0,
        authority_space: 0,
        mode: 0,
        object_type: 0,
        object_flags: 0,
        reserved0: 0,
        domain_key: 0,
        cid_low: 0,
        cid_high: 0,
        object_low: 0,
        object_high: 0,
        install_low: manifest.install_uuid_low,
        install_high: manifest.install_uuid_high,
    };
    unsafe { write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>()); }
    gate.sequence = 42;
    gate.opcode = GOVERN_OP;
    gate.caller_space = SPACE_DATA;
    gate.buffer_addr = (&mut query as *mut LunaGovernanceQuery) as u64;
    gate.buffer_size = size_of::<LunaGovernanceQuery>() as u64;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    if gate.status != 0 {
        set_volume_runtime(VOLUME_FATAL_UNRECOVERABLE, MODE_FATAL);
        return false;
    }
    set_volume_runtime(query.result_state, query.mode);
    !matches!(query.result_state, VOLUME_FATAL_INCOMPATIBLE | VOLUME_FATAL_UNRECOVERABLE)
}

fn reject_mutation_status() -> u32 {
    match unsafe { SUPERBLOCK.reserved[5] as u32 } {
        MODE_READONLY => STATUS_READONLY,
        MODE_RECOVERY | MODE_FATAL => STATUS_RECOVERY,
        _ => STATUS_DENIED,
    }
}

fn authorize_mutation(action: u32, domain_key: u64, gate: &mut LunaDataGate, object_type: u32, object_flags: u32, authority_hint: u32) -> bool {
    let Some(query) = governance_query(action, domain_key, gate.cid_low, gate.cid_high, gate.object_low, gate.object_high, object_type, object_flags, authority_hint) else {
        gate.status = reject_mutation_status();
        return false;
    };
    gate.writer_space = query.writer_space;
    gate.authority_space = query.authority_space;
    gate.volume_state = query.result_state;
    gate.volume_mode = query.mode;
    true
}

fn device_call(gate: &mut LunaDeviceGate) {
    let manifest = manifest();
    let entry: extern "sysv64" fn(*mut LunaDeviceGate) =
        unsafe { core::mem::transmute(manifest.device_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaDeviceGate);
}

fn device_write(text: &[u8]) -> bool {
    let _ = text;
    true
}

fn device_write_hex(prefix: &[u8], value: u64) {
    let mut line = [0u8; 64];
    let mut cursor = 0usize;
    let hex = *b"0123456789ABCDEF";
    let mut index = 0usize;
    while index < prefix.len() && cursor < line.len() {
        line[cursor] = prefix[index];
        cursor += 1;
        index += 1;
    }
    if cursor + 18 > line.len() {
        let _ = device_write(prefix);
        return;
    }
    line[cursor] = b'0';
    line[cursor + 1] = b'x';
    cursor += 2;
    let mut shift = 60u32;
    loop {
        line[cursor] = hex[((value >> shift) & 0xF) as usize];
        cursor += 1;
        if shift == 0 {
            break;
        }
        shift -= 4;
    }
    line[cursor] = b'\r';
    line[cursor + 1] = b'\n';
    let _ = device_write(&line[..cursor + 2]);
}

unsafe fn ata_read_sector(lba: u32, out: *mut u8) -> bool {
    let gate = unsafe { &mut *(manifest().device_gate_base as *mut LunaDeviceGate) };
    unsafe { write_bytes(gate as *mut LunaDeviceGate as *mut u8, 0, size_of::<LunaDeviceGate>()); }
    gate.sequence = 201;
    gate.opcode = LUNA_DEVICE_BLOCK_READ;
    gate.device_id = LUNA_DEVICE_ID_DISK0 as u32;
    gate.flags = lba;
    gate.cid_low = unsafe { DEVICE_READ_LOW };
    gate.cid_high = unsafe { DEVICE_READ_HIGH };
    gate.buffer_addr = out as u64;
    gate.buffer_size = 512;
    gate.size = 0;
    device_call(gate);
    if gate.status == 0 && gate.size == 512 {
        return true;
    }
    if !request_cap(CAP_DEVICE_READ, unsafe { &mut DEVICE_READ_LOW }, unsafe { &mut DEVICE_READ_HIGH }) {
        return false;
    }
    gate.cid_low = unsafe { DEVICE_READ_LOW };
    gate.cid_high = unsafe { DEVICE_READ_HIGH };
    gate.size = 0;
    device_call(gate);
    gate.status == 0 && gate.size == 512
}

unsafe fn ata_write_sector(lba: u32, src: *const u8) -> bool {
    let gate = unsafe { &mut *(manifest().device_gate_base as *mut LunaDeviceGate) };
    unsafe { write_bytes(gate as *mut LunaDeviceGate as *mut u8, 0, size_of::<LunaDeviceGate>()); }
    gate.sequence = 202;
    gate.opcode = LUNA_DEVICE_BLOCK_WRITE;
    gate.device_id = LUNA_DEVICE_ID_DISK0 as u32;
    gate.flags = lba;
    gate.cid_low = unsafe { DEVICE_WRITE_LOW };
    gate.cid_high = unsafe { DEVICE_WRITE_HIGH };
    gate.buffer_addr = src as u64;
    gate.buffer_size = 512;
    gate.size = 512;
    device_call(gate);
    if gate.status == 0 {
        return true;
    }
    if !request_cap(CAP_DEVICE_WRITE, unsafe { &mut DEVICE_WRITE_LOW }, unsafe { &mut DEVICE_WRITE_HIGH }) {
        return false;
    }
    gate.cid_low = unsafe { DEVICE_WRITE_LOW };
    gate.cid_high = unsafe { DEVICE_WRITE_HIGH };
    device_call(gate);
    gate.status == 0
}

unsafe fn load_bytes_from_disk(lba: u32, ptr: *mut u8, len: usize) -> bool {
    let sectors = len.div_ceil(512);
    let mut i = 0usize;
    while i < sectors {
        let offset = i * 512;
        let chunk = core::cmp::min(512usize, len.saturating_sub(offset));
        if !unsafe { ata_read_sector(lba + i as u32, addr_of_mut!(SECTOR_BUFFER) as *mut u8) } {
            return false;
        }
        unsafe {
            copy_nonoverlapping(addr_of!(SECTOR_BUFFER) as *const u8, ptr.add(offset), chunk);
        }
        i += 1;
    }
    true
}

unsafe fn save_bytes_to_disk(lba: u32, ptr: *const u8, len: usize) -> bool {
    let sectors = len.div_ceil(512);
    let mut i = 0usize;
    while i < sectors {
        let offset = i * 512;
        let chunk = core::cmp::min(512usize, len.saturating_sub(offset));
        unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
        unsafe {
            copy_nonoverlapping(ptr.add(offset), addr_of_mut!(SECTOR_BUFFER) as *mut u8, chunk);
        }
        if !unsafe { ata_write_sector(lba + i as u32, addr_of!(SECTOR_BUFFER) as *const u8) } {
            return false;
        }
        i += 1;
    }
    true
}

unsafe fn save_metadata() -> bool {
    unsafe {
        LAST_MUTATION_DEBUG = 0x5201;
        let _ = device_write(b"[DATA] meta save begin\r\n");
        if !save_bytes_to_disk(store_base_lba(), addr_of!(SUPERBLOCK) as *const u8, size_of::<StoreSuperblock>()) {
            LAST_MUTATION_DEBUG = 0x52E1;
            let _ = device_write(b"[DATA] meta super fail\r\n");
            return false;
        }
        LAST_MUTATION_DEBUG = 0x5202;
        let _ = device_write(b"[DATA] meta super ok\r\n");
        if !save_bytes_to_disk(store_base_lba() + 1, addr_of!(OBJECTS) as *const u8, size_of::<[ObjectRecord; OBJECT_CAPACITY]>()) {
            LAST_MUTATION_DEBUG = 0x52E2;
            let _ = device_write(b"[DATA] meta objects fail\r\n");
            return false;
        }
        LAST_MUTATION_DEBUG = 0x5203;
        let _ = device_write(b"[DATA] meta objects ok\r\n");
        true
    }
}

unsafe fn load_txn_record(out: &mut TxnRecord) -> bool {
    unsafe { load_bytes_from_disk(txn_log_lba(), out as *mut TxnRecord as *mut u8, size_of::<TxnRecord>()) }
}

unsafe fn save_txn_record(txn: &TxnRecord) -> bool {
    unsafe { save_bytes_to_disk(txn_log_lba(), txn as *const TxnRecord as *const u8, size_of::<TxnRecord>()) }
}

unsafe fn clear_txn_record() -> bool {
    unsafe { save_txn_record(&EMPTY_TXN) }
}

unsafe fn load_txn_aux(out: &mut LargeObjectHeader) -> bool {
    unsafe { load_bytes_from_disk(txn_aux_lba(), out as *mut LargeObjectHeader as *mut u8, size_of::<LargeObjectHeader>()) }
}

unsafe fn save_txn_aux(header: &LargeObjectHeader) -> bool {
    unsafe { save_bytes_to_disk(txn_aux_lba(), header as *const LargeObjectHeader as *const u8, size_of::<LargeObjectHeader>()) }
}

unsafe fn clear_txn_aux() -> bool {
    let header = EMPTY_LARGE_HEADER;
    unsafe { save_bytes_to_disk(txn_aux_lba(), addr_of!(header) as *const u8, size_of::<LargeObjectHeader>()) }
}

fn txn_kind_for_object_type(object_type: u32) -> u32 {
    if object_type == OBJECT_TYPE_SET {
        TXN_KIND_SET_UPDATE
    } else if object_type == OBJECT_TYPE_NAMESPACE {
        TXN_KIND_NAMESPACE_UPDATE
    } else {
        TXN_KIND_OBJECT_UPDATE
    }
}

fn fold_bytes(mut seed: u64, bytes: &[u8]) -> u64 {
    let mut index = 0usize;
    while index < bytes.len() {
        seed ^= bytes[index] as u64;
        seed = seed.rotate_left(5).wrapping_mul(0x100000001B3);
        index += 1;
    }
    seed
}

unsafe fn object_checksum() -> u64 {
    let bytes = unsafe {
        core::slice::from_raw_parts(
            addr_of!(OBJECTS) as *const u8,
            size_of::<[ObjectRecord; OBJECT_CAPACITY]>(),
        )
    };
    fold_bytes(0xCBF2_9CE4_8422_2325, bytes)
}

unsafe fn refresh_superblock_checksum() {
    unsafe {
        SUPERBLOCK.reserved[1] = object_checksum();
    }
}

fn valid_slot_index(slot_index: u32) -> bool {
    (slot_index as usize) < OBJECT_CAPACITY
}

unsafe fn metadata_matches_layout() -> bool {
    unsafe {
        SUPERBLOCK.object_capacity == OBJECT_CAPACITY as u32
            && SUPERBLOCK.slot_sectors == SLOT_SECTORS
            && SUPERBLOCK.store_base_lba == store_base_lba() as u64
            && SUPERBLOCK.data_start_lba == data_start_lba() as u64
            && SUPERBLOCK.reserved[6] == PROFILE_DATA
            && SUPERBLOCK.reserved[7] == manifest().install_uuid_low
            && SUPERBLOCK.reserved[8] == manifest().install_uuid_high
            && SUPERBLOCK.reserved[10] == peer_system_store_lba()
            && SUPERBLOCK.reserved[1] == object_checksum()
    }
}

unsafe fn metadata_layout_mismatch_reason() -> u64 {
    unsafe {
        if SUPERBLOCK.object_capacity != OBJECT_CAPACITY as u32 {
            LAST_REPAIR_LAYOUT_OBJECT_CAPACITY
        } else if SUPERBLOCK.slot_sectors != SLOT_SECTORS {
            LAST_REPAIR_LAYOUT_SLOT_SECTORS
        } else if SUPERBLOCK.store_base_lba != store_base_lba() as u64 {
            LAST_REPAIR_LAYOUT_STORE_BASE
        } else if SUPERBLOCK.data_start_lba != data_start_lba() as u64 {
            LAST_REPAIR_LAYOUT_DATA_START
        } else if SUPERBLOCK.reserved[6] != PROFILE_DATA {
            LAST_REPAIR_LAYOUT_PROFILE
        } else if SUPERBLOCK.reserved[7] != manifest().install_uuid_low {
            LAST_REPAIR_LAYOUT_INSTALL_LOW
        } else if SUPERBLOCK.reserved[8] != manifest().install_uuid_high {
            LAST_REPAIR_LAYOUT_INSTALL_HIGH
        } else if SUPERBLOCK.reserved[10] != peer_system_store_lba() {
            LAST_REPAIR_LAYOUT_PEER_LBA
        } else if SUPERBLOCK.reserved[1] != object_checksum() {
            LAST_REPAIR_LAYOUT_CHECKSUM
        } else {
            LAST_REPAIR_METADATA
        }
    }
}

unsafe fn scrub_invalid_records() -> u32 {
    let mut repaired = 0u32;
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        let record = unsafe { *object_ptr(index) };
        let mut invalid = 0u32;
        if record.live == 1 {
            let slot_count = unsafe { object_slot_count(&record) };
            if !valid_slot_index(record.slot_index)
                || slot_count.is_none()
                || (!is_large_object(&record) && record.size > SLOT_BYTES)
                || record.object_low == 0
                || record.object_high == 0
                || record.created_at == 0
                || record.modified_at == 0
            {
                invalid = 1u32;
            }
        } else if record.live != 0 {
            invalid = 1u32;
        }

        if invalid != 0 {
            unsafe {
                object_ptr(index).write(EMPTY_RECORD);
                write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
                if !ata_write_sector(object_slot_lba(index), addr_of!(SECTOR_BUFFER) as *const u8) {
                    return repaired;
                }
            }
            repaired = repaired.wrapping_add(1);
        }
        index += 1;
    }
    repaired
}

unsafe fn commit_metadata(clean_state: u64) -> bool {
    unsafe {
        SUPERBLOCK.reserved[0] = clean_state;
        refresh_superblock_checksum();
        save_metadata()
    }
}

unsafe fn rollback_seed_state(index: usize, previous_record: ObjectRecord, previous_nonce: u64) -> bool {
    unsafe {
        object_ptr(index).write(previous_record);
        SUPERBLOCK.next_nonce = previous_nonce;
        commit_metadata(STORE_STATE_CLEAN)
    }
}

unsafe fn abort_mutation() -> bool {
    unsafe { commit_metadata(STORE_STATE_CLEAN) }
}

unsafe fn begin_mutation() -> bool {
    unsafe {
        let previous_state = SUPERBLOCK.reserved[0];
        let failure_debug: u64;
        LAST_MUTATION_DEBUG = 0x5301;
        let _ = device_write(b"[DATA] mutation begin\r\n");
        device_write_hex(b"[DATA] mutation state old=", previous_state);
        SUPERBLOCK.reserved[0] = STORE_STATE_DIRTY;
        refresh_superblock_checksum();
        LAST_MUTATION_DEBUG = 0x5302;
        let _ = device_write(b"[DATA] mutation dirty set\r\n");
        if save_metadata() {
            LAST_MUTATION_DEBUG = 0x5303;
            let _ = device_write(b"[DATA] mutation begin ok\r\n");
            return true;
        }
        failure_debug = LAST_MUTATION_DEBUG;
        let _ = device_write(b"[DATA] mutation begin meta fail\r\n");
        SUPERBLOCK.reserved[0] = previous_state;
        refresh_superblock_checksum();
        if !save_metadata() {
            LAST_MUTATION_DEBUG = failure_debug | 0x0100;
        } else {
            LAST_MUTATION_DEBUG = failure_debug;
        }
        let _ = device_write(b"[DATA] mutation rollback state\r\n");
        false
    }
}

unsafe fn finish_applied_transaction(index: usize, txn: &TxnRecord) -> bool {
    unsafe {
        object_ptr(index).write(txn.new_record);
        if SUPERBLOCK.next_nonce <= txn.new_record.modified_at {
            SUPERBLOCK.next_nonce = txn.new_record.modified_at.wrapping_add(1);
        }
        refresh_superblock_checksum();
        if !save_metadata() {
            return false;
        }
        let mut applied = *txn;
        applied.state = TXN_STATE_APPLIED;
        if !save_txn_record(&applied) {
            return false;
        }
        if !zero_object_slots(&txn.old_record) {
            return false;
        }
        clear_txn_aux() && clear_txn_record() && commit_metadata(STORE_STATE_CLEAN)
    }
}

unsafe fn reject_transaction(txn: &TxnRecord) -> bool {
    unsafe {
        let _ = zero_staged_object_slots(&txn.new_record, txn.has_aux);
        clear_txn_aux() && clear_txn_record() && commit_metadata(STORE_STATE_CLEAN)
    }
}

unsafe fn replay_transaction() -> u64 {
    let mut txn = EMPTY_TXN;
    if !unsafe { load_txn_record(&mut txn) } {
        return LAST_REPAIR_NONE;
    }
    if txn.magic != TXN_MAGIC || txn.version != TXN_VERSION || txn.state == TXN_STATE_EMPTY {
        return LAST_REPAIR_NONE;
    }
    if txn.object_index as usize >= OBJECT_CAPACITY {
        let _ = unsafe { clear_txn_aux() };
        let _ = unsafe { clear_txn_record() };
        return LAST_REPAIR_METADATA;
    }

    match txn.state {
        TXN_STATE_PREPARED => {
            let _ = unsafe { reject_transaction(&txn) };
            LAST_REPAIR_REJECT
        }
        TXN_STATE_COMMITTED => {
            if governance_query(GOVERN_REPLAY, 0, 0, 0, txn.object_low, txn.object_high, txn.new_record.object_type, txn.new_record.flags, txn.authority_space).is_some() {
                let _ = unsafe { finish_applied_transaction(txn.object_index as usize, &txn) };
                LAST_REPAIR_REPLAY
            } else {
                set_volume_runtime(VOLUME_RECOVERY_REQUIRED, MODE_RECOVERY);
                let _ = unsafe { reject_transaction(&txn) };
                LAST_REPAIR_REJECT
            }
        }
        TXN_STATE_APPLIED => {
            if governance_query(GOVERN_REPLAY, 0, 0, 0, txn.object_low, txn.object_high, txn.new_record.object_type, txn.new_record.flags, txn.authority_space).is_some() {
                let _ = unsafe { zero_object_slots(&txn.old_record) };
                let _ = unsafe { clear_txn_aux() };
                let _ = unsafe { clear_txn_record() };
                let _ = unsafe { commit_metadata(STORE_STATE_CLEAN) };
                LAST_REPAIR_REPLAY
            } else {
                set_volume_runtime(VOLUME_RECOVERY_REQUIRED, MODE_RECOVERY);
                LAST_REPAIR_REJECT
            }
        }
        _ => {
            let _ = unsafe { clear_txn_aux() };
            let _ = unsafe { clear_txn_record() };
            LAST_REPAIR_METADATA
        }
    }
}

unsafe fn format_store() -> bool {
    unsafe {
        SUPERBLOCK = EMPTY_SUPERBLOCK;
        SUPERBLOCK.magic = STORE_MAGIC;
        SUPERBLOCK.version = STORE_VERSION;
        SUPERBLOCK.object_capacity = OBJECT_CAPACITY as u32;
        SUPERBLOCK.slot_sectors = SLOT_SECTORS;
        SUPERBLOCK.store_base_lba = store_base_lba() as u64;
        SUPERBLOCK.data_start_lba = data_start_lba() as u64;
        SUPERBLOCK.next_nonce = 1;
        SUPERBLOCK.format_count = 1;
        SUPERBLOCK.mount_count = 1;
        SUPERBLOCK.reserved[0] = STORE_STATE_CLEAN;
        SUPERBLOCK.reserved[2] = LAST_REPAIR_NONE;
        SUPERBLOCK.reserved[3] = 0;
        SUPERBLOCK.reserved[4] = VOLUME_HEALTHY as u64;
        SUPERBLOCK.reserved[5] = MODE_NORMAL as u64;
        SUPERBLOCK.reserved[6] = PROFILE_DATA;
        SUPERBLOCK.reserved[7] = manifest().install_uuid_low;
        SUPERBLOCK.reserved[8] = manifest().install_uuid_high;
        SUPERBLOCK.reserved[9] = ACTIVATION_PROVISIONED;
        SUPERBLOCK.reserved[10] = peer_system_store_lba();
        OBJECTS = [EMPTY_RECORD; OBJECT_CAPACITY];

        let mut index = 0usize;
        while index < OBJECT_CAPACITY {
            write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
            if !ata_write_sector(data_start_lba() + index as u32, addr_of!(SECTOR_BUFFER) as *const u8) {
                return false;
            }
            index += 1;
        }

        if !clear_txn_aux() || !clear_txn_record() {
            return false;
        }

        let ok = commit_metadata(STORE_STATE_CLEAN);
        if ok {
            set_volume_runtime(VOLUME_HEALTHY, MODE_NORMAL);
        }
        ok
    }
}

unsafe fn load_store() -> bool {
    unsafe {
        let mut last_repair = LAST_REPAIR_NONE;
        let mut last_scrubbed = 0u64;
        let mut mount_state = VOLUME_HEALTHY;
        if !load_bytes_from_disk(store_base_lba(), addr_of_mut!(SUPERBLOCK) as *mut u8, size_of::<StoreSuperblock>()) {
            let _ = device_write(b"[DATA] load super fail\r\n");
            return false;
        }
        if SUPERBLOCK.magic != STORE_MAGIC || SUPERBLOCK.version != STORE_VERSION {
            let _ = device_write(b"[DATA] format store\r\n");
            return format_store();
        }
        if !load_bytes_from_disk(store_base_lba() + 1, addr_of_mut!(OBJECTS) as *mut u8, size_of::<[ObjectRecord; OBJECT_CAPACITY]>()) {
            let _ = device_write(b"[DATA] load meta fail\r\n");
            return false;
        }
        let replay_result = replay_transaction();
        if replay_result != LAST_REPAIR_NONE {
            let _ = device_write(b"[DATA] txn replay\r\n");
            last_repair = replay_result;
        }
        if !metadata_matches_layout() {
            let _ = device_write(b"[DATA] metadata repair\r\n");
            if SUPERBLOCK.object_capacity != OBJECT_CAPACITY as u32
                || SUPERBLOCK.slot_sectors != SLOT_SECTORS
                || SUPERBLOCK.store_base_lba != store_base_lba() as u64
                || SUPERBLOCK.data_start_lba != data_start_lba() as u64
                || SUPERBLOCK.reserved[6] != PROFILE_DATA
                || SUPERBLOCK.reserved[7] != manifest().install_uuid_low
                || SUPERBLOCK.reserved[8] != manifest().install_uuid_high
                || SUPERBLOCK.reserved[10] != peer_system_store_lba()
            {
                let _ = device_write(b"[DATA] format store\r\n");
                return format_store();
            }
            last_repair = metadata_layout_mismatch_reason();
            let repaired = scrub_invalid_records();
            if repaired != 0u32 {
                let _ = device_write(b"[DATA] scrub objects\r\n");
                last_scrubbed = repaired as u64;
                mount_state = VOLUME_DEGRADED;
            } else {
                mount_state = VOLUME_DEGRADED;
            }
        } else if SUPERBLOCK.reserved[0] == STORE_STATE_DIRTY {
            let _ = device_write(b"[DATA] recovery replay\r\n");
            last_repair = LAST_REPAIR_REPLAY;
            mount_state = VOLUME_RECOVERY_REQUIRED;
            let repaired = scrub_invalid_records();
            if repaired != 0u32 {
                let _ = device_write(b"[DATA] scrub objects\r\n");
                last_repair = LAST_REPAIR_SCRUB;
                last_scrubbed = repaired as u64;
            }
        }
        SUPERBLOCK.mount_count = SUPERBLOCK.mount_count.wrapping_add(1);
        SUPERBLOCK.reserved[2] = last_repair;
        SUPERBLOCK.reserved[3] = last_scrubbed;
        if !governance_mount(mount_state) {
            return false;
        }
        commit_metadata(STORE_STATE_CLEAN)
    }
}

fn find_index(low: u64, high: u64) -> Option<usize> {
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        let record = unsafe { *object_ptr(index) };
        if record.live == 1 && record.object_low == low && record.object_high == high {
            return Some(index);
        }
        index += 1;
    }
    None
}

fn object_slot_lba(slot_index: usize) -> u32 {
    data_start_lba() + (slot_index as u32 * SLOT_SECTORS)
}

fn is_large_object(record: &ObjectRecord) -> bool {
    (record.flags & OBJECT_FLAG_LARGE) != 0
}

fn slots_for_bytes(size: u64) -> usize {
    if size == 0 {
        0
    } else {
        size.div_ceil(SLOT_BYTES) as usize
    }
}

fn extent_payload_bytes(extent: &LargeExtent) -> u64 {
    (extent.slot_count as u64) * SLOT_BYTES
}

unsafe fn read_extent(header: &LargeObjectHeader, index: usize) -> LargeExtent {
    unsafe { read_volatile((addr_of!(header.extents) as *const LargeExtent).add(index)) }
}

unsafe fn write_extent(header: &mut LargeObjectHeader, index: usize, extent: LargeExtent) {
    unsafe {
        write_volatile((addr_of_mut!(header.extents) as *mut LargeExtent).add(index), extent);
    }
}

unsafe fn read_large_header(slot_index: u32, out: &mut LargeObjectHeader) -> bool {
    unsafe { load_bytes_from_disk(object_slot_lba(slot_index as usize), out as *mut LargeObjectHeader as *mut u8, size_of::<LargeObjectHeader>()) }
}

fn large_header_valid(header: &LargeObjectHeader, total_size: u64) -> bool {
    if header.magic != LARGE_MAGIC || header.version != LARGE_VERSION {
        return false;
    }
    if header.extent_count == 0 || header.extent_count as usize > LARGE_MAX_EXTENTS {
        return false;
    }
    if header.total_size != total_size || total_size <= SLOT_BYTES {
        return false;
    }

    let mut next_offset = 0u64;
    let mut index = 0usize;
    while index < header.extent_count as usize {
        let extent = unsafe { read_extent(header, index) };
        if extent.slot_count == 0 || extent.byte_length == 0 {
            return false;
        }
        if extent.start_slot as usize >= OBJECT_CAPACITY || (extent.start_slot as usize + extent.slot_count as usize) > OBJECT_CAPACITY {
            return false;
        }
        if extent.logical_offset != next_offset {
            return false;
        }
        if extent.byte_length > extent_payload_bytes(&extent) {
            return false;
        }
        next_offset = next_offset.saturating_add(extent.byte_length);
        index += 1;
    }
    next_offset == total_size
}

unsafe fn object_slot_count(record: &ObjectRecord) -> Option<usize> {
    if record.live != 1 {
        return Some(0);
    }
    if record.slot_index as usize >= OBJECT_CAPACITY {
        return None;
    }
    if !is_large_object(record) {
        return Some(1);
    }
    let mut header = EMPTY_LARGE_HEADER;
    if !unsafe { read_large_header(record.slot_index, &mut header) } || !large_header_valid(&header, record.size) {
        return None;
    }
    let mut total = 1usize;
    let mut index = 0usize;
    while index < header.extent_count as usize {
        total += unsafe { read_extent(&header, index) }.slot_count as usize;
        index += 1;
    }
    Some(total)
}

unsafe fn collect_object_slots(record: &ObjectRecord, out_slots: *mut u32, capacity: usize) -> Option<usize> {
    if record.live != 1 {
        return Some(0);
    }
    if capacity == 0 || record.slot_index as usize >= OBJECT_CAPACITY {
        return None;
    }
    unsafe { write_volatile(out_slots, record.slot_index); }
    if !is_large_object(record) {
        return Some(1);
    }
    let mut header = EMPTY_LARGE_HEADER;
    if !unsafe { read_large_header(record.slot_index, &mut header) } || !large_header_valid(&header, record.size) {
        return None;
    }
    let mut count = 1usize;
    let mut extent_index = 0usize;
    while extent_index < header.extent_count as usize {
        let extent = unsafe { read_extent(&header, extent_index) };
        let mut slot = 0u32;
        while slot < extent.slot_count {
            if count >= capacity {
                return None;
            }
            unsafe { write_volatile(out_slots.add(count), extent.start_slot + slot); }
            count += 1;
            slot += 1;
        }
        extent_index += 1;
    }
    Some(count)
}

unsafe fn rebuild_slot_bitmap(skip_object: Option<usize>) {
    unsafe { write_bytes(addr_of_mut!(SLOT_BITMAP) as *mut u8, 0, OBJECT_CAPACITY); }
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        if Some(index) != skip_object {
            let record = unsafe { *object_ptr(index) };
            if record.live == 1 {
                let count = unsafe { collect_object_slots(&record, addr_of_mut!(SLOT_LIST) as *mut u32, OBJECT_CAPACITY) };
                if let Some(count) = count {
                    let mut slot_index = 0usize;
                    while slot_index < count {
                        let slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(slot_index)) };
                        if (slot as usize) < OBJECT_CAPACITY {
                            unsafe {
                                *addr_of_mut!(SLOT_BITMAP).cast::<u8>().add(slot as usize) = 1;
                            }
                        }
                        slot_index += 1;
                    }
                }
            }
        }
        index += 1;
    }
}

unsafe fn allocate_slot_list(count: usize, out_slots: *mut u32) -> bool {
    let mut found = 0usize;
    let mut slot = 0usize;
    while slot < OBJECT_CAPACITY {
        if unsafe { *addr_of!(SLOT_BITMAP).cast::<u8>().add(slot) } == 0 {
            unsafe {
                *addr_of_mut!(SLOT_BITMAP).cast::<u8>().add(slot) = 1;
                write_volatile(out_slots.add(found), slot as u32);
            }
            found += 1;
            if found == count {
                return true;
            }
        }
        slot += 1;
    }
    false
}

unsafe fn zero_slot(slot_index: u32) -> bool {
    unsafe {
        write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
        ata_write_sector(object_slot_lba(slot_index as usize), addr_of!(SECTOR_BUFFER) as *const u8)
    }
}

unsafe fn read_object_bytes(index: usize, out: *mut u8, capacity: usize) -> Option<usize> {
    let record = unsafe { *object_ptr(index) };
    if record.live != 1 || record.size as usize > capacity {
        return None;
    }
    if record.size == 0 {
        return Some(0);
    }
    if !unsafe { read_record_range(&record, 0, out, record.size as usize) } {
        return None;
    }
    Some(record.size as usize)
}

unsafe fn map_logical_slot(record: &ObjectRecord, logical_offset: u64, out_slot: &mut u32, out_in_slot: &mut usize) -> bool {
    if logical_offset >= record.size {
        unsafe {
            LAST_DRAW_DEBUG = 0xD341;
            LAST_DRAW_VALUE0 = logical_offset;
            LAST_DRAW_VALUE1 = record.size;
        }
        return false;
    }
    if !is_large_object(record) {
        *out_slot = record.slot_index;
        *out_in_slot = logical_offset as usize;
        return true;
    }

    let mut header = EMPTY_LARGE_HEADER;
    if !unsafe { read_large_header(record.slot_index, &mut header) } {
        unsafe {
            LAST_DRAW_DEBUG = 0xD342;
            LAST_DRAW_VALUE0 = record.slot_index as u64;
            LAST_DRAW_VALUE1 = record.size;
        }
        let _ = device_write(b"[DATA] draw large header read fail\r\n");
        device_write_hex(b"[DATA] draw header slot=", record.slot_index as u64);
        return false;
    }
    if !large_header_valid(&header, record.size) {
        unsafe {
            LAST_DRAW_DEBUG = 0xD343;
            LAST_DRAW_VALUE0 = record.slot_index as u64;
            LAST_DRAW_VALUE1 = record.size;
        }
        let _ = device_write(b"[DATA] draw large header invalid\r\n");
        device_write_hex(b"[DATA] draw header slot=", record.slot_index as u64);
        device_write_hex(b"[DATA] draw header size=", record.size);
        return false;
    }

    let mut extent_index = 0usize;
    while extent_index < header.extent_count as usize {
        let extent = unsafe { read_extent(&header, extent_index) };
        if logical_offset >= extent.logical_offset && logical_offset < extent.logical_offset + extent.byte_length {
            let relative = logical_offset - extent.logical_offset;
            *out_slot = extent.start_slot + (relative / SLOT_BYTES) as u32;
            *out_in_slot = (relative % SLOT_BYTES) as usize;
            return true;
        }
        extent_index += 1;
    }
    let _ = device_write(b"[DATA] draw extent miss\r\n");
    device_write_hex(b"[DATA] draw logical=", logical_offset);
    unsafe {
        LAST_DRAW_DEBUG = 0xD344;
        LAST_DRAW_VALUE0 = logical_offset;
        LAST_DRAW_VALUE1 = record.slot_index as u64;
    }
    false
}

unsafe fn read_record_range(record: &ObjectRecord, offset: u64, out: *mut u8, amount: usize) -> bool {
    if record.live != 1 {
        return false;
    }
    if amount == 0 {
        return true;
    }
    if offset + amount as u64 > record.size {
        return false;
    }

    let mut copied = 0usize;
    while copied < amount {
        let logical = offset + copied as u64;
        let mut slot = 0u32;
        let mut in_slot = 0usize;
        if !unsafe { map_logical_slot(record, logical, &mut slot, &mut in_slot) } {
            unsafe {
                if LAST_DRAW_DEBUG == 0 {
                    LAST_DRAW_DEBUG = 0xD345;
                    LAST_DRAW_VALUE0 = logical;
                    LAST_DRAW_VALUE1 = record.slot_index as u64;
                }
            }
            let _ = device_write(b"[DATA] draw map fail\r\n");
            return false;
        }
        if !unsafe { ata_read_sector(object_slot_lba(slot as usize), addr_of_mut!(SECTOR_BUFFER) as *mut u8) } {
            unsafe {
                LAST_DRAW_DEBUG = 0xD346;
                LAST_DRAW_VALUE0 = slot as u64;
                LAST_DRAW_VALUE1 = object_slot_lba(slot as usize) as u64;
            }
            let _ = device_write(b"[DATA] draw sector fail\r\n");
            device_write_hex(b"[DATA] draw slot=", slot as u64);
            return false;
        }
        let remaining = amount - copied;
        let available = core::cmp::min(remaining, SLOT_BYTES_USIZE - in_slot);
        unsafe {
            copy_nonoverlapping(
                (addr_of!(SECTOR_BUFFER) as *const u8).add(in_slot),
                out.add(copied),
                available,
            );
        }
        copied += available;
    }
    true
}

unsafe fn read_object_range(index: usize, offset: u64, out: *mut u8, amount: usize) -> bool {
    let record = unsafe { *object_ptr(index) };
    unsafe { read_record_range(&record, offset, out, amount) }
}

unsafe fn write_small_object(index: usize, payload: *const u8, size: usize) -> bool {
    let current = unsafe { *object_ptr(index) };
    let slot = if current.live == 1 && !is_large_object(&current) && valid_slot_index(current.slot_index) {
        current.slot_index
    } else {
        unsafe { rebuild_slot_bitmap(None); }
        if !unsafe { allocate_slot_list(1, addr_of_mut!(SLOT_LIST) as *mut u32) } {
            return false;
        }
        unsafe { read_volatile(addr_of!(SLOT_LIST) as *const u32) }
    };
    let _ = device_write(b"[DATA] write small begin\r\n");
    device_write_hex(b"[DATA] write small index=", index as u64);
    device_write_hex(b"[DATA] write small slot=", slot as u64);
    device_write_hex(b"[DATA] write small lba=", object_slot_lba(slot as usize) as u64);
    device_write_hex(b"[DATA] write small size=", size as u64);
    unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
    if size != 0 {
        unsafe {
            copy_nonoverlapping(payload, addr_of_mut!(SECTOR_BUFFER) as *mut u8, size);
        }
    }
    if !unsafe { ata_write_sector(object_slot_lba(slot as usize), addr_of!(SECTOR_BUFFER) as *const u8) } {
        let _ = device_write(b"[DATA] write small sector fail\r\n");
        return false;
    }
    let _ = device_write(b"[DATA] write small sector ok\r\n");
    unsafe {
        (*object_ptr(index)).slot_index = slot;
        (*object_ptr(index)).size = size as u64;
        (*object_ptr(index)).flags &= !OBJECT_FLAG_LARGE;
        (*object_ptr(index)).modified_at = SUPERBLOCK.next_nonce;
        SUPERBLOCK.next_nonce = SUPERBLOCK.next_nonce.wrapping_add(1);
    }
    true
}

unsafe fn write_large_object(index: usize, payload: *const u8, size: usize) -> bool {
    let data_slot_count = slots_for_bytes(size as u64);
    if data_slot_count == 0 {
        return unsafe { write_small_object(index, payload, size) };
    }
    unsafe { rebuild_slot_bitmap(Some(index)); }
    if !unsafe { allocate_slot_list(1 + data_slot_count, addr_of_mut!(SLOT_LIST) as *mut u32) } {
        return false;
    }

    let header_slot = unsafe { read_volatile(addr_of!(SLOT_LIST) as *const u32) };
    let mut header = EMPTY_LARGE_HEADER;
    header.extent_count = 0;
    header.total_size = size as u64;

    let mut data_index = 1usize;
    let mut logical_offset = 0u64;
    while data_index < 1 + data_slot_count {
        let start_slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(data_index)) };
        let mut slot_count = 1u32;
        while data_index + (slot_count as usize) < 1 + data_slot_count {
            let expected = start_slot + slot_count;
            let next_slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(data_index + slot_count as usize)) };
            if next_slot != expected {
                break;
            }
            slot_count += 1;
        }
        let chunk_slots = slot_count as usize;
        let remaining = size as u64 - logical_offset;
        let byte_length = core::cmp::min(remaining, slot_count as u64 * SLOT_BYTES);
        if header.extent_count as usize >= LARGE_MAX_EXTENTS {
            return false;
        }
        let extent_index = header.extent_count as usize;
        unsafe { write_extent(&mut header, extent_index, LargeExtent {
            start_slot,
            slot_count,
            logical_offset,
            byte_length,
        }); }
        header.extent_count += 1;
        logical_offset += byte_length;
        data_index += chunk_slots;
    }
    if logical_offset != size as u64 {
        return false;
    }

    unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
    unsafe {
        copy_nonoverlapping(
            addr_of!(header) as *const u8,
            addr_of_mut!(SECTOR_BUFFER) as *mut u8,
            size_of::<LargeObjectHeader>(),
        );
    }
    if !unsafe { ata_write_sector(object_slot_lba(header_slot as usize), addr_of!(SECTOR_BUFFER) as *const u8) } {
        return false;
    }

    let mut written = 0usize;
    let mut extent_index = 0usize;
    while extent_index < header.extent_count as usize {
        let extent = unsafe { read_extent(&header, extent_index) };
        let mut slot_offset = 0u32;
        while slot_offset < extent.slot_count {
            let remaining = size - written;
            let chunk = core::cmp::min(remaining, SLOT_BYTES_USIZE);
            unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
            unsafe {
                copy_nonoverlapping(payload.add(written), addr_of_mut!(SECTOR_BUFFER) as *mut u8, chunk);
            }
            if !unsafe { ata_write_sector(object_slot_lba((extent.start_slot + slot_offset) as usize), addr_of!(SECTOR_BUFFER) as *const u8) } {
                return false;
            }
            written += chunk;
            slot_offset += 1;
            if written == size {
                break;
            }
        }
        extent_index += 1;
    }

    unsafe {
        (*object_ptr(index)).slot_index = header_slot;
        (*object_ptr(index)).size = size as u64;
        (*object_ptr(index)).flags |= OBJECT_FLAG_LARGE;
        (*object_ptr(index)).modified_at = SUPERBLOCK.next_nonce;
        SUPERBLOCK.next_nonce = SUPERBLOCK.next_nonce.wrapping_add(1);
    }
    true
}

unsafe fn write_object_bytes(index: usize, payload: *const u8, size: usize) -> bool {
    if size <= SLOT_BYTES_USIZE {
        unsafe { write_small_object(index, payload, size) }
    } else {
        unsafe { write_large_object(index, payload, size) }
    }
}

unsafe fn prepare_staged_record(index: usize, new_size: usize, new_record: &mut ObjectRecord, header_out: &mut LargeObjectHeader) -> bool {
    let old_record = unsafe { *object_ptr(index) };
    unsafe { rebuild_slot_bitmap(None); }
    if new_size <= SLOT_BYTES_USIZE {
        if !unsafe { allocate_slot_list(1, addr_of_mut!(SLOT_LIST) as *mut u32) } {
            return false;
        }
        *new_record = old_record;
        new_record.slot_index = unsafe { read_volatile(addr_of!(SLOT_LIST) as *const u32) };
        new_record.size = new_size as u64;
        new_record.flags &= !OBJECT_FLAG_LARGE;
        new_record.modified_at = unsafe { SUPERBLOCK.next_nonce };
        *header_out = EMPTY_LARGE_HEADER;
        return true;
    }

    let data_slot_count = slots_for_bytes(new_size as u64);
    if !unsafe { allocate_slot_list(1 + data_slot_count, addr_of_mut!(SLOT_LIST) as *mut u32) } {
        return false;
    }

    let header_slot = unsafe { read_volatile(addr_of!(SLOT_LIST) as *const u32) };
    let mut header = EMPTY_LARGE_HEADER;
    header.total_size = new_size as u64;
    let mut data_index = 1usize;
    let mut logical_offset = 0u64;
    while data_index < 1 + data_slot_count {
        let start_slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(data_index)) };
        let mut slot_count = 1u32;
        while data_index + (slot_count as usize) < 1 + data_slot_count {
            let expected = start_slot + slot_count;
            let next_slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(data_index + slot_count as usize)) };
            if next_slot != expected {
                break;
            }
            slot_count += 1;
        }
        let extent_index = header.extent_count as usize;
        if extent_index >= LARGE_MAX_EXTENTS {
            return false;
        }
        let byte_length = core::cmp::min(new_size as u64 - logical_offset, slot_count as u64 * SLOT_BYTES);
        unsafe { write_extent(&mut header, extent_index, LargeExtent {
            start_slot,
            slot_count,
            logical_offset,
            byte_length,
        }); }
        header.extent_count += 1;
        logical_offset += byte_length;
        data_index += slot_count as usize;
    }

    *new_record = old_record;
    new_record.slot_index = header_slot;
    new_record.size = new_size as u64;
    new_record.flags |= OBJECT_FLAG_LARGE;
    new_record.modified_at = unsafe { SUPERBLOCK.next_nonce };
    *header_out = header;
    true
}

unsafe fn write_staged_payload(
    old_record: &ObjectRecord,
    new_record: &ObjectRecord,
    header: &LargeObjectHeader,
    patch_offset: u64,
    patch_addr: *const u8,
    patch_size: usize,
) -> bool {
    if !is_large_object(new_record) {
        unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
        if old_record.size != 0 {
            let copy_len = core::cmp::min(old_record.size as usize, SLOT_BYTES_USIZE);
            if !unsafe { read_record_range(old_record, 0, addr_of_mut!(SECTOR_BUFFER) as *mut u8, copy_len) } {
                return false;
            }
        }
        if patch_size != 0 {
            unsafe {
                copy_nonoverlapping(
                    patch_addr,
                    (addr_of_mut!(SECTOR_BUFFER) as *mut u8).add(patch_offset as usize),
                    patch_size,
                );
            }
        }
        return unsafe { ata_write_sector(object_slot_lba(new_record.slot_index as usize), addr_of!(SECTOR_BUFFER) as *const u8) };
    }

    unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
    unsafe {
        copy_nonoverlapping(
            header as *const LargeObjectHeader as *const u8,
            addr_of_mut!(SECTOR_BUFFER) as *mut u8,
            size_of::<LargeObjectHeader>(),
        );
    }
    if !unsafe { ata_write_sector(object_slot_lba(new_record.slot_index as usize), addr_of!(SECTOR_BUFFER) as *const u8) } {
        return false;
    }

    let mut written = 0usize;
    while written < new_record.size as usize {
        let logical_base = written as u64;
        unsafe { write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512); }
        if logical_base < old_record.size {
            let copy_len = core::cmp::min((old_record.size - logical_base) as usize, SLOT_BYTES_USIZE);
            if !unsafe { read_record_range(old_record, logical_base, addr_of_mut!(SECTOR_BUFFER) as *mut u8, copy_len) } {
                return false;
            }
        }
        let write_start = patch_offset;
        let write_end = patch_offset + patch_size as u64;
        let chunk_start = logical_base;
        let chunk_end = logical_base + SLOT_BYTES;
        if write_end > chunk_start && write_start < chunk_end {
            let overlap_start = core::cmp::max(write_start, chunk_start);
            let overlap_end = core::cmp::min(write_end, chunk_end);
            let dst_offset = (overlap_start - chunk_start) as usize;
            let src_offset = (overlap_start - write_start) as usize;
            let overlap = (overlap_end - overlap_start) as usize;
            unsafe {
                copy_nonoverlapping(
                    patch_addr.add(src_offset),
                    (addr_of_mut!(SECTOR_BUFFER) as *mut u8).add(dst_offset),
                    overlap,
                );
            }
        }

        let mut physical_slot = 0u32;
        let mut in_slot = 0usize;
        if !unsafe { map_logical_slot(new_record, logical_base, &mut physical_slot, &mut in_slot) } || in_slot != 0 {
            return false;
        }
        if !unsafe { ata_write_sector(object_slot_lba(physical_slot as usize), addr_of!(SECTOR_BUFFER) as *const u8) } {
            return false;
        }
        written += SLOT_BYTES_USIZE;
    }
    true
}

unsafe fn zero_object_slots(record: &ObjectRecord) -> bool {
    let count = unsafe { collect_object_slots(record, addr_of_mut!(SLOT_LIST) as *mut u32, OBJECT_CAPACITY) };
    let Some(count) = count else {
        return false;
    };
    let mut index = 0usize;
    while index < count {
        let slot = unsafe { read_volatile((addr_of!(SLOT_LIST) as *const u32).add(index)) };
        if !unsafe { zero_slot(slot) } {
            return false;
        }
        index += 1;
    }
    true
}

unsafe fn zero_staged_object_slots(record: &ObjectRecord, has_aux: u32) -> bool {
    if record.live != 1 {
        return true;
    }
    if !is_large_object(record) || has_aux == 0 {
        return unsafe { zero_object_slots(record) };
    }

    let mut header = EMPTY_LARGE_HEADER;
    if !unsafe { load_txn_aux(&mut header) } || !large_header_valid(&header, record.size) {
        return false;
    }
    if !unsafe { zero_slot(record.slot_index) } {
        return false;
    }
    let mut extent_index = 0usize;
    while extent_index < header.extent_count as usize {
        let extent = unsafe { read_extent(&header, extent_index) };
        let mut slot_offset = 0u32;
        while slot_offset < extent.slot_count {
            if !unsafe { zero_slot(extent.start_slot + slot_offset) } {
                return false;
            }
            slot_offset += 1;
        }
        extent_index += 1;
    }
    true
}

unsafe fn create_set(index: usize) -> bool {
    let buffer = unsafe { &mut *addr_of_mut!(SET_BUFFER) };
    unsafe {
        let header = EMPTY_SET_HEADER;
        copy_nonoverlapping(
            addr_of!(header) as *const u8,
            buffer.as_mut_ptr(),
            size_of::<SetHeader>(),
        );
        write_object_bytes(index, buffer.as_ptr(), size_of::<SetHeader>())
    }
}

unsafe fn parse_set(index: usize, out_refs: *mut ObjectRef, capacity: usize) -> Option<usize> {
    let record = unsafe { *object_ptr(index) };
    if record.live != 1 || record.object_type != OBJECT_TYPE_SET {
        return None;
    }
    let buffer = unsafe { &mut *addr_of_mut!(SET_BUFFER) };
    let size = unsafe { read_object_bytes(index, buffer.as_mut_ptr(), SET_BUFFER_BYTES) }?;
    if size < size_of::<SetHeader>() {
        return None;
    }
    let header = unsafe { *(buffer.as_ptr() as *const SetHeader) };
    if header.magic != SET_MAGIC || header.version != SET_VERSION {
        return None;
    }
    let max_members = (size - size_of::<SetHeader>()) / size_of::<ObjectRef>();
    if header.member_count as usize > max_members || header.member_count as usize > capacity {
        return None;
    }
    let members_ptr = unsafe { buffer.as_ptr().add(size_of::<SetHeader>()) as *const ObjectRef };
    let mut index_out = 0usize;
    while index_out < header.member_count as usize {
        unsafe {
            write_volatile(out_refs.add(index_out), read_volatile(members_ptr.add(index_out)));
        }
        index_out += 1;
    }
    Some(header.member_count as usize)
}

unsafe fn list_members(index: usize, out_refs: *mut ObjectRef, capacity: usize) -> Option<usize> {
    unsafe { parse_set(index, out_refs, capacity) }
}

#[allow(dead_code)]
unsafe fn add_member(index: usize, member: ObjectRef) -> bool {
    let record = unsafe { *object_ptr(index) };
    if record.live != 1 || record.object_type != OBJECT_TYPE_SET {
        let _ = device_write(b"[DATA] set add record invalid\r\n");
        return false;
    }
    let buffer = unsafe { &mut *addr_of_mut!(SET_BUFFER) };
    let size = unsafe { read_object_bytes(index, buffer.as_mut_ptr(), SET_BUFFER_BYTES) };
    let Some(size) = size else {
        let _ = device_write(b"[DATA] set add draw fail\r\n");
        return false;
    };
    if size < size_of::<SetHeader>() {
        let _ = device_write(b"[DATA] set add size fail\r\n");
        return false;
    }
    let header = unsafe { &mut *(buffer.as_mut_ptr() as *mut SetHeader) };
    if header.magic != SET_MAGIC || header.version != SET_VERSION {
        let _ = device_write(b"[DATA] set add header fail\r\n");
        return false;
    }
    let members_ptr = unsafe { buffer.as_mut_ptr().add(size_of::<SetHeader>()) as *mut ObjectRef };
    let mut cursor = 0usize;
    while cursor < header.member_count as usize {
        let existing = unsafe { read_volatile(members_ptr.add(cursor)) };
        if existing.low == member.low && existing.high == member.high {
            return true;
        }
        cursor += 1;
    }
    let next_size = size + size_of::<ObjectRef>();
    if next_size > MAX_OBJECT_BYTES {
        let _ = device_write(b"[DATA] set add range fail\r\n");
        return false;
    }
    let _ = device_write(b"[DATA] set add stage append\r\n");
    unsafe {
        write_volatile(members_ptr.add(header.member_count as usize), member);
    }
    header.member_count = header.member_count.wrapping_add(1);
    unsafe {
        let previous_record = *object_ptr(index);
        let previous_nonce = SUPERBLOCK.next_nonce;
        let _ = device_write(b"[DATA] set add begin\r\n");
        device_write_hex(b"[DATA] set add index=", index as u64);
        device_write_hex(b"[DATA] set add size=", size as u64);
        device_write_hex(b"[DATA] set add next=", next_size as u64);
        device_write_hex(b"[DATA] set add members=", header.member_count as u64);
        if !begin_mutation() {
            let _ = device_write(b"[DATA] set add begin fail\r\n");
            return false;
        }
        let _ = device_write(b"[DATA] set add mutation ok\r\n");
        let _ = device_write(b"[DATA] set add write start\r\n");
        if !write_object_bytes(index, buffer.as_ptr(), next_size) {
            let _ = device_write(b"[DATA] set add write fail\r\n");
            let _ = device_write(b"[DATA] set add rollback start\r\n");
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            let _ = device_write(b"[DATA] set add rollback done\r\n");
            return false;
        }
        let _ = device_write(b"[DATA] set add write ok\r\n");
        let _ = device_write(b"[DATA] set add commit start\r\n");
        if !commit_metadata(STORE_STATE_CLEAN) {
            let _ = device_write(b"[DATA] set add commit fail\r\n");
            let _ = device_write(b"[DATA] set add rollback start\r\n");
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            let _ = device_write(b"[DATA] set add rollback done\r\n");
            return false;
        }
        let _ = device_write(b"[DATA] set add ok\r\n");
        true
    }
}

#[allow(dead_code)]
unsafe fn remove_member(index: usize, member: ObjectRef) -> bool {
    let record = unsafe { *object_ptr(index) };
    if record.live != 1 || record.object_type != OBJECT_TYPE_SET {
        return false;
    }
    let buffer = unsafe { &mut *addr_of_mut!(SET_BUFFER) };
    let size = unsafe { read_object_bytes(index, buffer.as_mut_ptr(), SET_BUFFER_BYTES) };
    let Some(size) = size else {
        return false;
    };
    if size < size_of::<SetHeader>() {
        return false;
    }
    let header = unsafe { &mut *(buffer.as_mut_ptr() as *mut SetHeader) };
    if header.magic != SET_MAGIC || header.version != SET_VERSION {
        return false;
    }
    let members_ptr = unsafe { buffer.as_mut_ptr().add(size_of::<SetHeader>()) as *mut ObjectRef };
    let mut found = None;
    let mut cursor = 0usize;
    while cursor < header.member_count as usize {
        let existing = unsafe { read_volatile(members_ptr.add(cursor)) };
        if existing.low == member.low && existing.high == member.high {
            found = Some(cursor);
            break;
        }
        cursor += 1;
    }
    let Some(found) = found else {
        return true;
    };
    cursor = found;
    while cursor + 1 < header.member_count as usize {
        let next = unsafe { read_volatile(members_ptr.add(cursor + 1)) };
        unsafe { write_volatile(members_ptr.add(cursor), next); }
        cursor += 1;
    }
    header.member_count -= 1;
    unsafe {
        let previous_record = *object_ptr(index);
        let previous_nonce = SUPERBLOCK.next_nonce;
        if !begin_mutation() {
            return false;
        }
        if !write_object_bytes(index, buffer.as_ptr(), size - size_of::<ObjectRef>()) {
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            return false;
        }
        if !commit_metadata(STORE_STATE_CLEAN) {
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            return false;
        }
        true
    }
}

unsafe fn seed_object(gate: &mut LunaDataGate) {
    gate.result_count = 0x5100;
    gate.object_low = unsafe { DEVICE_WRITE_LOW };
    gate.object_high = unsafe { DEVICE_WRITE_HIGH };
    if !validate_cap(CAP_DATA_SEED, gate.cid_low, gate.cid_high, OP_SEED) {
        let _ = device_write(b"[DATA] seed invalid cap\r\n");
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if !authorize_mutation(GOVERN_WRITE, CAP_DATA_SEED, gate, gate.object_type, gate.object_flags, 0) {
        return;
    }
    gate.result_count = 0x5101;

    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        if unsafe { (*object_ptr(index)).live } == 0 {
            let nonce = unsafe { SUPERBLOCK.next_nonce };
            let stamp = (unsafe { SUPERBLOCK.mount_count } << 32) | nonce;
            gate.content_size = index as u64;
            device_write_hex(b"[DATA] seed type=", gate.object_type as u64);
            device_write_hex(b"[DATA] seed index=", index as u64);
            unsafe {
                let previous_record = *object_ptr(index);
                let previous_nonce = SUPERBLOCK.next_nonce;
                rebuild_slot_bitmap(None);
                gate.result_count = 0x5102;
                if !allocate_slot_list(1, addr_of_mut!(SLOT_LIST) as *mut u32) {
                    gate.status = STATUS_NO_SPACE;
                    return;
                }
                let slot = read_volatile(addr_of!(SLOT_LIST) as *const u32);
                gate.created_at = slot as u64;
                device_write_hex(b"[DATA] seed slot=", slot as u64);
                gate.result_count = 0x5103;
                if !begin_mutation() {
                    gate.result_count = LAST_MUTATION_DEBUG as u32;
                    let _ = device_write(b"[DATA] seed begin fail\r\n");
                    gate.status = STATUS_IO;
                    return;
                }
                object_ptr(index).write(ObjectRecord {
                    live: 1,
                    object_type: gate.object_type,
                    flags: gate.object_flags,
                    slot_index: slot,
                    size: 0,
                    created_at: stamp,
                    modified_at: stamp,
                    object_low: 0x4C41_4653_0000_0000u64 | index as u64,
                    object_high: nonce,
                    reserved: [0; 4],
                });
                let _ = device_write(b"[DATA] seed record write\r\n");
                SUPERBLOCK.next_nonce = nonce.wrapping_add(1);
                let ok = if gate.object_type == OBJECT_TYPE_SET {
                    create_set(index)
                } else {
                    gate.result_count = 0x5104;
                    write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
                    ata_write_sector(object_slot_lba(slot as usize), addr_of!(SECTOR_BUFFER) as *const u8)
                };
                if !ok {
                    gate.result_count = 0x51E4;
                    let _ = device_write(b"[DATA] seed slot fail\r\n");
                    if !rollback_seed_state(index, previous_record, previous_nonce) {
                        let _ = device_write(b"[DATA] seed rollback fail\r\n");
                    } else {
                        let _ = device_write(b"[DATA] seed rollback ok\r\n");
                    }
                    gate.status = STATUS_IO;
                    return;
                }
                let _ = device_write(b"[DATA] seed slot ok\r\n");
                gate.result_count = 0x5105;
                if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_SEED, gate, gate.object_type, gate.object_flags, gate.authority_space) || !commit_metadata(STORE_STATE_CLEAN) {
                    gate.result_count = 0x51E5;
                    let _ = device_write(b"[DATA] seed commit fail\r\n");
                    if !rollback_seed_state(index, previous_record, previous_nonce) {
                        let _ = device_write(b"[DATA] seed rollback fail\r\n");
                    } else {
                        let _ = device_write(b"[DATA] seed rollback ok\r\n");
                    }
                    gate.status = STATUS_IO;
                    return;
                }
                let _ = device_write(b"[DATA] seed commit ok\r\n");
                gate.object_low = (*object_ptr(index)).object_low;
                gate.object_high = (*object_ptr(index)).object_high;
                gate.created_at = (*object_ptr(index)).created_at;
                gate.content_size = 0;
                gate.result_count = 0x5106;
                gate.status = STATUS_OK;
                let _ = device_write(b"[DATA] seed ok\r\n");
            }
            return;
        }
        index += 1;
    }
    let _ = device_write(b"[DATA] seed no-space\r\n");
    gate.status = STATUS_NO_SPACE;
}

unsafe fn pour_span(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_POUR, gate.cid_low, gate.cid_high, OP_POUR) {
        let _ = device_write(b"[DATA] pour invalid cap\r\n");
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        let _ = device_write(b"[DATA] pour not found\r\n");
        gate.status = STATUS_NOT_FOUND;
        return;
    };

    let record = unsafe { *object_ptr(index) };
    if !authorize_mutation(GOVERN_WRITE, CAP_DATA_POUR, gate, record.object_type, record.flags, 0) {
        return;
    }
    let end = gate.offset.saturating_add(gate.size);
    if end > MAX_OBJECT_BYTES as u64 || gate.size > gate.buffer_size {
        let _ = device_write(b"[DATA] pour range fail\r\n");
        device_write_hex(b"[DATA] pour index=", index as u64);
        device_write_hex(b"[DATA] pour size=", gate.size);
        device_write_hex(b"[DATA] pour buffer=", gate.buffer_size);
        device_write_hex(b"[DATA] pour end=", end);
        gate.status = STATUS_RANGE;
        return;
    }
    let _ = device_write(b"[DATA] pour resolve ok\r\n");
    device_write_hex(b"[DATA] pour index=", index as u64);
    device_write_hex(b"[DATA] pour slot=", record.slot_index as u64);
    device_write_hex(b"[DATA] pour size=", record.size);
    device_write_hex(b"[DATA] pour write=", gate.size);
    device_write_hex(b"[DATA] pour lba=", object_slot_lba(record.slot_index as usize) as u64);

    if record.size == 0 && gate.offset == 0 {
        unsafe {
            let _ = device_write(b"[DATA] pour seed begin\r\n");
            let previous_record = *object_ptr(index);
            let previous_nonce = SUPERBLOCK.next_nonce;
            if !begin_mutation() {
                let _ = device_write(b"[DATA] pour seed begin fail\r\n");
                gate.status = STATUS_IO;
                return;
            }
            if !write_object_bytes(index, gate.buffer_addr as *const u8, gate.size as usize) {
                let _ = device_write(b"[DATA] pour seed write fail\r\n");
                let _ = rollback_seed_state(index, previous_record, previous_nonce);
                gate.status = STATUS_IO;
                return;
            }
            if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_POUR, gate, record.object_type, record.flags, gate.authority_space) || !commit_metadata(STORE_STATE_CLEAN) {
                let _ = device_write(b"[DATA] pour seed commit fail\r\n");
                let _ = rollback_seed_state(index, previous_record, previous_nonce);
                gate.status = STATUS_IO;
                return;
            }
            let _ = device_write(b"[DATA] pour seed ok\r\n");
            gate.object_type = (*object_ptr(index)).object_type;
            gate.object_flags = (*object_ptr(index)).flags;
            gate.content_size = (*object_ptr(index)).size;
        }
        gate.status = STATUS_OK;
        return;
    }

    unsafe {
        let new_size = core::cmp::max(record.size, end) as usize;
        let mut new_record = EMPTY_RECORD;
        let mut header = EMPTY_LARGE_HEADER;
        let mut txn = EMPTY_TXN;
        let previous_record = *object_ptr(index);
        let previous_nonce = SUPERBLOCK.next_nonce;

        let _ = device_write(b"[DATA] pour txn begin\r\n");
        if !begin_mutation() {
            let _ = device_write(b"[DATA] pour begin fail\r\n");
            gate.status = STATUS_IO;
            return;
        }
        if !prepare_staged_record(index, new_size, &mut new_record, &mut header) {
            let _ = device_write(b"[DATA] pour prep fail\r\n");
            let _ = abort_mutation();
            gate.status = STATUS_NO_SPACE;
            return;
        }
        let _ = device_write(b"[DATA] pour prep ok\r\n");

        txn.magic = TXN_MAGIC;
        txn.version = TXN_VERSION;
        txn.state = TXN_STATE_PREPARED;
        txn.kind = txn_kind_for_object_type(record.object_type);
        txn.object_index = index as u32;
        txn.has_aux = if is_large_object(&new_record) { 1 } else { 0 };
        txn.writer_space = gate.writer_space;
        txn.authority_space = gate.authority_space;
        txn.volume_state = gate.volume_state;
        txn.volume_mode = gate.volume_mode;
        txn.object_low = record.object_low;
        txn.object_high = record.object_high;
        txn.old_record = record;
        txn.new_record = new_record;

        if (txn.has_aux != 0 && !save_txn_aux(&header)) || !save_txn_record(&txn) {
            let _ = device_write(b"[DATA] pour txn save fail\r\n");
            let _ = clear_txn_aux();
            let _ = clear_txn_record();
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            gate.status = STATUS_IO;
            return;
        }
        let _ = device_write(b"[DATA] pour txn save ok\r\n");
        if !write_staged_payload(&record, &new_record, &header, gate.offset, gate.buffer_addr as *const u8, gate.size as usize) {
            let _ = device_write(b"[DATA] pour payload fail\r\n");
            let _ = zero_staged_object_slots(&new_record, txn.has_aux);
            let _ = clear_txn_aux();
            let _ = clear_txn_record();
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            gate.status = STATUS_IO;
            return;
        }
        let _ = device_write(b"[DATA] pour payload ok\r\n");

        if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_POUR, gate, new_record.object_type, new_record.flags, txn.authority_space) {
            let _ = device_write(b"[DATA] pour govern commit deny\r\n");
            let _ = zero_staged_object_slots(&new_record, txn.has_aux);
            let _ = clear_txn_aux();
            let _ = clear_txn_record();
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            return;
        }
        txn.writer_space = gate.writer_space;
        txn.authority_space = gate.authority_space;
        txn.volume_state = gate.volume_state;
        txn.volume_mode = gate.volume_mode;
        txn.state = TXN_STATE_COMMITTED;
        if !save_txn_record(&txn) {
            let _ = device_write(b"[DATA] pour txn commit fail\r\n");
            let _ = zero_staged_object_slots(&new_record, txn.has_aux);
            let _ = clear_txn_aux();
            let _ = clear_txn_record();
            let _ = rollback_seed_state(index, previous_record, previous_nonce);
            gate.status = STATUS_IO;
            return;
        }
        let _ = device_write(b"[DATA] pour txn commit ok\r\n");
        if !finish_applied_transaction(index, &txn) {
            let _ = device_write(b"[DATA] pour apply fail\r\n");
            gate.status = STATUS_IO;
            return;
        }
        let _ = device_write(b"[DATA] pour apply ok\r\n");
        gate.object_type = new_record.object_type;
        gate.object_flags = new_record.flags;
        gate.content_size = new_record.size;
    }
    gate.status = STATUS_OK;
}

unsafe fn draw_span(gate: &mut LunaDataGate) {
    unsafe {
        LAST_DRAW_DEBUG = 0xD300;
        LAST_DRAW_VALUE0 = 0;
        LAST_DRAW_VALUE1 = 0;
    }
    if !validate_cap(CAP_DATA_DRAW, gate.cid_low, gate.cid_high, OP_DRAW) {
        let _ = device_write(b"[DATA] draw invalid cap\r\n");
        gate.result_count = 0xD301;
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        let _ = device_write(b"[DATA] draw not found\r\n");
        gate.result_count = 0xD302;
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    let record = unsafe { *object_ptr(index) };
    let size = record.size;
    device_write_hex(b"[DATA] draw index=", index as u64);
    device_write_hex(b"[DATA] draw live=", record.live as u64);
    device_write_hex(b"[DATA] draw type=", record.object_type as u64);
    device_write_hex(b"[DATA] draw slot=", record.slot_index as u64);
    device_write_hex(b"[DATA] draw size=", record.size);
    gate.object_type = record.object_type;
    gate.object_flags = record.flags;
    gate.created_at = record.slot_index as u64;
    gate.content_size = record.size;
    if gate.offset > size {
        let _ = device_write(b"[DATA] draw range fail\r\n");
        gate.result_count = 0xD303;
        gate.status = STATUS_RANGE;
        return;
    }
    let amount = core::cmp::min(core::cmp::min(size - gate.offset, gate.size), gate.buffer_size);

    unsafe {
        if !read_object_range(index, gate.offset, gate.buffer_addr as *mut u8, amount as usize) {
            gate.result_count = LAST_DRAW_DEBUG as u32;
            gate.created_at = LAST_DRAW_VALUE0;
            gate.content_size = LAST_DRAW_VALUE1;
            gate.status = STATUS_IO;
            return;
        }
        gate.object_type = (*object_ptr(index)).object_type;
        gate.object_flags = (*object_ptr(index)).flags;
        gate.size = amount;
        gate.created_at = (*object_ptr(index)).created_at;
        gate.content_size = (*object_ptr(index)).size;
    }
    gate.result_count = 0xD3FF;
    gate.status = STATUS_OK;
}

unsafe fn shred_object(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_SHRED, gate.cid_low, gate.cid_high, OP_SHRED) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };

    if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_SHRED, gate, unsafe { (*object_ptr(index)).object_type }, unsafe { (*object_ptr(index)).flags }, 0) {
        return;
    }
    unsafe {
        let record = *object_ptr(index);
        if !begin_mutation() {
            gate.status = STATUS_IO;
            return;
        }
        object_ptr(index).write(EMPTY_RECORD);
        if !zero_object_slots(&record)
            || !commit_metadata(STORE_STATE_CLEAN)
        {
            gate.status = STATUS_IO;
            return;
        }
    }
    gate.status = STATUS_OK;
}

unsafe fn gather_set(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_GATHER, gate.cid_low, gate.cid_high, OP_GATHER) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let out = gate.buffer_addr as *mut ObjectRef;
    let capacity = (gate.buffer_size as usize) / size_of::<ObjectRef>();

    if gate.object_low != 0 || gate.object_high != 0 {
        let Some(index) = find_index(gate.object_low, gate.object_high) else {
            gate.status = STATUS_NOT_FOUND;
            return;
        };
        let Some(count) = (unsafe { list_members(index, out, capacity) }) else {
            gate.status = STATUS_METADATA;
            return;
        };
        gate.result_count = count as u32;
        gate.status = STATUS_OK;
        return;
    }

    let mut count = 0usize;
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        let record = unsafe { *object_ptr(index) };
        if record.live == 1 {
            if count >= capacity {
                gate.status = STATUS_NO_SPACE;
                return;
            }
            unsafe {
                write_volatile(out.add(count), ObjectRef { low: record.object_low, high: record.object_high });
            }
            count += 1;
        }
        index += 1;
    }
    gate.result_count = count as u32;
    gate.status = STATUS_OK;
}

unsafe fn set_add_member_gate(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_POUR, gate.cid_low, gate.cid_high, OP_SET_ADD) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_POUR, gate, OBJECT_TYPE_SET, 0, 0) {
        return;
    }
    if gate.buffer_size < size_of::<ObjectRef>() as u64 {
        gate.status = STATUS_RANGE;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    let member = unsafe { *(gate.buffer_addr as *const ObjectRef) };
    if member.low == 0 || member.high == 0 {
        gate.status = STATUS_RANGE;
        return;
    }

    unsafe {
        if add_member(index, member) {
            gate.status = STATUS_OK;
        } else {
            gate.status = STATUS_METADATA;
        }
    }
}

unsafe fn set_remove_member_gate(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_POUR, gate.cid_low, gate.cid_high, OP_SET_REMOVE) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if !authorize_mutation(GOVERN_COMMIT, CAP_DATA_POUR, gate, OBJECT_TYPE_SET, 0, 0) {
        return;
    }
    if gate.buffer_size < size_of::<ObjectRef>() as u64 {
        gate.status = STATUS_RANGE;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    let member = unsafe { *(gate.buffer_addr as *const ObjectRef) };
    if member.low == 0 || member.high == 0 {
        gate.status = STATUS_RANGE;
        return;
    }

    unsafe {
        if remove_member(index, member) {
            gate.status = STATUS_OK;
        } else {
            gate.status = STATUS_METADATA;
        }
    }
}

unsafe fn stat_store(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_GATHER, gate.cid_low, gate.cid_high, OP_STAT) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let mut live_count = 0u32;
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        if unsafe { (*object_ptr(index)).live } == 1 {
            live_count = live_count.wrapping_add(1);
        }
        index += 1;
    }

    gate.result_count = live_count;
    gate.object_type = unsafe { SUPERBLOCK.version };
    gate.object_flags = if unsafe { SUPERBLOCK.reserved[0] } == STORE_STATE_DIRTY { 1 } else { 0 };
      gate.size = unsafe { SUPERBLOCK.next_nonce };
      gate.created_at = unsafe { SUPERBLOCK.format_count };
      gate.content_size = unsafe { SUPERBLOCK.mount_count };
      gate.object_low = unsafe { SUPERBLOCK.reserved[2] };
      gate.object_high = unsafe { SUPERBLOCK.reserved[3] };
      gate.volume_state = unsafe { SUPERBLOCK.reserved[4] as u32 };
      gate.volume_mode = unsafe { SUPERBLOCK.reserved[5] as u32 };
      gate.status = STATUS_OK;
  }

unsafe fn verify_store(gate: &mut LunaDataGate) {
    if !validate_cap(CAP_DATA_GATHER, gate.cid_low, gate.cid_high, OP_VERIFY) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let mut live_count = 0u32;
    let mut invalid_count = 0u32;
    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        let record = unsafe { *object_ptr(index) };
        if record.live == 1 {
            live_count = live_count.wrapping_add(1);
            if !valid_slot_index(record.slot_index)
                || unsafe { object_slot_count(&record) }.is_none()
                || (!is_large_object(&record) && record.size > SLOT_BYTES)
                || record.object_low == 0
                || record.object_high == 0
                || record.created_at == 0
                || record.modified_at == 0
            {
                invalid_count = invalid_count.wrapping_add(1);
            }
        }
        index += 1;
    }

    gate.result_count = live_count;
    gate.object_type = unsafe { SUPERBLOCK.version };
    gate.object_flags = 0;
    if unsafe { SUPERBLOCK.reserved[0] } == STORE_STATE_DIRTY {
        gate.object_flags |= VERIFY_FLAG_DIRTY;
    }
    if !unsafe { metadata_matches_layout() } {
        gate.object_flags |= VERIFY_FLAG_LAYOUT_MISMATCH;
    }
    if unsafe { SUPERBLOCK.reserved[1] } != unsafe { object_checksum() } {
        gate.object_flags |= VERIFY_FLAG_CHECKSUM_MISMATCH;
    }
    if invalid_count != 0 {
        gate.object_flags |= VERIFY_FLAG_INVALID_RECORDS;
    }
    gate.size = invalid_count as u64;
    gate.created_at = unsafe { SUPERBLOCK.format_count };
    gate.content_size = unsafe { SUPERBLOCK.mount_count };
    gate.volume_state = unsafe { SUPERBLOCK.reserved[4] as u32 };
    gate.volume_mode = unsafe { SUPERBLOCK.reserved[5] as u32 };
    gate.status = STATUS_OK;
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn data_entry_boot(_bootview: *const u8) {
    let read_ok = request_cap(CAP_DEVICE_READ, unsafe { &mut DEVICE_READ_LOW }, unsafe { &mut DEVICE_READ_HIGH });
    let write_ok = request_cap(CAP_DEVICE_WRITE, unsafe { &mut DEVICE_WRITE_LOW }, unsafe { &mut DEVICE_WRITE_HIGH });
    let ok = read_ok && write_ok && unsafe { load_store() };
    if ok {
        let _ = device_write(b"[DATA] lafs disk online\r\n");
    } else {
        let _ = device_write(b"[DATA] lafs disk fail\r\n");
    }
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn data_entry_gate(gate: *mut LunaDataGate) {
    let gate = unsafe { &mut *gate };
    gate.status = STATUS_METADATA;
    gate.result_count = 0;
    gate.content_size = 0;
    gate.created_at = 0;
    gate.writer_space = 0;
    gate.authority_space = 0;
    gate.volume_state = unsafe { SUPERBLOCK.reserved[4] as u32 };
    gate.volume_mode = unsafe { SUPERBLOCK.reserved[5] as u32 };

    match gate.opcode {
        OP_SEED => unsafe { seed_object(gate) },
        OP_POUR => unsafe { pour_span(gate) },
        OP_DRAW => unsafe { draw_span(gate) },
        OP_SHRED => unsafe { shred_object(gate) },
        OP_GATHER => unsafe { gather_set(gate) },
        OP_STAT => unsafe { stat_store(gate) },
        OP_VERIFY => unsafe { verify_store(gate) },
        OP_SET_ADD => unsafe { set_add_member_gate(gate) },
        OP_SET_REMOVE => unsafe { set_remove_member_gate(gate) },
        _ => gate.status = STATUS_METADATA,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn memcpy(dest: *mut u8, src: *const u8, len: usize) -> *mut u8 {
    let mut index = 0usize;
    while index < len {
        unsafe {
            write_volatile(dest.add(index), read_volatile(src.add(index)));
        }
        index += 1;
    }
    dest
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn memset(dest: *mut u8, value: i32, len: usize) -> *mut u8 {
    let mut index = 0usize;
    while index < len {
        unsafe {
            write_volatile(dest.add(index), value as u8);
        }
        index += 1;
    }
    dest
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    loop {
        unsafe { asm!("hlt", options(nomem, nostack, preserves_flags)); }
    }
}
