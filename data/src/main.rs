#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::mem::size_of;
use core::panic::PanicInfo;
use core::ptr::{addr_of, addr_of_mut, copy_nonoverlapping, read_volatile, write_bytes, write_volatile};

const STORE_MAGIC: u32 = 0x5346_414C;
const STORE_VERSION: u32 = 2;
const STORE_BASE_LBA: u32 = 1024;
const OBJECT_CAPACITY: usize = 128;
const SLOT_SECTORS: u32 = 1;
const SLOT_BYTES: u64 = 512;
const RECORD_SECTORS: u32 = 22;
const DATA_START_LBA: u32 = STORE_BASE_LBA + 1 + RECORD_SECTORS;

const CAP_DATA_SEED_LOW: u64 = 0x44AF_0001;
const CAP_DATA_SEED_HIGH: u64 = 0x4400_1001;
const CAP_DATA_POUR_LOW: u64 = 0x44AF_0002;
const CAP_DATA_POUR_HIGH: u64 = 0x4400_1002;
const CAP_DATA_DRAW_LOW: u64 = 0x44AF_0003;
const CAP_DATA_DRAW_HIGH: u64 = 0x4400_1003;
const CAP_DATA_SHRED_LOW: u64 = 0x44AF_0004;
const CAP_DATA_SHRED_HIGH: u64 = 0x4400_1004;
const CAP_DATA_GATHER_LOW: u64 = 0x44AF_0005;
const CAP_DATA_GATHER_HIGH: u64 = 0x4400_1005;

const OP_SEED: u32 = 1;
const OP_POUR: u32 = 2;
const OP_DRAW: u32 = 3;
const OP_SHRED: u32 = 4;
const OP_GATHER: u32 = 5;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xD100;
const STATUS_NOT_FOUND: u32 = 0xD101;
const STATUS_NO_SPACE: u32 = 0xD102;
const STATUS_RANGE: u32 = 0xD103;
const STATUS_IO: u32 = 0xD104;
const STATUS_METADATA: u32 = 0xD105;

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

unsafe fn object_ptr(index: usize) -> *mut ObjectRecord {
    unsafe { (addr_of_mut!(OBJECTS) as *mut ObjectRecord).add(index) }
}

#[inline(always)]
unsafe fn outb(port: u16, value: u8) {
    unsafe { asm!("out dx, al", in("dx") port, in("al") value, options(nomem, nostack, preserves_flags)); }
}

#[inline(always)]
unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    unsafe { asm!("in al, dx", in("dx") port, out("al") value, options(nomem, nostack, preserves_flags)); }
    value
}

#[inline(always)]
unsafe fn outw(port: u16, value: u16) {
    unsafe { asm!("out dx, ax", in("dx") port, in("ax") value, options(nomem, nostack, preserves_flags)); }
}

