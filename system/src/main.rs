#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::panic::PanicInfo;
use core::ptr::{copy_nonoverlapping, write_bytes, write_volatile};

const LUNA_MANIFEST_ADDR: u64 = 0x39000;

const CAP_SYSTEM_REGISTER_LOW: u64 = 0x51C0_0001;
const CAP_SYSTEM_REGISTER_HIGH: u64 = 0x5100_3001;
const CAP_SYSTEM_QUERY_LOW: u64 = 0x51C0_0002;
const CAP_SYSTEM_QUERY_HIGH: u64 = 0x5100_3002;
const CAP_SYSTEM_ALLOCATE_LOW: u64 = 0x51C0_0003;
const CAP_SYSTEM_ALLOCATE_HIGH: u64 = 0x5100_3003;
const CAP_SYSTEM_EVENT_LOW: u64 = 0x51C0_0004;
const CAP_SYSTEM_EVENT_HIGH: u64 = 0x5100_3004;

const GATE_REQUEST_CAP: u32 = 1;
const CAP_LIFE_WAKE: u64 = 0xC901;
const CAP_LIFE_SPAWN: u64 = 0xC903;
const CAP_SYSTEM_REGISTER: u64 = 0xB101;
const CAP_SYSTEM_ALLOCATE: u64 = 0xB103;
const CAP_SYSTEM_EVENT: u64 = 0xB104;
const LIFE_WAKE: u32 = 1;
const LIFE_SPAWN: u32 = 4;
const SYSTEM_REGISTER: u32 = 1;
const SYSTEM_ALLOCATE: u32 = 3;
const SYSTEM_EVENT: u32 = 4;
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
const SPACE_TEST: u32 = 16;
const UNIT_LIVE: u32 = 2;
const RESOURCE_MEMORY: u32 = 1;
const RESOURCE_TIME: u32 = 2;

const OP_REGISTER: u32 = 1;
const OP_QUERY: u32 = 2;
const OP_ALLOCATE: u32 = 3;
const OP_EVENT: u32 = 4;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xB100;
const STATUS_NOT_FOUND: u32 = 0xB101;
const STATUS_NO_ROOM: u32 = 0xB102;

