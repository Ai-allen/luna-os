#![no_std]
#![no_main]
#![allow(static_mut_refs)]

use core::arch::asm;
use core::mem::{size_of, transmute};
use core::panic::PanicInfo;

include!("../../include/luna_layout.rs");
const LUNA_PROTO_VERSION: u32 = 0x0000_0250;
const LUNA_GATE_OK: u32 = 0;
const LUNA_GATE_UNKNOWN: u32 = 0xE001;
const LUNA_GATE_DENIED: u32 = 0xE002;
const LUNA_GATE_EXHAUSTED: u32 = 0xE003;
const OP_REQUEST_CAP: u32 = 1;
const OP_LIST_CAPS: u32 = 2;
const OP_ISSUE_SEAL: u32 = 3;
const OP_CONSUME_SEAL: u32 = 4;
const OP_REVOKE_CAP: u32 = 5;
const OP_VALIDATE_CAP: u32 = 6;
const OP_LIST_SEALS: u32 = 7;
const OP_REVOKE_DOMAIN: u32 = 8;
const OP_POLICY_QUERY: u32 = 9;
const OP_POLICY_SYNC: u32 = 10;
const OP_AUTH_LOGIN: u32 = 11;
const OP_GOVERN: u32 = 12;
const OP_AUTH_SESSION_OPEN: u32 = 13;
const OP_AUTH_SESSION_CLOSE: u32 = 14;
const OP_CRYPTO_SECRET: u32 = 15;
const OP_TRUST_EVAL: u32 = 16;
const OP_QUERY_GOVERN: u32 = 17;
const DEFAULT_TTL: u32 = 64;
const DEFAULT_USES: u32 = 4096;
const LUNA_SPACE_SECURITY: u32 = 3;
const LUNA_SPACE_SYSTEM: u32 = 1;
const LUNA_SPACE_DATA: u32 = 2;
const LUNA_SPACE_DEVICE: u32 = 5;
const LUNA_SPACE_NETWORK: u32 = 6;
const LUNA_SPACE_USER: u32 = 7;
const LUNA_SPACE_LIFECYCLE: u32 = 9;
const LUNA_SPACE_OBSERVABILITY: u32 = 10;
const LUNA_SPACE_PROGRAM: u32 = 12;
const LUNA_SPACE_PACKAGE: u32 = 13;
const LUNA_SPACE_UPDATE: u32 = 14;
const LUNA_NETWORK_PAIR_PEER: u32 = 3;
const LUNA_NETWORK_OPEN_SESSION: u32 = 4;
const LUNA_NETWORK_OPEN_CHANNEL: u32 = 5;
const SPACE_CAPACITY: usize = 128;
const SEAL_CAPACITY: usize = 64;
const LUNA_DATA_OBJECT_CAPACITY: usize = 128;
const LUNA_CAP_DATA_SEED: u64 = 0xD201;
const LUNA_CAP_DATA_POUR: u64 = 0xD202;
const LUNA_CAP_DATA_DRAW: u64 = 0xD203;
const LUNA_CAP_DATA_SHRED: u64 = 0xD204;
const LUNA_CAP_DATA_GATHER: u64 = 0xD205;
const LUNA_CAP_DATA_QUERY: u64 = 0xD206;
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
const LUNA_CAP_USER_AUTH: u64 = 0xA702;
const LUNA_CAP_TIME_PULSE: u64 = 0xA801;
const LUNA_CAP_TIME_CHIME: u64 = 0xA802;
const LUNA_CAP_DEVICE_LIST: u64 = 0xA501;
const LUNA_CAP_DEVICE_READ: u64 = 0xA502;
const LUNA_CAP_DEVICE_WRITE: u64 = 0xA503;
const LUNA_CAP_DEVICE_PROBE: u64 = 0xA504;
const LUNA_CAP_DEVICE_BIND: u64 = 0xA505;
const LUNA_CAP_DEVICE_MMIO: u64 = 0xA506;
const LUNA_CAP_DEVICE_DMA: u64 = 0xA507;
const LUNA_CAP_DEVICE_IRQ: u64 = 0xA508;
const LUNA_CAP_OBSERVE_LOG: u64 = 0xAA01;
const LUNA_CAP_OBSERVE_READ: u64 = 0xAA02;
const LUNA_CAP_OBSERVE_STATS: u64 = 0xAA03;
const LUNA_CAP_NETWORK_SEND: u64 = 0xA601;
const LUNA_CAP_NETWORK_RECV: u64 = 0xA602;
const LUNA_CAP_NETWORK_PAIR: u64 = 0xA603;
const LUNA_CAP_NETWORK_SESSION: u64 = 0xA604;
const LUNA_CAP_GRAPHICS_DRAW: u64 = 0xA401;
const LUNA_CAP_PACKAGE_INSTALL: u64 = 0xAD01;
const LUNA_CAP_PACKAGE_LIST: u64 = 0xAD02;
const LUNA_CAP_UPDATE_CHECK: u64 = 0xAE01;
const LUNA_CAP_UPDATE_APPLY: u64 = 0xAE02;
const LUNA_CAP_AI_INFER: u64 = 0xAB01;
const LUNA_CAP_AI_CREATE: u64 = 0xAB02;
const LUNA_QUERY_TARGET_PACKAGE_CATALOG: u32 = 1;
const LUNA_QUERY_TARGET_USER_FILES: u32 = 2;
const LUNA_QUERY_TARGET_OBSERVE_LOGS: u32 = 3;
const LUNA_QUERY_PROJECT_NAME: u32 = 1u32 << 0;
const LUNA_QUERY_PROJECT_LABEL: u32 = 1u32 << 1;
const LUNA_QUERY_PROJECT_REF: u32 = 1u32 << 2;
const LUNA_QUERY_PROJECT_TYPE: u32 = 1u32 << 3;
const LUNA_QUERY_PROJECT_STATE: u32 = 1u32 << 4;
const LUNA_QUERY_PROJECT_OWNER: u32 = 1u32 << 5;
const LUNA_QUERY_PROJECT_MESSAGE: u32 = 1u32 << 6;
const LUNA_QUERY_PROJECT_VERSION: u32 = 1u32 << 7;
const LUNA_QUERY_PROJECT_CREATED: u32 = 1u32 << 8;
const LUNA_QUERY_PROJECT_UPDATED: u32 = 1u32 << 9;
const LUNA_QUERY_FLAG_AUDIT_REQUIRED: u32 = 1u32 << 0;
const LUNA_QUERY_FLAG_REDACTED: u32 = 1u32 << 1;
const LUNA_DATA_OK: u32 = 0;
const LUNA_DATA_SEED_OBJECT: u32 = 1;
const LUNA_DATA_POUR_SPAN: u32 = 2;
const LUNA_DATA_DRAW_SPAN: u32 = 3;
const LUNA_DATA_GATHER_SET: u32 = 5;
const LUNA_DATA_SET_ADD_MEMBER: u32 = 8;
const LUNA_DATA_OBJECT_TYPE_SET: u32 = 0x4C534554;
const LUNA_DATA_OBJECT_TYPE_POLICY_STATE: u32 = 0x504F4C53;
const LUNA_DATA_OBJECT_TYPE_POLICY: u32 = 0x504F4C59;
const LUNA_DATA_OBJECT_TYPE_HOST_RECORD: u32 = 0x4C485354;
const LUNA_DATA_OBJECT_TYPE_USER_RECORD: u32 = 0x4C555352;
const LUNA_DATA_OBJECT_TYPE_USER_SECRET: u32 = 0x4C555345;
const LUNA_DATA_OBJECT_TYPE_USER_HOME_ROOT: u32 = 0x4C55484D;
const LUNA_DATA_OBJECT_TYPE_USER_PROFILE: u32 = 0x4C555046;
const LUNA_DATA_OBJECT_TYPE_LUNA_APP: u32 = 0x4C554E41;
const LUNA_DATA_OBJECT_TYPE_PACKAGE_INDEX: u32 = 0x504B474E;
const LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL: u32 = 0x504B4749;
const LUNA_DATA_OBJECT_TYPE_PACKAGE_STATE: u32 = 0x504B4753;
const LUNA_DATA_OBJECT_TYPE_TRUSTED_SOURCE: u32 = 0x54525354;
const LUNA_DATA_OBJECT_TYPE_UPDATE_TXN: u32 = 0x55505458;
const LUNA_POLICY_STATE_MAGIC: u32 = 0x504F4C53;
const LUNA_POLICY_STATE_VERSION: u32 = 1;
const LUNA_POLICY_RECORD_MAGIC: u32 = 0x504F4C59;
const LUNA_POLICY_RECORD_VERSION: u32 = 1;
const LUNA_POLICY_TYPE_SOURCE: u32 = 1;
const LUNA_POLICY_TYPE_UPDATE: u32 = 3;
const LUNA_POLICY_TYPE_SIGNER: u32 = 4;
const LUNA_POLICY_STATE_ALLOW: u32 = 1;
const LUNA_POLICY_STATE_DENY: u32 = 2;
const LUNA_POLICY_STATE_REVOKE: u32 = 3;
const LUNA_CRYPTO_KEY_USER_AUTH: u32 = 1;
const LUNA_CRYPTO_KEY_TRUST: u32 = 2;
const LUNA_CRYPTO_KEY_LDAT: u32 = 3;
const LUNA_USER_SECRET_MAGIC: u32 = 0x5553_4543;
const LUNA_USER_RECORD_VERSION: u32 = 1;
const LUNA_USER_SECRET_FLAG_SECURITY_OWNED: u32 = 1u32 << 0;
const LUNA_USER_SECRET_FLAG_INSTALL_BOUND: u32 = 1u32 << 1;
const LUNA_TRUST_RESULT_ALLOW: u32 = 1;
const LUNA_TRUST_RESULT_DENY: u32 = 2;
const LUNA_TRUST_RESULT_PROTO: u32 = 3;
const LUNA_TRUST_RESULT_INTEGRITY: u32 = 4;
const LUNA_TRUST_RESULT_SIGNATURE: u32 = 5;
const LUNA_TRUST_RESULT_SIGNER: u32 = 6;
const LUNA_TRUST_RESULT_SOURCE: u32 = 7;
const LUNA_TRUST_RESULT_BINDING: u32 = 8;
const LUNA_VOLUME_HEALTHY: u32 = 1;
const LUNA_VOLUME_DEGRADED: u32 = 2;
const LUNA_VOLUME_RECOVERY_REQUIRED: u32 = 3;
const LUNA_VOLUME_FATAL_INCOMPATIBLE: u32 = 4;
const LUNA_VOLUME_FATAL_UNRECOVERABLE: u32 = 5;
const LUNA_MODE_NORMAL: u32 = 1;
const LUNA_MODE_READONLY: u32 = 2;
const LUNA_MODE_RECOVERY: u32 = 3;
const LUNA_MODE_FATAL: u32 = 4;
const LUNA_MODE_INSTALL: u32 = 5;
const LUNA_DEVICE_BLOCK_WRITE: u32 = 9;
const LUNA_DEVICE_DISPLAY_PRESENT: u32 = 10;
const LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET: u32 = 11;
const LUNA_INSTALL_TARGET_BOUND: u64 = 0x2;
const LUNA_GOVERN_MOUNT: u32 = 1;
const LUNA_GOVERN_WRITE: u32 = 2;
const LUNA_GOVERN_COMMIT: u32 = 3;
const LUNA_GOVERN_REPLAY: u32 = 4;
const LUNA_DATA_OBJECT_TYPE_DRIVER_BIND: u32 = 0x4452_5642;
const LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING: u32 = 1u32 << 0;
const LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY: u32 = 1u32 << 1;
const LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK: u32 = 1u32 << 2;
const LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY: u32 = 1u32 << 3;
const LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY: u32 = 1u32 << 4;
const LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY: u32 = 1u32 << 5;
const LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY: u32 = 1u32 << 6;
const LUNA_DRIVER_POLICY_BLOCKER_MASK: u32 = 0x0000_FFFF;

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
    app_write_entry: u64,
    framebuffer_base: u64,
    framebuffer_size: u64,
    framebuffer_width: u32,
    framebuffer_height: u32,
    framebuffer_pixels_per_scanline: u32,
    framebuffer_pixel_format: u32,
    install_uuid_low: u64,
    install_uuid_high: u64,
    system_store_lba: u64,
    data_store_lba: u64,
    volume_state: u32,
    system_mode: u32,
    installer_target_flags: u64,
    installer_target_system_lba: u64,
    installer_target_data_lba: u64,
}

