#include "graphics_internal.h"

void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (g_fb_format == 1u) {
        return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    }
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

uint32_t palette_rgb(uint8_t index) {
    switch (index & 0x0Fu) {
        case 0x0: return rgb(12, 18, 28);
        case 0x1: return rgb(28, 58, 96);
        case 0x2: return rgb(36, 108, 88);
        case 0x3: return rgb(50, 124, 124);
        case 0x4: return rgb(140, 60, 64);
        case 0x5: return rgb(110, 78, 150);
        case 0x6: return rgb(168, 118, 58);
        case 0x7: return rgb(198, 210, 222);
        case 0x8: return rgb(82, 92, 110);
        case 0x9: return rgb(94, 160, 255);
        case 0xA: return rgb(80, 210, 160);
        case 0xB: return rgb(108, 212, 218);
        case 0xC: return rgb(240, 102, 110);
        case 0xD: return rgb(198, 140, 255);
        case 0xE: return rgb(255, 214, 84);
        default: return rgb(246, 248, 252);
    }
}

void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (g_framebuffer == 0 || x >= g_fb_width || y >= g_fb_height) {
        return;
    }
    g_framebuffer[y * g_fb_stride + x] = color;
}

void device_present_framebuffer(void) {
    volatile struct luna_device_gate *gate;

    if (g_framebuffer == 0 || g_fb_buffer_bytes == 0u || g_device_write_cid.low == 0u || g_device_write_cid.high == 0u) {
        return;
    }
    gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 46;
    gate->opcode = LUNA_DEVICE_DISPLAY_PRESENT;
    gate->caller_space = LUNA_SPACE_GRAPHICS;
    gate->actor_space = LUNA_SPACE_GRAPHICS;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_DISPLAY0;
    gate->buffer_addr = (uint64_t)(uintptr_t)g_framebuffer;
    gate->buffer_size = g_fb_buffer_bytes;
    gate->size = g_fb_buffer_bytes;
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
}

void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            framebuffer_put_pixel(x + col, y + row, color);
        }
    }
}

uint32_t cell_px(uint32_t x) {
    return x * g_cell_width;
}

uint32_t cell_py(uint32_t y) {
    return y * g_cell_height;
}

void framebuffer_stroke_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (width == 0u || height == 0u) {
        return;
    }
    framebuffer_fill_rect(x, y, width, 1u, color);
    if (height > 1u) {
        framebuffer_fill_rect(x, y + height - 1u, width, 1u, color);
    }
    if (height > 2u) {
        framebuffer_fill_rect(x, y + 1u, 1u, height - 2u, color);
        if (width > 1u) {
            framebuffer_fill_rect(x + width - 1u, y + 1u, 1u, height - 2u, color);
        }
    }
}

void framebuffer_fill_round_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color) {
    if (width == 0u || height == 0u) {
        return;
    }
    if (radius == 0u || width <= radius * 2u || height <= radius * 2u) {
        framebuffer_fill_rect(x, y, width, height, color);
        return;
    }
    framebuffer_fill_rect(x + radius, y, width - radius * 2u, height, color);
    framebuffer_fill_rect(x, y + radius, radius, height - radius * 2u, color);
    framebuffer_fill_rect(x + width - radius, y + radius, radius, height - radius * 2u, color);
    for (uint32_t row = 0u; row < radius; ++row) {
        uint32_t inset = (radius - row - 1u) / 2u;
        uint32_t span = width - inset * 2u;
        framebuffer_fill_rect(x + inset, y + row, span, 1u, color);
        framebuffer_fill_rect(x + inset, y + height - 1u - row, span, 1u, color);
    }
}