const STATE_ACTIVE: u32 = 1;
const CAPACITY: usize = 16;

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
struct LunaGate {
    sequence: u32,
    opcode: u32,
    caller_space: u64,
    domain_key: u64,
    cid_low: u64,
    cid_high: u64,
    status: u32,
    count: u32,
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
struct LunaBootView {
    security_gate_base: u64,
    data_gate_base: u64,
    lifecycle_gate_base: u64,
    system_gate_base: u64,
    time_gate_base: u64,
    device_gate_base: u64,
    observe_gate_base: u64,
    network_gate_base: u64,
    graphics_gate_base: u64,
    ai_gate_base: u64,
    package_gate_base: u64,
    update_gate_base: u64,
    program_gate_base: u64,
    data_buffer_base: u64,
    data_buffer_size: u64,
    list_buffer_base: u64,
    list_buffer_size: u64,
    serial_port: u32,
    reserved0: u32,
    reserve_base: u64,
    reserve_size: u64,
}

#[repr(C)]
struct LunaManifest {
    magic: u32,
    version: u32,
    security_base: u64,
    security_boot_entry: u64,
    security_gate_entry: u64,
    data_base: u64,
    data_boot_entry: u64,
    data_gate_entry: u64,
    lifecycle_base: u64,
    lifecycle_boot_entry: u64,
    lifecycle_gate_entry: u64,
    system_base: u64,
    system_boot_entry: u64,
    system_gate_entry: u64,
    time_base: u64,
    time_boot_entry: u64,
    time_gate_entry: u64,
    device_base: u64,
    device_boot_entry: u64,
    device_gate_entry: u64,
    observe_base: u64,
    observe_boot_entry: u64,
    observe_gate_entry: u64,
    network_base: u64,
    network_boot_entry: u64,
    network_gate_entry: u64,
    graphics_base: u64,
    graphics_boot_entry: u64,
    graphics_gate_entry: u64,
    ai_base: u64,
    ai_boot_entry: u64,
    ai_gate_entry: u64,
    package_base: u64,
    package_boot_entry: u64,
    package_gate_entry: u64,
    update_base: u64,
    update_boot_entry: u64,
    update_gate_entry: u64,
    program_base: u64,
    program_boot_entry: u64,
    program_gate_entry: u64,
    user_base: u64,
    user_boot_entry: u64,
    test_base: u64,
    test_boot_entry: u64,
    app_hello_base: u64,
    app_hello_size: u64,
    security_gate_base: u64,
    data_gate_base: u64,
    lifecycle_gate_base: u64,
    system_gate_base: u64,
    time_gate_base: u64,
    device_gate_base: u64,
    observe_gate_base: u64,
    network_gate_base: u64,
    graphics_gate_base: u64,
    ai_gate_base: u64,
    package_gate_base: u64,
    update_gate_base: u64,
    program_gate_base: u64,
    data_buffer_base: u64,
    data_buffer_size: u64,
    list_buffer_base: u64,
    list_buffer_size: u64,
    reserve_base: u64,
    reserve_size: u64,
    data_store_lba: u64,
    data_object_capacity: u64,
    data_slot_sectors: u64,
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
static mut NEXT_TICKET: u64 = 1;

unsafe fn record_ptr(index: usize) -> *mut LunaSystemRecord {
    unsafe { (core::ptr::addr_of_mut!(RECORDS) as *mut LunaSystemRecord).add(index) }
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
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == 0
}

fn lifecycle_call(gate: &mut LunaLifecycleGate) -> u32 {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let entry: extern "sysv64" fn(*mut LunaLifecycleGate) =
        unsafe { core::mem::transmute(manifest.lifecycle_gate_entry as usize as *const ()) };
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

fn allow_register(low: u64, high: u64) -> bool {
    (low, high) == (CAP_SYSTEM_REGISTER_LOW, CAP_SYSTEM_REGISTER_HIGH)
}

fn allow_query(low: u64, high: u64) -> bool {
    (low, high) == (CAP_SYSTEM_QUERY_LOW, CAP_SYSTEM_QUERY_HIGH) || allow_register(low, high)
}

fn allow_allocate(low: u64, high: u64) -> bool {
    (low, high) == (CAP_SYSTEM_ALLOCATE_LOW, CAP_SYSTEM_ALLOCATE_HIGH) || allow_register(low, high)
}

fn allow_event(low: u64, high: u64) -> bool {
    (low, high) == (CAP_SYSTEM_EVENT_LOW, CAP_SYSTEM_EVENT_HIGH) || allow_register(low, high)
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

unsafe fn register_space(gate: &mut LunaSystemGate) {
    if !allow_register(gate.cid_low, gate.cid_high) {
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
    if !allow_query(gate.cid_low, gate.cid_high) {
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

unsafe fn allocate_resource(gate: &mut LunaSystemGate) {
    if !allow_allocate(gate.cid_low, gate.cid_high) {
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
    if !allow_event(gate.cid_low, gate.cid_high) {
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
    serial_write(announce);
    if spawn_space(life_gate, life_spawn_low, life_spawn_high, space_id, bootview as u64, entry_addr) == STATUS_OK {
        register_base(gate, sys_reg_low, sys_reg_high, space_id, name);
        system_entry_gate(gate as *mut LunaSystemGate);
        serial_write(ready);
    }
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn system_entry_boot(_bootview: *const u8) {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let bootview = _bootview as *const LunaBootView;
    let gate = unsafe { &mut *(manifest.system_gate_base as *mut LunaSystemGate) };
    let life_gate = unsafe { &mut *(manifest.lifecycle_gate_base as *mut LunaLifecycleGate) };
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
    }
    serial_write(b"[SYSTEM] atlas online\r\n");

    let _ = request_cap(CAP_LIFE_WAKE, SPACE_SYSTEM as u64, &mut life_wake_low, &mut life_wake_high);
    let _ = request_cap(CAP_LIFE_SPAWN, SPACE_SYSTEM as u64, &mut life_spawn_low, &mut life_spawn_high);
    let _ = request_cap(CAP_SYSTEM_REGISTER, SPACE_SYSTEM as u64, &mut sys_reg_low, &mut sys_reg_high);
    let _ = request_cap(CAP_SYSTEM_ALLOCATE, SPACE_SYSTEM as u64, &mut sys_alloc_low, &mut sys_alloc_high);
    let _ = request_cap(CAP_SYSTEM_EVENT, SPACE_SYSTEM as u64, &mut sys_event_low, &mut sys_event_high);

    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_BOOT);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_SECURITY);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_DATA);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_LIFECYCLE);
    wake_space(life_gate, life_wake_low, life_wake_high, SPACE_SYSTEM);

    register_base(gate, sys_reg_low, sys_reg_high, SPACE_BOOT, b"BOOT");
    system_entry_gate(gate as *mut LunaSystemGate);
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_SECURITY, b"SECURITY");
    system_entry_gate(gate as *mut LunaSystemGate);
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_DATA, b"DATA");
    system_entry_gate(gate as *mut LunaSystemGate);
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_LIFECYCLE, b"LIFECYCLE");
    system_entry_gate(gate as *mut LunaSystemGate);
    register_base(gate, sys_reg_low, sys_reg_high, SPACE_SYSTEM, b"SYSTEM");
    system_entry_gate(gate as *mut LunaSystemGate);

    gate.sequence = 400;
    gate.opcode = SYSTEM_ALLOCATE;
    gate.status = 0;
    gate.space_id = SPACE_DATA;
    gate.resource_type = RESOURCE_MEMORY;
    gate.amount = 8;
    gate.cid_low = sys_alloc_low;
    gate.cid_high = sys_alloc_high;
    system_entry_gate(gate as *mut LunaSystemGate);

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
    gate.event_word = 0x4C4146534449534B;
    gate.cid_low = sys_event_low;
    gate.cid_high = sys_event_high;
    system_entry_gate(gate as *mut LunaSystemGate);

    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_TIME, manifest.time_boot_entry, b"TIME", b"[SYSTEM] Spawning TIME space...\r\n", b"[SYSTEM] Space 8 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_DEVICE, manifest.device_boot_entry, b"DEVICE", b"[SYSTEM] Spawning DEVICE space...\r\n", b"[SYSTEM] Space 5 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_OBSERVABILITY, manifest.observe_boot_entry, b"OBSERVE", b"[SYSTEM] Spawning OBSERVE space...\r\n", b"[SYSTEM] Space 10 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_NETWORK, manifest.network_boot_entry, b"NETWORK", b"[SYSTEM] Spawning NETWORK space...\r\n", b"[SYSTEM] Space 6 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_GRAPHICS, manifest.graphics_boot_entry, b"GRAPHICS", b"[SYSTEM] Spawning GRAPHICS space...\r\n", b"[SYSTEM] Space 4 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_PACKAGE, manifest.package_boot_entry, b"PACKAGE", b"[SYSTEM] Spawning PACKAGE space...\r\n", b"[SYSTEM] Space 13 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_UPDATE, manifest.update_boot_entry, b"UPDATE", b"[SYSTEM] Spawning UPDATE space...\r\n", b"[SYSTEM] Space 14 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_AI, manifest.ai_boot_entry, b"AI", b"[SYSTEM] Spawning AI space...\r\n", b"[SYSTEM] Space 11 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_PROGRAM, manifest.program_boot_entry, b"PROGRAM", b"[SYSTEM] Spawning PROGRAM space...\r\n", b"[SYSTEM] Space 12 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_USER, manifest.user_boot_entry, b"USER", b"[SYSTEM] Spawning USER space...\r\n", b"[SYSTEM] Space 7 ready.\r\n");
    spawn_and_register(manifest, bootview, gate, life_gate, sys_reg_low, sys_reg_high, life_spawn_low, life_spawn_high, SPACE_TEST, manifest.test_boot_entry, b"TEST", b"[SYSTEM] Spawning TEST space...\r\n", b"[SYSTEM] Space 16 ready.\r\n");
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn system_entry_gate(gate: *mut LunaSystemGate) {
    let gate = unsafe { &mut *gate };
    gate.result_count = 0;
    match gate.opcode {
        OP_REGISTER => unsafe { register_space(gate) },
        OP_QUERY => unsafe { query_space(gate) },
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

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    loop {
        unsafe {
            asm!("hlt", options(nomem, nostack, preserves_flags));
        }
    }
}
