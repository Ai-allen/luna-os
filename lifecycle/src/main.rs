#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::panic::PanicInfo;

const CAP_LIFE_WAKE_LOW: u64 = 0x71FE_0001;
const CAP_LIFE_WAKE_HIGH: u64 = 0x7100_2001;
const CAP_LIFE_READ_LOW: u64 = 0x71FE_0002;
const CAP_LIFE_READ_HIGH: u64 = 0x7100_2002;
const CAP_LIFE_SPAWN_LOW: u64 = 0x71FE_0003;
const CAP_LIFE_SPAWN_HIGH: u64 = 0x7100_2003;

const OP_WAKE: u32 = 1;
const OP_REST: u32 = 2;
const OP_READ: u32 = 3;
const OP_SPAWN: u32 = 4;

const STATUS_OK: u32 = 0;
const STATUS_INVALID_CAP: u32 = 0xC100;
const STATUS_NOT_FOUND: u32 = 0xC101;
const STATUS_NO_ROOM: u32 = 0xC102;

const SPACE_CAPACITY: usize = 16;
const STATE_LIVE: u32 = 2;
const STATE_REST: u32 = 3;

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

fn allow_wake(low: u64, high: u64) -> bool {
    (low, high) == (CAP_LIFE_WAKE_LOW, CAP_LIFE_WAKE_HIGH)
}

fn allow_read(low: u64, high: u64) -> bool {
    (low, high) == (CAP_LIFE_READ_LOW, CAP_LIFE_READ_HIGH) || allow_wake(low, high)
}

fn allow_spawn(low: u64, high: u64) -> bool {
    (low, high) == (CAP_LIFE_SPAWN_LOW, CAP_LIFE_SPAWN_HIGH) || allow_wake(low, high)
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
    if !allow_wake(gate.cid_low, gate.cid_high) {
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
    if !allow_wake(gate.cid_low, gate.cid_high) {
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
    if !allow_read(gate.cid_low, gate.cid_high) {
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
    if !allow_spawn(gate.cid_low, gate.cid_high) {
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
    serial_write(b"[LIFECYCLE] Spawning space ");
    let tens = ((gate.space_id / 10) % 10) as u8;
    let ones = (gate.space_id % 10) as u8;
    if gate.space_id >= 10 {
        serial_putc((b'0' + tens) as u8);
    }
    serial_putc((b'0' + ones) as u8);
    serial_write(b"\r\n");
    boot_fn(bootview);
    gate.epoch = unsafe { NEXT_EPOCH.wrapping_sub(1) };
    gate.status = STATUS_OK;
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn lifecycle_entry_boot(_bootview: *const u8) {
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
