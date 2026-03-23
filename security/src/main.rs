#![no_std]
#![no_main]

use core::arch::asm;
use core::panic::PanicInfo;

const LUNA_GATE_OK: u32 = 0;
const LUNA_GATE_UNKNOWN: u32 = 0xE001;

const LUNA_CAP_DATA_SEED: u64 = 0xD201;
const LUNA_CAP_DATA_POUR: u64 = 0xD202;
const LUNA_CAP_DATA_DRAW: u64 = 0xD203;
const LUNA_CAP_DATA_SHRED: u64 = 0xD204;
const LUNA_CAP_DATA_GATHER: u64 = 0xD205;
const LUNA_CAP_LIFE_WAKE: u64 = 0xC901;
const LUNA_CAP_LIFE_READ: u64 = 0xC902;
const LUNA_CAP_LIFE_SPAWN: u64 = 0xC903;
const LUNA_CAP_SYSTEM_REGISTER: u64 = 0xB101;
const LUNA_CAP_SYSTEM_QUERY: u64 = 0xB102;
const LUNA_CAP_SYSTEM_ALLOCATE: u64 = 0xB103;
const LUNA_CAP_SYSTEM_EVENT: u64 = 0xB104;
const LUNA_CAP_PROGRAM_LOAD: u64 = 0xA201;
const LUNA_CAP_PROGRAM_START: u64 = 0xA202;
const LUNA_CAP_PROGRAM_STOP: u64 = 0xA203;
const LUNA_CAP_USER_SHELL: u64 = 0xA701;
const LUNA_CAP_TIME_PULSE: u64 = 0xA801;
const LUNA_CAP_TIME_CHIME: u64 = 0xA802;
const LUNA_CAP_DEVICE_LIST: u64 = 0xA501;
const LUNA_CAP_DEVICE_READ: u64 = 0xA502;
const LUNA_CAP_DEVICE_WRITE: u64 = 0xA503;
const LUNA_CAP_OBSERVE_LOG: u64 = 0xAA01;
const LUNA_CAP_OBSERVE_READ: u64 = 0xAA02;
const LUNA_CAP_OBSERVE_STATS: u64 = 0xAA03;
const LUNA_CAP_NETWORK_SEND: u64 = 0xA601;
const LUNA_CAP_NETWORK_RECV: u64 = 0xA602;
const LUNA_CAP_GRAPHICS_DRAW: u64 = 0xA401;
const LUNA_CAP_PACKAGE_INSTALL: u64 = 0xAD01;
const LUNA_CAP_PACKAGE_LIST: u64 = 0xAD02;
const LUNA_CAP_UPDATE_CHECK: u64 = 0xAE01;
const LUNA_CAP_UPDATE_APPLY: u64 = 0xAE02;
const LUNA_CAP_AI_INFER: u64 = 0xAB01;
const LUNA_CAP_AI_CREATE: u64 = 0xAB02;

