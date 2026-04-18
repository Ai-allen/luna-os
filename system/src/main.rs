#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::mem::size_of;
use core::panic::PanicInfo;
use core::ptr::{copy_nonoverlapping, write_bytes, write_volatile};

include!("../../include/luna_layout.rs");
include!("../../include/luna_proto.rs");
include!("../../include/luna_installer_efi_payload.rs");
include!("../../include/luna_installer_runtime_reset.rs");

const GATE_REQUEST_CAP: u32 = 1;
const GATE_VALIDATE_CAP: u32 = 6;
const CAP_LIFE_WAKE: u64 = 0xC901;
const CAP_LIFE_SPAWN: u64 = 0xC903;
const CAP_SYSTEM_REGISTER: u64 = 0xB101;
const CAP_SYSTEM_QUERY: u64 = 0xB102;
const CAP_SYSTEM_ALLOCATE: u64 = 0xB103;
const CAP_SYSTEM_EVENT: u64 = 0xB104;
const CAP_DEVICE_WRITE: u64 = 0xA503;
const CAP_DEVICE_READ: u64 = 0xA502;
const LIFE_WAKE: u32 = 1;
const LIFE_SPAWN: u32 = 4;
const SYSTEM_REGISTER: u32 = 1;
const SYSTEM_ALLOCATE: u32 = 3;
const SYSTEM_EVENT: u32 = 4;
const DEVICE_WRITE: u32 = 3;
const DEVICE_BLOCK_READ: u32 = 8;
const DEVICE_BLOCK_WRITE: u32 = 9;
const SPACE_BOOT: u32 = 0;
const SPACE_SYSTEM: u32 = 1;
const SPACE_DATA: u32 = 2;
const SPACE_SECURITY: u32 = 3;
const SPACE_GRAPHICS: u32 = 4;
const SPACE_DEVICE: u32 = 5;
const SPACE_NETWORK: u32 = 6;
const SPACE_USER: u32 = 7;
const SPACE_TIME: u32 = 8;
const SPACE_LIFECYCLE: u32 = 9;
const SPACE_OBSERVABILITY: u32 = 10;
const SPACE_AI: u32 = 11;
const SPACE_PROGRAM: u32 = 12;
const SPACE_PACKAGE: u32 = 13;
const SPACE_UPDATE: u32 = 14;
const UNIT_LIVE: u32 = 2;
const RESOURCE_MEMORY: u32 = 1;
const RESOURCE_TIME: u32 = 2;

const OP_REGISTER: u32 = 1;
const OP_QUERY: u32 = 2;
const OP_ALLOCATE: u32 = 3;
const OP_EVENT: u32 = 4;
const OP_QUERY_SPACES: u32 = 5;
const OP_QUERY_EVENTS: u32 = 6;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xB100;
const STATUS_NOT_FOUND: u32 = 0xB101;
const STATUS_NO_ROOM: u32 = 0xB102;

const STATE_ACTIVE: u32 = 1;
const STATE_BOOTING: u32 = 4;
const CAPACITY: usize = 16;
const EVENT_CAPACITY: usize = 32;

const EVENT_BOOT_LIVE: u64 = 0x424F4F544C495645;
const EVENT_SECURITY_READY: u64 = 0x5345435245414459;
const EVENT_LIFECYCLE_READY: u64 = 0x4C49464552454144;
const EVENT_SYSTEM_READY: u64 = 0x5359535245414459;
const EVENT_DEVICE_READY: u64 = 0x4445565245414459;
const EVENT_DATA_READY: u64 = 0x4C4146534449534B;
const EVENT_GRAPHICS_READY: u64 = 0x4756585245414459;
const EVENT_NETWORK_READY: u64 = 0x4E45545245414459;
const EVENT_TIME_READY: u64 = 0x54494D4552454144;
const EVENT_OBSERVE_READY: u64 = 0x4F42535245414459;
const EVENT_PROGRAM_READY: u64 = 0x50524F4752454144;
const EVENT_AI_READY: u64 = 0x4149524541445921;
const EVENT_PACKAGE_READY: u64 = 0x504B475245414459;
const EVENT_UPDATE_READY: u64 = 0x5550445245414459;
const EVENT_USER_BOOTING: u64 = 0x555352424F4F5449;
const EVENT_USER_READY: u64 = 0x5553455252454144;
const DISK_SECTOR_SIZE: usize = 512;
const DISK_TOTAL_SECTORS: u64 = 131072;
const ESP_START_LBA: u64 = 2048;
const ESP_SECTORS: u64 = 16384;
const ESP_RESERVED_SECTORS: u64 = 1;
const ESP_FAT_COUNT: u64 = 2;
const ESP_ROOT_ENTRIES: usize = 512;
const ESP_FAT_SECTORS: u64 = 64;
const ESP_SECTORS_PER_CLUSTER: u64 = 1;
const ESP_ROOT_DIR_SECTORS: u64 = 32;
const ESP_DATA_START_LBA: u64 = 161;
const ESP_STAGE_FILE_CLUSTER: u16 = 64;
const SYSTEM_SECTORS: u64 = 16384;
const DATA_SECTORS: u64 = 49152;
const RECOVERY_START_LBA: u64 = 83968;
const RECOVERY_SECTORS: u64 = 32768;
const DATA_META_SECTORS: u64 = 25;
const DATA_OBJECT_CAPACITY: u32 = 128;
const DATA_SLOT_SECTORS: u32 = 1;
const STORE_MAGIC: u32 = 0x5346_414C;
const STORE_VERSION: u32 = 3;
const STORE_STATE_CLEAN: u64 = 0x434C_4541_4E00_0001;
const GPT_ENTRY_COUNT: usize = 128;
const GPT_ENTRY_SIZE: usize = 128;
const GPT_ENTRIES_SECTORS: u64 = 32;
const INSTALLER_CAP_USES: u32 = 65535;

#[repr(C)]
pub struct LunaSystemGate {
    sequence: u32,
    opcode: u32,
    status: u32,
    space_id: u32,
    resource_type: u32,
    amount: u32,
    result_count: u32,
    cid_low: u64,
    cid_high: u64,
    ticket: u64,
    event_word: u64,
    buffer_addr: u64,
    buffer_size: u64,
    name: [u8; 16],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct LunaSystemRecord {
    space_id: u32,
    state: u32,
    resource_memory: u32,
    resource_time: u32,
    last_event: u64,
    name: [u8; 16],
}

#[repr(C)]
#[derive(Copy, Clone)]
struct LunaSystemEventRecord {
    sequence: u64,
    space_id: u32,
    state: u32,
    event_word: u64,
    name: [u8; 16],
}

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
}

#[repr(C)]
struct LunaLifecycleGate {
    sequence: u32,
    opcode: u32,
    status: u32,
    space_id: u32,
    state: u32,
    result_count: u32,
    cid_low: u64,
    cid_high: u64,
    epoch: u64,
    flags: u64,
    buffer_addr: u64,
    buffer_size: u64,
    entry_addr: u64,
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

#[repr(C)]
#[derive(Copy, Clone)]
struct InstallerStoreSuperblock {
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

const EMPTY_RECORD: LunaSystemRecord = LunaSystemRecord {
    space_id: 0,
    state: 0,
    resource_memory: 0,
    resource_time: 0,
    last_event: 0,
    name: [0; 16],
};

static mut RECORDS: [LunaSystemRecord; CAPACITY] = [EMPTY_RECORD; CAPACITY];
static mut EVENT_LOG: [LunaSystemEventRecord; EVENT_CAPACITY] = [LunaSystemEventRecord {
    sequence: 0,
    space_id: 0,
    state: 0,
    event_word: 0,
    name: [0; 16],
}; EVENT_CAPACITY];
static mut EVENT_HEAD: usize = 0;
static mut EVENT_COUNT: usize = 0;
static mut NEXT_EVENT_SEQUENCE: u64 = 1;
static mut NEXT_TICKET: u64 = 1;
static mut LIFECYCLE_GATE_ENTRY_ADDR: u64 = 0;
static mut DEVICE_WRITE_LOW: u64 = 0;
static mut DEVICE_WRITE_HIGH: u64 = 0;
static mut DEVICE_LOG_READY: bool = false;

unsafe fn record_ptr(index: usize) -> *mut LunaSystemRecord {
    unsafe { (core::ptr::addr_of_mut!(RECORDS) as *mut LunaSystemRecord).add(index) }
}

unsafe fn event_ptr(index: usize) -> *mut LunaSystemEventRecord {
    unsafe { (core::ptr::addr_of_mut!(EVENT_LOG) as *mut LunaSystemEventRecord).add(index) }
}

#[inline(always)]
unsafe fn outb(port: u16, value: u8) {
    unsafe {
        asm!("out dx, al", in("dx") port, in("al") value, options(nomem, nostack, preserves_flags));
    }
}

#[inline(always)]
unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    unsafe {
        asm!("in al, dx", in("dx") port, out("al") value, options(nomem, nostack, preserves_flags));
    }
    value
}

fn serial_putc(value: u8) {
    while unsafe { inb(0x3FD) } & 0x20 == 0 {}
    unsafe { outb(0x3F8, value) };
}

fn serial_write(text: &[u8]) {
    for byte in text {
        serial_putc(*byte);
    }
}

fn device_write(text: &[u8], bootview: *const LunaBootView) -> bool {
    if bootview.is_null() {
        return false;
    }
    let bootview = unsafe { &*bootview };
    let gate = unsafe { &mut *(bootview.device_gate_base as *mut LunaDeviceGate) };
    unsafe {
        write_bytes(gate as *mut LunaDeviceGate as *mut u8, 0, core::mem::size_of::<LunaDeviceGate>());
    }
    gate.sequence = 900;
    gate.opcode = DEVICE_WRITE;
    gate.device_id = 1;
    gate.cid_low = unsafe { DEVICE_WRITE_LOW };
    gate.cid_high = unsafe { DEVICE_WRITE_HIGH };
    gate.size = text.len() as u64;
    gate.buffer_addr = text.as_ptr() as u64;
    gate.buffer_size = text.len() as u64;
    let entry: extern "sysv64" fn(*mut LunaDeviceGate) =
        unsafe { core::mem::transmute((*(LUNA_MANIFEST_ADDR as *const LunaManifest)).device_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaDeviceGate);
    gate.status == STATUS_OK
}

fn log_write(text: &[u8], bootview: *const LunaBootView) {
    if unsafe { DEVICE_LOG_READY } && device_write(text, bootview) {
        return;
    }
    serial_write(text);
}

fn request_cap(domain_key: u64, caller_space: u64, out_low: &mut u64, out_high: &mut u64) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    gate.sequence = 1;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = caller_space;
    gate.domain_key = domain_key;
    gate.cid_low = 0;
    gate.cid_high = 0;
    gate.status = 0;
    gate.count = 0;
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

fn request_cap_with_uses(
    domain_key: u64,
    caller_space: u64,
    uses: u32,
    out_low: &mut u64,
    out_high: &mut u64,
) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>());
    }
    gate.sequence = 2;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = caller_space;
    gate.domain_key = domain_key;
    gate.target_space = 0;
    gate.target_gate = 0;
    gate.ttl = 0;
    gate.uses = uses;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == 0
}

