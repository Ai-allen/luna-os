#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::mem::size_of;
use core::panic::PanicInfo;

include!("../../include/luna_layout.rs");
include!("../../include/luna_proto.rs");
const GATE_REQUEST_CAP: u32 = 1;
const GATE_VALIDATE_CAP: u32 = 6;
const CAP_LIFE_WAKE: u64 = 0xC901;
const CAP_LIFE_READ: u64 = 0xC902;
const CAP_LIFE_SPAWN: u64 = 0xC903;
const CAP_DEVICE_WRITE: u64 = 0xA503;

const OP_WAKE: u32 = 1;
const OP_REST: u32 = 2;
const OP_READ: u32 = 3;
const OP_SPAWN: u32 = 4;
const DEVICE_WRITE: u32 = 3;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xC100;
const STATUS_NOT_FOUND: u32 = 0xC101;
const STATUS_NO_ROOM: u32 = 0xC102;

const SPACE_CAPACITY: usize = 16;
const STATE_LIVE: u32 = 2;
const STATE_REST: u32 = 3;
const STACK_DATA_TOP: u64 = 0xC0000;
const STACK_LIFECYCLE_TOP: u64 = 0x9C000;
const STACK_SYSTEM_TOP: u64 = 0xA0000;
const STACK_PROGRAM_TOP: u64 = 0x1C0000;
const STACK_TIME_TOP: u64 = 0x1C4000;
const STACK_DEVICE_TOP: u64 = 0x1C8000;
const STACK_OBSERVE_TOP: u64 = 0x1CC000;
const STACK_NETWORK_TOP: u64 = 0x1D0000;
const STACK_GRAPHICS_TOP: u64 = 0x1D4000;
const STACK_PACKAGE_TOP: u64 = 0x1D8000;
const STACK_UPDATE_TOP: u64 = 0x1DC000;
const STACK_AI_TOP: u64 = 0x1E0000;
const STACK_USER_TOP: u64 = 0x1E4000;
const STACK_DIAG_TOP: u64 = 0x1E8000;

#[repr(C)]
pub struct LunaLifecycleGate {
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
    caller_space: u64,
    actor_space: u64,
    cid_low: u64,
    cid_high: u64,
    size: u64,
    buffer_addr: u64,
    buffer_size: u64,
}

type SpaceBootFn = extern "sysv64" fn(*const LunaBootView);

#[repr(C)]
#[derive(Copy, Clone)]
struct LunaUnitRecord {
    space_id: u32,
    state: u32,
    epoch: u64,
    flags: u64,
}

const EMPTY_UNIT: LunaUnitRecord = LunaUnitRecord {
    space_id: 0,
    state: 0,
    epoch: 0,
    flags: 0,
};

static mut UNITS: [LunaUnitRecord; SPACE_CAPACITY] = [EMPTY_UNIT; SPACE_CAPACITY];
static mut NEXT_EPOCH: u64 = 1;
static mut BOOTVIEW_PTR: *const LunaBootView = core::ptr::null();
static mut DEVICE_WRITE_LOW: u64 = 0;
static mut DEVICE_WRITE_HIGH: u64 = 0;
static mut DEVICE_LOG_READY: bool = false;

unsafe fn unit_ptr(index: usize) -> *mut LunaUnitRecord {
    unsafe { (core::ptr::addr_of_mut!(UNITS) as *mut LunaUnitRecord).add(index) }
}