#[repr(C)] pub struct LunaGate{sequence:u32,opcode:u32,caller_space:u64,domain_key:u64,cid_low:u64,cid_high:u64,status:u32,count:u32,target_space:u32,target_gate:u32,ttl:u32,uses:u32,seal_low:u64,seal_high:u64,nonce:u64,buffer_addr:u64,buffer_size:u64}
#[repr(C)] #[derive(Copy,Clone)] struct LunaPolicyQuery{policy_type:u32,state:u32,flags:u32,reserved0:u32,target_id:u64,binding_id:u64}
#[repr(C)] #[derive(Copy,Clone)] struct LunaUserAuthRequest{secret_low:u64,secret_high:u64,username:[u8;16],password:[u8;32],flags:u32,reserved0:u32}
#[repr(C)] #[derive(Copy,Clone)] struct LunaAuthSessionRequest{user_low:u64,user_high:u64,secret_low:u64,secret_high:u64,session_low:u64,session_high:u64,flags:u32,reserved0:u32}
#[repr(C)] #[derive(Copy,Clone)] struct LunaCryptoSecretRequest{key_class:u32,flags:u32,subject_low:u64,subject_high:u64,salt:u64,output_low:u64,output_high:u64,username:[u8;16],secret:[u8;32]}
#[repr(C)] #[derive(Copy,Clone)] struct LunaTrustEvalRequest{result_state:u32,flags:u32,abi_major:u16,abi_minor:u16,sdk_major:u16,sdk_minor:u16,bundle_id:u64,source_id:u64,signer_id:u64,app_version:u64,integrity_check:u64,header_bytes:u64,entry_offset:u64,signature_check:u64,blob_addr:u64,blob_size:u64,binding_id:u64,result_integrity:u64,result_signature:u64,capability_count:u32,min_proto_version:u32,max_proto_version:u32,reserved0:u32,name:[u8;16],capability_keys:[u64;4]}
#[repr(C)] #[derive(Copy,Clone)] struct LunaQueryRequest{target:u32,filter_flags:u32,projection_flags:u32,sort_mode:u32,limit:u32,namespace_id:u32,object_type:u32,state:u32,owner_low:u64,owner_high:u64,scope_low:u64,scope_high:u64,result_flags:u32,reserved0:u32}
#[repr(C)] #[derive(Copy,Clone)] struct LunaUserSecretRecord{magic:u32,version:u32,flags:u32,reserved0:u32,user_low:u64,user_high:u64,salt:u64,password_fold:u64,username:[u8;16]}
#[repr(C)] #[derive(Copy,Clone)] struct LunaCapRecord{holder_space:u32,target_space:u32,target_gate:u32,uses_left:u32,ttl:u32,reserved0:u32,domain_key:u64,cid_low:u64,cid_high:u64}
#[repr(C)] #[derive(Copy,Clone)] struct LunaSealRecord{holder_space:u32,target_space:u32,target_gate:u32,reserved0:u32,seal_low:u64,seal_high:u64}
#[repr(C)] #[derive(Copy,Clone,Default)] struct LunaObjectRef{low:u64,high:u64}
#[repr(C)] struct LunaDataGate{sequence:u32,opcode:u32,status:u32,object_type:u32,object_flags:u32,result_count:u32,cid_low:u64,cid_high:u64,object_low:u64,object_high:u64,offset:u64,size:u64,buffer_addr:u64,buffer_size:u64,created_at:u64,content_size:u64,writer_space:u32,authority_space:u32,volume_state:u32,volume_mode:u32}
#[repr(C)] #[derive(Copy,Clone)] struct LunaGovernanceQuery{action:u32,result_state:u32,writer_space:u32,authority_space:u32,mode:u32,object_type:u32,object_flags:u32,reserved0:u32,domain_key:u64,cid_low:u64,cid_high:u64,object_low:u64,object_high:u64,install_low:u64,install_high:u64}
#[repr(C)] struct LunaManifest{
magic:u32,version:u32,
security_base:u64,security_boot_entry:u64,security_gate_entry:u64,
data_base:u64,data_boot_entry:u64,data_gate_entry:u64,
lifecycle_base:u64,lifecycle_boot_entry:u64,lifecycle_gate_entry:u64,
system_base:u64,system_boot_entry:u64,system_gate_entry:u64,
time_base:u64,time_boot_entry:u64,time_gate_entry:u64,
device_base:u64,device_boot_entry:u64,device_gate_entry:u64,
observe_base:u64,observe_boot_entry:u64,observe_gate_entry:u64,
network_base:u64,network_boot_entry:u64,network_gate_entry:u64,
graphics_base:u64,graphics_boot_entry:u64,graphics_gate_entry:u64,
ai_base:u64,ai_boot_entry:u64,ai_gate_entry:u64,
package_base:u64,package_boot_entry:u64,package_gate_entry:u64,
update_base:u64,update_boot_entry:u64,update_gate_entry:u64,
program_base:u64,program_boot_entry:u64,program_gate_entry:u64,
user_base:u64,user_boot_entry:u64,
diag_base:u64,diag_boot_entry:u64,
app_hello_base:u64,app_hello_size:u64,
bootview_base:u64,
security_gate_base:u64,data_gate_base:u64,lifecycle_gate_base:u64,system_gate_base:u64,time_gate_base:u64,device_gate_base:u64,observe_gate_base:u64,network_gate_base:u64,graphics_gate_base:u64,ai_gate_base:u64,package_gate_base:u64,update_gate_base:u64,program_gate_base:u64,
data_buffer_base:u64,data_buffer_size:u64,list_buffer_base:u64,list_buffer_size:u64,reserve_base:u64,reserve_size:u64,
data_store_lba:u64,data_object_capacity:u64,data_slot_sectors:u64,
app_files_base:u64,app_files_size:u64,app_notes_base:u64,app_notes_size:u64,app_console_base:u64,app_console_size:u64,app_guard_base:u64,app_guard_size:u64,
program_app_write_entry:u64,
session_script_base:u64,session_script_size:u64,
system_store_lba:u64,install_uuid_low:u64,install_uuid_high:u64}
#[repr(C)] #[derive(Copy,Clone)] struct LunaPolicyStateRecord{magic:u32,version:u32,policy_set:LunaObjectRef}
#[repr(C)] #[derive(Copy,Clone)] struct LunaPolicyRecord{magic:u32,version:u32,policy_type:u32,state:u32,flags:u32,reserved0:u32,target_id:u64,binding_id:u64}
#[derive(Copy,Clone)] struct CapabilitySpec{domain_key:u64,base_low:u64,base_high:u64}
#[derive(Copy,Clone)] struct SessionCid{live:u32,holder_space:u32,target_space:u32,target_gate:u32,domain_key:u64,cid_low:u64,cid_high:u64,ttl:u32,uses_left:u32}
#[derive(Copy,Clone)] struct InvokeSeal{live:u32,holder_space:u32,target_space:u32,target_gate:u32,seal_low:u64,seal_high:u64}
const EMPTY_CID:SessionCid=SessionCid{live:0,holder_space:0,target_space:0,target_gate:0,domain_key:0,cid_low:0,cid_high:0,ttl:0,uses_left:0};
const EMPTY_SEAL:InvokeSeal=InvokeSeal{live:0,holder_space:0,target_space:0,target_gate:0,seal_low:0,seal_high:0};
const EMPTY_POLICY_QUERY:LunaPolicyQuery=LunaPolicyQuery{policy_type:0,state:0,flags:0,reserved0:0,target_id:0,binding_id:0};
const EMPTY_AUTH_REQUEST:LunaUserAuthRequest=LunaUserAuthRequest{secret_low:0,secret_high:0,username:[0;16],password:[0;32],flags:0,reserved0:0};
const EMPTY_AUTH_SESSION_REQUEST:LunaAuthSessionRequest=LunaAuthSessionRequest{user_low:0,user_high:0,secret_low:0,secret_high:0,session_low:0,session_high:0,flags:0,reserved0:0};
const EMPTY_CRYPTO_SECRET_REQUEST:LunaCryptoSecretRequest=LunaCryptoSecretRequest{key_class:0,flags:0,subject_low:0,subject_high:0,salt:0,output_low:0,output_high:0,username:[0;16],secret:[0;32]};
const EMPTY_TRUST_REQUEST:LunaTrustEvalRequest=LunaTrustEvalRequest{result_state:0,flags:0,abi_major:0,abi_minor:0,sdk_major:0,sdk_minor:0,bundle_id:0,source_id:0,signer_id:0,app_version:0,integrity_check:0,header_bytes:0,entry_offset:0,signature_check:0,blob_addr:0,blob_size:0,binding_id:0,result_integrity:0,result_signature:0,capability_count:0,min_proto_version:0,max_proto_version:0,reserved0:0,name:[0;16],capability_keys:[0;4]};
const EMPTY_QUERY_REQUEST:LunaQueryRequest=LunaQueryRequest{target:0,filter_flags:0,projection_flags:0,sort_mode:0,limit:0,namespace_id:0,object_type:0,state:0,owner_low:0,owner_high:0,scope_low:0,scope_high:0,result_flags:0,reserved0:0};
const EMPTY_GOVERNANCE_QUERY:LunaGovernanceQuery=LunaGovernanceQuery{action:0,result_state:0,writer_space:0,authority_space:0,mode:0,object_type:0,object_flags:0,reserved0:0,domain_key:0,cid_low:0,cid_high:0,object_low:0,object_high:0,install_low:0,install_high:0};