fn request_cap_scoped(
    domain_key: u64,
    caller_space: u64,
    target_space: u32,
    target_gate: u32,
    out_low: &mut u64,
    out_high: &mut u64,
) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>());
    }
    gate.sequence = 3;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = caller_space;
    gate.domain_key = domain_key;
    gate.target_space = target_space;
    gate.target_gate = target_gate;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == 0
}

fn request_cap_scoped_with_uses(
    domain_key: u64,
    caller_space: u64,
    target_space: u32,
    target_gate: u32,
    uses: u32,
    out_low: &mut u64,
    out_high: &mut u64,
) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>());
    }
    gate.sequence = 4;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = caller_space;
    gate.domain_key = domain_key;
    gate.target_space = target_space;
    gate.target_gate = target_gate;
    gate.ttl = 0;
    gate.uses = uses;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == 0
}

fn validate_cap(domain_key: u64, cid_low: u64, cid_high: u64, target_gate: u32) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        write_bytes(gate as *mut LunaGate as *mut u8, 0, core::mem::size_of::<LunaGate>());
    }
    gate.sequence = 2;
    gate.opcode = GATE_VALIDATE_CAP;
    gate.caller_space = 0;
    gate.domain_key = domain_key;
    gate.cid_low = cid_low;
    gate.cid_high = cid_high;
    gate.target_space = SPACE_SYSTEM;
    gate.target_gate = target_gate;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    gate.status == 0
}

