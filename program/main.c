#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_MANIFEST_ADDR 0x39000ull
#define LUNA_PROGRAM_APP_MAGIC 0x50414E55u
#define LUNA_PROGRAM_APP_VERSION 1u

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);

struct luna_app_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_offset;
    uint32_t capability_count;
    uint32_t reserved0;
    char name[16];
    uint64_t capability_keys[4];
};

static const struct luna_bootview *g_bootview = 0;
static uint64_t g_loaded_ticket = 0;
static uint64_t g_loaded_app_base = 0;
static uint64_t g_loaded_app_size = 0;
static uint64_t g_loaded_entry = 0;
static char g_loaded_name[16];

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_putc(char value) {
    while ((inb(0x3FD) & 0x20) == 0) {
    }
    outb(0x3F8, (uint8_t)value);
}

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static void copy_name(char out[16], const char in[16]) {
    for (size_t i = 0; i < 16; ++i) {
        out[i] = in[i];
    }
}

static int text_equal16(const char lhs[16], const char rhs[16]) {
    for (size_t i = 0; i < 16; ++i) {
        if (lhs[i] != rhs[i]) {
            return 0;
        }
        if (lhs[i] == '\0') {
            return 1;
        }
    }
    return 1;
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 30;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static int allow_load(uint64_t low, uint64_t high) {
    return low == 0x62D00001ull && high == 0x62004001ull;
}

static int allow_start(uint64_t low, uint64_t high) {
    return low == 0x62D00002ull && high == 0x62004002ull;
}

static int allow_stop(uint64_t low, uint64_t high) {
    return low == 0x62D00003ull && high == 0x62004003ull;
}

static uint32_t validate_package(const struct luna_app_header *header) {
    if (header->magic != LUNA_PROGRAM_APP_MAGIC || header->version != LUNA_PROGRAM_APP_VERSION) {
        return LUNA_PROGRAM_ERR_BAD_PACKAGE;
    }
    return LUNA_PROGRAM_OK;
}

static const struct luna_app_header *find_embedded_app(const char name[16], uint64_t *app_base, uint64_t *app_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    const struct luna_app_header *header = (const struct luna_app_header *)(uintptr_t)manifest->app_hello_base;
    if (text_equal16(header->name, name)) {
        *app_base = manifest->app_hello_base;
        *app_size = manifest->app_hello_size;
        return header;
    }
    return 0;
}

static void program_load(struct luna_program_gate *gate) {
    uint64_t app_base = 0;
    uint64_t app_size = 0;
    const struct luna_app_header *header;

    if (!allow_load(gate->cid_low, gate->cid_high)) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }

    header = find_embedded_app(gate->name, &app_base, &app_size);
    if (header == 0) {
        gate->status = LUNA_PROGRAM_ERR_NOT_FOUND;
        return;
    }
    if (validate_package(header) != LUNA_PROGRAM_OK) {
        gate->status = LUNA_PROGRAM_ERR_BAD_PACKAGE;
        return;
    }

    g_loaded_ticket = 1;
    g_loaded_app_base = app_base;
    g_loaded_app_size = app_size;
    g_loaded_entry = app_base + header->entry_offset;
    copy_name(g_loaded_name, header->name);

    gate->ticket = g_loaded_ticket;
    gate->app_base = g_loaded_app_base;
    gate->app_size = g_loaded_app_size;
    gate->entry_addr = g_loaded_entry;
    gate->result_count = 1;
    gate->status = LUNA_PROGRAM_OK;
}

static void program_start(struct luna_program_gate *gate) {
    const struct luna_app_header *header;
    typedef void (SYSV_ABI *app_entry_fn_t)(const struct luna_bootview *bootview);

    if (!allow_start(gate->cid_low, gate->cid_high)) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }
    if (gate->ticket != g_loaded_ticket || g_loaded_ticket == 0) {
        gate->status = LUNA_PROGRAM_ERR_BAD_TICKET;
        return;
    }

    header = (const struct luna_app_header *)(uintptr_t)g_loaded_app_base;
    if (validate_package(header) != LUNA_PROGRAM_OK) {
        gate->status = LUNA_PROGRAM_ERR_BAD_PACKAGE;
        return;
    }

    for (uint32_t i = 0; i < header->capability_count && i < 4; ++i) {
        struct luna_cid ignored = {0, 0};
        if (request_capability(header->capability_keys[i], &ignored) != LUNA_GATE_OK) {
            gate->status = LUNA_PROGRAM_ERR_BAD_PACKAGE;
            return;
        }
    }

    ((app_entry_fn_t)(uintptr_t)g_loaded_entry)(g_bootview);
    gate->status = LUNA_PROGRAM_OK;
}

static void program_stop(struct luna_program_gate *gate) {
    if (!allow_stop(gate->cid_low, gate->cid_high)) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }
    if (gate->ticket != g_loaded_ticket || g_loaded_ticket == 0) {
        gate->status = LUNA_PROGRAM_ERR_BAD_TICKET;
        return;
    }
    g_loaded_ticket = 0;
    g_loaded_app_base = 0;
    g_loaded_app_size = 0;
    g_loaded_entry = 0;
    zero_bytes(g_loaded_name, sizeof(g_loaded_name));
    gate->status = LUNA_PROGRAM_OK;
}

void SYSV_ABI program_entry_boot(const struct luna_bootview *bootview) {
    g_bootview = bootview;
    serial_write("[PROGRAM] runtime ready\r\n");
}

void SYSV_ABI program_entry_gate(struct luna_program_gate *gate) {
    gate->status = LUNA_PROGRAM_ERR_NOT_FOUND;
    gate->result_count = 0;

    if (gate->opcode == LUNA_PROGRAM_LOAD_APP) {
        program_load(gate);
    } else if (gate->opcode == LUNA_PROGRAM_START_APP) {
        program_start(gate);
    } else if (gate->opcode == LUNA_PROGRAM_STOP_APP) {
        program_stop(gate);
    }
}