static CAP_TABLE:[CapabilitySpec;42]=[
CapabilitySpec{domain_key:LUNA_CAP_DATA_SEED,base_low:0x44AF0001,base_high:0x44001001},CapabilitySpec{domain_key:LUNA_CAP_DATA_POUR,base_low:0x44AF0002,base_high:0x44001002},CapabilitySpec{domain_key:LUNA_CAP_DATA_DRAW,base_low:0x44AF0003,base_high:0x44001003},CapabilitySpec{domain_key:LUNA_CAP_DATA_SHRED,base_low:0x44AF0004,base_high:0x44001004},CapabilitySpec{domain_key:LUNA_CAP_DATA_GATHER,base_low:0x44AF0005,base_high:0x44001005},CapabilitySpec{domain_key:LUNA_CAP_DATA_QUERY,base_low:0x44AF0006,base_high:0x44001006},CapabilitySpec{domain_key:LUNA_CAP_LIFE_WAKE,base_low:0x71FE0001,base_high:0x71002001},CapabilitySpec{domain_key:LUNA_CAP_LIFE_READ,base_low:0x71FE0002,base_high:0x71002002},CapabilitySpec{domain_key:LUNA_CAP_LIFE_SPAWN,base_low:0x71FE0003,base_high:0x71002003},CapabilitySpec{domain_key:LUNA_CAP_SYSTEM_REGISTER,base_low:0x51C00001,base_high:0x51003001},CapabilitySpec{domain_key:LUNA_CAP_SYSTEM_QUERY,base_low:0x51C00002,base_high:0x51003002},CapabilitySpec{domain_key:LUNA_CAP_SYSTEM_ALLOCATE,base_low:0x51C00003,base_high:0x51003003},CapabilitySpec{domain_key:LUNA_CAP_SYSTEM_EVENT,base_low:0x51C00004,base_high:0x51003004},CapabilitySpec{domain_key:LUNA_CAP_PROGRAM_LOAD,base_low:0x62D00001,base_high:0x62004001},CapabilitySpec{domain_key:LUNA_CAP_PROGRAM_START,base_low:0x62D00002,base_high:0x62004002},CapabilitySpec{domain_key:LUNA_CAP_PROGRAM_STOP,base_low:0x62D00003,base_high:0x62004003},CapabilitySpec{domain_key:LUNA_CAP_USER_SHELL,base_low:0x6A110001,base_high:0x6A005001},CapabilitySpec{domain_key:LUNA_CAP_USER_AUTH,base_low:0x6A110002,base_high:0x6A005002},CapabilitySpec{domain_key:LUNA_CAP_TIME_PULSE,base_low:0x68A10001,base_high:0x68006001},CapabilitySpec{domain_key:LUNA_CAP_TIME_CHIME,base_low:0x68A10002,base_high:0x68006002},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_LIST,base_low:0x65A10001,base_high:0x65007001},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_READ,base_low:0x65A10002,base_high:0x65007002},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_WRITE,base_low:0x65A10003,base_high:0x65007003},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_PROBE,base_low:0x65A10004,base_high:0x65007004},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_BIND,base_low:0x65A10005,base_high:0x65007005},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_MMIO,base_low:0x65A10006,base_high:0x65007006},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_DMA,base_low:0x65A10007,base_high:0x65007007},CapabilitySpec{domain_key:LUNA_CAP_DEVICE_IRQ,base_low:0x65A10008,base_high:0x65007008},CapabilitySpec{domain_key:LUNA_CAP_OBSERVE_LOG,base_low:0x6AA10001,base_high:0x6A008001},CapabilitySpec{domain_key:LUNA_CAP_OBSERVE_READ,base_low:0x6AA10002,base_high:0x6A008002},CapabilitySpec{domain_key:LUNA_CAP_OBSERVE_STATS,base_low:0x6AA10003,base_high:0x6A008003},CapabilitySpec{domain_key:LUNA_CAP_NETWORK_SEND,base_low:0x66A10001,base_high:0x66009001},CapabilitySpec{domain_key:LUNA_CAP_NETWORK_RECV,base_low:0x66A10002,base_high:0x66009002},CapabilitySpec{domain_key:LUNA_CAP_NETWORK_PAIR,base_low:0x66A10003,base_high:0x66009003},CapabilitySpec{domain_key:LUNA_CAP_NETWORK_SESSION,base_low:0x66A10004,base_high:0x66009004},CapabilitySpec{domain_key:LUNA_CAP_GRAPHICS_DRAW,base_low:0x64A10001,base_high:0x6400A001},CapabilitySpec{domain_key:LUNA_CAP_PACKAGE_INSTALL,base_low:0x6DA10001,base_high:0x6D00B001},CapabilitySpec{domain_key:LUNA_CAP_PACKAGE_LIST,base_low:0x6DA10002,base_high:0x6D00B002},CapabilitySpec{domain_key:LUNA_CAP_UPDATE_CHECK,base_low:0x6EA10001,base_high:0x6E00C001},CapabilitySpec{domain_key:LUNA_CAP_UPDATE_APPLY,base_low:0x6EA10002,base_high:0x6E00C002},CapabilitySpec{domain_key:LUNA_CAP_AI_INFER,base_low:0x6BA10001,base_high:0x6B00D001},CapabilitySpec{domain_key:LUNA_CAP_AI_CREATE,base_low:0x6BA10002,base_high:0x6B00D002}];

static mut SESSION_TABLE:[SessionCid;SPACE_CAPACITY]=[EMPTY_CID;SPACE_CAPACITY];
static mut SEAL_TABLE:[InvokeSeal;SEAL_CAPACITY]=[EMPTY_SEAL;SEAL_CAPACITY];
static mut NEXT_NONCE:u64=1;
static mut POLICY_STATE_OBJECT:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut POLICY_SET_OBJECT:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut DATA_SEED_CID:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut DATA_POUR_CID:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut DATA_DRAW_CID:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut DATA_GATHER_CID:LunaObjectRef=LunaObjectRef{low:0,high:0};
static mut GOVERN_VOLUME_STATE:u32=LUNA_VOLUME_HEALTHY;
static mut GOVERN_SYSTEM_MODE:u32=LUNA_MODE_NORMAL;
static mut INSTALLER_TARGET_FLAGS:u64=0;
#[derive(Copy,Clone)] struct AuthSession{live:u32,holder_space:u32,user_low:u64,user_high:u64,secret_low:u64,secret_high:u64,session_low:u64,session_high:u64,install_low:u64,install_high:u64}
const EMPTY_AUTH_SESSION:AuthSession=AuthSession{live:0,holder_space:0,user_low:0,user_high:0,secret_low:0,secret_high:0,session_low:0,session_high:0,install_low:0,install_high:0};
static mut AUTH_SESSION_TABLE:[AuthSession;32]=[EMPTY_AUTH_SESSION;32];

#[inline(always)] unsafe fn outb(port:u16,value:u8){unsafe{asm!("out dx, al",in("dx") port,in("al") value,options(nomem,nostack,preserves_flags));}}
#[inline(always)] unsafe fn inb(port:u16)->u8{let value:u8;unsafe{asm!("in al, dx",in("dx") port,out("al") value,options(nomem,nostack,preserves_flags));}value}
fn manifest()->&'static mut LunaManifest{unsafe{&mut *(LUNA_MANIFEST_ADDR as *mut LunaManifest)}}
fn serial_putc(value:u8){while unsafe{inb(0x3FD)}&0x20==0{} unsafe{outb(0x3F8,value)};}
fn serial_write(text:&[u8]){for byte in text{serial_putc(*byte);}}
fn serial_hex32(value:u32){let mut shift=28u32;while shift<=28{let nibble=((value>>shift)&0xFu8 as u32) as u8;let ch=if nibble<10{b'0'+nibble}else{b'A'+(nibble-10)};serial_putc(ch);if shift==0{break}shift-=4;}}
fn serial_hex64(value:u64){serial_hex32((value>>32) as u32);serial_hex32(value as u32);}
fn zero_bytes(ptr:*mut u8,len:usize){let mut i=0usize;while i<len{unsafe{*ptr.add(i)=0};i+=1;}}
fn call_data_gate(entry:u64,gate:*mut LunaDataGate){let func:extern "sysv64" fn(*mut LunaDataGate)=unsafe{transmute(entry as usize)};func(gate);}
fn find_spec(domain_key:u64)->Option<CapabilitySpec>{let mut i=0usize;while i<CAP_TABLE.len(){let s=unsafe{*CAP_TABLE.get_unchecked(i)};if s.domain_key==domain_key{return Some(s)}i+=1;}None}
fn find_cid(low:u64,high:u64)->Option<usize>{let mut i=0usize;while i<SPACE_CAPACITY{let e=unsafe{*SESSION_TABLE.get_unchecked(i)};if e.live==1&&e.cid_low==low&&e.cid_high==high{return Some(i)}i+=1;}None}
fn find_seal(low:u64,high:u64)->Option<usize>{let mut i=0usize;while i<SEAL_CAPACITY{let e=unsafe{*SEAL_TABLE.get_unchecked(i)};if e.live==1&&e.seal_low==low&&e.seal_high==high{return Some(i)}i+=1;}None}
fn alloc_cid_slot()->Option<usize>{let mut i=0usize;while i<SPACE_CAPACITY{if unsafe{(*SESSION_TABLE.get_unchecked(i)).live}==0{return Some(i)}i+=1;}None}
fn alloc_seal_slot()->Option<usize>{let mut i=0usize;while i<SEAL_CAPACITY{if unsafe{(*SEAL_TABLE.get_unchecked(i)).live}==0{return Some(i)}i+=1;}None}
fn alloc_auth_session_slot()->Option<usize>{let mut i=0usize;while i<32{if unsafe{(*AUTH_SESSION_TABLE.get_unchecked(i)).live}==0{return Some(i)}i+=1;}None}
fn find_auth_session(low:u64,high:u64)->Option<usize>{let mut i=0usize;while i<32{let s=unsafe{*AUTH_SESSION_TABLE.get_unchecked(i)};if s.live==1&&s.session_low==low&&s.session_high==high{return Some(i)}i+=1;}None}
fn next_nonce()->u64{unsafe{let n=NEXT_NONCE;NEXT_NONCE=NEXT_NONCE.wrapping_add(1);n}}
fn issue_internal_cid(domain_key:u64,target_space:u32,target_gate:u32)->LunaObjectRef{let Some(spec)=find_spec(domain_key) else{return LunaObjectRef{low:0,high:0}};let Some(index)=alloc_cid_slot() else{return LunaObjectRef{low:0,high:0}};let nonce=next_nonce();let cid_low=spec.base_low^nonce.rotate_left(7)^((index as u64)<<24);let cid_high=spec.base_high^nonce.rotate_right(11)^((LUNA_SPACE_SECURITY as u64)<<32);unsafe{*SESSION_TABLE.get_unchecked_mut(index)=SessionCid{live:1,holder_space:LUNA_SPACE_SECURITY,target_space,target_gate,domain_key,cid_low,cid_high,ttl:DEFAULT_TTL,uses_left:0xFFFF_FFFF};}LunaObjectRef{low:cid_low,high:cid_high}}