fn device_block_io(
    bootview: &LunaBootView,
    opcode: u32,
    device_id: u32,
    cid_low: u64,
    cid_high: u64,
    lba: u32,
    buffer: *mut u8,
) -> bool {
    let gate = unsafe { &mut *(bootview.device_gate_base as *mut LunaDeviceGate) };
    unsafe {
        write_bytes(gate as *mut LunaDeviceGate as *mut u8, 0, size_of::<LunaDeviceGate>());
    }
    gate.sequence = 901;
    gate.opcode = opcode;
    gate.device_id = device_id;
    gate.flags = lba;
    gate.cid_low = cid_low;
    gate.cid_high = cid_high;
    gate.size = DISK_SECTOR_SIZE as u64;
    gate.buffer_addr = buffer as u64;
    gate.buffer_size = DISK_SECTOR_SIZE as u64;
    let entry: extern "sysv64" fn(*mut LunaDeviceGate) =
        unsafe { core::mem::transmute((*(LUNA_MANIFEST_ADDR as *const LunaManifest)).device_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaDeviceGate);
    gate.status == STATUS_OK
}

fn fold_bytes(seed: u64, blob: &[u8]) -> u64 {
    let mut value = seed;
    let mut index = 0usize;
    while index < blob.len() {
        value ^= blob[index] as u64;
        value = value.rotate_left(5).wrapping_mul(0x100000001B3);
        index += 1;
    }
    value
}

fn crc32_bytes(blob: &[u8]) -> u32 {
    let mut crc = 0xFFFF_FFFFu32;
    let mut index = 0usize;
    while index < blob.len() {
        crc ^= blob[index] as u32;
        let mut bit = 0u32;
        while bit < 8 {
            let mask = 0u32.wrapping_sub(crc & 1);
            crc = (crc >> 1) ^ (0xEDB8_8320u32 & mask);
            bit += 1;
        }
        index += 1;
    }
    !crc
}

fn store_superblock_bytes(
    store_base_lba: u64,
    peer_lba: u64,
    profile: u64,
    activation: u64,
    install_low: u64,
    install_high: u64,
) -> [u8; DISK_SECTOR_SIZE] {
    let mut out = [0u8; DISK_SECTOR_SIZE];
    let zero_objects = [0u8; DATA_OBJECT_CAPACITY as usize * 88usize];
    let mut superblock = InstallerStoreSuperblock {
        magic: STORE_MAGIC,
        version: STORE_VERSION,
        object_capacity: DATA_OBJECT_CAPACITY,
        slot_sectors: DATA_SLOT_SECTORS,
        store_base_lba,
        data_start_lba: store_base_lba + DATA_META_SECTORS,
        next_nonce: 1,
        format_count: 1,
        mount_count: 0,
        reserved: [0; 55],
    };
    superblock.reserved[0] = STORE_STATE_CLEAN;
    superblock.reserved[1] = fold_bytes(0xCBF2_9CE4_8422_2325, &zero_objects);
    superblock.reserved[4] = LUNA_VOLUME_HEALTHY as u64;
    superblock.reserved[5] = LUNA_MODE_NORMAL as u64;
    superblock.reserved[6] = profile;
    superblock.reserved[7] = install_low;
    superblock.reserved[8] = install_high;
    superblock.reserved[9] = activation;
    superblock.reserved[10] = peer_lba;
    unsafe {
        copy_nonoverlapping(
            (&superblock as *const InstallerStoreSuperblock).cast::<u8>(),
            out.as_mut_ptr(),
            size_of::<InstallerStoreSuperblock>(),
        );
    }
    out
}

fn write_u32_le(blob: &mut [u8], offset: usize, value: u32) {
    let bytes = value.to_le_bytes();
    unsafe { copy_nonoverlapping(bytes.as_ptr(), blob.as_mut_ptr().add(offset), 4); }
}

fn write_u64_le(blob: &mut [u8], offset: usize, value: u64) {
    let bytes = value.to_le_bytes();
    unsafe { copy_nonoverlapping(bytes.as_ptr(), blob.as_mut_ptr().add(offset), 8); }
}

fn write_guid(blob: &mut [u8], offset: usize, d1: u32, d2: u16, d3: u16, d4: [u8; 8]) {
    let d1_bytes = d1.to_le_bytes();
    let d2_bytes = d2.to_le_bytes();
    let d3_bytes = d3.to_le_bytes();
    unsafe {
        copy_nonoverlapping(d1_bytes.as_ptr(), blob.as_mut_ptr().add(offset), 4);
        copy_nonoverlapping(d2_bytes.as_ptr(), blob.as_mut_ptr().add(offset + 4), 2);
        copy_nonoverlapping(d3_bytes.as_ptr(), blob.as_mut_ptr().add(offset + 6), 2);
        copy_nonoverlapping(d4.as_ptr(), blob.as_mut_ptr().add(offset + 8), 8);
    }
}

fn write_partition_name(blob: &mut [u8], offset: usize, name: &[u16]) {
    let mut index = 0usize;
    while index < name.len() && offset + index * 2 + 1 < blob.len() {
        let bytes = unsafe { *name.get_unchecked(index) }.to_le_bytes();
        unsafe {
            *blob.as_mut_ptr().add(offset + index * 2) = bytes[0];
            *blob.as_mut_ptr().add(offset + index * 2 + 1) = bytes[1];
        }
        index += 1;
    }
}

fn make_partition_guid(seed: u64, slot: u8) -> [u8; 16] {
    let mut out = [0u8; 16];
    let mut value = seed ^ ((slot as u64) << 56) ^ 0x4C55_4E41_5F50_4152;
    let mut index = 0usize;
    while index < out.len() {
        value = value.rotate_left(9).wrapping_mul(0x9E37_79B9_7F4A_7C15);
        unsafe { *out.as_mut_ptr().add(index) = (value >> 24) as u8; }
        index += 1;
    }
    out[7] = (out[7] & 0x0F) | 0x40;
    out[8] = (out[8] & 0x3F) | 0x80;
    out
}

fn write_gpt_partition_entry(
    entries: &mut [u8],
    index: usize,
    type_guid: (u32, u16, u16, [u8; 8]),
    part_guid: [u8; 16],
    first_lba: u64,
    last_lba: u64,
    name: &[u16],
) {
    let offset = index * GPT_ENTRY_SIZE;
    write_guid(entries, offset, type_guid.0, type_guid.1, type_guid.2, type_guid.3);
    unsafe { copy_nonoverlapping(part_guid.as_ptr(), entries.as_mut_ptr().add(offset + 16), 16); }
    write_u64_le(entries, offset + 32, first_lba);
    write_u64_le(entries, offset + 40, last_lba);
    write_u64_le(entries, offset + 48, 0);
    write_partition_name(entries, offset + 56, name);
}

fn build_gpt_headers(
    install_low: u64,
    entries: &[u8],
) -> ([u8; DISK_SECTOR_SIZE], [u8; DISK_SECTOR_SIZE], [u8; 16]) {
    let mut disk_guid = [0u8; 16];
    let low = install_low.to_le_bytes();
    let high = (install_low ^ 0xA5A5_5A5A_C3C3_3C3C).to_le_bytes();
    let mut index = 0usize;
    while index < 8 {
        unsafe {
            *disk_guid.as_mut_ptr().add(index) = *low.as_ptr().add(index);
            *disk_guid.as_mut_ptr().add(index + 8) = *high.as_ptr().add(index);
        }
        index += 1;
    }
    disk_guid[7] = (disk_guid[7] & 0x0F) | 0x40;
    disk_guid[8] = (disk_guid[8] & 0x3F) | 0x80;

    let entries_crc = crc32_bytes(entries);
    let mut primary = [0u8; DISK_SECTOR_SIZE];
    unsafe { copy_nonoverlapping(b"EFI PART".as_ptr(), primary.as_mut_ptr(), 8); }
    write_u32_le(&mut primary, 8, 0x0001_0000);
    write_u32_le(&mut primary, 12, 92);
    write_u64_le(&mut primary, 24, 1);
    write_u64_le(&mut primary, 32, DISK_TOTAL_SECTORS - 1);
    write_u64_le(&mut primary, 40, 34);
    write_u64_le(&mut primary, 48, DISK_TOTAL_SECTORS - 34);
    unsafe { copy_nonoverlapping(disk_guid.as_ptr(), primary.as_mut_ptr().add(56), 16); }
    write_u64_le(&mut primary, 72, 2);
    write_u32_le(&mut primary, 80, GPT_ENTRY_COUNT as u32);
    write_u32_le(&mut primary, 84, GPT_ENTRY_SIZE as u32);
    write_u32_le(&mut primary, 88, entries_crc);
    write_u32_le(&mut primary, 16, 0);
    let primary_crc = crc32_bytes(&primary[..92]);
    write_u32_le(&mut primary, 16, primary_crc);

    let mut backup = [0u8; DISK_SECTOR_SIZE];
    unsafe { copy_nonoverlapping(b"EFI PART".as_ptr(), backup.as_mut_ptr(), 8); }
    write_u32_le(&mut backup, 8, 0x0001_0000);
    write_u32_le(&mut backup, 12, 92);
    write_u64_le(&mut backup, 24, DISK_TOTAL_SECTORS - 1);
    write_u64_le(&mut backup, 32, 1);
    write_u64_le(&mut backup, 40, 34);
    write_u64_le(&mut backup, 48, DISK_TOTAL_SECTORS - 34);
    unsafe { copy_nonoverlapping(disk_guid.as_ptr(), backup.as_mut_ptr().add(56), 16); }
    write_u64_le(&mut backup, 72, DISK_TOTAL_SECTORS - 33);
    write_u32_le(&mut backup, 80, GPT_ENTRY_COUNT as u32);
    write_u32_le(&mut backup, 84, GPT_ENTRY_SIZE as u32);
    write_u32_le(&mut backup, 88, entries_crc);
    write_u32_le(&mut backup, 16, 0);
    let backup_crc = crc32_bytes(&backup[..92]);
    write_u32_le(&mut backup, 16, backup_crc);

    (primary, backup, disk_guid)
}

fn short_name(name: &[u8], ext: &[u8]) -> [u8; 11] {
    let mut out = [b' '; 11];
    let mut index = 0usize;
    while index < name.len() && index < 8 {
        out[index] = name[index];
        index += 1;
    }
    index = 0;
    while index < ext.len() && index < 3 {
        out[8 + index] = ext[index];
        index += 1;
    }
    out
}

fn write_fat_dir_entry(out: &mut [u8], name83: [u8; 11], attr: u8, cluster: u16, size: u32) {
    let date: u16 = 0x5A58;
    let time: u16 = 0x0000;
    let mut index = 0usize;
    while index < 11 {
        out[index] = name83[index];
        index += 1;
    }
    out[11] = attr;
    out[12] = 0;
    out[13] = 0;
    out[14] = (time & 0xFF) as u8;
    out[15] = (time >> 8) as u8;
    out[16] = (date & 0xFF) as u8;
    out[17] = (date >> 8) as u8;
    out[18] = (date & 0xFF) as u8;
    out[19] = (date >> 8) as u8;
    out[20] = 0;
    out[21] = 0;
    out[22] = (time & 0xFF) as u8;
    out[23] = (time >> 8) as u8;
    out[24] = (date & 0xFF) as u8;
    out[25] = (date >> 8) as u8;
    out[26] = (cluster & 0xFF) as u8;
    out[27] = (cluster >> 8) as u8;
    write_u32_le(out, 28, size);
}

fn installer_stage_blob() -> &'static [u8] {
    unsafe {
        core::slice::from_raw_parts(
            LUNA_IMAGE_BASE as *const u8,
            (LUNA_BOOTVIEW_ADDR - LUNA_IMAGE_BASE) as usize,
        )
    }
}

fn patch_stage_manifest_field(out: &mut [u8; DISK_SECTOR_SIZE], stage_sector_index: usize, field_offset: usize, value: u64) {
    let manifest_offset = (LUNA_MANIFEST_ADDR - LUNA_IMAGE_BASE) as usize;
    let sector_offset = stage_sector_index * DISK_SECTOR_SIZE;
    let field_start = manifest_offset + field_offset;
    let field_end = field_start + 8;
    if field_end <= sector_offset || field_start >= sector_offset + DISK_SECTOR_SIZE {
        return;
    }
    let local = field_start - sector_offset;
    let bytes = value.to_le_bytes();
    let mut index = 0usize;
    while index < 8 {
        out[local + index] = bytes[index];
        index += 1;
    }
}

fn patch_stage_manifest_runtime_field(
    out: &mut [u8; DISK_SECTOR_SIZE],
    stage_sector_index: usize,
    field_offset: usize,
    runtime_value: u64,
    runtime_bootview_base: u64,
    canonical_bootview_base: u64,
) {
    let canonical = if runtime_value >= runtime_bootview_base {
        runtime_value - runtime_bootview_base + canonical_bootview_base
    } else {
        runtime_value
    };
    patch_stage_manifest_field(out, stage_sector_index, field_offset, canonical);
}

fn patch_stage_runtime_bytes(out: &mut [u8; DISK_SECTOR_SIZE], stage_sector_index: usize, stage_offset: usize, value: &[u8]) {
    let sector_offset = stage_sector_index * DISK_SECTOR_SIZE;
    let field_end = stage_offset + value.len();
    if field_end <= sector_offset || stage_offset >= sector_offset + DISK_SECTOR_SIZE {
        return;
    }
    let copy_start = core::cmp::max(stage_offset, sector_offset);
    let copy_end = core::cmp::min(field_end, sector_offset + DISK_SECTOR_SIZE);
    let mut index = 0usize;
    while copy_start + index < copy_end {
        out[copy_start + index - sector_offset] = value[copy_start + index - stage_offset];
        index += 1;
    }
}

fn patch_stage_runtime_zero(out: &mut [u8; DISK_SECTOR_SIZE], stage_sector_index: usize, stage_offset: usize, size: usize) {
    let zeros = [0u8; 8];
    let mut offset = 0usize;
    while offset < size {
        let chunk = core::cmp::min(size - offset, zeros.len());
        patch_stage_runtime_bytes(out, stage_sector_index, stage_offset + offset, &zeros[..chunk]);
        offset += chunk;
    }
}