#[repr(C)]
pub struct LunaGate {
    sequence: u32,
    opcode: u32,
    caller_space: u64,
    domain_key: u64,
    cid_low: u64,
    cid_high: u64,
    status: u32,
    count: u32,
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

fn grant(gate: &mut LunaGate, low: u64, high: u64) {
    gate.cid_low = low;
    gate.cid_high = high;
    gate.status = LUNA_GATE_OK;
    gate.count = 1;
}

fn issue_capability(gate: &mut LunaGate) {
    match gate.domain_key {
        LUNA_CAP_DATA_SEED => grant(gate, 0x44AF_0001, 0x4400_1001),
        LUNA_CAP_DATA_POUR => grant(gate, 0x44AF_0002, 0x4400_1002),
        LUNA_CAP_DATA_DRAW => grant(gate, 0x44AF_0003, 0x4400_1003),
        LUNA_CAP_DATA_SHRED => grant(gate, 0x44AF_0004, 0x4400_1004),
        LUNA_CAP_DATA_GATHER => grant(gate, 0x44AF_0005, 0x4400_1005),
        LUNA_CAP_LIFE_WAKE => grant(gate, 0x71FE_0001, 0x7100_2001),
        LUNA_CAP_LIFE_READ => grant(gate, 0x71FE_0002, 0x7100_2002),
        LUNA_CAP_LIFE_SPAWN => grant(gate, 0x71FE_0003, 0x7100_2003),
        LUNA_CAP_SYSTEM_REGISTER => grant(gate, 0x51C0_0001, 0x5100_3001),
        LUNA_CAP_SYSTEM_QUERY => grant(gate, 0x51C0_0002, 0x5100_3002),
        LUNA_CAP_SYSTEM_ALLOCATE => grant(gate, 0x51C0_0003, 0x5100_3003),
        LUNA_CAP_SYSTEM_EVENT => grant(gate, 0x51C0_0004, 0x5100_3004),
        LUNA_CAP_PROGRAM_LOAD => grant(gate, 0x62D0_0001, 0x6200_4001),
        LUNA_CAP_PROGRAM_START => grant(gate, 0x62D0_0002, 0x6200_4002),
        LUNA_CAP_PROGRAM_STOP => grant(gate, 0x62D0_0003, 0x6200_4003),
        LUNA_CAP_USER_SHELL => grant(gate, 0x6A11_0001, 0x6A00_5001),
        LUNA_CAP_TIME_PULSE => grant(gate, 0x68A1_0001, 0x6800_6001),
        LUNA_CAP_TIME_CHIME => grant(gate, 0x68A1_0002, 0x6800_6002),
        LUNA_CAP_DEVICE_LIST => grant(gate, 0x65A1_0001, 0x6500_7001),
        LUNA_CAP_DEVICE_READ => grant(gate, 0x65A1_0002, 0x6500_7002),
        LUNA_CAP_DEVICE_WRITE => grant(gate, 0x65A1_0003, 0x6500_7003),
        LUNA_CAP_OBSERVE_LOG => grant(gate, 0x6AA1_0001, 0x6A00_8001),
        LUNA_CAP_OBSERVE_READ => grant(gate, 0x6AA1_0002, 0x6A00_8002),
        LUNA_CAP_OBSERVE_STATS => grant(gate, 0x6AA1_0003, 0x6A00_8003),
        LUNA_CAP_NETWORK_SEND => grant(gate, 0x66A1_0001, 0x6600_9001),
        LUNA_CAP_NETWORK_RECV => grant(gate, 0x66A1_0002, 0x6600_9002),
        LUNA_CAP_GRAPHICS_DRAW => grant(gate, 0x64A1_0001, 0x6400_A001),
        LUNA_CAP_PACKAGE_INSTALL => grant(gate, 0x6DA1_0001, 0x6D00_B001),
        LUNA_CAP_PACKAGE_LIST => grant(gate, 0x6DA1_0002, 0x6D00_B002),
        LUNA_CAP_UPDATE_CHECK => grant(gate, 0x6EA1_0001, 0x6E00_C001),
        LUNA_CAP_UPDATE_APPLY => grant(gate, 0x6EA1_0002, 0x6E00_C002),
        LUNA_CAP_AI_INFER => grant(gate, 0x6BA1_0001, 0x6B00_D001),
        LUNA_CAP_AI_CREATE => grant(gate, 0x6BA1_0002, 0x6B00_D002),
        _ => {
            gate.status = LUNA_GATE_UNKNOWN;
            gate.count = 0;
        }
    }
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn security_entry_boot(_bootview: *const u8) {
    serial_write(b"[SECURITY] ready\r\n");
}

#[unsafe(no_mangle)]
pub extern "sysv64" fn security_entry_gate(gate: *mut LunaGate) {
    let gate = unsafe { &mut *gate };
    match gate.opcode {
        1 => issue_capability(gate),
        2 => {
            gate.cid_low = 0x44AF_0001;
            gate.cid_high = 0x4400_1001;
            gate.status = LUNA_GATE_OK;
            gate.count = 33;
        }
        _ => {
            gate.status = LUNA_GATE_UNKNOWN;
            gate.count = 0;
        }
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