fn data_seed_object(object_type:u32,object_flags:u32,out:&mut LunaObjectRef)->u32{let m=manifest();let gate=m.data_gate_base as *mut LunaDataGate;zero_bytes(gate.cast::<u8>(),size_of::<LunaDataGate>());unsafe{(*gate).sequence=200;(*gate).opcode=LUNA_DATA_SEED_OBJECT;(*gate).cid_low=DATA_SEED_CID.low;(*gate).cid_high=DATA_SEED_CID.high;(*gate).object_type=object_type;(*gate).object_flags=object_flags;call_data_gate(m.data_gate_entry,gate);out.low=(*gate).object_low;out.high=(*gate).object_high;(*gate).status}}
fn data_pour_span(object:LunaObjectRef,buffer:*const u8,size:usize)->u32{let m=manifest();let gate=m.data_gate_base as *mut LunaDataGate;zero_bytes(gate.cast::<u8>(),size_of::<LunaDataGate>());unsafe{(*gate).sequence=201;(*gate).opcode=LUNA_DATA_POUR_SPAN;(*gate).cid_low=DATA_POUR_CID.low;(*gate).cid_high=DATA_POUR_CID.high;(*gate).object_low=object.low;(*gate).object_high=object.high;(*gate).buffer_addr=buffer as u64;(*gate).buffer_size=size as u64;(*gate).size=size as u64;call_data_gate(m.data_gate_entry,gate);(*gate).status}}
fn data_draw_span(object:LunaObjectRef,buffer:*mut u8,size:usize,out_size:&mut u64)->u32{let m=manifest();let gate=m.data_gate_base as *mut LunaDataGate;zero_bytes(buffer,size);zero_bytes(gate.cast::<u8>(),size_of::<LunaDataGate>());unsafe{(*gate).sequence=202;(*gate).opcode=LUNA_DATA_DRAW_SPAN;(*gate).cid_low=DATA_DRAW_CID.low;(*gate).cid_high=DATA_DRAW_CID.high;(*gate).object_low=object.low;(*gate).object_high=object.high;(*gate).buffer_addr=buffer as u64;(*gate).buffer_size=size as u64;(*gate).size=size as u64;call_data_gate(m.data_gate_entry,gate);*out_size=(*gate).size;(*gate).status}}
fn data_gather_set(object:LunaObjectRef,refs:&mut [LunaObjectRef],out_count:&mut u32)->u32{let m=manifest();let gate=m.data_gate_base as *mut LunaDataGate;zero_bytes(refs.as_mut_ptr().cast::<u8>(),refs.len()*size_of::<LunaObjectRef>());zero_bytes(gate.cast::<u8>(),size_of::<LunaDataGate>());unsafe{(*gate).sequence=203;(*gate).opcode=LUNA_DATA_GATHER_SET;(*gate).cid_low=DATA_GATHER_CID.low;(*gate).cid_high=DATA_GATHER_CID.high;(*gate).object_low=object.low;(*gate).object_high=object.high;(*gate).buffer_addr=refs.as_mut_ptr() as u64;(*gate).buffer_size=(refs.len()*size_of::<LunaObjectRef>()) as u64;call_data_gate(m.data_gate_entry,gate);*out_count=(*gate).result_count;(*gate).status}}
fn data_set_add_member(set_object:LunaObjectRef,member:LunaObjectRef)->u32{let m=manifest();let gate=m.data_gate_base as *mut LunaDataGate;zero_bytes(gate.cast::<u8>(),size_of::<LunaDataGate>());unsafe{(*gate).sequence=204;(*gate).opcode=LUNA_DATA_SET_ADD_MEMBER;(*gate).cid_low=DATA_POUR_CID.low;(*gate).cid_high=DATA_POUR_CID.high;(*gate).object_low=set_object.low;(*gate).object_high=set_object.high;(*gate).buffer_addr=(&member as *const LunaObjectRef) as u64;(*gate).buffer_size=size_of::<LunaObjectRef>() as u64;call_data_gate(m.data_gate_entry,gate);(*gate).status}}