fn installer_write_stage_sector(
    bootview: &LunaBootView,
    stage_sector_index: usize,
    out: &mut [u8; DISK_SECTOR_SIZE],
) {
    let stage_blob = installer_stage_blob();
    installer_write_cluster_sector((stage_sector_index + 2) as u16, stage_blob, out);

    let manifest = core::mem::MaybeUninit::<LunaManifest>::uninit();
    let base = manifest.as_ptr() as usize;
    let runtime_manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };

    let bootview_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).bootview_base) as usize - base };
    let security_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).security_gate_base) as usize - base };
    let data_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).data_gate_base) as usize - base };
    let lifecycle_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).lifecycle_gate_base) as usize - base };
    let system_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).system_gate_base) as usize - base };
    let time_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).time_gate_base) as usize - base };
    let device_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).device_gate_base) as usize - base };
    let observe_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).observe_gate_base) as usize - base };
    let network_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).network_gate_base) as usize - base };
    let graphics_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).graphics_gate_base) as usize - base };
    let ai_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).ai_gate_base) as usize - base };
    let package_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).package_gate_base) as usize - base };
    let update_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).update_gate_base) as usize - base };
    let program_gate_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).program_gate_base) as usize - base };
    let data_buffer_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).data_buffer_base) as usize - base };
    let list_buffer_base_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).list_buffer_base) as usize - base };
    let system_store_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).system_store_lba) as usize - base };
    let data_store_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).data_store_lba) as usize - base };
    let install_low_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).install_uuid_low) as usize - base };
    let install_high_offset = unsafe { core::ptr::addr_of!((*manifest.as_ptr()).install_uuid_high) as usize - base };

    patch_stage_manifest_runtime_field(out, stage_sector_index, bootview_base_offset, runtime_manifest.bootview_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, security_gate_base_offset, runtime_manifest.security_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, data_gate_base_offset, runtime_manifest.data_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, lifecycle_gate_base_offset, runtime_manifest.lifecycle_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, system_gate_base_offset, runtime_manifest.system_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, time_gate_base_offset, runtime_manifest.time_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, device_gate_base_offset, runtime_manifest.device_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, observe_gate_base_offset, runtime_manifest.observe_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, network_gate_base_offset, runtime_manifest.network_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, graphics_gate_base_offset, runtime_manifest.graphics_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, ai_gate_base_offset, runtime_manifest.ai_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, package_gate_base_offset, runtime_manifest.package_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, update_gate_base_offset, runtime_manifest.update_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, program_gate_base_offset, runtime_manifest.program_gate_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, data_buffer_base_offset, runtime_manifest.data_buffer_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_runtime_field(out, stage_sector_index, list_buffer_base_offset, runtime_manifest.list_buffer_base, runtime_manifest.bootview_base, LUNA_BOOTVIEW_ADDR);
    patch_stage_manifest_field(out, stage_sector_index, system_store_offset, bootview.installer_target_system_lba);
    patch_stage_manifest_field(out, stage_sector_index, data_store_offset, bootview.installer_target_data_lba);
    patch_stage_manifest_field(out, stage_sector_index, install_low_offset, bootview.install_uuid_low);
    patch_stage_manifest_field(out, stage_sector_index, install_high_offset, bootview.install_uuid_high);
    patch_stage_runtime_zero(
        out,
        stage_sector_index,
        LUNA_INSTALLER_DEVICE_FIRMWARE_READ_DIAG_OFFSET,
        LUNA_INSTALLER_DEVICE_FIRMWARE_READ_DIAG_SIZE,
    );
    patch_stage_runtime_zero(
        out,
        stage_sector_index,
        LUNA_INSTALLER_DEVICE_AHCI_ABAR_OFFSET,
        LUNA_INSTALLER_DEVICE_AHCI_ABAR_SIZE,
    );
    patch_stage_runtime_zero(
        out,
        stage_sector_index,
        LUNA_INSTALLER_DEVICE_AHCI_PORT_OFFSET,
        LUNA_INSTALLER_DEVICE_AHCI_PORT_SIZE,
    );
    patch_stage_runtime_zero(
        out,
        stage_sector_index,
        LUNA_INSTALLER_DEVICE_AHCI_READY_OFFSET,
        LUNA_INSTALLER_DEVICE_AHCI_READY_SIZE,
    );
}

fn installer_fat_entry(cluster: u16, script_clusters: u16, efi_clusters: u16, stage_clusters: u16) -> u16 {
    const ROOT_CLUSTER: u16 = 2;
    const BOOT_DIR_CLUSTER: u16 = 3;
    const SCRIPT_CLUSTER: u16 = 4;
    const EFI_CLUSTER_START: u16 = 5;

    if cluster == 0 {
        return 0xFFF8;
    }
    if cluster == 1 || cluster == ROOT_CLUSTER || cluster == BOOT_DIR_CLUSTER {
        return 0xFFFF;
    }
    if cluster >= SCRIPT_CLUSTER && cluster < SCRIPT_CLUSTER + script_clusters {
        return if cluster == SCRIPT_CLUSTER + script_clusters - 1 {
            0xFFFF
        } else {
            cluster + 1
        };
    }
    if cluster >= EFI_CLUSTER_START && cluster < EFI_CLUSTER_START + efi_clusters {
        return if cluster == EFI_CLUSTER_START + efi_clusters - 1 {
            0xFFFF
        } else {
            cluster + 1
        };
    }
    if cluster >= ESP_STAGE_FILE_CLUSTER && cluster < ESP_STAGE_FILE_CLUSTER + stage_clusters {
        return if cluster == ESP_STAGE_FILE_CLUSTER + stage_clusters - 1 {
            0xFFFF
        } else {
            cluster + 1
        };
    }
    0
}

fn installer_write_cluster_sector(cluster: u16, source: &[u8], out: &mut [u8; DISK_SECTOR_SIZE]) {
    if cluster < 2 {
        return;
    }
    let start = ((cluster as usize) - 2) * DISK_SECTOR_SIZE;
    if start >= source.len() {
        return;
    }
    let end = core::cmp::min(start + DISK_SECTOR_SIZE, source.len());
    unsafe {
        copy_nonoverlapping(source.as_ptr().add(start), out.as_mut_ptr(), end - start);
    }
}

fn build_installer_esp_sector(
    bootview: &LunaBootView,
    esp_sector_index: u64,
    out: &mut [u8; DISK_SECTOR_SIZE],
) {
    const SCRIPT_BLOB: &[u8] = b"FS0:\\EFI\\BOOT\\BOOTX64.EFI\r\n";
    const ROOT_CLUSTER: u16 = 2;
    const BOOT_DIR_CLUSTER: u16 = 3;
    const SCRIPT_CLUSTER: u16 = 4;
    const EFI_CLUSTER_START: u16 = 5;

    unsafe { write_bytes(out.as_mut_ptr(), 0, DISK_SECTOR_SIZE); }

    let stage_blob = installer_stage_blob();
    let script_clusters = SCRIPT_BLOB.len().div_ceil(DISK_SECTOR_SIZE) as u16;
    let efi_clusters = LUNA_INSTALLER_EFI_PAYLOAD_BYTES.div_ceil(DISK_SECTOR_SIZE) as u16;
    let stage_clusters = stage_blob.len().div_ceil(DISK_SECTOR_SIZE) as u16;

    if esp_sector_index == 0 {
        out[0..3].copy_from_slice(&[0xEB, 0x3C, 0x90]);
        out[3..11].copy_from_slice(b"LUNAFAT ");
        out[11] = (DISK_SECTOR_SIZE & 0xFF) as u8;
        out[12] = (DISK_SECTOR_SIZE >> 8) as u8;
        out[13] = ESP_SECTORS_PER_CLUSTER as u8;
        out[14] = ESP_RESERVED_SECTORS as u8;
        out[15] = 0;
        out[16] = ESP_FAT_COUNT as u8;
        out[17] = (ESP_ROOT_ENTRIES & 0xFF) as u8;
        out[18] = ((ESP_ROOT_ENTRIES >> 8) & 0xFF) as u8;
        out[19] = (ESP_SECTORS & 0xFF) as u8;
        out[20] = ((ESP_SECTORS >> 8) & 0xFF) as u8;
        out[21] = 0xF8;
        out[22] = ESP_FAT_SECTORS as u8;
        out[23] = 0;
        out[24] = 63;
        out[25] = 0;
        out[26] = 255;
        out[27] = 0;
        write_u32_le(out, 28, ESP_START_LBA as u32);
        out[36] = 0x80;
        out[38] = 0x29;
        write_u32_le(out, 39, 0x4C55_4E41);
        out[43..54].copy_from_slice(b"LUNAESP    ");
        out[54..62].copy_from_slice(b"FAT16   ");
        out[510] = 0x55;
        out[511] = 0xAA;
        return;
    }

    let fat_end = ESP_RESERVED_SECTORS + ESP_FAT_COUNT * ESP_FAT_SECTORS;
    if esp_sector_index >= ESP_RESERVED_SECTORS && esp_sector_index < fat_end {
        let sector_in_fat = ((esp_sector_index - ESP_RESERVED_SECTORS) % ESP_FAT_SECTORS) as usize;
        let first_entry = sector_in_fat * (DISK_SECTOR_SIZE / 2);
        let mut slot = 0usize;
        while slot < (DISK_SECTOR_SIZE / 2) {
            let entry = installer_fat_entry((first_entry + slot) as u16, script_clusters, efi_clusters, stage_clusters);
            out[slot * 2] = (entry & 0xFF) as u8;
            out[slot * 2 + 1] = (entry >> 8) as u8;
            slot += 1;
        }
        return;
    }

    let root_dir_start = ESP_RESERVED_SECTORS + ESP_FAT_COUNT * ESP_FAT_SECTORS;
    let root_dir_end = root_dir_start + ESP_ROOT_DIR_SECTORS;
    if esp_sector_index >= root_dir_start && esp_sector_index < root_dir_end {
        let entry_base = ((esp_sector_index - root_dir_start) as usize) * 16;
        if entry_base == 0 {
            write_fat_dir_entry(&mut out[0..32], short_name(b"EFI", b""), 0x10, ROOT_CLUSTER, 0);
            write_fat_dir_entry(&mut out[32..64], short_name(b"STARTUP", b"NSH"), 0x20, SCRIPT_CLUSTER, SCRIPT_BLOB.len() as u32);
            write_fat_dir_entry(
                &mut out[64..96],
                short_name(b"LUNAOS", b"IMG"),
                0x20,
                ESP_STAGE_FILE_CLUSTER,
                stage_blob.len() as u32,
            );
        }
        return;
    }

    let data_sector_index = esp_sector_index - ESP_DATA_START_LBA;
    let cluster = (data_sector_index / ESP_SECTORS_PER_CLUSTER) as u16 + 2;
    if cluster == ROOT_CLUSTER {
        write_fat_dir_entry(&mut out[0..32], short_name(b".", b""), 0x10, ROOT_CLUSTER, 0);
        write_fat_dir_entry(&mut out[32..64], short_name(b"..", b""), 0x10, 0, 0);
        write_fat_dir_entry(&mut out[64..96], short_name(b"BOOT", b""), 0x10, BOOT_DIR_CLUSTER, 0);
        return;
    }
    if cluster == BOOT_DIR_CLUSTER {
        write_fat_dir_entry(&mut out[0..32], short_name(b".", b""), 0x10, BOOT_DIR_CLUSTER, 0);
        write_fat_dir_entry(&mut out[32..64], short_name(b"..", b""), 0x10, ROOT_CLUSTER, 0);
        write_fat_dir_entry(
            &mut out[64..96],
            short_name(b"BOOTX64", b"EFI"),
            0x20,
            EFI_CLUSTER_START,
            LUNA_INSTALLER_EFI_PAYLOAD_BYTES as u32,
        );
        return;
    }
    if cluster >= SCRIPT_CLUSTER && cluster < SCRIPT_CLUSTER + script_clusters {
        installer_write_cluster_sector(cluster - SCRIPT_CLUSTER + 2, SCRIPT_BLOB, out);
        return;
    }
    if cluster >= EFI_CLUSTER_START && cluster < EFI_CLUSTER_START + efi_clusters {
        installer_write_cluster_sector(cluster - EFI_CLUSTER_START + 2, &LUNA_INSTALLER_EFI_PAYLOAD, out);
        return;
    }
    if cluster >= ESP_STAGE_FILE_CLUSTER && cluster < ESP_STAGE_FILE_CLUSTER + stage_clusters {
        installer_write_stage_sector(bootview, (cluster - ESP_STAGE_FILE_CLUSTER) as usize, out);
    }
}