#[inline(always)]
unsafe fn outb(port: u16, value: u8) {
    unsafe {
        asm!(
            "out dx, al",
            in("dx") port,
            in("al") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

#[inline(always)]
unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    unsafe {
        asm!(
            "in al, dx",
            in("dx") port,
            out("al") value,
            options(nomem, nostack, preserves_flags)
        );
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

fn request_cap(domain_key: u64, out_low: &mut u64, out_high: &mut u64) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        core::ptr::write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>());
    }
    gate.sequence = 80;
    gate.opcode = GATE_REQUEST_CAP;
    gate.caller_space = 9;
    gate.domain_key = domain_key;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    *out_low = gate.cid_low;
    *out_high = gate.cid_high;
    gate.status == STATUS_OK
}

fn try_enable_device_log() {
    if unsafe { DEVICE_LOG_READY } {
        return;
    }
    let mut low = 0u64;
    let mut high = 0u64;
    if request_cap(CAP_DEVICE_WRITE, &mut low, &mut high) {
        unsafe {
            DEVICE_WRITE_LOW = low;
            DEVICE_WRITE_HIGH = high;
            DEVICE_LOG_READY = true;
        }
    }
}

fn device_write(text: &[u8]) -> bool {
    let bootview = unsafe { BOOTVIEW_PTR };
    if bootview.is_null() {
        return false;
    }
    let bootview = unsafe { &*bootview };
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(bootview.device_gate_base as *mut LunaDeviceGate) };
    unsafe {
        core::ptr::write_bytes(gate as *mut LunaDeviceGate as *mut u8, 0, size_of::<LunaDeviceGate>());
    }
    gate.sequence = 81;
    gate.opcode = DEVICE_WRITE;
    gate.device_id = 1;
    gate.caller_space = 9;
    gate.actor_space = 9;
    gate.cid_low = unsafe { DEVICE_WRITE_LOW };
    gate.cid_high = unsafe { DEVICE_WRITE_HIGH };
    gate.size = text.len() as u64;
    gate.buffer_addr = text.as_ptr() as u64;
    gate.buffer_size = text.len() as u64;
    let entry: extern "sysv64" fn(*mut LunaDeviceGate) =
        unsafe { core::mem::transmute(manifest.device_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaDeviceGate);
    gate.status == STATUS_OK
}

fn log_write(text: &[u8]) {
    if unsafe { DEVICE_LOG_READY } && device_write(text) {
        return;
    }
    serial_write(text);
}

fn log_spawn_line(space_id: u32) {
    let mut line = *b"[LIFECYCLE] Spawning space 00\r\n";
    line[27] = b'0' + ((space_id / 10) % 10) as u8;
    line[28] = b'0' + (space_id % 10) as u8;
    if space_id < 10 {
        line[27] = b' ';
    }
    log_write(&line);
}

fn stack_top(space_id: u32) -> Option<u64> {
    match space_id {
        2 => Some(STACK_DATA_TOP),
        9 => Some(STACK_LIFECYCLE_TOP),
        1 => Some(STACK_SYSTEM_TOP),
        12 => Some(STACK_PROGRAM_TOP),
        8 => Some(STACK_TIME_TOP),
        5 => Some(STACK_DEVICE_TOP),
        10 => Some(STACK_OBSERVE_TOP),
        6 => Some(STACK_NETWORK_TOP),
        4 => Some(STACK_GRAPHICS_TOP),
        13 => Some(STACK_PACKAGE_TOP),
        14 => Some(STACK_UPDATE_TOP),
        11 => Some(STACK_AI_TOP),
        7 => Some(STACK_USER_TOP),
        16 => Some(STACK_DIAG_TOP),
        _ => None,
    }
}

unsafe fn run_space_boot(entry: SpaceBootFn, bootview: *const LunaBootView, new_stack_top: u64) {
    let entry_addr = entry as usize;
    unsafe {
        asm!(
            "mov rax, rsp",
            "mov rsp, {stack_top}",
            "and rsp, -16",
            "sub rsp, 16",
            "mov [rsp], rax",
            "mov rdi, {bootview}",
            "call {entry}",
            "mov rsp, [rsp]",
            stack_top = in(reg) new_stack_top,
            bootview = in(reg) bootview,
            entry = in(reg) entry_addr,
            lateout("rax") _,
            clobber_abi("sysv64"),
        );
    }
}

fn validate_cap(domain_key: u64, cid_low: u64, cid_high: u64, target_gate: u32) -> bool {
    let manifest = unsafe { &*(LUNA_MANIFEST_ADDR as *const LunaManifest) };
    let gate = unsafe { &mut *(manifest.security_gate_base as *mut LunaGate) };
    unsafe {
        core::ptr::write_bytes(gate as *mut LunaGate as *mut u8, 0, size_of::<LunaGate>());
    }
    gate.sequence = 81;
    gate.opcode = GATE_VALIDATE_CAP;
    gate.caller_space = 0;
    gate.domain_key = domain_key;
    gate.cid_low = cid_low;
    gate.cid_high = cid_high;
    gate.target_space = 9;
    gate.target_gate = target_gate;
    let entry: extern "sysv64" fn(*mut LunaGate) =
        unsafe { core::mem::transmute(manifest.security_gate_entry as usize as *const ()) };
    entry(gate as *mut LunaGate);
    gate.status == STATUS_OK
}

fn find_index(space_id: u32) -> Option<usize> {
    let mut index = 0usize;
    while index < SPACE_CAPACITY {
        if unsafe { (*unit_ptr(index)).epoch != 0 && (*unit_ptr(index)).space_id == space_id } {
            return Some(index);
        }
        index += 1;
    }
    None
}

unsafe fn upsert(space_id: u32, state: u32, flags: u64) -> bool {
    if let Some(index) = find_index(space_id) {
        unsafe {
            (*unit_ptr(index)).state = state;
            (*unit_ptr(index)).epoch = NEXT_EPOCH;
            (*unit_ptr(index)).flags = flags;
            NEXT_EPOCH = NEXT_EPOCH.wrapping_add(1);
        }
        return true;
    }

    let mut index = 0usize;
    while index < SPACE_CAPACITY {
        if unsafe { (*unit_ptr(index)).epoch } == 0 {
            unsafe {
                unit_ptr(index).write(LunaUnitRecord {
                    space_id,
                    state,
                    epoch: NEXT_EPOCH,
                    flags,
                });
                NEXT_EPOCH = NEXT_EPOCH.wrapping_add(1);
            }
            return true;
        }
        index += 1;
    }
    false
}

unsafe fn wake_unit(gate: &mut LunaLifecycleGate) {
    if !validate_cap(CAP_LIFE_WAKE, gate.cid_low, gate.cid_high, OP_WAKE) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if unsafe { upsert(gate.space_id, gate.state, gate.flags) } {
        gate.epoch = unsafe { NEXT_EPOCH.wrapping_sub(1) };
        gate.status = STATUS_OK;
    } else {
        gate.status = STATUS_NO_ROOM;
    }
}

unsafe fn rest_unit(gate: &mut LunaLifecycleGate) {
    if !validate_cap(CAP_LIFE_WAKE, gate.cid_low, gate.cid_high, OP_REST) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if let Some(index) = find_index(gate.space_id) {
        unsafe {
            (*unit_ptr(index)).state = STATE_REST;
            (*unit_ptr(index)).epoch = NEXT_EPOCH;
            NEXT_EPOCH = NEXT_EPOCH.wrapping_add(1);
            gate.epoch = (*unit_ptr(index)).epoch;
        }
        gate.status = STATUS_OK;
    } else {
        gate.status = STATUS_NOT_FOUND;
    }
}

unsafe fn read_units(gate: &mut LunaLifecycleGate) {
    if !validate_cap(CAP_LIFE_READ, gate.cid_low, gate.cid_high, OP_READ) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }

    let capacity = (gate.buffer_size as usize) / core::mem::size_of::<LunaUnitRecord>();
    let out = gate.buffer_addr as *mut LunaUnitRecord;
    let mut count = 0usize;
    let mut index = 0usize;

    while index < SPACE_CAPACITY {
        let unit = unsafe { *unit_ptr(index) };
        if unit.epoch != 0 {
            if count >= capacity {
                gate.status = STATUS_NO_ROOM;
                return;
            }
            unsafe {
                *out.add(count) = unit;
            }
            count += 1;
        }
        index += 1;
    }

    gate.result_count = count as u32;
    gate.status = STATUS_OK;
}

