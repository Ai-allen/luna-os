#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_AUX_DIAG_SPACE_ID 16u

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *system_gate_fn_t)(struct luna_system_gate *gate);
typedef void (SYSV_ABI *time_gate_fn_t)(struct luna_time_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *observe_gate_fn_t)(struct luna_observe_gate *gate);
typedef void (SYSV_ABI *network_gate_fn_t)(struct luna_network_gate *gate);
typedef void (SYSV_ABI *graphics_gate_fn_t)(struct luna_graphics_gate *gate);
typedef void (SYSV_ABI *package_gate_fn_t)(struct luna_package_gate *gate);
typedef void (SYSV_ABI *update_gate_fn_t)(struct luna_update_gate *gate);
typedef void (SYSV_ABI *ai_gate_fn_t)(struct luna_ai_gate *gate);

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

static int bytes_equal(const uint8_t *lhs, const uint8_t *rhs, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (lhs[i] != rhs[i]) {
            return 0;
        }
    }
    return 1;
}

static int text_equal(const char *lhs, const char *rhs) {
    while (*lhs != '\0' || *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }
    return 1;
}

static int text_equal16(const char lhs[16], const char *rhs) {
    size_t i = 0;
    while (i < 16) {
        char r = rhs[i];
        if (lhs[i] != r) {
            return 0;
        }
        if (r == '\0') {
            return 1;
        }
        ++i;
    }
    return 1;
}