fn run_installer_apply(bootview: &LunaBootView) -> bool {
    if bootview.system_mode != LUNA_MODE_INSTALL {
        return true;
    }
    if (bootview.installer_target_flags & LUNA_INSTALL_TARGET_BOUND) == 0 {
        log_write(b"[INSTALLER] target bind missing\r\n", bootview);
        return false;
    }

    let mut read_low = 0u64;
    let mut read_high = 0u64;
    let mut write_low = 0u64;
    let mut write_high = 0u64;
    if !request_cap_with_uses(
        CAP_DEVICE_READ,
        SPACE_SYSTEM as u64,
        INSTALLER_CAP_USES,
        &mut read_low,
        &mut read_high,
    ) {
        log_write(b"[INSTALLER] target read cap fail\r\n", bootview);
        return false;
    }
    if !request_cap_scoped_with_uses(
        CAP_DEVICE_WRITE,
        SPACE_SYSTEM as u64,
        SPACE_DEVICE,
        LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET,
        INSTALLER_CAP_USES,
        &mut write_low,
        &mut write_high,
    ) {
        log_write(b"[INSTALLER] writer identity fail\r\n", bootview);
        return false;
    }
    log_write(b"[INSTALLER] target bind ok\r\n", bootview);
    log_write(b"[INSTALLER] writer identity ok\r\n", bootview);

    let mut mbr = [0u8; DISK_SECTOR_SIZE];
    unsafe { copy_nonoverlapping(b"LunaOS GPT skeleton".as_ptr(), mbr.as_mut_ptr(), 20); }
    mbr[446] = 0x00;
    mbr[450] = 0xEE;
    write_u32_le(&mut mbr, 454, 1);
    write_u32_le(&mut mbr, 458, (DISK_TOTAL_SECTORS - 1) as u32);
    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    let mut entries = [0u8; GPT_ENTRY_COUNT * GPT_ENTRY_SIZE];
    write_gpt_partition_entry(
        &mut entries,
        0,
        (0xC12A_7328, 0xF81F, 0x11D2, [0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B]),
        make_partition_guid(bootview.install_uuid_low, 1),
        ESP_START_LBA,
        ESP_START_LBA + ESP_SECTORS - 1,
        &[0x0045, 0x0046, 0x0049, 0x0020, 0x0053, 0x0079, 0x0073, 0x0074, 0x0065, 0x006D],
    );
    write_gpt_partition_entry(
        &mut entries,
        1,
        (0xA19D_880F, 0x05FC, 0x4D3B, [0xA0, 0x06, 0x74, 0x3F, 0x0F, 0x84, 0x91, 0x1E]),
        make_partition_guid(bootview.install_uuid_low, 2),
        bootview.installer_target_system_lba,
        bootview.installer_target_system_lba + SYSTEM_SECTORS - 1,
        &[0x004C, 0x0075, 0x006E, 0x0061, 0x0020, 0x0053, 0x0079, 0x0073, 0x0074, 0x0065, 0x006D],
    );
    write_gpt_partition_entry(
        &mut entries,
        2,
        (0x6A82_CB45, 0x1DD2, 0x11B2, [0xA7, 0xA6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31]),
        make_partition_guid(bootview.install_uuid_low, 3),
        bootview.installer_target_data_lba,
        bootview.installer_target_data_lba + DATA_SECTORS - 1,
        &[0x004C, 0x0075, 0x006E, 0x0061, 0x0020, 0x0044, 0x0061, 0x0074, 0x0061],
    );
    write_gpt_partition_entry(
        &mut entries,
        3,
        (0x6A89_8CC3, 0x1DD2, 0x11B2, [0x99, 0xA6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31]),
        make_partition_guid(bootview.install_uuid_low, 4),
        RECOVERY_START_LBA,
        RECOVERY_START_LBA + RECOVERY_SECTORS - 1,
        &[0x004C, 0x0075, 0x006E, 0x0061, 0x0020, 0x0052, 0x0065, 0x0063, 0x006F, 0x0076, 0x0065, 0x0072, 0x0079],
    );
    let (primary_header, backup_header, _) = build_gpt_headers(bootview.install_uuid_low, &entries);

    let mut sector = [0u8; DISK_SECTOR_SIZE];
    if !device_block_io(bootview, DEVICE_BLOCK_WRITE, LUNA_DEVICE_ID_DISK1, write_low, write_high, 0, mbr.as_mut_ptr()) {
        log_write(b"[INSTALLER] gpt mbr fail\r\n", bootview);
        if device_block_io(bootview, DEVICE_BLOCK_WRITE, LUNA_DEVICE_ID_DISK1, write_low, write_high, 1, mbr.as_mut_ptr()) {
            log_write(b"[INSTALLER] target probe lba1 ok\r\n", bootview);
        } else {
            log_write(b"[INSTALLER] target probe lba1 fail\r\n", bootview);
        }
        if device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            bootview.installer_target_system_lba as u32,
            mbr.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] target probe lsys ok\r\n", bootview);
        } else {
            log_write(b"[INSTALLER] target probe lsys fail\r\n", bootview);
        }
        return false;
    }
    if !device_block_io(bootview, DEVICE_BLOCK_WRITE, LUNA_DEVICE_ID_DISK1, write_low, write_high, 1, primary_header.as_ptr() as *mut u8) {
        log_write(b"[INSTALLER] gpt header fail\r\n", bootview);
        return false;
    }
    let mut entry_sector_index = 0u64;
    while entry_sector_index < GPT_ENTRIES_SECTORS {
        let start = (entry_sector_index as usize) * DISK_SECTOR_SIZE;
        unsafe { copy_nonoverlapping(entries.as_ptr().add(start), sector.as_mut_ptr(), DISK_SECTOR_SIZE); }
        if !device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            (2 + entry_sector_index) as u32,
            sector.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] gpt entries fail\r\n", bootview);
            return false;
        }
        entry_sector_index += 1;
    }
    let backup_entries_lba = DISK_TOTAL_SECTORS - 33;
    entry_sector_index = 0;
    while entry_sector_index < GPT_ENTRIES_SECTORS {
        let start = (entry_sector_index as usize) * DISK_SECTOR_SIZE;
        unsafe { copy_nonoverlapping(entries.as_ptr().add(start), sector.as_mut_ptr(), DISK_SECTOR_SIZE); }
        if !device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            (backup_entries_lba + entry_sector_index) as u32,
            sector.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] gpt backup entries fail\r\n", bootview);
            return false;
        }
        entry_sector_index += 1;
    }
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_WRITE,
        LUNA_DEVICE_ID_DISK1,
        write_low,
        write_high,
        (DISK_TOTAL_SECTORS - 1) as u32,
        backup_header.as_ptr() as *mut u8,
    ) {
        log_write(b"[INSTALLER] gpt backup header fail\r\n", bootview);
        return false;
    }

    let mut esp_sector_index = 0u64;
    while esp_sector_index < ESP_SECTORS {
        build_installer_esp_sector(bootview, esp_sector_index, &mut sector);
        if !device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            (ESP_START_LBA + esp_sector_index) as u32,
            sector.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] esp target write fail\r\n", bootview);
            return false;
        }
        esp_sector_index += 1;
    }
    log_write(b"[INSTALLER] esp write ok\r\n", bootview);

    sector = store_superblock_bytes(
        bootview.installer_target_system_lba,
        bootview.installer_target_data_lba,
        LUNA_NATIVE_PROFILE_SYSTEM as u64,
        LUNA_ACTIVATION_ACTIVE as u64,
        bootview.install_uuid_low,
        bootview.install_uuid_high,
    );
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_WRITE,
        LUNA_DEVICE_ID_DISK1,
        write_low,
        write_high,
        bootview.installer_target_system_lba as u32,
        sector.as_mut_ptr(),
    ) {
        log_write(b"[INSTALLER] lsys super fail\r\n", bootview);
        return false;
    }
    sector = [0u8; DISK_SECTOR_SIZE];
    let mut meta_index = 1u64;
    while meta_index <= DATA_META_SECTORS {
        if !device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            (bootview.installer_target_system_lba + meta_index) as u32,
            sector.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] lsys meta fail\r\n", bootview);
            return false;
        }
        meta_index += 1;
    }
    log_write(b"[INSTALLER] lsys write ok\r\n", bootview);

    sector = store_superblock_bytes(
        bootview.installer_target_data_lba,
        bootview.installer_target_system_lba,
        LUNA_NATIVE_PROFILE_DATA as u64,
        LUNA_ACTIVATION_PROVISIONED as u64,
        bootview.install_uuid_low,
        bootview.install_uuid_high,
    );
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_WRITE,
        LUNA_DEVICE_ID_DISK1,
        write_low,
        write_high,
        bootview.installer_target_data_lba as u32,
        sector.as_mut_ptr(),
    ) {
        log_write(b"[INSTALLER] ldat super fail\r\n", bootview);
        return false;
    }
    sector = [0u8; DISK_SECTOR_SIZE];
    meta_index = 1;
    while meta_index <= DATA_META_SECTORS {
        if !device_block_io(
            bootview,
            DEVICE_BLOCK_WRITE,
            LUNA_DEVICE_ID_DISK1,
            write_low,
            write_high,
            (bootview.installer_target_data_lba + meta_index) as u32,
            sector.as_mut_ptr(),
        ) {
            log_write(b"[INSTALLER] ldat meta fail\r\n", bootview);
            return false;
        }
        meta_index += 1;
    }
    log_write(b"[INSTALLER] ldat write ok\r\n", bootview);

    sector = [0u8; DISK_SECTOR_SIZE];
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_WRITE,
        LUNA_DEVICE_ID_DISK1,
        write_low,
        write_high,
        RECOVERY_START_LBA as u32,
        sector.as_mut_ptr(),
    ) {
        log_write(b"[INSTALLER] lrcv write fail\r\n", bootview);
        return false;
    }
    log_write(b"[INSTALLER] lrcv write ok\r\n", bootview);

    if !device_block_io(bootview, DEVICE_BLOCK_READ, LUNA_DEVICE_ID_DISK1, read_low, read_high, 1, sector.as_mut_ptr()) {
        log_write(b"[INSTALLER] verify gpt read fail\r\n", bootview);
        return false;
    }
    if &sector[0..8] != b"EFI PART" {
        log_write(b"[INSTALLER] verify gpt header fail\r\n", bootview);
        return false;
    }
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_READ,
        LUNA_DEVICE_ID_DISK1,
        read_low,
        read_high,
        bootview.installer_target_system_lba as u32,
        sector.as_mut_ptr(),
    ) {
        log_write(b"[INSTALLER] verify lsys read fail\r\n", bootview);
        return false;
    }
    let system_super = unsafe { &*(sector.as_ptr() as *const LunaStoreSuperblock) };
    if system_super.magic != STORE_MAGIC
        || system_super.version != STORE_VERSION
        || system_super.reserved[6] != LUNA_NATIVE_PROFILE_SYSTEM as u64
        || system_super.reserved[7] != bootview.install_uuid_low
        || system_super.reserved[8] != bootview.install_uuid_high
        || system_super.reserved[9] != LUNA_ACTIVATION_ACTIVE as u64
        || system_super.reserved[10] != bootview.installer_target_data_lba {
        log_write(b"[INSTALLER] verify lsys contract fail\r\n", bootview);
        return false;
    }
    if !device_block_io(
        bootview,
        DEVICE_BLOCK_READ,
        LUNA_DEVICE_ID_DISK1,
        read_low,
        read_high,
        bootview.installer_target_data_lba as u32,
        sector.as_mut_ptr(),
    ) {
        log_write(b"[INSTALLER] verify ldat read fail\r\n", bootview);
        return false;
    }
    let data_super = unsafe { &*(sector.as_ptr() as *const LunaStoreSuperblock) };
    if data_super.magic != STORE_MAGIC
        || data_super.version != STORE_VERSION
        || data_super.reserved[6] != LUNA_NATIVE_PROFILE_DATA as u64
        || data_super.reserved[7] != bootview.install_uuid_low
        || data_super.reserved[8] != bootview.install_uuid_high
        || data_super.reserved[10] != bootview.installer_target_system_lba {
        log_write(b"[INSTALLER] verify ldat contract fail\r\n", bootview);
        return false;
    }
    log_write(b"[INSTALLER] verify ok\r\n", bootview);
    true
}

