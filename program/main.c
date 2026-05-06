#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_PROGRAM_APP_MAGIC 0x50414E55u
#define LUNA_PROGRAM_APP_VERSION 1u
#define LUNA_PROGRAM_BUNDLE_MAGIC 0x4C554E42u
#define LUNA_PROGRAM_BUNDLE_VERSION_V2 2u
#define LUNA_PROGRAM_BUNDLE_SIGNATURE_SEED 0x9A11D00D55AA7711ull
#define LUNA_DATA_OBJECT_TYPE_LUNA_APP 0x4C554E41u
#define LUNA_PROGRAM_DATA_APP_BYTES (256u * 1024u)

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *graphics_gate_fn_t)(struct luna_graphics_gate *gate);
typedef void (SYSV_ABI *package_gate_fn_t)(struct luna_package_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);

struct luna_app_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_offset;
    uint32_t capability_count;
    uint32_t reserved0;
    char name[16];
    uint64_t capability_keys[4];
};

struct luna_bundle_header {
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

struct luna_app_metadata {
    char name[16];
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
    uint64_t capability_keys[4];
};

struct luna_embedded_app {
    uint64_t base;
    uint64_t size;
};

static const struct luna_bootview *g_bootview = 0;
static uint64_t g_loaded_ticket = 0;
static uint64_t g_loaded_app_base = 0;
static uint64_t g_loaded_app_size = 0;
static uint64_t g_loaded_entry = 0;
static char g_loaded_name[16];
static uint8_t g_loaded_app_image[LUNA_PROGRAM_DATA_APP_BYTES];
static uint64_t g_last_data_draw_stage = 0;
static uint64_t g_last_data_draw_type = 0;
static uint64_t g_last_data_draw_flags = 0;
static uint64_t g_last_data_draw_value0 = 0;
static uint64_t g_last_data_draw_value1 = 0;
static struct luna_cid g_device_write_cid = {0, 0};
static struct luna_cid g_graphics_draw_cid = {0, 0};
static struct luna_cid g_package_list_cid = {0, 0};
static struct luna_cid g_data_draw_cid = {0, 0};
static volatile struct luna_manifest *g_manifest = 0;
static struct luna_package_record g_package_catalog[LUNA_PACKAGE_CAPACITY];
static uint32_t g_package_count = 0u;

struct luna_app_panel {
    const char *name;
    char title[16];
    uint32_t window_id;
    uint32_t window_x;
    uint32_t window_y;
    uint32_t window_width;
    uint32_t window_height;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t cursor_x;
    uint32_t cursor_y;
};

static struct luna_app_panel g_panels[] = {
    { "files", "Files", 0u, 2u, 2u, 26u, 9u, 3u, 3u, 24u, 7u, 0u, 0u },
    { "notes", "Notes", 0u, 29u, 2u, 27u, 9u, 30u, 3u, 25u, 7u, 0u, 0u },
    { "guard", "Guard", 0u, 57u, 2u, 21u, 11u, 58u, 3u, 19u, 9u, 0u, 0u },
    { "console", "Console", 0u, 2u, 12u, 37u, 11u, 3u, 13u, 35u, 9u, 0u, 0u },
    { "hello", "Settings", 0u, 40u, 14u, 17u, 7u, 41u, 15u, 15u, 5u, 0u, 0u },
};

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        out[i] = in[i];
    }
}

