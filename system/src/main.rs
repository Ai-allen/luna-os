#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::panic::PanicInfo;
use core::ptr::{copy_nonoverlapping, write_bytes, write_volatile};

include!("../../include/luna_layout.rs");
include!("../../include/luna_proto.rs");

const GATE_REQUEST_CAP: u32 = 1;
const GATE_VALIDATE_CAP: u32 = 6;
const CAP_LIFE_WAKE: u64 = 0xC901;
const CAP_LIFE_SPAWN: u64 = 0xC903;
const CAP_SYSTEM_REGISTER: u64 = 0xB101;
const CAP_SYSTEM_QUERY: u64 = 0xB102;
const CAP_SYSTEM_ALLOCATE: u64 = 0xB103;
const CAP_SYSTEM_EVENT: u64 = 0xB104;
const CAP_DEVICE_WRITE: u64 = 0xA503;
const LIFE_WAKE: u32 = 1;
const LIFE_SPAWN: u32 = 4;
const SYSTEM_REGISTER: u32 = 1;
const SYSTEM_ALLOCATE: u32 = 3;
const SYSTEM_EVENT: u32 = 4;
const DEVICE_WRITE: u32 = 3;
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
