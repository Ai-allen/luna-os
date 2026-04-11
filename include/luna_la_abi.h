#ifndef LUNA_LA_ABI_H
#define LUNA_LA_ABI_H

#include <stdint.h>

#define LUNA_PROGRAM_APP_MAGIC 0x50414E55u
#define LUNA_PROGRAM_APP_VERSION 1u
#define LUNA_PROGRAM_BUNDLE_MAGIC 0x4C554E42u
#define LUNA_PROGRAM_BUNDLE_VERSION 2u
#define LUNA_LA_ABI_MAJOR 1u
#define LUNA_LA_ABI_MINOR 0u
#define LUNA_SDK_MAJOR 1u
#define LUNA_SDK_MINOR 0u
#define LUNA_PROGRAM_BUNDLE_FLAG_DRIVER 0x00000001u
#define LUNA_CAP_DEVICE_PROBE 0xA504u
#define LUNA_CAP_DEVICE_BIND 0xA505u
#define LUNA_CAP_DEVICE_MMIO 0xA506u
#define LUNA_CAP_DEVICE_DMA 0xA507u
#define LUNA_CAP_DEVICE_IRQ 0xA508u

struct luna_la_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_offset;
    uint32_t capability_count;
    uint32_t reserved0;
    char name[16];
    uint64_t capability_keys[4];
};

struct luna_luna_header_v2 {
    uint32_t magic;
    uint32_t version;
    uint64_t bundle_id;
    uint64_t source_id;
    uint64_t app_version;
    uint64_t integrity_check;
    uint64_t header_bytes;
    uint64_t entry_offset;
    uint64_t signer_id;
    uint64_t signature_check;
    uint32_t capability_count;
    uint32_t flags;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t sdk_major;
    uint16_t sdk_minor;
    uint32_t min_proto_version;
    uint32_t max_proto_version;
    char name[16];
    uint64_t capability_keys[4];
};

#endif