static void copy_name(volatile char out[16], const char in[16]) {
    for (size_t i = 0; i < 16; ++i) {
        out[i] = 0;
        if (in[i] == '\0') {
            break;
        }
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

static size_t text_length16(const char text[16]) {
    size_t len = 0u;
    while (len < 16u && text[len] != '\0') {
        len += 1u;
    }
    return len;
}

static int app_name_matches(const char header_name[16], const char request_name[16]) {
    size_t header_len = text_length16(header_name);
    size_t request_len = text_length16(request_name);

    if (text_equal16(header_name, request_name)) {
        return 1;
    }
    if (request_len == header_len + 5u &&
        request_name[header_len] == '.' &&
        request_name[header_len + 1u] == 'l' &&
        request_name[header_len + 2u] == 'u' &&
        request_name[header_len + 3u] == 'n' &&
        request_name[header_len + 4u] == 'a') {
        for (size_t i = 0; i < header_len; ++i) {
            if (header_name[i] != request_name[i]) {
                return 0;
            }
        }
        return 1;
    }
    if (request_len == header_len + 3u &&
        request_name[header_len] == '.' &&
        request_name[header_len + 1u] == 'l' &&
        request_name[header_len + 2u] == 'a') {
        for (size_t i = 0; i < header_len; ++i) {
            if (header_name[i] != request_name[i]) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

static int panel_name_matches(const char request_name[16], const char *panel_name) {
    size_t panel_len = 0u;

    while (panel_name[panel_len] != '\0') {
        if (panel_len >= 15u || request_name[panel_len] != panel_name[panel_len]) {
            return 0;
        }
        panel_len += 1u;
    }
    if (request_name[panel_len] == '\0') {
        return 1;
    }
    if (request_name[panel_len] == '.' &&
        request_name[panel_len + 1u] == 'l' &&
        request_name[panel_len + 2u] == 'u' &&
        request_name[panel_len + 3u] == 'n' &&
        request_name[panel_len + 4u] == 'a' &&
        request_name[panel_len + 5u] == '\0') {
        return 1;
    }
    return request_name[panel_len] == '.' &&
        request_name[panel_len + 1u] == 'l' &&
        request_name[panel_len + 2u] == 'a' &&
        request_name[panel_len + 3u] == '\0';
}

static uint32_t refresh_package_catalog(void) {
    volatile struct luna_package_gate *gate =
        (volatile struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base;

    zero_bytes(g_package_catalog, sizeof(g_package_catalog));
    zero_bytes((void *)(uintptr_t)g_manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 36;
    gate->opcode = LUNA_PACKAGE_LIST;
    gate->cid_low = g_package_list_cid.low;
    gate->cid_high = g_package_list_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)g_package_catalog;
    gate->buffer_size = sizeof(g_package_catalog);
    ((package_gate_fn_t)(uintptr_t)g_manifest->package_gate_entry)(
        (struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base
    );
    if (gate->status != LUNA_PACKAGE_OK) {
        g_package_count = 0u;
        return gate->status;
    }
    g_package_count = gate->result_count;
    if (g_package_count > LUNA_PACKAGE_CAPACITY) {
        g_package_count = LUNA_PACKAGE_CAPACITY;
    }
    return LUNA_PACKAGE_OK;
}

static int panel_matches_package_name(const char *panel_name, const char package_name[16]) {
    size_t index = 0u;
    while (panel_name[index] != '\0' && package_name[index] != '\0') {
        if (panel_name[index] != package_name[index]) {
            return 0;
        }
        index += 1u;
    }
    return panel_name[index] == '\0' &&
        ((package_name[index] == '.' &&
          package_name[index + 1u] == 'l' &&
          package_name[index + 2u] == 'u' &&
          package_name[index + 3u] == 'n' &&
          package_name[index + 4u] == 'a' &&
          package_name[index + 5u] == '\0') ||
         (package_name[index] == '.' &&
          package_name[index + 1u] == 'l' &&
          package_name[index + 2u] == 'a' &&
          package_name[index + 3u] == '\0'));
}

static const struct luna_package_record *find_package_record_for_panel(const struct luna_app_panel *panel) {
    if (panel == 0) {
        return 0;
    }
    for (uint32_t i = 0u; i < g_package_count; ++i) {
        if (panel_matches_package_name(panel->name, g_package_catalog[i].name)) {
            return &g_package_catalog[i];
        }
    }
    return 0;
}

static void apply_package_metadata(struct luna_app_panel *panel) {
    const struct luna_package_record *record = find_package_record_for_panel(panel);

    if (panel == 0 || record == 0) {
        return;
    }
    if (record->label[0] != '\0') {
        copy_name(panel->title, record->label);
    }
    if (record->preferred_width >= 6u && record->preferred_height >= 4u) {
        panel->window_x = record->preferred_x;
        panel->window_y = record->preferred_y;
        panel->window_width = record->preferred_width;
        panel->window_height = record->preferred_height;
        panel->x = panel->window_x + 1u;
        panel->y = panel->window_y + 1u;
        panel->width = panel->window_width - 2u;
        panel->height = panel->window_height - 2u;
    }
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest = g_manifest;
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

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)g_manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)g_manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 32;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = 0;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_PROGRAM;
    gate->target_gate = target_gate;
    ((security_gate_fn_t)(uintptr_t)g_manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)g_manifest->security_gate_base
    );
    return gate->status;
}

static uint32_t data_draw_span(
    struct luna_cid cid,
    struct luna_object_ref object,
    uint64_t offset,
    void *buffer,
    uint64_t buffer_size,
    uint64_t *out_size,
    uint64_t *out_content_size,
    uint32_t *out_type
) {
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base;
    void *stage = 0;
    void *target = buffer;

    if (g_manifest != 0 &&
        g_manifest->data_buffer_base != 0u &&
        g_manifest->data_buffer_size >= buffer_size) {
        stage = (void *)(uintptr_t)g_manifest->data_buffer_base;
        target = stage;
    }
    zero_bytes(target, (size_t)buffer_size);
    if (target != buffer) {
        zero_bytes(buffer, (size_t)buffer_size);
    }
    zero_bytes((void *)(uintptr_t)g_manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 37;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object.low;
    gate->object_high = object.high;
    gate->offset = offset;
    gate->size = buffer_size;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)g_manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)g_manifest->data_gate_base
    );
    g_last_data_draw_stage = gate->result_count;
    g_last_data_draw_type = gate->object_type;
    g_last_data_draw_flags = gate->object_flags;
    g_last_data_draw_value0 = gate->created_at;
    g_last_data_draw_value1 = gate->content_size;
    *out_size = gate->size;
    if (out_content_size != 0) {
        *out_content_size = gate->content_size;
    }
    if (out_type != 0) {
        *out_type = gate->object_type;
    }
    if (gate->status == LUNA_DATA_OK && target != buffer && gate->size != 0u) {
        copy_bytes(buffer, target, (size_t)gate->size);
    }
    return gate->status;
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;
    if (text != 0 && text[0] == '[') {
        return;
    }
    while (text[size] != '\0') {
        size += 1u;
    }
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 31;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->actor_space = LUNA_SPACE_PROGRAM;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
}

static void graphics_draw_char(uint32_t window_id, uint32_t x, uint32_t y, char ch, uint8_t attr) {
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 33;
    gate->opcode = LUNA_GRAPHICS_DRAW_CHAR;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->actor_space = LUNA_SPACE_PROGRAM;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    gate->x = x;
    gate->y = y;
    gate->glyph = (uint32_t)(uint8_t)ch;
    gate->attr = (uint32_t)attr;
    ((graphics_gate_fn_t)(uintptr_t)g_manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base
    );
}

static uint32_t graphics_create_window(struct luna_app_panel *panel) {
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base;

    if (panel == 0) {
        return LUNA_GRAPHICS_ERR_NOT_FOUND;
    }

    zero_bytes((void *)(uintptr_t)g_manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 34;
    gate->opcode = LUNA_GRAPHICS_CREATE_WINDOW;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->actor_space = LUNA_SPACE_PROGRAM;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->x = panel->window_x;
    gate->y = panel->window_y;
    gate->width = panel->window_width;
    gate->height = panel->window_height;
    gate->buffer_addr = (uint64_t)(uintptr_t)panel->title;
    gate->buffer_size = text_length16(panel->title);
    ((graphics_gate_fn_t)(uintptr_t)g_manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base
    );
    if (gate->status == LUNA_GRAPHICS_OK) {
        panel->window_id = gate->window_id;
    }
    return gate->status;
}

static uint32_t graphics_set_active_window(uint32_t window_id) {
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)g_manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 35;
    gate->opcode = LUNA_GRAPHICS_SET_ACTIVE_WINDOW;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->actor_space = LUNA_SPACE_PROGRAM;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    ((graphics_gate_fn_t)(uintptr_t)g_manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)g_manifest->graphics_gate_base
    );
    return gate->status;
}

static struct luna_app_panel *find_panel(const char name[16]) {
    if (panel_name_matches(name, "files")) {
        return &g_panels[0];
    }
    if (panel_name_matches(name, "notes")) {
        return &g_panels[1];
    }
    if (panel_name_matches(name, "guard")) {
        return &g_panels[2];
    }
    if (panel_name_matches(name, "console")) {
        return &g_panels[3];
    }
    if (panel_name_matches(name, "hello")) {
        return &g_panels[4];
    }
    return 0;
}

static void clear_panel(struct luna_app_panel *panel) {
    if (panel == 0) {
        return;
    }
    for (uint32_t row = 0; row < panel->height; ++row) {
        for (uint32_t col = 0; col < panel->width; ++col) {
            graphics_draw_char(panel->window_id, col, row, ' ', 0x17u);
        }
    }
    panel->cursor_x = 0u;
    panel->cursor_y = 0u;
}

static void panel_newline(struct luna_app_panel *panel) {
    panel->cursor_x = 0u;
    if (panel->cursor_y + 1u < panel->height) {
        panel->cursor_y += 1u;
    } else {
        panel->cursor_y = panel->height - 1u;
    }
}

static void panel_write(struct luna_app_panel *panel, const char *text) {
    if (panel == 0 || text == 0) {
        return;
    }
    while (*text != '\0') {
        if (*text == '\r') {
            ++text;
            continue;
        }
        if (*text == '\n') {
            panel_newline(panel);
            ++text;
            continue;
        }
        if (panel->cursor_x >= panel->width) {
            panel_newline(panel);
        }
        graphics_draw_char(panel->window_id, panel->cursor_x, panel->cursor_y, *text, 0x1Fu);
        panel->cursor_x += 1u;
        ++text;
    }
}

static void bind_view(struct luna_program_gate *gate) {
    struct luna_app_panel *panel;
    if (validate_capability(LUNA_CAP_PROGRAM_LOAD, gate->cid_low, gate->cid_high, LUNA_PROGRAM_BIND_VIEW) != LUNA_GATE_OK) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }
    panel = find_panel(gate->name);
    if (panel == 0) {
        gate->status = LUNA_PROGRAM_ERR_NOT_FOUND;
        return;
    }
    panel->window_id = gate->window_id;
    panel->x = gate->x;
    panel->y = gate->y;
    panel->width = gate->width;
    panel->height = gate->height;
    panel->cursor_x = 0u;
    panel->cursor_y = 0u;
    gate->status = LUNA_PROGRAM_OK;
}

