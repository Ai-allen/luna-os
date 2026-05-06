#include "graphics_internal.h"

struct luna_cid g_device_write_cid = {0, 0};
struct luna_cid g_device_read_cid = {0, 0};
volatile struct luna_manifest *g_manifest = 0;
uint16_t g_text_cells[80u * 25u];
uint32_t g_active_window = 0;
uint32_t *g_framebuffer = 0;
uint32_t g_fb_width = 0;
uint32_t g_fb_height = 0;
uint32_t g_fb_stride = 0;
uint32_t g_fb_format = 0;
uint64_t g_fb_buffer_bytes = 0u;
uint32_t g_cell_width = 8;
uint32_t g_cell_height = 16;
uint32_t g_launcher_open = 0;
uint32_t g_launcher_index = 0;
uint32_t g_cursor_x = 40u;
uint32_t g_cursor_y = 12u;
uint32_t g_control_open = 0u;
uint32_t g_render_hover_kind = 0u;
uint32_t g_render_hover_window = 0u;
struct luna_desktop_shell_state g_shell_state = {
    .version = 1u,
    .entry_count = 5u,
    .entries = {
        { "files.la", "Files", 3u, 4u, 0u },
        { "notes.la", "Notes", 3u, 8u, 0u },
        { "guard.la", "Guard", 3u, 12u, 0u },
        { "console.la", "Console", 3u, 16u, 0u },
        { "hello.la", "Settings", 3u, 20u, 0u },
    },
};
struct luna_window_record g_windows[8];

uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 41;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_GRAPHICS;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint64_t caller_space, uint32_t target_gate) {
    volatile struct luna_gate *gate = (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 43;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = caller_space != 0u ? caller_space : LUNA_SPACE_GRAPHICS;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_GRAPHICS;
    gate->target_gate = target_gate;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)((struct luna_gate *)(uintptr_t)g_manifest->security_gate_base);
    return gate->status;
}

void device_write(const char *text) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    uint64_t size = 0;
    while (text[size] != '\0') {
        size += 1u;
    }
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 42;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_GRAPHICS;
    gate->actor_space = LUNA_SPACE_GRAPHICS;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
}

void SYSV_ABI graphics_entry_boot(const struct luna_bootview *bootview) {
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid) != LUNA_GATE_OK) {
        g_device_read_cid.low = 0u;
        g_device_read_cid.high = 0u;
    }
    init_framebuffer(bootview);
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        zero_bytes(g_text_cells, sizeof(g_text_cells));
        render_scene();
        if (g_framebuffer != 0) {
            device_write("[GRAPHICS] framebuffer ready\r\n");
        } else {
            device_write("[GRAPHICS] console ready\r\n");
        }
    } else {
        zero_bytes(g_text_cells, sizeof(g_text_cells));
        render_scene();
    }
}