static uint32_t request_cap(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 10;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_AUX_DIAG_SPACE_ID;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t data_call(volatile struct luna_manifest *manifest) {
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)((struct luna_data_gate *)(uintptr_t)manifest->data_gate_base);
    return ((volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base)->status;
}

static uint32_t lifecycle_call(volatile struct luna_manifest *manifest) {
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)((struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base);
    return ((volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base)->status;
}

static uint32_t system_call(volatile struct luna_manifest *manifest) {
    ((system_gate_fn_t)(uintptr_t)manifest->system_gate_entry)((struct luna_system_gate *)(uintptr_t)manifest->system_gate_base);
    return ((volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base)->status;
}

static uint32_t time_call(volatile struct luna_manifest *manifest) {
    ((time_gate_fn_t)(uintptr_t)manifest->time_gate_entry)((struct luna_time_gate *)(uintptr_t)manifest->time_gate_base);
    return ((volatile struct luna_time_gate *)(uintptr_t)manifest->time_gate_base)->status;
}

static uint32_t device_call(volatile struct luna_manifest *manifest) {
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)manifest->device_gate_base);
    return ((volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base)->status;
}

static uint32_t observe_call(volatile struct luna_manifest *manifest) {
    ((observe_gate_fn_t)(uintptr_t)manifest->observe_gate_entry)((struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base);
    return ((volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base)->status;
}

static uint32_t network_call(volatile struct luna_manifest *manifest) {
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)((struct luna_network_gate *)(uintptr_t)manifest->network_gate_base);
    return ((volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base)->status;
}

static uint32_t graphics_call(volatile struct luna_manifest *manifest) {
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)((struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base);
    return ((volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base)->status;
}

static uint32_t package_call(volatile struct luna_manifest *manifest) {
    ((package_gate_fn_t)(uintptr_t)manifest->package_gate_entry)((struct luna_package_gate *)(uintptr_t)manifest->package_gate_base);
    return ((volatile struct luna_package_gate *)(uintptr_t)manifest->package_gate_base)->status;
}

static uint32_t update_call(volatile struct luna_manifest *manifest) {
    ((update_gate_fn_t)(uintptr_t)manifest->update_gate_entry)((struct luna_update_gate *)(uintptr_t)manifest->update_gate_base);
    return ((volatile struct luna_update_gate *)(uintptr_t)manifest->update_gate_base)->status;
}

static uint32_t ai_call(volatile struct luna_manifest *manifest) {
    ((ai_gate_fn_t)(uintptr_t)manifest->ai_gate_entry)((struct luna_ai_gate *)(uintptr_t)manifest->ai_gate_base);
    return ((volatile struct luna_ai_gate *)(uintptr_t)manifest->ai_gate_base)->status;
}

static int contains_space(const struct luna_unit_record *records, size_t count, uint32_t space_id) {
    for (size_t i = 0; i < count; ++i) {
        if (records[i].space_id == space_id && records[i].state == LUNA_UNIT_LIVE) {
            return 1;
        }
    }
    return 0;
}

void SYSV_ABI test_entry_boot(const struct luna_bootview *bootview) {
    static const uint8_t payload[] = "lafs-persist-check";
    static const uint8_t net_payload[] = "ping";
    volatile struct luna_manifest *manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *data_gate = (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    volatile struct luna_lifecycle_gate *life_gate = (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
    volatile struct luna_system_gate *system_gate = (volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base;
    volatile struct luna_time_gate *time_gate = (volatile struct luna_time_gate *)(uintptr_t)manifest->time_gate_base;
    volatile struct luna_device_gate *device_gate = (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    volatile struct luna_observe_gate *observe_gate = (volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base;
    volatile struct luna_network_gate *network_gate = (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    volatile struct luna_graphics_gate *graphics_gate = (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;
    volatile struct luna_package_gate *package_gate = (volatile struct luna_package_gate *)(uintptr_t)manifest->package_gate_base;
    volatile struct luna_update_gate *update_gate = (volatile struct luna_update_gate *)(uintptr_t)manifest->update_gate_base;
    volatile struct luna_ai_gate *ai_gate = (volatile struct luna_ai_gate *)(uintptr_t)manifest->ai_gate_base;
    struct luna_cid cid_seed = {0}, cid_pour = {0}, cid_draw = {0}, cid_gather = {0};
    struct luna_cid cid_life_read = {0}, cid_system_query = {0};
    struct luna_cid cid_time_pulse = {0}, cid_time_chime = {0};
    struct luna_cid cid_device_list = {0}, cid_device_write = {0};
    struct luna_cid cid_observe_log = {0}, cid_observe_read = {0}, cid_observe_stats = {0};
    struct luna_cid cid_network_send = {0}, cid_network_recv = {0};
    struct luna_cid cid_graphics_draw = {0};
    struct luna_cid cid_package_install = {0}, cid_package_list = {0};
    struct luna_cid cid_update_check = {0}, cid_update_apply = {0};
    struct luna_cid cid_ai_infer = {0}, cid_ai_create = {0};
    struct luna_object_ref *objects = (struct luna_object_ref *)(uintptr_t)manifest->list_buffer_base;
    struct luna_unit_record *units = (struct luna_unit_record *)(uintptr_t)manifest->list_buffer_base;
    struct luna_system_record *system_info = (struct luna_system_record *)(uintptr_t)manifest->list_buffer_base;
    struct luna_device_record *dev_info = (struct luna_device_record *)(uintptr_t)manifest->list_buffer_base;
    struct luna_observe_stats *stats = (struct luna_observe_stats *)(uintptr_t)manifest->list_buffer_base;
    struct luna_package_record *pkg = (struct luna_package_record *)(uintptr_t)manifest->list_buffer_base;
    uint8_t *buffer = (uint8_t *)(uintptr_t)manifest->data_buffer_base;
    struct luna_object_ref object_id = {0};
    uint64_t t0 = 0;
    uint64_t t1 = 0;

    (void)bootview;

    if (request_cap(LUNA_CAP_DATA_SEED, &cid_seed) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_DATA_POUR, &cid_pour) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_DATA_DRAW, &cid_draw) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_DATA_GATHER, &cid_gather) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_LIFE_READ, &cid_life_read) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_SYSTEM_QUERY, &cid_system_query) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_TIME_PULSE, &cid_time_pulse) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_TIME_CHIME, &cid_time_chime) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_DEVICE_LIST, &cid_device_list) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_DEVICE_WRITE, &cid_device_write) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_OBSERVE_LOG, &cid_observe_log) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_OBSERVE_READ, &cid_observe_read) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_OBSERVE_STATS, &cid_observe_stats) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_NETWORK_SEND, &cid_network_send) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_NETWORK_RECV, &cid_network_recv) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_GRAPHICS_DRAW, &cid_graphics_draw) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_PACKAGE_INSTALL, &cid_package_install) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_PACKAGE_LIST, &cid_package_list) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_UPDATE_CHECK, &cid_update_check) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_UPDATE_APPLY, &cid_update_apply) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_AI_INFER, &cid_ai_infer) != LUNA_GATE_OK ||
        request_cap(LUNA_CAP_AI_CREATE, &cid_ai_create) != LUNA_GATE_OK) {
        serial_write("[TEST] cap grant fail\r\n");
        return;
    }

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    data_gate->sequence = 20;
    data_gate->opcode = LUNA_DATA_GATHER_SET;
    data_gate->cid_low = cid_gather.low;
    data_gate->cid_high = cid_gather.high;
    data_gate->buffer_addr = manifest->list_buffer_base;
    data_gate->buffer_size = manifest->list_buffer_size;
    if (data_call(manifest) != LUNA_DATA_OK) {
        serial_write("[TEST] data gather fail\r\n");
        return;
    }

    if (data_gate->result_count == 0u) {
        zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
        data_gate->sequence = 21;
        data_gate->opcode = LUNA_DATA_SEED_OBJECT;
        data_gate->cid_low = cid_seed.low;
        data_gate->cid_high = cid_seed.high;
        data_gate->object_type = 0x4C414653u;
        data_gate->object_flags = 0x1u;
        if (data_call(manifest) != LUNA_DATA_OK) {
            serial_write("[TEST] data seed fail\r\n");
            return;
        }
        object_id.low = data_gate->object_low;
        object_id.high = data_gate->object_high;

        zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
        for (size_t i = 0; i < sizeof(payload) - 1u; ++i) {
            buffer[i] = payload[i];
        }

        zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
        data_gate->sequence = 22;
        data_gate->opcode = LUNA_DATA_POUR_SPAN;
        data_gate->cid_low = cid_pour.low;
        data_gate->cid_high = cid_pour.high;
        data_gate->object_low = object_id.low;
        data_gate->object_high = object_id.high;
        data_gate->buffer_addr = manifest->data_buffer_base;
        data_gate->buffer_size = manifest->data_buffer_size;
        data_gate->size = sizeof(payload) - 1u;
        if (data_call(manifest) != LUNA_DATA_OK) {
            serial_write("[TEST] data pour fail\r\n");
            return;
        }

        zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
        zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
        data_gate->sequence = 23;
        data_gate->opcode = LUNA_DATA_DRAW_SPAN;
        data_gate->cid_low = cid_draw.low;
        data_gate->cid_high = cid_draw.high;
        data_gate->object_low = object_id.low;
        data_gate->object_high = object_id.high;
        data_gate->buffer_addr = manifest->data_buffer_base;
        data_gate->buffer_size = manifest->data_buffer_size;
        data_gate->size = sizeof(payload) - 1u;
        if (data_call(manifest) != LUNA_DATA_OK ||
            data_gate->size != sizeof(payload) - 1u ||
            !bytes_equal(buffer, payload, sizeof(payload) - 1u)) {
            serial_write("[TEST] data arm fail\r\n");
            return;
        }
        serial_write("[TEST] data armed\r\n");
    } else {
        object_id = objects[0];
        zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
        zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
        data_gate->sequence = 24;
        data_gate->opcode = LUNA_DATA_DRAW_SPAN;
        data_gate->cid_low = cid_draw.low;
        data_gate->cid_high = cid_draw.high;
        data_gate->object_low = object_id.low;
        data_gate->object_high = object_id.high;
        data_gate->buffer_addr = manifest->data_buffer_base;
        data_gate->buffer_size = manifest->data_buffer_size;
        data_gate->size = sizeof(payload) - 1u;
        if (data_call(manifest) != LUNA_DATA_OK ||
            data_gate->size != sizeof(payload) - 1u ||
            !bytes_equal(buffer, payload, sizeof(payload) - 1u)) {
            serial_write("[TEST] persistence fail\r\n");
            return;
        }
        serial_write("[TEST] persistence ok\r\n");
    }

    zero_bytes((void *)(uintptr_t)manifest->time_gate_base, sizeof(struct luna_time_gate));
    time_gate->sequence = 30;
    time_gate->opcode = LUNA_TIME_READ_PULSE;
    time_gate->cid_low = cid_time_pulse.low;
    time_gate->cid_high = cid_time_pulse.high;
    if (time_call(manifest) != LUNA_TIME_OK) {
        serial_write("[TEST] time pulse fail\r\n");
        return;
    }
    t0 = time_gate->arg0;
    zero_bytes((void *)(uintptr_t)manifest->time_gate_base, sizeof(struct luna_time_gate));
    time_gate->sequence = 31;
    time_gate->opcode = LUNA_TIME_SET_CHIME;
    time_gate->cid_low = cid_time_chime.low;
    time_gate->cid_high = cid_time_chime.high;
    time_gate->arg0 = 1u;
    if (time_call(manifest) != LUNA_TIME_OK) {
        serial_write("[TEST] time chime fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->time_gate_base, sizeof(struct luna_time_gate));
    time_gate->sequence = 32;
    time_gate->opcode = LUNA_TIME_READ_PULSE;
    time_gate->cid_low = cid_time_pulse.low;
    time_gate->cid_high = cid_time_pulse.high;
    if (time_call(manifest) != LUNA_TIME_OK) {
        serial_write("[TEST] time pulse fail\r\n");
        return;
    }
    t1 = time_gate->arg0;
    if (t1 <= t0) {
        serial_write("[TEST] time delta fail\r\n");
        return;
    }
    serial_write("[TEST] time ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    device_gate->sequence = 40;
    device_gate->opcode = LUNA_DEVICE_LIST;
    device_gate->cid_low = cid_device_list.low;
    device_gate->cid_high = cid_device_list.high;
    device_gate->buffer_addr = manifest->list_buffer_base;
    device_gate->buffer_size = manifest->list_buffer_size;
    if (device_call(manifest) != LUNA_DEVICE_OK || device_gate->result_count < 1u || dev_info->device_id != 1u || !text_equal16(dev_info->name, "serial0")) {
        serial_write("[TEST] device list fail\r\n");
        return;
    }
    serial_write("[TEST] device ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    observe_gate->sequence = 50;
    observe_gate->opcode = LUNA_OBSERVE_LOG;
    observe_gate->space_id = LUNA_AUX_DIAG_SPACE_ID;
    observe_gate->level = 2u;
    observe_gate->cid_low = cid_observe_log.low;
    observe_gate->cid_high = cid_observe_log.high;
    zero_bytes((void *)observe_gate->message, sizeof(observe_gate->message));
    for (size_t i = 0; i < 9u; ++i) {
        observe_gate->message[i] = "test alive"[i];
    }
    if (observe_call(manifest) != LUNA_OBSERVE_OK) {
        serial_write("[TEST] observe log fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    observe_gate->sequence = 51;
    observe_gate->opcode = LUNA_OBSERVE_GET_LOGS;
    observe_gate->cid_low = cid_observe_read.low;
    observe_gate->cid_high = cid_observe_read.high;
    observe_gate->buffer_addr = manifest->list_buffer_base;
    observe_gate->buffer_size = manifest->list_buffer_size;
    if (observe_call(manifest) != LUNA_OBSERVE_OK || observe_gate->result_count == 0u) {
        serial_write("[TEST] observe read fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    observe_gate->sequence = 52;
    observe_gate->opcode = LUNA_OBSERVE_GET_STATS;
    observe_gate->cid_low = cid_observe_stats.low;
    observe_gate->cid_high = cid_observe_stats.high;
    observe_gate->buffer_addr = manifest->list_buffer_base;
    observe_gate->buffer_size = manifest->list_buffer_size;
    if (observe_call(manifest) != LUNA_OBSERVE_OK || stats->count == 0u) {
        serial_write("[TEST] observe stats fail\r\n");
        return;
    }
    serial_write("[TEST] observe ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
    for (size_t i = 0; i < sizeof(net_payload) - 1u; ++i) {
        buffer[i] = net_payload[i];
    }
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    network_gate->sequence = 60;
    network_gate->opcode = LUNA_NETWORK_SEND_PACKET;
    network_gate->cid_low = cid_network_send.low;
    network_gate->cid_high = cid_network_send.high;
    network_gate->buffer_addr = manifest->data_buffer_base;
    network_gate->buffer_size = manifest->data_buffer_size;
    network_gate->size = sizeof(net_payload) - 1u;
    if (network_call(manifest) != LUNA_NETWORK_OK) {
        serial_write("[TEST] network send fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    network_gate->sequence = 61;
    network_gate->opcode = LUNA_NETWORK_RECV_PACKET;
    network_gate->cid_low = cid_network_recv.low;
    network_gate->cid_high = cid_network_recv.high;
    network_gate->buffer_addr = manifest->data_buffer_base;
    network_gate->buffer_size = manifest->data_buffer_size;
    if (network_call(manifest) != LUNA_NETWORK_OK || network_gate->size != sizeof(net_payload) - 1u || !bytes_equal(buffer, net_payload, sizeof(net_payload) - 1u)) {
        serial_write("[TEST] network recv fail\r\n");
        return;
    }
    serial_write("[TEST] network ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    graphics_gate->sequence = 70;
    graphics_gate->opcode = LUNA_GRAPHICS_DRAW_CHAR;
    graphics_gate->cid_low = cid_graphics_draw.low;
    graphics_gate->cid_high = cid_graphics_draw.high;
    graphics_gate->x = 1u;
    graphics_gate->y = 0u;
    graphics_gate->glyph = (uint32_t)'G';
    graphics_gate->attr = 0x1Eu;
    if (graphics_call(manifest) != LUNA_GRAPHICS_OK) {
        serial_write("[TEST] graphics fail\r\n");
        return;
    }
    serial_write("[TEST] graphics ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->package_gate_base, sizeof(struct luna_package_gate));
    package_gate->sequence = 80;
    package_gate->opcode = LUNA_PACKAGE_INSTALL;
    package_gate->cid_low = cid_package_install.low;
    package_gate->cid_high = cid_package_install.high;
    for (size_t i = 0; i < 6u; ++i) {
        package_gate->name[i] = "hello"[i];
    }
    if (package_call(manifest) != LUNA_PACKAGE_OK) {
        serial_write("[TEST] package install fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->package_gate_base, sizeof(struct luna_package_gate));
    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    package_gate->sequence = 81;
    package_gate->opcode = LUNA_PACKAGE_LIST;
    package_gate->cid_low = cid_package_list.low;
    package_gate->cid_high = cid_package_list.high;
    package_gate->buffer_addr = manifest->list_buffer_base;
    package_gate->buffer_size = manifest->list_buffer_size;
    if (package_call(manifest) != LUNA_PACKAGE_OK || package_gate->result_count != 1u || !text_equal16(pkg->name, "hello")) {
        serial_write("[TEST] package list fail\r\n");
        return;
    }
    serial_write("[TEST] package ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->update_gate_base, sizeof(struct luna_update_gate));
    update_gate->sequence = 90;
    update_gate->opcode = LUNA_UPDATE_CHECK;
    update_gate->cid_low = cid_update_check.low;
    update_gate->cid_high = cid_update_check.high;
    if (update_call(manifest) != LUNA_UPDATE_OK || update_gate->target_version <= update_gate->current_version) {
        serial_write("[TEST] update check fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->update_gate_base, sizeof(struct luna_update_gate));
    update_gate->sequence = 91;
    update_gate->opcode = LUNA_UPDATE_APPLY;
    update_gate->cid_low = cid_update_apply.low;
    update_gate->cid_high = cid_update_apply.high;
    if (update_call(manifest) != LUNA_UPDATE_OK || update_gate->flags == 0u) {
        serial_write("[TEST] update apply fail\r\n");
        return;
    }
    serial_write("[TEST] update ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
    for (size_t i = 0; i < 12u; ++i) {
        buffer[i] = "list spaces"[i];
    }
    zero_bytes((void *)(uintptr_t)manifest->ai_gate_base, sizeof(struct luna_ai_gate));
    ai_gate->sequence = 100;
    ai_gate->opcode = LUNA_AI_INFER;
    ai_gate->cid_low = cid_ai_infer.low;
    ai_gate->cid_high = cid_ai_infer.high;
    ai_gate->buffer_addr = manifest->data_buffer_base;
    ai_gate->buffer_size = manifest->data_buffer_size;
    if (ai_call(manifest) != LUNA_AI_OK || !text_equal((char *)buffer, "BOOT SECURITY DATA LIFECYCLE SYSTEM TIME DEVICE OBSERVE NETWORK GRAPHICS PACKAGE UPDATE AI PROGRAM USER")) {
        serial_write("[TEST] ai infer fail\r\n");
        return;
    }
    zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
    for (size_t i = 0; i < 8u; ++i) {
        buffer[i] = "PROGRAM"[i];
    }
    zero_bytes((void *)(uintptr_t)manifest->ai_gate_base, sizeof(struct luna_ai_gate));
    ai_gate->sequence = 101;
    ai_gate->opcode = LUNA_AI_CREATE_SPACE;
    ai_gate->cid_low = cid_ai_create.low;
    ai_gate->cid_high = cid_ai_create.high;
    ai_gate->buffer_addr = manifest->data_buffer_base;
    ai_gate->buffer_size = manifest->data_buffer_size;
    if (ai_call(manifest) != LUNA_AI_OK) {
        serial_write("[TEST] ai create fail\r\n");
        return;
    }
    serial_write("[TEST] ai ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    life_gate->sequence = 110;
    life_gate->opcode = LUNA_LIFE_READ_UNITS;
    life_gate->cid_low = cid_life_read.low;
    life_gate->cid_high = cid_life_read.high;
    life_gate->buffer_addr = manifest->list_buffer_base;
    life_gate->buffer_size = manifest->list_buffer_size;
    if (lifecycle_call(manifest) != LUNA_LIFE_OK ||
        life_gate->result_count != 15u ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_BOOT) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_SECURITY) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_DATA) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_GRAPHICS) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_DEVICE) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_NETWORK) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_USER) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_TIME) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_LIFECYCLE) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_OBSERVABILITY) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_AI) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_PROGRAM) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_PACKAGE) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_UPDATE) ||
        !contains_space(units, life_gate->result_count, LUNA_SPACE_SYSTEM) ||
        contains_space(units, life_gate->result_count, LUNA_AUX_DIAG_SPACE_ID)) {
        serial_write("[TEST] lifecycle query fail\r\n");
        return;
    }
    serial_write("[TEST] lifecycle query ok\r\n");

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->system_gate_base, sizeof(struct luna_system_gate));
    system_gate->sequence = 120;
    system_gate->opcode = LUNA_SYSTEM_QUERY_SPACE;
    system_gate->space_id = LUNA_SPACE_DATA;
    system_gate->cid_low = cid_system_query.low;
    system_gate->cid_high = cid_system_query.high;
    system_gate->buffer_addr = manifest->list_buffer_base;
    system_gate->buffer_size = manifest->list_buffer_size;
    if (system_call(manifest) != LUNA_SYSTEM_OK ||
        system_gate->result_count != 1u ||
        system_info->space_id != LUNA_SPACE_DATA ||
        system_info->state != LUNA_SYSTEM_STATE_ACTIVE ||
        !text_equal16(system_info->name, "DATA")) {
        serial_write("[TEST] system query fail\r\n");
        return;
    }
    serial_write("[TEST] system query ok\r\n");
}