static void bind_panel(struct luna_app_panel *panel) {
    apply_package_metadata(panel);
    panel->cursor_x = 0u;
    panel->cursor_y = 0u;
}

static uint32_t ensure_window_bound(struct luna_app_panel *panel) {
    if (panel == 0) {
        return LUNA_PROGRAM_ERR_NOT_FOUND;
    }
    if (g_graphics_draw_cid.low == 0u && g_graphics_draw_cid.high == 0u) {
        (void)request_capability(LUNA_CAP_GRAPHICS_DRAW, &g_graphics_draw_cid);
    }
    if (g_package_count == 0u && (g_package_list_cid.low != 0u || g_package_list_cid.high != 0u)) {
        (void)refresh_package_catalog();
    }
    if (g_graphics_draw_cid.low == 0u && g_graphics_draw_cid.high == 0u) {
        return LUNA_PROGRAM_ERR_NOT_FOUND;
    }
    if (panel->window_id == 0u) {
        apply_package_metadata(panel);
        if (graphics_create_window(panel) != LUNA_GRAPHICS_OK) {
            return LUNA_PROGRAM_ERR_NOT_FOUND;
        }
    }
    (void)graphics_set_active_window(panel->window_id);
    bind_panel(panel);
    return LUNA_PROGRAM_OK;
}