fn approve_cap_request(gate:&mut LunaGate)->bool{if gate.domain_key==LUNA_CAP_NETWORK_PAIR{if gate.target_space==0{gate.target_space=LUNA_SPACE_NETWORK}if gate.target_space!=LUNA_SPACE_NETWORK{return false}if gate.target_gate!=0&&gate.target_gate!=LUNA_NETWORK_PAIR_PEER{return false}}if gate.domain_key==LUNA_CAP_NETWORK_SESSION{if gate.target_space==0{gate.target_space=LUNA_SPACE_NETWORK}if gate.target_space!=LUNA_SPACE_NETWORK{return false}if gate.target_gate!=0&&gate.target_gate!=LUNA_NETWORK_OPEN_SESSION&&gate.target_gate!=LUNA_NETWORK_OPEN_CHANNEL{return false}}if gate.domain_key==LUNA_CAP_DEVICE_WRITE&&gate.target_gate!=0{if gate.target_space==0{gate.target_space=LUNA_SPACE_DEVICE}if gate.target_space!=LUNA_SPACE_DEVICE{return false}if gate.target_gate!=LUNA_DEVICE_DISPLAY_PRESENT&&gate.target_gate!=LUNA_DEVICE_BLOCK_WRITE&&gate.target_gate!=LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET{return false}if gate.target_gate==LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET{if gate.caller_space!=LUNA_SPACE_SYSTEM as u64{return false}if unsafe{GOVERN_SYSTEM_MODE}!=LUNA_MODE_INSTALL{return false}if unsafe{INSTALLER_TARGET_FLAGS}&LUNA_INSTALL_TARGET_BOUND==0{return false}}}true}
fn issue_session_cid(gate:&mut LunaGate){if !approve_cap_request(gate){gate.status=LUNA_GATE_DENIED;gate.count=0;return}let Some(spec)=find_spec(gate.domain_key) else{gate.status=LUNA_GATE_UNKNOWN;gate.count=0;return};let Some(index)=alloc_cid_slot() else{gate.status=LUNA_GATE_EXHAUSTED;gate.count=0;return};let ttl=if gate.ttl==0{DEFAULT_TTL}else{gate.ttl};let uses=if gate.uses==0{DEFAULT_USES}else{gate.uses};let nonce=next_nonce();let caller=gate.caller_space as u32;let cid_low=spec.base_low^nonce.rotate_left(7)^((index as u64)<<24);let cid_high=spec.base_high^nonce.rotate_right(11)^((caller as u64)<<32);unsafe{*SESSION_TABLE.get_unchecked_mut(index)=SessionCid{live:1,holder_space:caller,target_space:gate.target_space,target_gate:gate.target_gate,domain_key:spec.domain_key,cid_low,cid_high,ttl,uses_left:uses};}gate.cid_low=cid_low;gate.cid_high=cid_high;gate.ttl=ttl;gate.uses=uses;gate.nonce=nonce;gate.status=LUNA_GATE_OK;gate.count=1;}
fn validate_cap(gate:&mut LunaGate){
let Some(index)=find_cid(gate.cid_low,gate.cid_high) else{
if gate.domain_key==LUNA_CAP_DEVICE_WRITE&&gate.target_gate==LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET{serial_write(b"[SECURITY] install writer deny reason=missing-cid\r\n");}
gate.status=LUNA_GATE_DENIED;gate.count=0;return};
unsafe{
let cid=SESSION_TABLE.get_unchecked_mut(index);
let denied=cid.live!=1||cid.domain_key!=gate.domain_key||cid.uses_left==0||(gate.caller_space!=0&&cid.holder_space!=gate.caller_space as u32)||(gate.target_space!=0&&cid.target_space!=0&&cid.target_space!=gate.target_space)||(gate.target_gate!=0&&cid.target_gate!=0&&cid.target_gate!=gate.target_gate);
if denied{
if gate.domain_key==LUNA_CAP_DEVICE_WRITE&&gate.target_gate==LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET{
serial_write(b"[SECURITY] install writer deny uses=");
serial_hex32(cid.uses_left);
serial_write(b" holder=");
serial_hex32(cid.holder_space);
serial_write(b" target-space=");
serial_hex32(cid.target_space);
serial_write(b" target-gate=");
serial_hex32(cid.target_gate);
serial_write(b"\r\n");
}
gate.status=LUNA_GATE_DENIED;gate.count=0;return}
cid.uses_left=cid.uses_left.saturating_sub(1);gate.ttl=cid.ttl;gate.uses=cid.uses_left;}
gate.status=LUNA_GATE_OK;gate.count=1;}
fn consume_session_cap(gate:&mut LunaGate,domain_key:u64)->bool{let Some(index)=find_cid(gate.cid_low,gate.cid_high) else{return false};unsafe{let cid=SESSION_TABLE.get_unchecked_mut(index);if cid.live!=1||cid.domain_key!=domain_key||cid.uses_left==0||(gate.caller_space!=0&&cid.holder_space!=gate.caller_space as u32){return false}cid.uses_left=cid.uses_left.saturating_sub(1);gate.ttl=cid.ttl;gate.uses=cid.uses_left;}true}
fn issue_seal(gate:&mut LunaGate){let Some(cid_index)=find_cid(gate.cid_low,gate.cid_high) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};let Some(seal_index)=alloc_seal_slot() else{gate.status=LUNA_GATE_EXHAUSTED;gate.count=0;return};unsafe{let cid=SESSION_TABLE.get_unchecked_mut(cid_index);if cid.uses_left==0||cid.holder_space!=gate.caller_space as u32{gate.status=LUNA_GATE_DENIED;gate.count=0;return}cid.uses_left=cid.uses_left.saturating_sub(1);cid.target_space=gate.target_space;cid.target_gate=gate.target_gate;let nonce=next_nonce();let seal_low=cid.cid_low^0x5EA1_0000^nonce.rotate_left(9);let seal_high=cid.cid_high^0x5EA1_1000^((gate.target_gate as u64)<<32)^nonce.rotate_right(7);*SEAL_TABLE.get_unchecked_mut(seal_index)=InvokeSeal{live:1,holder_space:gate.caller_space as u32,target_space:gate.target_space,target_gate:gate.target_gate,seal_low,seal_high};gate.seal_low=seal_low;gate.seal_high=seal_high;gate.uses=cid.uses_left;gate.nonce=nonce;}gate.status=LUNA_GATE_OK;gate.count=1;}
fn consume_seal(gate:&mut LunaGate){let Some(index)=find_seal(gate.seal_low,gate.seal_high) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};unsafe{let seal=SEAL_TABLE.get_unchecked_mut(index);if seal.holder_space!=gate.caller_space as u32||(gate.target_space!=0&&seal.target_space!=gate.target_space)||(gate.target_gate!=0&&seal.target_gate!=gate.target_gate){gate.status=LUNA_GATE_DENIED;gate.count=0;return}seal.live=0;}gate.status=LUNA_GATE_OK;gate.count=1;}
fn revoke_cap(gate:&mut LunaGate){let Some(index)=find_cid(gate.cid_low,gate.cid_high) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};unsafe{*SESSION_TABLE.get_unchecked_mut(index)=EMPTY_CID;}gate.status=LUNA_GATE_OK;gate.count=1;}
fn revoke_domain(gate:&mut LunaGate){let mut revoked=0u32;let holder=gate.caller_space as u32;let mut i=0usize;while i<SPACE_CAPACITY{let entry=unsafe{*SESSION_TABLE.get_unchecked(i)};if entry.live==1&&entry.holder_space==holder&&entry.domain_key==gate.domain_key{unsafe{*SESSION_TABLE.get_unchecked_mut(i)=EMPTY_CID;}revoked=revoked.wrapping_add(1);}i+=1;}gate.status=LUNA_GATE_OK;gate.count=revoked;}
fn list_caps(gate:&mut LunaGate){if gate.caller_space==0{gate.status=LUNA_GATE_OK;gate.count=CAP_TABLE.len() as u32;return}let mut count=0u32;let mut written=0usize;let max_records=if gate.buffer_addr==0||gate.buffer_size==0{0usize}else{(gate.buffer_size as usize)/size_of::<LunaCapRecord>()};let records=gate.buffer_addr as *mut LunaCapRecord;let mut i=0usize;while i<SPACE_CAPACITY{let entry=unsafe{*SESSION_TABLE.get_unchecked(i)};if entry.live==1&&entry.holder_space==gate.caller_space as u32{count=count.wrapping_add(1);if written<max_records{unsafe{*records.add(written)=LunaCapRecord{holder_space:entry.holder_space,target_space:entry.target_space,target_gate:entry.target_gate,uses_left:entry.uses_left,ttl:entry.ttl,reserved0:0,domain_key:entry.domain_key,cid_low:entry.cid_low,cid_high:entry.cid_high};}written+=1;}}i+=1;}gate.status=LUNA_GATE_OK;gate.count=count;}
fn list_seals(gate:&mut LunaGate){let mut count=0u32;let mut written=0usize;let max_records=if gate.buffer_addr==0||gate.buffer_size==0{0usize}else{(gate.buffer_size as usize)/size_of::<LunaSealRecord>()};let records=gate.buffer_addr as *mut LunaSealRecord;let mut i=0usize;while i<SEAL_CAPACITY{let entry=unsafe{*SEAL_TABLE.get_unchecked(i)};if entry.live==1&&(gate.caller_space==0||entry.holder_space==gate.caller_space as u32){count=count.wrapping_add(1);if written<max_records{unsafe{*records.add(written)=LunaSealRecord{holder_space:entry.holder_space,target_space:entry.target_space,target_gate:entry.target_gate,reserved0:0,seal_low:entry.seal_low,seal_high:entry.seal_high};}written+=1;}}i+=1;}gate.status=LUNA_GATE_OK;gate.count=count;}