static uint8_t glyph_row(char ch, uint32_t row) {
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - ('a' - 'A'));
    }
    switch (ch) {
        case 'A': { static const uint8_t v[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return v[row]; }
        case 'B': { static const uint8_t v[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; return v[row]; }
        case 'C': { static const uint8_t v[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; return v[row]; }
        case 'D': { static const uint8_t v[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; return v[row]; }
        case 'E': { static const uint8_t v[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return v[row]; }
        case 'F': { static const uint8_t v[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return v[row]; }
        case 'G': { static const uint8_t v[7] = {0x0E,0x11,0x10,0x10,0x13,0x11,0x0F}; return v[row]; }
        case 'H': { static const uint8_t v[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; return v[row]; }
        case 'I': { static const uint8_t v[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; return v[row]; }
        case 'J': { static const uint8_t v[7] = {0x1F,0x02,0x02,0x02,0x12,0x12,0x0C}; return v[row]; }
        case 'K': { static const uint8_t v[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; return v[row]; }
        case 'L': { static const uint8_t v[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return v[row]; }
        case 'M': { static const uint8_t v[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return v[row]; }
        case 'N': { static const uint8_t v[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return v[row]; }
        case 'O': { static const uint8_t v[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return v[row]; }
        case 'P': { static const uint8_t v[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return v[row]; }
        case 'Q': { static const uint8_t v[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; return v[row]; }
        case 'R': { static const uint8_t v[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return v[row]; }
        case 'S': { static const uint8_t v[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; return v[row]; }
        case 'T': { static const uint8_t v[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return v[row]; }
        case 'U': { static const uint8_t v[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return v[row]; }
        case 'V': { static const uint8_t v[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; return v[row]; }
        case 'W': { static const uint8_t v[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; return v[row]; }
        case 'X': { static const uint8_t v[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; return v[row]; }
        case 'Y': { static const uint8_t v[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; return v[row]; }
        case 'Z': { static const uint8_t v[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; return v[row]; }
        case '0': { static const uint8_t v[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; return v[row]; }
        case '1': { static const uint8_t v[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; return v[row]; }
        case '2': { static const uint8_t v[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; return v[row]; }
        case '3': { static const uint8_t v[7] = {0x1E,0x01,0x01,0x06,0x01,0x01,0x1E}; return v[row]; }
        case '4': { static const uint8_t v[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; return v[row]; }
        case '5': { static const uint8_t v[7] = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; return v[row]; }
        case '6': { static const uint8_t v[7] = {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}; return v[row]; }
        case '7': { static const uint8_t v[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; return v[row]; }
        case '8': { static const uint8_t v[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; return v[row]; }
        case '9': { static const uint8_t v[7] = {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}; return v[row]; }
        case '[': { static const uint8_t v[7] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; return v[row]; }
        case ']': { static const uint8_t v[7] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; return v[row]; }
        case '-': { static const uint8_t v[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; return v[row]; }
        case '_': { static const uint8_t v[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}; return v[row]; }
        case '.': { static const uint8_t v[7] = {0x00,0x00,0x00,0x00,0x00,0x06,0x06}; return v[row]; }
        case ':': { static const uint8_t v[7] = {0x00,0x06,0x06,0x00,0x06,0x06,0x00}; return v[row]; }
        case '/': { static const uint8_t v[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; return v[row]; }
        case '<': { static const uint8_t v[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02}; return v[row]; }
        case '>': { static const uint8_t v[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08}; return v[row]; }
        default: { static const uint8_t v[7] = {0x1F,0x11,0x15,0x15,0x15,0x11,0x1F}; return v[row]; }
    }
}

void render_glyph(uint32_t px, uint32_t py, char ch, uint32_t fg, uint32_t bg) {
    for (uint32_t row = 0u; row < 7u; ++row) {
        uint8_t bits = glyph_row(ch, row);
        for (uint32_t col = 0u; col < 5u; ++col) {
            uint32_t color = (bits & (uint8_t)(1u << (4u - col))) != 0u ? fg : bg;
            framebuffer_put_pixel(px + col + 1u, py + row + 3u, color);
        }
    }
}

void legacy_device_put(uint32_t x, uint32_t y, uint8_t glyph, uint8_t attr) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    struct luna_display_cell cell;
    if (g_device_write_cid.low == 0u && g_device_write_cid.high == 0u) {
        return;
    }
    cell.x = x;
    cell.y = y;
    cell.glyph = glyph;
    cell.attr = attr;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 44;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_GRAPHICS;
    gate->actor_space = LUNA_SPACE_GRAPHICS;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_DISPLAY0;
    gate->buffer_addr = (uint64_t)(uintptr_t)&cell;
    gate->buffer_size = sizeof(cell);
    gate->size = sizeof(cell);
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
}

void put_cell(uint32_t x, uint32_t y, uint8_t glyph, uint8_t attr) {
    if (x >= 80u || y >= 25u) {
        return;
    }
    g_text_cells[y * 80u + x] = (uint16_t)(((uint16_t)attr << 8) | glyph);
    if (g_framebuffer != 0) {
        uint32_t bg = palette_rgb((uint8_t)(attr >> 4));
        uint32_t fg = palette_rgb((uint8_t)(attr & 0x0Fu));
        framebuffer_fill_rect(cell_px(x), cell_py(y), g_cell_width, g_cell_height, bg);
        render_glyph(cell_px(x), cell_py(y), (char)glyph, fg, bg);
    } else {
        legacy_device_put(x, y, glyph, attr);
    }
}

void draw_text(uint32_t x, uint32_t y, uint8_t attr, const char *text) {
    while (text != 0 && *text != '\0' && x < 80u) {
        put_cell(x, y, (uint8_t)*text, attr);
        ++x;
        ++text;
    }
}

void fill_cells(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t attr) {
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            put_cell(x + col, y + row, (uint8_t)' ', attr);
        }
    }
}

void clear_screen(uint8_t attr) {
    fill_cells(0u, 0u, 80u, 25u, attr);
}

void framebuffer_fill_cell_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    framebuffer_fill_rect(cell_px(x), cell_py(y), cell_px(width), cell_py(height), color);
}

void framebuffer_fill_cell_round_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color) {
    framebuffer_fill_round_rect(cell_px(x), cell_py(y), cell_px(width), cell_py(height), radius, color);
}

void framebuffer_stroke_cell_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    framebuffer_stroke_rect(cell_px(x), cell_py(y), cell_px(width), cell_py(height), color);
}

void framebuffer_draw_progress_strip(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t filled,
    uint32_t rail,
    uint32_t glow
) {
    if (width == 0u) {
        return;
    }
    if (filled > width) {
        filled = width;
    }
    framebuffer_fill_cell_round_rect(x, y, width, 1u, ui_radius_px(UI_RADIUS_MD), rail);
    if (filled != 0u) {
        framebuffer_fill_cell_round_rect(x, y, filled, 1u, ui_radius_px(UI_RADIUS_MD), glow);
    }
}

void framebuffer_draw_surface(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t body,
    uint32_t border,
    uint32_t radius
) {
    if (width == 0u || height == 0u) {
        return;
    }
    framebuffer_fill_cell_round_rect(x, y, width, height, radius, body);
    framebuffer_stroke_cell_rect(x, y, width, height, border);
}

void framebuffer_draw_surface_row(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t body,
    uint32_t border,
    uint32_t accent,
    uint32_t selected
) {
    if (width == 0u) {
        return;
    }
    framebuffer_draw_surface(x, y, width, 2u, body, border, ui_radius_px(UI_RADIUS_MD));
    framebuffer_fill_cell_round_rect(x + 1u, y, 1u, 2u, ui_radius_px(UI_RADIUS_MD), selected != 0u ? accent : border);
    if (width > 4u) {
        framebuffer_fill_cell_round_rect(
            x + width - 3u,
            y + 1u,
            2u,
            1u,
            ui_radius_px(UI_RADIUS_MD),
            selected != 0u ? ui_token_rgb(UI_COLOR_TEXT_PRIMARY) : ui_token_rgb(UI_COLOR_TEXT_SECONDARY)
        );
    }
}

int device_read_display_info(struct luna_display_info *info) {
    volatile struct luna_device_gate *gate = (volatile struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base;
    zero_bytes((void *)(uintptr_t)g_manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 45;
    gate->opcode = LUNA_DEVICE_READ;
    gate->caller_space = LUNA_SPACE_GRAPHICS;
    gate->actor_space = LUNA_SPACE_GRAPHICS;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_DISPLAY0;
    gate->buffer_addr = (uint64_t)(uintptr_t)info;
    gate->buffer_size = sizeof(*info);
    ((device_gate_fn_t)(uintptr_t)g_manifest->device_gate_entry)((struct luna_device_gate *)(uintptr_t)g_manifest->device_gate_base);
    return gate->status == LUNA_DEVICE_OK && gate->size >= sizeof(*info);
}

void init_framebuffer_from_info(const struct luna_display_info *info) {
    uint64_t pixels;
    uint64_t bytes;

    if (info == 0 || info->framebuffer_base == 0u || info->framebuffer_width == 0u || info->framebuffer_height == 0u) {
        return;
    }
    g_fb_width = info->framebuffer_width;
    g_fb_height = info->framebuffer_height;
    g_fb_stride = info->framebuffer_pixels_per_scanline;
    g_fb_format = info->framebuffer_pixel_format;
    pixels = (uint64_t)g_fb_stride * (uint64_t)g_fb_height;
    bytes = pixels * sizeof(uint32_t);
    if (g_manifest->reserve_base == 0u || g_manifest->reserve_size < bytes) {
        g_framebuffer = 0;
        g_fb_buffer_bytes = 0u;
        return;
    }
    g_framebuffer = (uint32_t *)(uintptr_t)g_manifest->reserve_base;
    g_fb_buffer_bytes = bytes;
    zero_bytes(g_framebuffer, (size_t)g_fb_buffer_bytes);
    g_cell_width = g_fb_width / 80u;
    g_cell_height = g_fb_height / 25u;
    if (g_cell_width > 9u) {
        g_cell_width = 9u;
    }
    if (g_cell_height > 16u) {
        g_cell_height = 16u;
    }
    if (g_cell_width < 8u) {
        g_cell_width = 8u;
    }
    if (g_cell_height < 14u) {
        g_cell_height = 14u;
    }
}

void init_framebuffer(const struct luna_bootview *bootview) {
    struct luna_display_info fallback;
    g_framebuffer = 0;
    g_fb_width = 0u;
    g_fb_height = 0u;
    g_fb_stride = 0u;
    g_fb_format = 0u;
    g_fb_buffer_bytes = 0u;
    g_cell_width = 8u;
    g_cell_height = 16u;
    if (g_device_read_cid.low != 0u || g_device_read_cid.high != 0u) {
        struct luna_display_info info;
        zero_bytes(&info, sizeof(info));
        if (device_read_display_info(&info)) {
            init_framebuffer_from_info(&info);
            return;
        }
    }
    if (bootview == 0) {
        return;
    }
    zero_bytes(&fallback, sizeof(fallback));
    fallback.framebuffer_base = bootview->framebuffer_base;
    fallback.framebuffer_size = bootview->framebuffer_size;
    fallback.framebuffer_width = bootview->framebuffer_width;
    fallback.framebuffer_height = bootview->framebuffer_height;
    fallback.framebuffer_pixels_per_scanline = bootview->framebuffer_pixels_per_scanline;
    fallback.framebuffer_pixel_format = bootview->framebuffer_pixel_format;
    init_framebuffer_from_info(&fallback);
}