void SYSV_ABI program_app_write(const struct luna_bootview *bootview, const char *text) {
    struct luna_app_panel *panel;
    (void)bootview;
    if (text == 0 || g_manifest == 0) {
        return;
    }
    panel = find_panel(g_loaded_name);
    if (panel != 0 &&
        !panel_name_matches(g_loaded_name, "console") &&
        (g_graphics_draw_cid.low != 0 || g_graphics_draw_cid.high != 0)) {
        panel_write(panel, text);
    }
    if (g_device_write_cid.low != 0 || g_device_write_cid.high != 0) {
        device_write(text);
    }
}

static void append_u64_hex_fixed(char *out, uint64_t value, uint32_t digits) {
    static const char table[] = "0123456789ABCDEF";
    uint32_t index = 0u;
    while (index < digits) {
        uint32_t shift = (digits - 1u - index) * 4u;
        out[index] = table[(value >> shift) & 0xFu];
        index += 1u;
    }
    out[digits] = '\0';
}

static void device_write_ref_line(const char *prefix, struct luna_object_ref ref) {
    char low_hex[17];
    char high_hex[17];
    if (prefix != 0 && prefix[0] == '[') {
        return;
    }
    append_u64_hex_fixed(low_hex, ref.low, 16u);
    append_u64_hex_fixed(high_hex, ref.high, 16u);
    device_write(prefix);
    device_write(" low=0x");
    device_write(low_hex);
    device_write(" high=0x");
    device_write(high_hex);
    device_write("\r\n");
}

static void device_write_hex_value(const char *prefix, uint64_t value) {
    char hex[17];
    if (prefix != 0 && prefix[0] == '[') {
        return;
    }
    append_u64_hex_fixed(hex, value, 16u);
    device_write(prefix);
    device_write("0x");
    device_write(hex);
    device_write("\r\n");
}

static uint64_t fold_bytes(uint64_t seed, const uint8_t *bytes, uint64_t size) {
    for (uint64_t i = 0u; i < size; ++i) {
        seed ^= (uint64_t)bytes[i];
        seed = (seed << 5) | (seed >> 59);
        seed *= 0x100000001B3ull;
    }
    return seed;
}