fn lifecycle_call(gate: &mut LunaLifecycleGate) -> u32 {
    let entry: extern "sysv64" fn(*mut LunaLifecycleGate) =
        unsafe {
            core::mem::transmute(
                if LIFECYCLE_GATE_ENTRY_ADDR != 0 {
                    LIFECYCLE_GATE_ENTRY_ADDR
                } else {
                    (*(LUNA_MANIFEST_ADDR as *const LunaManifest)).lifecycle_gate_entry
                } as usize as *const ()
            )
        };
    entry(gate as *mut LunaLifecycleGate);
    gate.status
}

fn register_base(gate: &mut LunaSystemGate, cid_low: u64, cid_high: u64, space_id: u32, name: &[u8]) {
    gate.sequence = 100 + space_id;
    gate.opcode = SYSTEM_REGISTER;
    gate.status = 0;
    gate.space_id = space_id;
    gate.cid_low = cid_low;
    gate.cid_high = cid_high;
    gate.result_count = 0;
    let mut i = 0usize;
    while i < 16 {
        gate.name[i] = 0;
        if i < name.len() {
            gate.name[i] = name[i];
        }
        i += 1;
    }
}

fn encode_name16(name: &[u8]) -> [u8; 16] {
    let mut out = [0u8; 16];
    let mut i = 0usize;
    while i < 16 && i < name.len() {
        out[i] = name[i];
        i += 1;
    }
    out
}

fn ready_event_for_space(space_id: u32) -> u64 {
    match space_id {
        SPACE_BOOT => EVENT_BOOT_LIVE,
        SPACE_SECURITY => EVENT_SECURITY_READY,
        SPACE_LIFECYCLE => EVENT_LIFECYCLE_READY,
        SPACE_SYSTEM => EVENT_SYSTEM_READY,
        SPACE_DEVICE => EVENT_DEVICE_READY,
        SPACE_DATA => EVENT_DATA_READY,
        SPACE_GRAPHICS => EVENT_GRAPHICS_READY,
        SPACE_NETWORK => EVENT_NETWORK_READY,
        SPACE_TIME => EVENT_TIME_READY,
        SPACE_OBSERVABILITY => EVENT_OBSERVE_READY,
        SPACE_PROGRAM => EVENT_PROGRAM_READY,
        SPACE_AI => EVENT_AI_READY,
        SPACE_PACKAGE => EVENT_PACKAGE_READY,
        SPACE_UPDATE => EVENT_UPDATE_READY,
        SPACE_USER => EVENT_USER_READY,
        _ => 0,
    }
}

fn booting_event_for_space(space_id: u32) -> u64 {
    match space_id {
        SPACE_USER => EVENT_USER_BOOTING,
        _ => 0,
    }
}

unsafe fn log_system_event(space_id: u32, state: u32, event_word: u64, name: [u8; 16]) {
    if event_word == 0 {
        return;
    }
    let slot = unsafe { EVENT_HEAD };
    unsafe {
        (*event_ptr(slot)).sequence = NEXT_EVENT_SEQUENCE;
        (*event_ptr(slot)).space_id = space_id;
        (*event_ptr(slot)).state = state;
        (*event_ptr(slot)).event_word = event_word;
        (*event_ptr(slot)).name = name;
        EVENT_HEAD = (EVENT_HEAD + 1) % EVENT_CAPACITY;
        if EVENT_COUNT < EVENT_CAPACITY {
            EVENT_COUNT += 1;
        }
        NEXT_EVENT_SEQUENCE = NEXT_EVENT_SEQUENCE.wrapping_add(1);
    }
}