#[inline(always)]
unsafe fn inw(port: u16) -> u16 {
    let value: u16;
    unsafe { asm!("in ax, dx", in("dx") port, out("ax") value, options(nomem, nostack, preserves_flags)); }
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

unsafe fn io_delay() {
    let mut count = 0u32;
    while count < 4 {
        let _ = unsafe { inb(0x3F6) };
        count += 1;
    }
}

unsafe fn ata_wait_ready() -> bool {
    let mut loops = 0u32;
    while loops < 100_000 {
        let status = unsafe { inb(0x1F7) };
        if status & 0x80 == 0 && status & 0x08 != 0 {
            return true;
        }
        if status & 0x01 != 0 {
            return false;
        }
        loops += 1;
    }
    false
}

unsafe fn ata_wait_idle() -> bool {
    let mut loops = 0u32;
    while loops < 100_000 {
        let status = unsafe { inb(0x1F7) };
        if status & 0x80 == 0 {
            return status & 0x01 == 0;
        }
        loops += 1;
    }
    false
}

unsafe fn ata_select(lba: u32, sector_count: u8, command: u8) {
    unsafe {
        outb(0x1F6, 0xE0 | ((lba >> 24) as u8 & 0x0F));
        outb(0x1F2, sector_count);
        outb(0x1F3, lba as u8);
        outb(0x1F4, (lba >> 8) as u8);
        outb(0x1F5, (lba >> 16) as u8);
        outb(0x1F7, command);
        io_delay();
    }
}

unsafe fn ata_read_sector(lba: u32, out: *mut u8) -> bool {
    unsafe {
        ata_select(lba, 1, 0x20);
        if !ata_wait_ready() {
            return false;
        }
        let mut i = 0usize;
        while i < 256 {
            let word = inw(0x1F0);
            write_volatile(out.add(i * 2), (word & 0xFF) as u8);
            write_volatile(out.add(i * 2 + 1), (word >> 8) as u8);
            i += 1;
        }
        ata_wait_idle()
    }
}

unsafe fn ata_write_sector(lba: u32, src: *const u8) -> bool {
    unsafe {
        ata_select(lba, 1, 0x30);
        if !ata_wait_ready() {
            return false;
        }
        let mut i = 0usize;
        while i < 256 {
            let lo = read_volatile(src.add(i * 2)) as u16;
            let hi = read_volatile(src.add(i * 2 + 1)) as u16;
            outw(0x1F0, lo | (hi << 8));
            i += 1;
        }
        outb(0x1F7, 0xE7);
        ata_wait_idle()
    }
}

unsafe fn load_bytes_from_disk(lba: u32, ptr: *mut u8, len: usize) -> bool {
    let sectors = len.div_ceil(512);
    let mut i = 0usize;
    while i < sectors {
        if !unsafe { ata_read_sector(lba + i as u32, ptr.add(i * 512)) } {
            return false;
        }
        i += 1;
    }
    true
}

unsafe fn save_bytes_to_disk(lba: u32, ptr: *const u8, len: usize) -> bool {
    let sectors = len.div_ceil(512);
    let mut i = 0usize;
    while i < sectors {
        if !unsafe { ata_write_sector(lba + i as u32, ptr.add(i * 512)) } {
            return false;
        }
        i += 1;
    }
    true
}

unsafe fn save_metadata() -> bool {
    unsafe {
        save_bytes_to_disk(STORE_BASE_LBA, addr_of!(SUPERBLOCK) as *const u8, size_of::<StoreSuperblock>())
            && save_bytes_to_disk(STORE_BASE_LBA + 1, addr_of!(OBJECTS) as *const u8, size_of::<[ObjectRecord; OBJECT_CAPACITY]>())
    }
}

unsafe fn format_store() -> bool {
    unsafe {
        SUPERBLOCK = EMPTY_SUPERBLOCK;
        SUPERBLOCK.magic = STORE_MAGIC;
        SUPERBLOCK.version = STORE_VERSION;
        SUPERBLOCK.object_capacity = OBJECT_CAPACITY as u32;
        SUPERBLOCK.slot_sectors = SLOT_SECTORS;
        SUPERBLOCK.store_base_lba = STORE_BASE_LBA as u64;
        SUPERBLOCK.data_start_lba = DATA_START_LBA as u64;
        SUPERBLOCK.next_nonce = 1;
        SUPERBLOCK.format_count = 1;
        SUPERBLOCK.mount_count = 1;
        OBJECTS = [EMPTY_RECORD; OBJECT_CAPACITY];

        let mut index = 0usize;
        while index < OBJECT_CAPACITY {
            write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
            if !ata_write_sector(DATA_START_LBA + index as u32, addr_of!(SECTOR_BUFFER) as *const u8) {
                return false;
            }
            index += 1;
        }

        save_metadata()
    }
}

unsafe fn load_store() -> bool {
    unsafe {
        if !load_bytes_from_disk(STORE_BASE_LBA, addr_of_mut!(SUPERBLOCK) as *mut u8, size_of::<StoreSuperblock>()) {
            return false;
        }
        if SUPERBLOCK.magic != STORE_MAGIC || SUPERBLOCK.version != STORE_VERSION {
            return format_store();
        }
        if !load_bytes_from_disk(STORE_BASE_LBA + 1, addr_of_mut!(OBJECTS) as *mut u8, size_of::<[ObjectRecord; OBJECT_CAPACITY]>()) {
            return false;
        }
        SUPERBLOCK.mount_count = SUPERBLOCK.mount_count.wrapping_add(1);
        save_metadata()
    }
}

fn allow_seed(low: u64, high: u64) -> bool {
    (low, high) == (CAP_DATA_SEED_LOW, CAP_DATA_SEED_HIGH)
}

fn allow_pour(low: u64, high: u64) -> bool {
    (low, high) == (CAP_DATA_POUR_LOW, CAP_DATA_POUR_HIGH)
}

fn allow_draw(low: u64, high: u64) -> bool {
    (low, high) == (CAP_DATA_DRAW_LOW, CAP_DATA_DRAW_HIGH) || allow_pour(low, high)
}

fn allow_shred(low: u64, high: u64) -> bool {
    (low, high) == (CAP_DATA_SHRED_LOW, CAP_DATA_SHRED_HIGH) || allow_pour(low, high)
}

fn allow_gather(low: u64, high: u64) -> bool {
    (low, high) == (CAP_DATA_GATHER_LOW, CAP_DATA_GATHER_HIGH) || allow_pour(low, high)
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
    DATA_START_LBA + (slot_index as u32 * SLOT_SECTORS)
}

unsafe fn seed_object(gate: &mut LunaDataGate) {
    if !allow_seed(gate.cid_low, gate.cid_high) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let mut index = 0usize;
    while index < OBJECT_CAPACITY {
        if unsafe { (*object_ptr(index)).live } == 0 {
            let nonce = unsafe { SUPERBLOCK.next_nonce };
            let stamp = (unsafe { SUPERBLOCK.mount_count } << 32) | nonce;
            unsafe {
                object_ptr(index).write(ObjectRecord {
                    live: 1,
                    object_type: gate.object_type,
                    flags: gate.object_flags,
                    slot_index: index as u32,
                    size: 0,
                    created_at: stamp,
                    modified_at: stamp,
                    object_low: 0x4C41_4653_0000_0000u64 | index as u64,
                    object_high: nonce,
                    reserved: [0; 4],
                });
                SUPERBLOCK.next_nonce = nonce.wrapping_add(1);
                write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
                if !ata_write_sector(object_slot_lba(index), addr_of!(SECTOR_BUFFER) as *const u8) || !save_metadata() {
                    gate.status = STATUS_IO;
                    return;
                }
                gate.object_low = (*object_ptr(index)).object_low;
                gate.object_high = (*object_ptr(index)).object_high;
                gate.created_at = (*object_ptr(index)).created_at;
                gate.content_size = 0;
                gate.status = STATUS_OK;
            }
            return;
        }
        index += 1;
    }
    gate.status = STATUS_NO_SPACE;
}

unsafe fn pour_span(gate: &mut LunaDataGate) {
    if !allow_pour(gate.cid_low, gate.cid_high) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };

    let end = gate.offset.saturating_add(gate.size);
    if end > SLOT_BYTES || gate.size > gate.buffer_size {
        gate.status = STATUS_RANGE;
        return;
    }

    unsafe {
        if !ata_read_sector(object_slot_lba(index), addr_of_mut!(SECTOR_BUFFER) as *mut u8) {
            gate.status = STATUS_IO;
            return;
        }
        copy_nonoverlapping(
            gate.buffer_addr as *const u8,
            (addr_of_mut!(SECTOR_BUFFER) as *mut u8).add(gate.offset as usize),
            gate.size as usize,
        );
        if !ata_write_sector(object_slot_lba(index), addr_of!(SECTOR_BUFFER) as *const u8) {
            gate.status = STATUS_IO;
            return;
        }
        if end > (*object_ptr(index)).size {
            (*object_ptr(index)).size = end;
        }
        (*object_ptr(index)).modified_at = SUPERBLOCK.next_nonce;
        SUPERBLOCK.next_nonce = SUPERBLOCK.next_nonce.wrapping_add(1);
        if !save_metadata() {
            gate.status = STATUS_IO;
            return;
        }
        gate.content_size = (*object_ptr(index)).size;
    }
    gate.status = STATUS_OK;
}