static uint64_t compute_bundle_integrity(const void *blob, uint64_t blob_size, uint64_t header_bytes) {
    const uint8_t *bytes = (const uint8_t *)blob;
    if (blob == 0 || header_bytes > blob_size) {
        return 0u;
    }
    return fold_bytes(0xCBF29CE484222325ull, bytes + header_bytes, blob_size - header_bytes);
}

static uint64_t compute_bundle_signature(const struct luna_app_metadata *meta, const void *blob, uint64_t blob_size) {
    const uint8_t *payload = (const uint8_t *)blob + meta->header_bytes;
    uint64_t payload_size = blob_size - meta->header_bytes;
    uint8_t stage[128];
    uint64_t seed = LUNA_PROGRAM_BUNDLE_SIGNATURE_SEED ^ meta->signer_id ^ (meta->source_id << 1);

    zero_bytes(stage, sizeof(stage));
    copy_bytes(stage, meta->name, sizeof(meta->name));
    seed = fold_bytes(seed, stage, sizeof(meta->name));

    zero_bytes(stage, sizeof(stage));
    copy_bytes(stage + 0u, &meta->bundle_id, sizeof(meta->bundle_id));
    copy_bytes(stage + 8u, &meta->source_id, sizeof(meta->source_id));
    copy_bytes(stage + 16u, &meta->app_version, sizeof(meta->app_version));
    copy_bytes(stage + 24u, &meta->capability_count, sizeof(meta->capability_count));
    copy_bytes(stage + 28u, &meta->flags, sizeof(meta->flags));
    copy_bytes(stage + 32u, &meta->abi_major, sizeof(meta->abi_major));
    copy_bytes(stage + 34u, &meta->abi_minor, sizeof(meta->abi_minor));
    copy_bytes(stage + 36u, &meta->sdk_major, sizeof(meta->sdk_major));
    copy_bytes(stage + 38u, &meta->sdk_minor, sizeof(meta->sdk_minor));
    copy_bytes(stage + 40u, &meta->min_proto_version, sizeof(meta->min_proto_version));
    copy_bytes(stage + 44u, &meta->max_proto_version, sizeof(meta->max_proto_version));
    copy_bytes(stage + 48u, &meta->signer_id, sizeof(meta->signer_id));
    copy_bytes(stage + 56u, meta->capability_keys, sizeof(meta->capability_keys));
    seed = fold_bytes(seed, stage, 56u + sizeof(meta->capability_keys));
    return fold_bytes(seed, payload, payload_size);
}

static int parse_luna_metadata(const void *blob, uint64_t blob_size, struct luna_app_metadata *out) {
    const struct luna_bundle_header *bundle = (const struct luna_bundle_header *)blob;
    const struct luna_app_header *legacy = (const struct luna_app_header *)blob;
    struct luna_app_metadata meta;
    if (blob == 0) {
        return 0;
    }
    zero_bytes(&meta, sizeof(meta));
    if (blob_size >= sizeof(struct luna_bundle_header) &&
        bundle->magic == LUNA_PROGRAM_BUNDLE_MAGIC &&
        bundle->version == LUNA_PROGRAM_BUNDLE_VERSION_V2) {
        if (bundle->name[0] == '\0' ||
            bundle->header_bytes < sizeof(struct luna_bundle_header) ||
            bundle->entry_offset < bundle->header_bytes ||
            bundle->entry_offset >= blob_size) {
            return 0;
        }
        copy_name(meta.name, bundle->name);
        meta.bundle_id = bundle->bundle_id;
        meta.source_id = bundle->source_id;
        meta.app_version = bundle->app_version;
        meta.integrity_check = bundle->integrity_check;
        meta.header_bytes = bundle->header_bytes;
        meta.entry_offset = bundle->entry_offset;
        meta.signer_id = bundle->signer_id;
        meta.signature_check = bundle->signature_check;
        meta.capability_count = bundle->capability_count;
        meta.flags = bundle->flags;
        meta.abi_major = bundle->abi_major;
        meta.abi_minor = bundle->abi_minor;
        meta.sdk_major = bundle->sdk_major;
        meta.sdk_minor = bundle->sdk_minor;
        meta.min_proto_version = bundle->min_proto_version;
        meta.max_proto_version = bundle->max_proto_version;
        for (uint32_t i = 0u; i < 4u; ++i) {
            meta.capability_keys[i] = bundle->capability_keys[i];
        }
        if (meta.integrity_check != compute_bundle_integrity(blob, blob_size, meta.header_bytes) ||
            meta.signature_check != compute_bundle_signature(&meta, blob, blob_size)) {
            return 0;
        }
    } else {
        if (blob_size < sizeof(struct luna_app_header) ||
            legacy->magic != LUNA_PROGRAM_APP_MAGIC ||
            legacy->version != LUNA_PROGRAM_APP_VERSION ||
            legacy->name[0] == '\0' ||
            legacy->entry_offset < sizeof(struct luna_app_header) ||
            legacy->entry_offset >= blob_size) {
            return 0;
        }
        copy_name(meta.name, legacy->name);
        meta.bundle_id = 0u;
        meta.source_id = 0u;
        meta.app_version = 1u;
        meta.integrity_check = 0u;
        meta.header_bytes = sizeof(struct luna_app_header);
        meta.entry_offset = legacy->entry_offset;
        meta.signer_id = 0u;
        meta.signature_check = 0u;
        meta.capability_count = legacy->capability_count;
        meta.flags = 0u;
        meta.abi_major = 0u;
        meta.abi_minor = 0u;
        meta.sdk_major = 0u;
        meta.sdk_minor = 0u;
        meta.min_proto_version = 0u;
        meta.max_proto_version = 0u;
        for (uint32_t i = 0u; i < 4u; ++i) {
            meta.capability_keys[i] = legacy->capability_keys[i];
        }
    }
    if (out != 0) {
        *out = meta;
    }
    return 1;
}