fn load_policy_query(gate:&mut LunaGate)->Option<LunaPolicyQuery>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaPolicyQuery>() as u64{return None}let mut query=EMPTY_POLICY_QUERY;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaPolicyQuery,&mut query as *mut LunaPolicyQuery,1);}Some(query)}
fn store_policy_query(gate:&mut LunaGate,query:&LunaPolicyQuery){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaPolicyQuery>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaPolicyQuery,gate.buffer_addr as *mut LunaPolicyQuery,1);}}
fn load_governance_query(gate:&mut LunaGate)->Option<LunaGovernanceQuery>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaGovernanceQuery>() as u64{return None}let mut query=EMPTY_GOVERNANCE_QUERY;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaGovernanceQuery,&mut query as *mut LunaGovernanceQuery,1);}Some(query)}
fn store_governance_query(gate:&mut LunaGate,query:&LunaGovernanceQuery){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaGovernanceQuery>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaGovernanceQuery,gate.buffer_addr as *mut LunaGovernanceQuery,1);}}
fn authoritative_space_for_object(object_type:u32,writer_space:u32)->u32{match object_type{LUNA_DATA_OBJECT_TYPE_POLICY|LUNA_DATA_OBJECT_TYPE_POLICY_STATE=>LUNA_SPACE_SECURITY,LUNA_DATA_OBJECT_TYPE_HOST_RECORD|LUNA_DATA_OBJECT_TYPE_USER_RECORD|LUNA_DATA_OBJECT_TYPE_USER_SECRET|LUNA_DATA_OBJECT_TYPE_USER_HOME_ROOT|LUNA_DATA_OBJECT_TYPE_USER_PROFILE=>LUNA_SPACE_USER,LUNA_DATA_OBJECT_TYPE_LUNA_APP|LUNA_DATA_OBJECT_TYPE_PACKAGE_INDEX|LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL|LUNA_DATA_OBJECT_TYPE_PACKAGE_STATE|LUNA_DATA_OBJECT_TYPE_TRUSTED_SOURCE=>LUNA_SPACE_PACKAGE,LUNA_DATA_OBJECT_TYPE_UPDATE_TXN=>LUNA_SPACE_UPDATE,LUNA_DATA_OBJECT_TYPE_SET=>match writer_space{LUNA_SPACE_SECURITY|LUNA_SPACE_USER|LUNA_SPACE_PACKAGE|LUNA_SPACE_UPDATE|LUNA_SPACE_OBSERVABILITY|LUNA_SPACE_DEVICE|LUNA_SPACE_LIFECYCLE|LUNA_SPACE_SYSTEM=>writer_space,_=>0},_=>writer_space}}
fn derive_mode_for_state(state:u32)->u32{match state{LUNA_VOLUME_HEALTHY=>unsafe{if GOVERN_SYSTEM_MODE==LUNA_MODE_INSTALL{LUNA_MODE_INSTALL}else{LUNA_MODE_NORMAL}},LUNA_VOLUME_DEGRADED=>LUNA_MODE_READONLY,LUNA_VOLUME_RECOVERY_REQUIRED=>LUNA_MODE_RECOVERY,_=>LUNA_MODE_FATAL}}
fn volume_state_rank(state:u32)->u32{match state{LUNA_VOLUME_HEALTHY=>0,LUNA_VOLUME_DEGRADED=>1,LUNA_VOLUME_RECOVERY_REQUIRED=>2,LUNA_VOLUME_FATAL_INCOMPATIBLE|LUNA_VOLUME_FATAL_UNRECOVERABLE=>3,_=>0}}
fn merged_governance_state(requested:u32)->u32{let current=unsafe{GOVERN_VOLUME_STATE};if requested==0{return current}if current==0{return requested}if volume_state_rank(current)>=volume_state_rank(requested){current}else{requested}}
fn effective_mode_for_state(state:u32)->u32{if unsafe{GOVERN_SYSTEM_MODE}==LUNA_MODE_INSTALL&&state==LUNA_VOLUME_DEGRADED{LUNA_MODE_INSTALL}else{derive_mode_for_state(state)}}
fn driver_bind_blockers(query:&LunaGovernanceQuery)->u32{if query.object_type!=LUNA_DATA_OBJECT_TYPE_DRIVER_BIND{return 0}query.object_flags&LUNA_DRIVER_POLICY_BLOCKER_MASK}
fn driver_bind_updates_floor(query:&LunaGovernanceQuery)->bool{driver_bind_blockers(query)&LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING!=0}
fn apply_driver_bind_governance(query:&mut LunaGovernanceQuery){let blockers=driver_bind_blockers(query);if blockers&LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING!=0{if query.result_state<LUNA_VOLUME_RECOVERY_REQUIRED{query.result_state=LUNA_VOLUME_RECOVERY_REQUIRED;}return}if blockers&(LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY|LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK|LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY|LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY|LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY|LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY)!=0&&query.result_state==LUNA_VOLUME_HEALTHY{query.result_state=LUNA_VOLUME_DEGRADED;}}
fn writer_allowed(writer:u32,authority:u32,mode:u32,action:u32)->bool{if action==LUNA_GOVERN_REPLAY{return writer==LUNA_SPACE_DATA||writer==0}match mode{LUNA_MODE_NORMAL=>writer==authority||authority==0,LUNA_MODE_INSTALL=>false,LUNA_MODE_READONLY=>writer==LUNA_SPACE_OBSERVABILITY&&action==LUNA_GOVERN_WRITE,LUNA_MODE_RECOVERY=>matches!(writer,LUNA_SPACE_SECURITY|LUNA_SPACE_UPDATE|LUNA_SPACE_LIFECYCLE|LUNA_SPACE_DATA),_=>false}}
fn govern(gate:&mut LunaGate){let Some(mut query)=load_governance_query(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if query.cid_low!=0||query.cid_high!=0{let Some(index)=find_cid(query.cid_low,query.cid_high) else{store_governance_query(gate,&query);gate.status=LUNA_GATE_DENIED;gate.count=0;return};let cid=unsafe{*SESSION_TABLE.get_unchecked(index)};if query.domain_key!=0&&cid.domain_key!=query.domain_key{store_governance_query(gate,&query);gate.status=LUNA_GATE_DENIED;gate.count=0;return}query.writer_space=cid.holder_space;}if query.authority_space==0{query.authority_space=authoritative_space_for_object(query.object_type,query.writer_space);}if query.install_low==0{let m=manifest();query.install_low=m.install_uuid_low;query.install_high=m.install_uuid_high;}if query.action==LUNA_GOVERN_MOUNT{query.result_state=merged_governance_state(query.result_state);query.mode=effective_mode_for_state(query.result_state);unsafe{GOVERN_VOLUME_STATE=query.result_state;GOVERN_SYSTEM_MODE=query.mode;}store_governance_query(gate,&query);gate.status=if matches!(query.result_state,LUNA_VOLUME_FATAL_INCOMPATIBLE|LUNA_VOLUME_FATAL_UNRECOVERABLE){LUNA_GATE_DENIED}else{LUNA_GATE_OK};gate.count=if gate.status==LUNA_GATE_OK{1}else{0};return}if query.result_state==0{query.result_state=unsafe{GOVERN_VOLUME_STATE};}apply_driver_bind_governance(&mut query);query.result_state=merged_governance_state(query.result_state);query.mode=effective_mode_for_state(query.result_state);if (query.object_type!=LUNA_DATA_OBJECT_TYPE_DRIVER_BIND||driver_bind_updates_floor(&query))&&volume_state_rank(query.result_state)>volume_state_rank(unsafe{GOVERN_VOLUME_STATE}){unsafe{GOVERN_VOLUME_STATE=query.result_state;GOVERN_SYSTEM_MODE=query.mode;}}let allowed=!matches!(query.result_state,LUNA_VOLUME_FATAL_INCOMPATIBLE|LUNA_VOLUME_FATAL_UNRECOVERABLE)&&writer_allowed(query.writer_space,query.authority_space,query.mode,query.action);store_governance_query(gate,&query);gate.status=if allowed{LUNA_GATE_OK}else{LUNA_GATE_DENIED};gate.count=if allowed{1}else{0};}
fn ensure_policy_store()->bool{unsafe{if POLICY_SET_OBJECT.low!=0||POLICY_SET_OBJECT.high!=0{return true}}let mut refs=[LunaObjectRef{low:0,high:0};LUNA_DATA_OBJECT_CAPACITY];let mut count=0u32;if data_gather_set(LunaObjectRef{low:0,high:0},&mut refs,&mut count)==LUNA_DATA_OK{let mut i=0usize;while i<count as usize&&i<refs.len(){let mut state=LunaPolicyStateRecord{magic:0,version:0,policy_set:LunaObjectRef{low:0,high:0}};let mut size=0u64;if data_draw_span(refs[i],(&mut state as *mut LunaPolicyStateRecord).cast::<u8>(),size_of::<LunaPolicyStateRecord>(),&mut size)==LUNA_DATA_OK&&size>=size_of::<LunaPolicyStateRecord>() as u64&&state.magic==LUNA_POLICY_STATE_MAGIC&&state.version==LUNA_POLICY_STATE_VERSION{unsafe{POLICY_STATE_OBJECT=refs[i];POLICY_SET_OBJECT=state.policy_set;}return true}i+=1;}}let mut state_ref=LunaObjectRef{low:0,high:0};let mut set_ref=LunaObjectRef{low:0,high:0};if data_seed_object(LUNA_DATA_OBJECT_TYPE_SET,0,&mut set_ref)!=LUNA_DATA_OK||data_seed_object(LUNA_DATA_OBJECT_TYPE_POLICY_STATE,0,&mut state_ref)!=LUNA_DATA_OK{return false}let state=LunaPolicyStateRecord{magic:LUNA_POLICY_STATE_MAGIC,version:LUNA_POLICY_STATE_VERSION,policy_set:set_ref};if data_pour_span(state_ref,(&state as *const LunaPolicyStateRecord).cast::<u8>(),size_of::<LunaPolicyStateRecord>())!=LUNA_DATA_OK{return false}unsafe{POLICY_STATE_OBJECT=state_ref;POLICY_SET_OBJECT=set_ref;}true}
fn find_policy(policy_type:u32,target_id:u64,out_ref:&mut LunaObjectRef,out_record:&mut LunaPolicyRecord)->bool{if !ensure_policy_store(){return false}let mut refs=[LunaObjectRef{low:0,high:0};LUNA_DATA_OBJECT_CAPACITY];let mut count=0u32;let set_object=unsafe{POLICY_SET_OBJECT};if data_gather_set(set_object,&mut refs,&mut count)!=LUNA_DATA_OK{return false}let mut i=0usize;while i<count as usize&&i<refs.len(){let mut candidate=LunaPolicyRecord{magic:0,version:0,policy_type:0,state:0,flags:0,reserved0:0,target_id:0,binding_id:0};let mut size=0u64;if data_draw_span(refs[i],(&mut candidate as *mut LunaPolicyRecord).cast::<u8>(),size_of::<LunaPolicyRecord>(),&mut size)==LUNA_DATA_OK&&size>=size_of::<LunaPolicyRecord>() as u64&&candidate.magic==LUNA_POLICY_RECORD_MAGIC&&candidate.version==LUNA_POLICY_RECORD_VERSION&&candidate.policy_type==policy_type&&candidate.target_id==target_id{*out_ref=refs[i];*out_record=candidate;return true}i+=1;}false}
fn policy_lookup(policy_type:u32,target_id:u64,out_query:&mut LunaPolicyQuery)->bool{let mut object_ref=LunaObjectRef{low:0,high:0};let mut record=LunaPolicyRecord{magic:0,version:0,policy_type:0,state:0,flags:0,reserved0:0,target_id:0,binding_id:0};if !find_policy(policy_type,target_id,&mut object_ref,&mut record){return false}out_query.policy_type=policy_type;out_query.state=record.state;out_query.flags=record.flags;out_query.target_id=target_id;out_query.binding_id=record.binding_id;true}
fn upsert_policy(query:&LunaPolicyQuery)->bool{let mut object_ref=LunaObjectRef{low:0,high:0};let mut record=LunaPolicyRecord{magic:LUNA_POLICY_RECORD_MAGIC,version:LUNA_POLICY_RECORD_VERSION,policy_type:query.policy_type,state:query.state,flags:query.flags,reserved0:0,target_id:query.target_id,binding_id:query.binding_id};if find_policy(query.policy_type,query.target_id,&mut object_ref,&mut record){record.state=query.state;record.flags=query.flags;record.binding_id=query.binding_id;return data_pour_span(object_ref,(&record as *const LunaPolicyRecord).cast::<u8>(),size_of::<LunaPolicyRecord>())==LUNA_DATA_OK}if !ensure_policy_store()||data_seed_object(LUNA_DATA_OBJECT_TYPE_POLICY,0,&mut object_ref)!=LUNA_DATA_OK{return false}let new_record=LunaPolicyRecord{magic:LUNA_POLICY_RECORD_MAGIC,version:LUNA_POLICY_RECORD_VERSION,policy_type:query.policy_type,state:query.state,flags:query.flags,reserved0:0,target_id:query.target_id,binding_id:query.binding_id};data_pour_span(object_ref,(&new_record as *const LunaPolicyRecord).cast::<u8>(),size_of::<LunaPolicyRecord>())==LUNA_DATA_OK&&data_set_add_member(unsafe{POLICY_SET_OBJECT},object_ref)==LUNA_DATA_OK}
fn policy_query(gate:&mut LunaGate){let Some(mut query)=load_policy_query(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};let mut object_ref=LunaObjectRef{low:0,high:0};let mut record=LunaPolicyRecord{magic:0,version:0,policy_type:0,state:0,flags:0,reserved0:0,target_id:0,binding_id:0};if !find_policy(query.policy_type,query.target_id,&mut object_ref,&mut record){query.state=LUNA_POLICY_STATE_DENY;query.flags=0;query.binding_id=0;store_policy_query(gate,&query);gate.status=LUNA_GATE_DENIED;gate.count=0;return}if query.binding_id!=0&&record.binding_id!=0&&query.binding_id!=record.binding_id{query.state=LUNA_POLICY_STATE_DENY;query.flags=record.flags;query.binding_id=record.binding_id;store_policy_query(gate,&query);gate.status=LUNA_GATE_DENIED;gate.count=0;return}query.state=record.state;query.flags=record.flags;query.binding_id=record.binding_id;store_policy_query(gate,&query);gate.status=if record.state==LUNA_POLICY_STATE_ALLOW{LUNA_GATE_OK}else{LUNA_GATE_DENIED};gate.count=if record.state==LUNA_POLICY_STATE_ALLOW{1}else{0};}
fn policy_sync(gate:&mut LunaGate){let Some(query)=load_policy_query(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if query.policy_type<LUNA_POLICY_TYPE_SOURCE||query.policy_type>LUNA_POLICY_TYPE_SIGNER||query.state<LUNA_POLICY_STATE_ALLOW||query.state>LUNA_POLICY_STATE_REVOKE{gate.status=LUNA_GATE_DENIED;gate.count=0;return}if !upsert_policy(&query){gate.status=LUNA_GATE_EXHAUSTED;gate.count=0;return}gate.status=LUNA_GATE_OK;gate.count=1;}
fn load_auth_request(gate:&mut LunaGate)->Option<LunaUserAuthRequest>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaUserAuthRequest>() as u64{return None}let mut query=EMPTY_AUTH_REQUEST;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaUserAuthRequest,&mut query as *mut LunaUserAuthRequest,1);}Some(query)}
fn load_auth_session_request(gate:&mut LunaGate)->Option<LunaAuthSessionRequest>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaAuthSessionRequest>() as u64{return None}let mut query=EMPTY_AUTH_SESSION_REQUEST;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaAuthSessionRequest,&mut query as *mut LunaAuthSessionRequest,1);}Some(query)}
fn store_auth_session_request(gate:&mut LunaGate,query:&LunaAuthSessionRequest){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaAuthSessionRequest>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaAuthSessionRequest,gate.buffer_addr as *mut LunaAuthSessionRequest,1);}}
fn load_crypto_secret_request(gate:&mut LunaGate)->Option<LunaCryptoSecretRequest>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaCryptoSecretRequest>() as u64{return None}let mut query=EMPTY_CRYPTO_SECRET_REQUEST;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaCryptoSecretRequest,&mut query as *mut LunaCryptoSecretRequest,1);}Some(query)}
fn store_crypto_secret_request(gate:&mut LunaGate,query:&LunaCryptoSecretRequest){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaCryptoSecretRequest>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaCryptoSecretRequest,gate.buffer_addr as *mut LunaCryptoSecretRequest,1);}}
fn load_trust_request(gate:&mut LunaGate)->Option<LunaTrustEvalRequest>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaTrustEvalRequest>() as u64{return None}let mut query=EMPTY_TRUST_REQUEST;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaTrustEvalRequest,&mut query as *mut LunaTrustEvalRequest,1);}Some(query)}
fn store_trust_request(gate:&mut LunaGate,query:&LunaTrustEvalRequest){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaTrustEvalRequest>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaTrustEvalRequest,gate.buffer_addr as *mut LunaTrustEvalRequest,1);}}
fn load_query_request(gate:&mut LunaGate)->Option<LunaQueryRequest>{if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaQueryRequest>() as u64{return None}let mut query=EMPTY_QUERY_REQUEST;unsafe{core::ptr::copy_nonoverlapping(gate.buffer_addr as *const LunaQueryRequest,&mut query as *mut LunaQueryRequest,1);}Some(query)}
fn store_query_request(gate:&mut LunaGate,query:&LunaQueryRequest){if gate.buffer_addr==0||gate.buffer_size<size_of::<LunaQueryRequest>() as u64{return}unsafe{core::ptr::copy_nonoverlapping(query as *const LunaQueryRequest,gate.buffer_addr as *mut LunaQueryRequest,1);}}
fn fixed_string_len(bytes:&[u8])->usize{let mut i=0usize;while i<bytes.len(){if bytes[i]==0{return i}i+=1;}bytes.len()}
fn bytes_equal(left:&[u8],right:&[u8])->bool{if left.len()!=right.len(){return false}let mut i=0usize;while i<left.len(){if left[i]!=right[i]{return false}i+=1;}true}
fn fold_bytes(mut seed:u64,blob:&[u8])->u64{let mut i=0usize;while i<blob.len(){seed^=blob[i] as u64;seed=seed.rotate_left(9).wrapping_mul(0x100_0000_01B3);i+=1;}seed}
fn fold_bundle_bytes(mut seed:u64,blob:&[u8])->u64{let mut i=0usize;while i<blob.len(){seed^=blob[i] as u64;seed=seed.rotate_left(5).wrapping_mul(0x100_0000_01B3);i+=1;}seed}
fn derive_secret_pair(key_class:u32,subject_low:u64,subject_high:u64,username:&[u8;16],secret:&[u8;32],salt:u64)->(u64,u64){let m=manifest();let mut value=0x9E37_79B9_7F4A_7C15u64^salt^m.install_uuid_low^m.install_uuid_high.rotate_left(13)^subject_low.rotate_left(7)^subject_high.rotate_right(11)^((key_class as u64)<<48);let mut i=0usize;let user_len=fixed_string_len(username);while i<user_len{value^=username[i] as u64;value=value.rotate_left(7).wrapping_mul(0x100_0000_01B3);i+=1;}i=0;let secret_len=fixed_string_len(secret);while i<secret_len{value^=secret[i] as u64;value=value.rotate_left(11).wrapping_mul(0x100_0000_01B3);i+=1;}value^=((user_len as u64)<<32)|(secret_len as u64);let low=value.rotate_left(17)^0xA5A5_5A5A_C3C3_3C3C;let high=(low^m.install_uuid_low.rotate_right(7)^subject_low^subject_high^((key_class as u64)<<32)).rotate_left(9);(low,high)}
fn compute_bundle_integrity(blob_addr:u64,blob_size:u64,header_bytes:u64)->u64{if blob_addr==0||header_bytes>blob_size{return 0}let payload_size=(blob_size-header_bytes) as usize;let payload=unsafe{core::slice::from_raw_parts((blob_addr+header_bytes) as *const u8,payload_size)};fold_bundle_bytes(0xCBF2_9CE4_8422_2325u64,payload)}
fn compute_bundle_signature(request:&LunaTrustEvalRequest)->u64{if request.blob_addr==0||request.header_bytes>request.blob_size{return 0}let payload_size=(request.blob_size-request.header_bytes) as usize;let payload=unsafe{core::slice::from_raw_parts((request.blob_addr+request.header_bytes) as *const u8,payload_size)};let mut seed=0x9A11_D00D_55AA_7711u64^request.signer_id^(request.source_id<<1);let mut stage=[0u8;128];for b in stage.iter_mut(){*b=0;}unsafe{core::ptr::copy_nonoverlapping(request.name.as_ptr(),stage.as_mut_ptr(),request.name.len());}seed=fold_bundle_bytes(seed,&stage[..request.name.len()]);for b in stage.iter_mut(){*b=0;}unsafe{core::ptr::copy_nonoverlapping((&request.bundle_id as *const u64).cast::<u8>(),stage.as_mut_ptr().add(0),8);core::ptr::copy_nonoverlapping((&request.source_id as *const u64).cast::<u8>(),stage.as_mut_ptr().add(8),8);core::ptr::copy_nonoverlapping((&request.app_version as *const u64).cast::<u8>(),stage.as_mut_ptr().add(16),8);core::ptr::copy_nonoverlapping((&request.capability_count as *const u32).cast::<u8>(),stage.as_mut_ptr().add(24),4);core::ptr::copy_nonoverlapping((&request.flags as *const u32).cast::<u8>(),stage.as_mut_ptr().add(28),4);core::ptr::copy_nonoverlapping((&request.abi_major as *const u16).cast::<u8>(),stage.as_mut_ptr().add(32),2);core::ptr::copy_nonoverlapping((&request.abi_minor as *const u16).cast::<u8>(),stage.as_mut_ptr().add(34),2);core::ptr::copy_nonoverlapping((&request.sdk_major as *const u16).cast::<u8>(),stage.as_mut_ptr().add(36),2);core::ptr::copy_nonoverlapping((&request.sdk_minor as *const u16).cast::<u8>(),stage.as_mut_ptr().add(38),2);core::ptr::copy_nonoverlapping((&request.min_proto_version as *const u32).cast::<u8>(),stage.as_mut_ptr().add(40),4);core::ptr::copy_nonoverlapping((&request.max_proto_version as *const u32).cast::<u8>(),stage.as_mut_ptr().add(44),4);core::ptr::copy_nonoverlapping((&request.signer_id as *const u64).cast::<u8>(),stage.as_mut_ptr().add(48),8);core::ptr::copy_nonoverlapping(request.capability_keys.as_ptr().cast::<u8>(),stage.as_mut_ptr().add(56),32);}seed=fold_bundle_bytes(seed,&stage[..88]);fold_bundle_bytes(seed,payload)}
fn auth_login(gate:&mut LunaGate){let Some(request)=load_auth_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if !consume_session_cap(gate,LUNA_CAP_USER_AUTH){serial_write(b"audit auth.login denied reason=cap\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}let mut record=LunaUserSecretRecord{magic:0,version:0,flags:0,reserved0:0,user_low:0,user_high:0,salt:0,password_fold:0,username:[0;16]};let mut size=0u64;let secret=LunaObjectRef{low:request.secret_low,high:request.secret_high};if secret.low==0||secret.high==0||data_draw_span(secret,(&mut record as *mut LunaUserSecretRecord).cast::<u8>(),size_of::<LunaUserSecretRecord>(),&mut size)!=LUNA_DATA_OK||size<size_of::<LunaUserSecretRecord>() as u64||record.magic!=LUNA_USER_SECRET_MAGIC||record.version!=LUNA_USER_RECORD_VERSION{serial_write(b"audit auth.login denied reason=secret\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}let req_user_len=fixed_string_len(&request.username);let rec_user_len=fixed_string_len(&record.username);if req_user_len==0||req_user_len!=rec_user_len||!bytes_equal(&request.username[..req_user_len],&record.username[..rec_user_len]){serial_write(b"audit auth.login denied reason=user\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if (record.flags&LUNA_USER_SECRET_FLAG_INSTALL_BOUND)==0{serial_write(b"audit auth.login denied reason=secret-policy\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if derive_secret_pair(LUNA_CRYPTO_KEY_USER_AUTH,record.user_low,record.user_high,&request.username,&request.password,record.salt).0!=record.password_fold{serial_write(b"audit auth.login denied reason=password\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}serial_write(b"audit auth.login allow\r\n");gate.status=LUNA_GATE_OK;gate.count=1;}
fn auth_session_open(gate:&mut LunaGate){let Some(mut request)=load_auth_session_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if !consume_session_cap(gate,LUNA_CAP_USER_AUTH){serial_write(b"audit auth.session denied reason=cap\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}let Some(index)=alloc_auth_session_slot() else{gate.status=LUNA_GATE_EXHAUSTED;gate.count=0;return};let nonce=next_nonce();let m=manifest();request.session_low=0x5353_4E4Cu64^request.user_low^request.secret_low^nonce.rotate_left(5);request.session_high=0x4155_5448u64^request.user_high^request.secret_high^m.install_uuid_low^nonce.rotate_right(7);unsafe{*AUTH_SESSION_TABLE.get_unchecked_mut(index)=AuthSession{live:1,holder_space:gate.caller_space as u32,user_low:request.user_low,user_high:request.user_high,secret_low:request.secret_low,secret_high:request.secret_high,session_low:request.session_low,session_high:request.session_high,install_low:m.install_uuid_low,install_high:m.install_uuid_high};}store_auth_session_request(gate,&request);serial_write(b"audit auth.session issue\r\n");gate.seal_low=request.session_low;gate.seal_high=request.session_high;gate.status=LUNA_GATE_OK;gate.count=1;}
fn auth_session_close(gate:&mut LunaGate){let Some(request)=load_auth_session_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};let Some(index)=find_auth_session(request.session_low,request.session_high) else{serial_write(b"audit auth.session revoke denied reason=missing\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return};let session=unsafe{AUTH_SESSION_TABLE.get_unchecked_mut(index)};if session.holder_space!=gate.caller_space as u32{serial_write(b"audit auth.session revoke denied reason=holder\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}*session=EMPTY_AUTH_SESSION;serial_write(b"audit auth.session revoke\r\n");gate.status=LUNA_GATE_OK;gate.count=1;}
fn crypto_secret(gate:&mut LunaGate){let Some(mut request)=load_crypto_secret_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if !consume_session_cap(gate,LUNA_CAP_USER_AUTH){serial_write(b"audit crypto.secret denied reason=cap\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}let (low,high)=derive_secret_pair(request.key_class,request.subject_low,request.subject_high,&request.username,&request.secret,request.salt);request.output_low=low;request.output_high=high;store_crypto_secret_request(gate,&request);serial_write(b"audit crypto.secret release\r\n");gate.status=LUNA_GATE_OK;gate.count=1;}
fn trust_eval(gate:&mut LunaGate){let Some(mut request)=load_trust_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};if !consume_session_cap(gate,LUNA_CAP_PACKAGE_INSTALL){serial_write(b"audit trust.eval denied reason=cap\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if request.min_proto_version!=0&&(request.min_proto_version>LUNA_PROTO_VERSION||(request.max_proto_version!=0&&request.max_proto_version<LUNA_PROTO_VERSION)){request.result_state=LUNA_TRUST_RESULT_PROTO;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=proto\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}request.result_integrity=compute_bundle_integrity(request.blob_addr,request.blob_size,request.header_bytes);request.result_signature=compute_bundle_signature(&request);if request.integrity_check!=request.result_integrity{request.result_state=LUNA_TRUST_RESULT_INTEGRITY;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=integrity\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if request.source_id!=0&&(request.signer_id==0||request.signature_check==0||request.signature_check!=request.result_signature){request.result_state=LUNA_TRUST_RESULT_SIGNATURE;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=signature\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if request.signer_id!=0{let mut signer=EMPTY_POLICY_QUERY;let mut source=EMPTY_POLICY_QUERY;if !policy_lookup(LUNA_POLICY_TYPE_SIGNER,request.signer_id,&mut signer)||signer.state!=LUNA_POLICY_STATE_ALLOW{request.result_state=LUNA_TRUST_RESULT_SIGNER;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=signer\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if !policy_lookup(LUNA_POLICY_TYPE_SOURCE,request.source_id,&mut source)||source.state!=LUNA_POLICY_STATE_ALLOW{request.result_state=LUNA_TRUST_RESULT_SOURCE;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=source\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}if source.binding_id!=0&&source.binding_id!=request.signer_id{request.result_state=LUNA_TRUST_RESULT_BINDING;request.binding_id=source.binding_id;store_trust_request(gate,&request);serial_write(b"audit trust.eval denied reason=binding\r\n");gate.status=LUNA_GATE_DENIED;gate.count=0;return}request.binding_id=source.binding_id;}else{request.binding_id=0;}request.result_state=LUNA_TRUST_RESULT_ALLOW;store_trust_request(gate,&request);serial_write(b"audit trust.eval allow\r\n");gate.status=LUNA_GATE_OK;gate.count=1;}
fn query_govern(gate:&mut LunaGate){let Some(mut request)=load_query_request(gate) else{gate.status=LUNA_GATE_DENIED;gate.count=0;return};match request.target{LUNA_QUERY_TARGET_PACKAGE_CATALOG=>{request.projection_flags&=LUNA_QUERY_PROJECT_NAME|LUNA_QUERY_PROJECT_LABEL|LUNA_QUERY_PROJECT_REF|LUNA_QUERY_PROJECT_TYPE|LUNA_QUERY_PROJECT_STATE|LUNA_QUERY_PROJECT_VERSION|LUNA_QUERY_PROJECT_CREATED|LUNA_QUERY_PROJECT_UPDATED;request.result_flags|=LUNA_QUERY_FLAG_AUDIT_REQUIRED;}LUNA_QUERY_TARGET_USER_FILES=>{if gate.caller_space as u32!=LUNA_SPACE_USER&&gate.caller_space as u32!=LUNA_SPACE_DATA&&gate.caller_space as u32!=LUNA_SPACE_SYSTEM{request.result_flags|=LUNA_QUERY_FLAG_REDACTED;store_query_request(gate,&request);gate.status=LUNA_GATE_DENIED;gate.count=0;serial_write(b"audit lasql.query denied target=files caller=");serial_hex32(gate.caller_space as u32);serial_write(b"\r\n");return}request.projection_flags&=LUNA_QUERY_PROJECT_NAME|LUNA_QUERY_PROJECT_REF|LUNA_QUERY_PROJECT_TYPE|LUNA_QUERY_PROJECT_STATE|LUNA_QUERY_PROJECT_OWNER|LUNA_QUERY_PROJECT_CREATED|LUNA_QUERY_PROJECT_UPDATED;request.result_flags|=LUNA_QUERY_FLAG_AUDIT_REQUIRED;}LUNA_QUERY_TARGET_OBSERVE_LOGS=>{if gate.caller_space as u32!=LUNA_SPACE_USER&&gate.caller_space as u32!=LUNA_SPACE_PROGRAM&&gate.caller_space as u32!=LUNA_SPACE_OBSERVABILITY&&gate.caller_space as u32!=LUNA_SPACE_SYSTEM{request.result_flags|=LUNA_QUERY_FLAG_REDACTED;store_query_request(gate,&request);gate.status=LUNA_GATE_DENIED;gate.count=0;serial_write(b"audit lasql.query denied target=logs\r\n");return}request.projection_flags&=LUNA_QUERY_PROJECT_MESSAGE|LUNA_QUERY_PROJECT_STATE|LUNA_QUERY_PROJECT_OWNER|LUNA_QUERY_PROJECT_CREATED;request.result_flags|=LUNA_QUERY_FLAG_AUDIT_REQUIRED|LUNA_QUERY_FLAG_REDACTED;} _=>{store_query_request(gate,&request);gate.status=LUNA_GATE_DENIED;gate.count=0;return}}store_query_request(gate,&request);serial_write(b"audit lasql.query govern allow\r\n");gate.status=LUNA_GATE_OK;gate.count=1;}

#[unsafe(no_mangle)] pub extern "sysv64" fn security_entry_boot(_bootview:*const u8){
serial_write(b"[SECURITY] boot enter\r\n");
if !_bootview.is_null(){let bootview=unsafe{&*(_bootview as *const LunaBootView)};unsafe{GOVERN_VOLUME_STATE=bootview.volume_state;GOVERN_SYSTEM_MODE=bootview.system_mode;}}else{unsafe{GOVERN_VOLUME_STATE=LUNA_VOLUME_HEALTHY;GOVERN_SYSTEM_MODE=LUNA_MODE_NORMAL;}}
if !_bootview.is_null(){let bootview=unsafe{&*(_bootview as *const LunaBootView)};unsafe{INSTALLER_TARGET_FLAGS=bootview.installer_target_flags;}}else{unsafe{INSTALLER_TARGET_FLAGS=0;}}
serial_write(b"[SECURITY] seed cid\r\n");
unsafe{DATA_SEED_CID=issue_internal_cid(LUNA_CAP_DATA_SEED,LUNA_SPACE_DATA,LUNA_DATA_SEED_OBJECT);}
serial_write(b"[SECURITY] pour cid\r\n");
unsafe{DATA_POUR_CID=issue_internal_cid(LUNA_CAP_DATA_POUR,LUNA_SPACE_DATA,LUNA_DATA_POUR_SPAN);}
serial_write(b"[SECURITY] draw cid\r\n");
unsafe{DATA_DRAW_CID=issue_internal_cid(LUNA_CAP_DATA_DRAW,LUNA_SPACE_DATA,LUNA_DATA_DRAW_SPAN);}
serial_write(b"[SECURITY] gather cid\r\n");
unsafe{DATA_GATHER_CID=issue_internal_cid(LUNA_CAP_DATA_GATHER,LUNA_SPACE_DATA,LUNA_DATA_GATHER_SET);}
serial_write(b"[SECURITY] ready\r\n");
}
#[unsafe(no_mangle)] pub extern "sysv64" fn security_entry_gate(gate:*mut LunaGate){let gate=unsafe{&mut *gate};match gate.opcode{OP_REQUEST_CAP=>issue_session_cid(gate),OP_LIST_CAPS=>list_caps(gate),OP_ISSUE_SEAL=>issue_seal(gate),OP_CONSUME_SEAL=>consume_seal(gate),OP_REVOKE_CAP=>revoke_cap(gate),OP_VALIDATE_CAP=>validate_cap(gate),OP_LIST_SEALS=>list_seals(gate),OP_REVOKE_DOMAIN=>revoke_domain(gate),OP_POLICY_QUERY=>policy_query(gate),OP_POLICY_SYNC=>policy_sync(gate),OP_AUTH_LOGIN=>auth_login(gate),OP_GOVERN=>govern(gate),OP_AUTH_SESSION_OPEN=>auth_session_open(gate),OP_AUTH_SESSION_CLOSE=>auth_session_close(gate),OP_CRYPTO_SECRET=>crypto_secret(gate),OP_TRUST_EVAL=>trust_eval(gate),OP_QUERY_GOVERN=>query_govern(gate),_=>{gate.status=LUNA_GATE_UNKNOWN;gate.count=0;}}}
#[unsafe(no_mangle)] pub extern "C" fn memset(dst:*mut u8,value:i32,len:usize)->*mut u8{let mut i=0usize;while i<len{unsafe{*dst.add(i)=value as u8;}i+=1;}dst}
#[panic_handler] fn panic(_info:&PanicInfo<'_>)->!{loop{unsafe{asm!("hlt",options(nomem,nostack,preserves_flags));}}}