fn wake_space(life_gate: &mut LunaLifecycleGate, cid_low: u64, cid_high: u64, space_id: u32) {
    life_gate.sequence = 10 + space_id;
    life_gate.opcode = LIFE_WAKE;
    life_gate.status = 0;
    life_gate.space_id = space_id;
    life_gate.state = UNIT_LIVE;
    life_gate.result_count = 0;
    life_gate.cid_low = cid_low;
    life_gate.cid_high = cid_high;
    life_gate.epoch = 0;
    life_gate.flags = 0;
    life_gate.buffer_addr = 0;
    life_gate.buffer_size = 0;
    life_gate.entry_addr = 0;
    let _ = lifecycle_call(life_gate);
}

fn spawn_space(life_gate: &mut LunaLifecycleGate, cid_low: u64, cid_high: u64, space_id: u32, bootview_addr: u64, entry_addr: u64) -> u32 {
    life_gate.sequence = 200 + space_id;
    life_gate.opcode = LIFE_SPAWN;
    life_gate.status = 0;
    life_gate.space_id = space_id;
    life_gate.state = UNIT_LIVE;
    life_gate.result_count = 0;
    life_gate.cid_low = cid_low;
    life_gate.cid_high = cid_high;
    life_gate.epoch = 0;
    life_gate.flags = 0;
    life_gate.buffer_addr = bootview_addr;
    life_gate.buffer_size = core::mem::size_of::<LunaBootView>() as u64;
    life_gate.entry_addr = entry_addr;
    lifecycle_call(life_gate)
}

fn find_index(space_id: u32) -> Option<usize> {
    let mut index = 0usize;
    while index < CAPACITY {
        let record = unsafe { *record_ptr(index) };
        if record.state != 0 && record.space_id == space_id {
            return Some(index);
        }
        index += 1;
    }
    None
}

unsafe fn upsert_record(space_id: u32, name: [u8; 16]) -> Option<usize> {
    if let Some(index) = find_index(space_id) {
        unsafe {
            (*record_ptr(index)).state = STATE_ACTIVE;
            (*record_ptr(index)).name = name;
        }
        return Some(index);
    }
    let mut index = 0usize;
    while index < CAPACITY {
        if unsafe { (*record_ptr(index)).state } == 0 {
            unsafe {
                record_ptr(index).write(LunaSystemRecord {
                    space_id,
                    state: STATE_ACTIVE,
                    resource_memory: 0,
                    resource_time: 0,
                    last_event: 0,
                    name,
                });
            }
            return Some(index);
        }
        index += 1;
    }
    None
}

unsafe fn stage_record(space_id: u32, name: [u8; 16], state: u32, event_word: u64) -> Option<usize> {
    if let Some(index) = find_index(space_id) {
        unsafe {
            (*record_ptr(index)).state = state;
            (*record_ptr(index)).name = name;
            (*record_ptr(index)).last_event = event_word;
        }
        unsafe { log_system_event(space_id, state, event_word, name); }
        return Some(index);
    }
    let mut index = 0usize;
    while index < CAPACITY {
        if unsafe { (*record_ptr(index)).state } == 0 {
            unsafe {
                record_ptr(index).write(LunaSystemRecord {
                    space_id,
                    state,
                    resource_memory: 0,
                    resource_time: 0,
                    last_event: event_word,
                    name,
                });
            }
            unsafe { log_system_event(space_id, state, event_word, name); }
            return Some(index);
        }
        index += 1;
    }
    None
}

unsafe fn register_space(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_REGISTER, gate.cid_low, gate.cid_high, OP_REGISTER) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if unsafe { upsert_record(gate.space_id, gate.name) }.is_some() {
        gate.ticket = unsafe { NEXT_TICKET };
        unsafe { NEXT_TICKET = NEXT_TICKET.wrapping_add(1); }
        gate.status = STATUS_OK;
    } else {
        gate.status = STATUS_NO_ROOM;
    }
}

unsafe fn query_space(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_QUERY, gate.cid_low, gate.cid_high, OP_QUERY) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    let Some(index) = find_index(gate.space_id) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    if gate.buffer_size < core::mem::size_of::<LunaSystemRecord>() as u64 {
        gate.status = STATUS_NO_ROOM;
        return;
    }
    unsafe {
        copy_nonoverlapping(record_ptr(index) as *const u8, gate.buffer_addr as *mut u8, core::mem::size_of::<LunaSystemRecord>());
    }
    gate.result_count = 1;
    gate.status = STATUS_OK;
}

unsafe fn query_spaces(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_QUERY, gate.cid_low, gate.cid_high, OP_QUERY_SPACES) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    if gate.buffer_size < core::mem::size_of::<[LunaSystemRecord; CAPACITY]>() as u64 {
        gate.status = STATUS_NO_ROOM;
        return;
    }

    let mut out_count = 0u32;
    let mut index = 0usize;
    let mut out_ptr = gate.buffer_addr as *mut LunaSystemRecord;
    while index < CAPACITY {
        let record = unsafe { *record_ptr(index) };
        if record.state != 0 {
            unsafe {
                (*out_ptr).space_id = record.space_id;
                (*out_ptr).state = record.state;
                (*out_ptr).resource_memory = record.resource_memory;
                (*out_ptr).resource_time = record.resource_time;
                (*out_ptr).last_event = record.last_event;
                let mut name_index = 0usize;
                while name_index < 16 {
                    (*out_ptr).name[name_index] = record.name[name_index];
                    name_index += 1;
                }
                out_ptr = out_ptr.add(1);
            }
            out_count = out_count.wrapping_add(1);
        }
        index += 1;
    }
    gate.result_count = out_count;
    gate.status = STATUS_OK;
}

unsafe fn query_events(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_QUERY, gate.cid_low, gate.cid_high, OP_QUERY_EVENTS) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let max_records = (gate.buffer_size as usize) / core::mem::size_of::<LunaSystemEventRecord>();
    if max_records == 0 {
        gate.status = STATUS_NO_ROOM;
        return;
    }

    let available = unsafe { EVENT_COUNT };
    let out_total = if available < max_records { available } else { max_records };
    let out_ptr = gate.buffer_addr as *mut LunaSystemEventRecord;
    let mut out_index = 0usize;
    while out_index < out_total {
        let logical = unsafe { (EVENT_HEAD + EVENT_CAPACITY - 1 - out_index) % EVENT_CAPACITY };
        let record = unsafe { *event_ptr(logical) };
        unsafe {
            (*out_ptr.add(out_index)).sequence = record.sequence;
            (*out_ptr.add(out_index)).space_id = record.space_id;
            (*out_ptr.add(out_index)).state = record.state;
            (*out_ptr.add(out_index)).event_word = record.event_word;
            let mut name_index = 0usize;
            while name_index < 16 {
                (*out_ptr.add(out_index)).name[name_index] = record.name[name_index];
                name_index += 1;
            }
        }
        out_index += 1;
    }
    gate.result_count = out_total as u32;
    gate.status = STATUS_OK;
}

unsafe fn allocate_resource(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_ALLOCATE, gate.cid_low, gate.cid_high, OP_ALLOCATE) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    let Some(index) = find_index(gate.space_id) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    unsafe {
        if gate.resource_type == 1 {
            (*record_ptr(index)).resource_memory = (*record_ptr(index)).resource_memory.wrapping_add(gate.amount);
        } else if gate.resource_type == 2 {
            (*record_ptr(index)).resource_time = (*record_ptr(index)).resource_time.wrapping_add(gate.amount);
        }
    }
    gate.status = STATUS_OK;
}

unsafe fn report_event(gate: &mut LunaSystemGate) {
    if !validate_cap(CAP_SYSTEM_EVENT, gate.cid_low, gate.cid_high, OP_EVENT) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    let Some(index) = find_index(gate.space_id) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    unsafe {
        (*record_ptr(index)).last_event = gate.event_word;
    }
    gate.status = STATUS_OK;
}