static uint32_t validate_package_blob(const void *blob, uint64_t blob_size, struct luna_app_metadata *out) {
    if (!parse_luna_metadata(blob, blob_size, out)) {
        return LUNA_PROGRAM_ERR_BAD_PACKAGE;
    }
    if (out != 0 && (out->flags & LUNA_PROGRAM_BUNDLE_FLAG_DRIVER) != 0u) {
        return LUNA_PROGRAM_ERR_BAD_PACKAGE;
    }
    if (out != 0 &&
        out->abi_major != 0u &&
        out->abi_major != LUNA_LA_ABI_MAJOR) {
        return LUNA_PROGRAM_ERR_BAD_PACKAGE;
    }
    if (out != 0 &&
        out->min_proto_version != 0u &&
        (out->min_proto_version > LUNA_PROTO_VERSION ||
         (out->max_proto_version != 0u && out->max_proto_version < LUNA_PROTO_VERSION))) {
        return LUNA_PROGRAM_ERR_BAD_PACKAGE;
    }
    return LUNA_PROGRAM_OK;
}

static uint32_t package_resolve(const char name[16], struct luna_package_resolve_record *out) {
    volatile struct luna_package_gate *gate =
        (volatile struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base;
    if (g_package_list_cid.low == 0u && g_package_list_cid.high == 0u) {
        (void)request_capability(LUNA_CAP_PACKAGE_LIST, &g_package_list_cid);
    }
    zero_bytes(out, sizeof(*out));
    zero_bytes((void *)(uintptr_t)g_manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 39;
    gate->opcode = LUNA_PACKAGE_RESOLVE;
    gate->cid_low = g_package_list_cid.low;
    gate->cid_high = g_package_list_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = sizeof(*out);
    copy_name(gate->name, name);
    ((package_gate_fn_t)(uintptr_t)g_manifest->package_gate_entry)(
        (struct luna_package_gate *)(uintptr_t)g_manifest->package_gate_base
    );
    return gate->status;
}

static const struct luna_app_metadata *load_data_app(
    const char name[16],
    uint64_t *app_base,
    uint64_t *app_size,
    uint32_t *out_status
) {
    static struct luna_app_metadata meta;
    struct luna_package_resolve_record resolved;
    void *app_buffer = g_loaded_app_image;
    uint64_t app_buffer_size = sizeof(g_loaded_app_image);
    uint8_t header_buf[sizeof(struct luna_bundle_header)] = {0};
    uint64_t header_buf_size = sizeof(struct luna_bundle_header);
    uint64_t size = 0u;
    uint64_t content_size = 0u;
    uint32_t object_type = 0u;

    if (g_manifest == 0 ||
        g_manifest->data_buffer_base == 0u ||
        g_manifest->data_buffer_size == 0u ||
        app_buffer_size == 0u) {
        device_write("[PROGRAM] data buffer missing\r\n");
        return 0;
    }
    if (g_manifest->data_buffer_size < app_buffer_size) {
        app_buffer_size = g_manifest->data_buffer_size;
    }
    if (header_buf_size > app_buffer_size) {
        header_buf_size = app_buffer_size;
    }

    zero_bytes(&resolved, sizeof(resolved));
    if (out_status != 0) {
        *out_status = LUNA_PACKAGE_ERR_NOT_FOUND;
    }
    {
        uint32_t status = package_resolve(name, &resolved);
        if (out_status != 0) {
            *out_status = status;
        }
        if (status != LUNA_PACKAGE_OK) {
            device_write("[PROGRAM] resolve fail\r\n");
            return 0;
        }
    }
    device_write("[PROGRAM] resolve ok\r\n");
    if (resolved.app_object.low == 0u && resolved.app_object.high == 0u) {
        device_write("[PROGRAM] resolve empty\r\n");
        return 0;
    }
    device_write_ref_line("[PROGRAM] resolve app", resolved.app_object);
    {
        uint32_t status = data_draw_span(
            g_data_draw_cid,
            resolved.app_object,
            0u,
            header_buf,
            header_buf_size,
            &size,
            &content_size,
            &object_type
        );
        if (status != LUNA_DATA_OK) {
            device_write_hex_value("[PROGRAM] data dbg stage=", g_last_data_draw_stage);
            device_write_hex_value("[PROGRAM] data dbg type=", g_last_data_draw_type);
            device_write_hex_value("[PROGRAM] data dbg flags=", g_last_data_draw_flags);
            device_write_hex_value("[PROGRAM] data dbg value0=", g_last_data_draw_value0);
            device_write_hex_value("[PROGRAM] data dbg value1=", g_last_data_draw_value1);
            if (status == LUNA_DATA_ERR_NOT_FOUND) {
                device_write("[PROGRAM] data draw missing\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PROGRAM] data draw io\r\n");
            } else {
                device_write("[PROGRAM] data draw fail\r\n");
            }
            return 0;
        }
    }
    device_write_hex_value("[PROGRAM] data head type=", object_type);
    device_write_hex_value("[PROGRAM] data head size=", size);
    device_write_hex_value("[PROGRAM] data head content=", content_size);
    if (object_type != LUNA_DATA_OBJECT_TYPE_LUNA_APP || content_size == 0u || content_size > app_buffer_size) {
        device_write("[PROGRAM] data type fail\r\n");
        return 0;
    }
    {
        uint32_t status = data_draw_span(
            g_data_draw_cid,
            resolved.app_object,
            0u,
            app_buffer,
            content_size,
            &size,
            &content_size,
            &object_type
        );
        if (status != LUNA_DATA_OK) {
            device_write_hex_value("[PROGRAM] data dbg stage=", g_last_data_draw_stage);
            device_write_hex_value("[PROGRAM] data dbg type=", g_last_data_draw_type);
            device_write_hex_value("[PROGRAM] data dbg flags=", g_last_data_draw_flags);
            device_write_hex_value("[PROGRAM] data dbg value0=", g_last_data_draw_value0);
            device_write_hex_value("[PROGRAM] data dbg value1=", g_last_data_draw_value1);
            if (status == LUNA_DATA_ERR_NOT_FOUND) {
                device_write("[PROGRAM] data full missing\r\n");
            } else if (status == LUNA_DATA_ERR_IO) {
                device_write("[PROGRAM] data full io\r\n");
            } else {
                device_write("[PROGRAM] data full fail\r\n");
            }
            return 0;
        }
    }
    device_write_hex_value("[PROGRAM] data full size=", size);
    device_write_hex_value("[PROGRAM] data full content=", content_size);
    if (validate_package_blob(app_buffer, content_size, &meta) != LUNA_PROGRAM_OK) {
        device_write("audit program.load denied reason=abi-or-proto\r\n");
        device_write("[PROGRAM] data validate fail\r\n");
        return 0;
    }
    device_write("audit program.load verified=PROGRAM\r\n");
    device_write_hex_value("[PROGRAM] data entry=", meta.entry_offset);
    device_write("[PROGRAM] data parse ok\r\n");
    *app_base = (uint64_t)(uintptr_t)app_buffer;
    *app_size = content_size;
    return &meta;
}

static const struct luna_app_metadata *find_embedded_app(const char name[16], uint64_t *app_base, uint64_t *app_size) {
    static struct luna_app_metadata meta;
    struct luna_embedded_app apps[5] = {
        { g_manifest->app_guard_base, g_manifest->app_guard_size },
        { g_manifest->app_hello_base, g_manifest->app_hello_size },
        { g_manifest->app_files_base, g_manifest->app_files_size },
        { g_manifest->app_notes_base, g_manifest->app_notes_size },
        { g_manifest->app_console_base, g_manifest->app_console_size },
    };

    for (size_t i = 0; i < 5; ++i) {
        if (apps[i].base == 0 || apps[i].size == 0) {
            continue;
        }
        if (validate_package_blob((const void *)(uintptr_t)apps[i].base, apps[i].size, &meta) != LUNA_PROGRAM_OK) {
            continue;
        }
        if (app_name_matches(meta.name, name)) {
            *app_base = apps[i].base;
            *app_size = apps[i].size;
            return &meta;
        }
    }
    return 0;
}

static void program_load(struct luna_program_gate *gate) {
    uint64_t app_base = 0;
    uint64_t app_size = 0;
    uint32_t data_status = LUNA_PACKAGE_ERR_NOT_FOUND;
    const struct luna_app_metadata *meta;

    if (validate_capability(LUNA_CAP_PROGRAM_LOAD, gate->cid_low, gate->cid_high, LUNA_PROGRAM_LOAD_APP) != LUNA_GATE_OK) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }

    meta = load_data_app(gate->name, &app_base, &app_size, &data_status);
    if (meta == 0) {
        device_write("[PROGRAM] embedded fallback\r\n");
        meta = find_embedded_app(gate->name, &app_base, &app_size);
    }
    if (meta == 0) {
        device_write("[PROGRAM] load not found\r\n");
        gate->status = LUNA_PROGRAM_ERR_NOT_FOUND;
        return;
    }

    g_loaded_ticket = 1;
    g_loaded_app_base = app_base;
    g_loaded_app_size = app_size;
    g_loaded_entry = app_base + meta->entry_offset;
    copy_name(g_loaded_name, meta->name);
    {
        struct luna_app_panel *panel = find_panel(g_loaded_name);
        if (panel != 0) {
            if (ensure_window_bound(panel) != LUNA_PROGRAM_OK) {
                device_write("[PROGRAM] panel bind fail\r\n");
                gate->status = LUNA_PROGRAM_ERR_NOT_FOUND;
                return;
            }
            clear_panel(panel);
        }
    }

    gate->ticket = g_loaded_ticket;
    gate->app_base = g_loaded_app_base;
    gate->app_size = g_loaded_app_size;
    gate->entry_addr = g_loaded_entry;
    gate->result_count = 1;
    gate->status = LUNA_PROGRAM_OK;
}

static void program_start(struct luna_program_gate *gate) {
    struct luna_app_metadata meta;
    typedef void (SYSV_ABI *app_entry_fn_t)(const struct luna_bootview *bootview);

    if (validate_capability(LUNA_CAP_PROGRAM_START, gate->cid_low, gate->cid_high, LUNA_PROGRAM_START_APP) != LUNA_GATE_OK) {
        gate->status = LUNA_PROGRAM_ERR_INVALID_CAP;
        return;
    }
    if (gate->ticket != g_loaded_ticket || g_loaded_ticket == 0) {
        gate->status = LUNA_PROGRAM_ERR_BAD_TICKET;
        return;
    }

    if (validate_package_blob((const void *)(uintptr_t)g_loaded_app_base, g_loaded_app_size, &meta) != LUNA_PROGRAM_OK) {
        device_write("audit program.start denied reason=abi-or-proto\r\n");
        device_write("[PROGRAM] start validate fail\r\n");
        gate->status = LUNA_PROGRAM_ERR_BAD_PACKAGE;
        return;
    }
    device_write("audit program.start abi=ok\r\n");
    device_write("[PROGRAM] start validate ok\r\n");

    for (uint32_t i = 0; i < meta.capability_count && i < 4; ++i) {
        struct luna_cid ignored = {0, 0};
        if (request_capability(meta.capability_keys[i], &ignored) != LUNA_GATE_OK) {
            device_write("audit program.start denied reason=capability\r\n");
            device_write("[PROGRAM] start cap fail\r\n");
            gate->status = LUNA_PROGRAM_ERR_BAD_PACKAGE;
            return;
        }
    }
    device_write("audit program.start approved=SECURITY\r\n");
    device_write("[PROGRAM] start entry begin\r\n");
    device_write_hex_value("[PROGRAM] start handoff=", g_loaded_entry);

    ((app_entry_fn_t)(uintptr_t)g_loaded_entry)(g_bootview);
    device_write("[PROGRAM] start entry return\r\n");
    gate->status = LUNA_PROGRAM_OK;
}

static void program_stop(struct luna_program_gate *gate) {
    if (validate_capability(LUNA_CAP_PROGRAM_STOP, gate->cid_low, gate->cid_high, LUNA_PROGRAM_STOP_APP) != LUNA_GATE_OK) {
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
    g_manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    (void)request_capability(LUNA_CAP_PACKAGE_LIST, &g_package_list_cid);
    if (request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) == LUNA_GATE_OK) {
        device_write("[PROGRAM] runtime ready\r\n");
    }
    (void)request_capability(LUNA_CAP_GRAPHICS_DRAW, &g_graphics_draw_cid);
    (void)request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid);
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
    } else if (gate->opcode == LUNA_PROGRAM_BIND_VIEW) {
        bind_view(gate);
    }
}