unsafe fn draw_span(gate: &mut LunaDataGate) {
    if !allow_draw(gate.cid_low, gate.cid_high) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };
    let size = unsafe { (*object_ptr(index)).size };
    if gate.offset > size {
        gate.status = STATUS_RANGE;
        return;
    }
    let amount = core::cmp::min(core::cmp::min(size - gate.offset, gate.size), gate.buffer_size);

    unsafe {
        if !ata_read_sector(object_slot_lba(index), addr_of_mut!(SECTOR_BUFFER) as *mut u8) {
            gate.status = STATUS_IO;
            return;
        }
        copy_nonoverlapping(
            (addr_of!(SECTOR_BUFFER) as *const u8).add(gate.offset as usize),
            gate.buffer_addr as *mut u8,
            amount as usize,
        );
        gate.size = amount;
        gate.created_at = (*object_ptr(index)).created_at;
        gate.content_size = (*object_ptr(index)).size;
    }
    gate.status = STATUS_OK;
}

unsafe fn shred_object(gate: &mut LunaDataGate) {
    if !allow_shred(gate.cid_low, gate.cid_high) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let Some(index) = find_index(gate.object_low, gate.object_high) else {
        gate.status = STATUS_NOT_FOUND;
        return;
    };

    unsafe {
        object_ptr(index).write(EMPTY_RECORD);
        write_bytes(addr_of_mut!(SECTOR_BUFFER) as *mut u8, 0, 512);
        if !ata_write_sector(object_slot_lba(index), addr_of!(SECTOR_BUFFER) as *const u8) || !save_metadata() {
            gate.status = STATUS_IO;
            return;
        }
    }
    gate.status = STATUS_OK;
}

unsafe fn gather_set(gate: &mut LunaDataGate) {
    if !allow_gather(gate.cid_low, gate.cid_high) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let out = gate.buffer_addr as *mut ObjectRef;
    let capacity = (gate.buffer_size as usize) / size_of::<ObjectRef>();
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

#[unsafe(no_mangle)]
pub extern "sysv64" fn data_entry_boot(_bootview: *const u8) {
    let ok = unsafe { load_store() };
    if ok {
        serial_write(b"[DATA] lafs disk online\r\n");
    } else {
        serial_write(b"[DATA] lafs disk fail\r\n");
    }
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn data_entry_gate(gate: *mut LunaDataGate) {
    let gate = unsafe { &mut *gate };
    gate.status = STATUS_METADATA;
    gate.result_count = 0;

    match gate.opcode {
        OP_SEED => unsafe { seed_object(gate) },
        OP_POUR => unsafe { pour_span(gate) },
        OP_DRAW => unsafe { draw_span(gate) },
        OP_SHRED => unsafe { shred_object(gate) },
        OP_GATHER => unsafe { gather_set(gate) },
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