fn spawn_and_register(_manifest: &LunaManifest, bootview: *const LunaBootView, gate: &mut LunaSystemGate, life_gate: &mut LunaLifecycleGate, sys_reg_low: u64, sys_reg_high: u64, life_spawn_low: u64, life_spawn_high: u64, space_id: u32, entry_addr: u64, name: &[u8], announce: &[u8], ready: &[u8]) {
    let encoded_name = encode_name16(name);
    let view = unsafe { &*bootview };
    life_gate.flags = ((view.system_mode as u64) << 32) | (view.volume_state as u64);
    let _ = unsafe { stage_record(space_id, encoded_name, STATE_BOOTING, booting_event_for_space(space_id)) };
    log_write(announce, bootview);
    if spawn_space(life_gate, life_spawn_low, life_spawn_high, space_id, bootview as u64, entry_addr) == STATUS_OK {
        if space_id == SPACE_DEVICE {
            if request_cap(CAP_DEVICE_WRITE, SPACE_SYSTEM as u64, unsafe { &mut DEVICE_WRITE_LOW }, unsafe { &mut DEVICE_WRITE_HIGH }) {
                unsafe { DEVICE_LOG_READY = true; }
            }
        }
        register_base(gate, sys_reg_low, sys_reg_high, space_id, name);
        system_entry_gate(gate as *mut LunaSystemGate);
        let _ = unsafe { stage_record(space_id, encoded_name, STATE_ACTIVE, ready_event_for_space(space_id)) };
        if !ready.is_empty() {
            log_write(ready, bootview);
        }
    } else {
        let _ = unsafe { stage_record(space_id, encoded_name, 0, 0) };
    }
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn system_entry_boot(_bootview: *const u8) {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    unsafe {
        LIFECYCLE_GATE_ENTRY_ADDR = manifest.lifecycle_gate_entry;
    }
    let bootview = _bootview as *const LunaBootView;
    let gate = unsafe { &mut *(manifest.system_gate_base as *mut LunaSystemGate) };
    let life_gate = &mut LunaLifecycleGate {
        sequence: 0,
        opcode: 0,
        status: 0,
        space_id: 0,
        state: 0,
        result_count: 0,
        cid_low: 0,
        cid_high: 0,
        epoch: 0,
        flags: 0,
        buffer_addr: 0,
        buffer_size: 0,
        entry_addr: 0,
    };
    let mut life_wake_low = 0u64;
    let mut life_wake_high = 0u64;
    let mut life_spawn_low = 0u64;
    let mut life_spawn_high = 0u64;
    let mut sys_reg_low = 0u64;
    let mut sys_reg_high = 0u64;
    let mut sys_alloc_low = 0u64;
    let mut sys_alloc_high = 0u64;
    let mut sys_event_low = 0u64;
    let mut sys_event_high = 0u64;

    unsafe {
        write_bytes(core::ptr::addr_of_mut!(RECORDS) as *mut u8, 0, core::mem::size_of::<[LunaSystemRecord; CAPACITY]>());
        write_bytes(core::ptr::addr_of_mut!(EVENT_LOG) as *mut u8, 0, core::mem::size_of::<[LunaSystemEventRecord; EVENT_CAPACITY]>());
        EVENT_HEAD = 0;
        EVENT_COUNT = 0;
        NEXT_EVENT_SEQUENCE = 1;
    }
    serial_write(b"[SYSTEM] atlas online\r\n");

    let _ = request_cap(CAP_LIFE_WAKE, SPACE_SYSTEM as u64, &mut life_wake_low, &mut life_wake_high);
    let _ = request_cap(CAP_LIFE_SPAWN, SPACE_SYSTEM as u64, &mut life_spawn_low, &mut life_spawn_high);
    let _ = request_cap(CAP_SYSTEM_REGISTER, SPACE_SYSTEM as u64, &mut sys_reg_low, &mut sys_reg_high);
    let _ = request_cap(CAP_SYSTEM_ALLOCATE, SPACE_SYSTEM as u64, &mut sys_alloc_low, &mut sys_alloc_high);
    let _ = request_cap(CAP_SYSTEM_EVENT, SPACE_SYSTEM as u64, &mut sys_event_low, &mut sys_event_high);

    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_BOOT);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_SECURITY);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_LIFECYCLE);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_SYSTEM);

    register_base(gate, sys_reg_low, sys_reg_high, SPACE_BOOT, b"BOOT");
    system_entry_gate(gate as *mut LunaSystemGate);
    let _ = unsafe { stage_record(SPACE_BOOT, encode_name16(b"BOOT"), STATE_ACTIVE, EVENT_BOOT_LIVE) };
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_SECURITY, b"SECURITY");
    system_entry_gate(gate as *mut LunaSystemGate);
    let _ = unsafe { stage_record(SPACE_SECURITY, encode_name16(b"SECURITY"), STATE_ACTIVE, EVENT_SECURITY_READY) };
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_LIFECYCLE, b"LIFECYCLE");
    system_entry_gate(gate as *mut LunaSystemGate);
    let _ = unsafe { stage_record(SPACE_LIFECYCLE, encode_name16(b"LIFECYCLE"), STATE_ACTIVE, EVENT_LIFECYCLE_READY) };
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_SYSTEM, b"SYSTEM");
    system_entry_gate(gate as *mut LunaSystemGate);
    let _ = unsafe { stage_record(SPACE_SYSTEM, encode_name16(b"SYSTEM"), STATE_ACTIVE, EVENT_SYSTEM_READY) };

    gate.sequence = 401;
    gate.opcode = SYSTEM_ALLOCATE;
    gate.status = 0;
    gate.space_id = SPACE_SYSTEM;
    gate.resource_type = RESOURCE_TIME;
    gate.amount = 2;
    gate.cid_low = sys_alloc_low;
    gate.cid_high = sys_alloc_high;
    system_entry_gate(gate as *mut LunaSystemGate);

    gate.sequence = 500;
    gate.opcode = SYSTEM_EVENT;
    gate.status = 0;
    gate.space_id = SPACE_DATA;
    gate.event_word = EVENT_DATA_READY;
    gate.cid_low = sys_event_low;
    gate.cid_high = sys_event_high;
    system_entry_gate(gate as *mut LunaSystemGate);

    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_DEVICE, manifest.device_boot_entry, b"DEVICE", b"[SYSTEM] Spawning DEVICE space...\r\n", b"[SYSTEM] Space 5 ready.\r\n");
    if !bootview.is_null() {
        let view = unsafe { &*bootview };
        if view.system_mode == LUNA_MODE_INSTALL && !run_installer_apply(view) {
            log_write(b"[SYSTEM] installer gate=blocked\r\n", bootview);
            return;
        }
    }
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_DATA, manifest.data_boot_entry, b"DATA", b"[SYSTEM] Spawning DATA space...\r\n", b"[SYSTEM] Space 2 ready.\r\n");

    let storage_state = unsafe { (*bootview).volume_state };
    if storage_state == LUNA_VOLUME_FATAL_INCOMPATIBLE || storage_state == LUNA_VOLUME_FATAL_UNRECOVERABLE {
        log_write(b"[SYSTEM] storage gate=fatal\r\n", bootview);
        return;
    }

    gate.sequence = 400;
    gate.opcode = SYSTEM_ALLOCATE;
    gate.status = 0;
    gate.space_id = SPACE_DATA;
    gate.resource_type = RESOURCE_MEMORY;
    gate.amount = 8;
    gate.cid_low = sys_alloc_low;
    gate.cid_high = sys_alloc_high;
    system_entry_gate(gate as *mut LunaSystemGate);

    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_GRAPHICS, manifest.graphics_boot_entry, b"GRAPHICS", b"[SYSTEM] Spawning GRAPHICS space...\r\n", b"[SYSTEM] Space 4 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_NETWORK, manifest.network_boot_entry, b"NETWORK", b"[SYSTEM] Spawning NETWORK space...\r\n", b"[SYSTEM] Space 6 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_TIME, manifest.time_boot_entry, b"TIME", b"[SYSTEM] Spawning TIME space...\r\n", b"[SYSTEM] Space 8 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_OBSERVABILITY, manifest.observe_boot_entry, b"OBSERVE", b"[SYSTEM] Spawning OBSERVE space...\r\n", b"[SYSTEM] Space 10 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_PROGRAM, manifest.program_boot_entry, b"PROGRAM", b"[SYSTEM] Spawning PROGRAM space...\r\n", b"[SYSTEM] Space 12 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_AI, manifest.ai_boot_entry, b"AI", b"[SYSTEM] Spawning AI space...\r\n", b"[SYSTEM] Space 11 ready.\r\n");
    if storage_state == LUNA_VOLUME_RECOVERY_REQUIRED {
        log_write(b"[SYSTEM] storage gate=recovery\r\n", bootview);
        return;
    }
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_PACKAGE, manifest.package_boot_entry, b"PACKAGE", b"[SYSTEM] Spawning PACKAGE space...\r\n", b"[SYSTEM] Space 13 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_UPDATE, manifest.update_boot_entry, b"UPDATE", b"[SYSTEM] Spawning UPDATE space...\r\n", b"[SYSTEM] Space 14 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_USER, manifest.user_boot_entry, b"USER", b"[SYSTEM] Spawning USER space...\r\n", b"");
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn system_entry_gate(gate: *mut LunaSystemGate) {
    let gate = unsafe { &mut *gate };
    gate.result_count = 0;
    match gate.opcode {
        OP_REGISTER => unsafe { register_space(gate) },
        OP_QUERY => unsafe { query_space(gate) },
        OP_QUERY_SPACES => unsafe { query_spaces(gate) },
        OP_QUERY_EVENTS => unsafe { query_events(gate) },
        OP_ALLOCATE => unsafe { allocate_resource(gate) },
        OP_EVENT => unsafe { report_event(gate) },
        _ => gate.status = STATUS_NOT_FOUND,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ___chkstk_ms() {}

#[unsafe(no_mangle)]
pub extern "C" fn __chkstk() {}

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

#[unsafe(no_mangle)]
pub unsafe extern "C" fn memcpy(dest: *mut u8, src: *const u8, len: usize) -> *mut u8 {
    let mut index = 0usize;
    while index < len {
        unsafe {
            write_volatile(dest.add(index), *src.add(index));
        }
        index += 1;
    }
    dest
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    loop {
        unsafe {
            asm!("hlt", options(nomem, nostack, preserves_flags));
        }
    }
}