unsafe fn spawn_space(gate: &mut LunaLifecycleGate) {
    if !validate_cap(CAP_LIFE_SPAWN, gate.cid_low, gate.cid_high, OP_SPAWN) {
        gate.status = STATUS_INVALID_CAP;
        return;
    }
    if gate.buffer_addr == 0 || gate.entry_addr == 0 {
        gate.status = STATUS_NOT_FOUND;
        return;
    }
    let bootview = gate.buffer_addr as *const LunaBootView;
    let entry = gate.entry_addr as usize as *const ();
    let boot_fn: SpaceBootFn = unsafe { core::mem::transmute(entry) };

    let _ = unsafe { upsert(gate.space_id, STATE_LIVE, gate.flags) };
    log_spawn_line(gate.space_id);
    if let Some(new_stack_top) = stack_top(gate.space_id) {
        if gate.space_id == 5 {
            serial_write(b"[LIFECYCLE] device boot enter\r\n");
        }
        unsafe { run_space_boot(boot_fn, bootview, new_stack_top) };
        if gate.space_id == 5 {
            serial_write(b"[LIFECYCLE] device boot return\r\n");
        }
    } else {
        boot_fn(bootview);
    }
    if gate.space_id == 5 {
        serial_write(b"[LIFECYCLE] device log enable\r\n");
        try_enable_device_log();
        serial_write(b"[LIFECYCLE] device log ready\r\n");
    }
    gate.epoch = unsafe { NEXT_EPOCH.wrapping_sub(1) };
    gate.status = STATUS_OK;
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn lifecycle_entry_boot(bootview: *const u8) {
    unsafe {
        BOOTVIEW_PTR = bootview as *const LunaBootView;
    }
    unsafe {
        let _ = upsert(9, STATE_LIVE, 0);
    }
    serial_write(b"[LIFECYCLE] ledger online\r\n");
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn lifecycle_entry_gate(gate: *mut LunaLifecycleGate) {
    let gate = unsafe { &mut *gate };
    match gate.opcode {
        OP_WAKE => unsafe { wake_unit(gate) },
        OP_REST => unsafe { rest_unit(gate) },
        OP_READ => unsafe { read_units(gate) },
        OP_SPAWN => unsafe { spawn_space(gate) },
        _ => gate.status = STATUS_NOT_FOUND,
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    loop {
        unsafe {
            asm!("hlt", options(nomem, nostack, preserves_flags));
        }
    }
}
