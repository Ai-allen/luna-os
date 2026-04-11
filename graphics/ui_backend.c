#include "graphics_internal.h"

uint32_t ui_rgb_color(uint32_t rgb_color) {
    uint8_t r = (uint8_t)((rgb_color >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((rgb_color >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(rgb_color & 0xFFu);
    return rgb(r, g, b);
}

uint32_t ui_token_rgb(UiColorRole role) {
    return ui_rgb_color(ui_color(role));
}

uint32_t ui_radius_px(UiRadiusToken token) {
    int32_t radius = ui_radius(token);
    return (uint32_t)(radius > 0 ? radius : 0);
}

void ui_backend_fill_round_rect(Rect r, int32_t radius, uint32_t color) {
    if (g_framebuffer == 0 || r.width <= 0 || r.height <= 0) {
        return;
    }
    framebuffer_fill_round_rect(
        (uint32_t)r.x,
        (uint32_t)r.y,
        (uint32_t)r.width,
        (uint32_t)r.height,
        (uint32_t)(radius > 0 ? radius : 0),
        ui_rgb_color(color)
    );
}

void ui_backend_stroke_round_rect(Rect r, int32_t radius, int32_t stroke_width, uint32_t color) {
    (void)stroke_width;
    if (g_framebuffer == 0 || r.width <= 0 || r.height <= 0) {
        return;
    }
    framebuffer_stroke_rect(
        (uint32_t)r.x,
        (uint32_t)r.y,
        (uint32_t)r.width,
        (uint32_t)r.height,
        ui_rgb_color(color)
    );
    if (radius > 0) {
        framebuffer_fill_round_rect(
            (uint32_t)r.x,
            (uint32_t)r.y,
            (uint32_t)(radius > r.width ? r.width : radius),
            (uint32_t)(radius > r.height ? r.height : radius),
            (uint32_t)radius,
            ui_rgb_color(color)
        );
    }
}

void ui_backend_draw_text(Vec2 pos, uint32_t color, const char *text) {
    uint32_t px = (uint32_t)pos.x;
    uint32_t py = (uint32_t)pos.y;
    uint32_t fg = ui_rgb_color(color);

    if (g_framebuffer == 0 || text == 0) {
        return;
    }
    while (*text != '\0') {
        render_glyph(px, py, *text, fg, 0u);
        px += g_cell_width;
        text += 1;
    }
}

void ui_begin_frame(void) {
    UiContext context;

    context.backend.fill_round_rect = ui_backend_fill_round_rect;
    context.backend.stroke_round_rect = ui_backend_stroke_round_rect;
    context.backend.draw_text = ui_backend_draw_text;
    context.tokens = ui_default_tokens();
    ui_set_context(context);
}

int ui_rect_contains_cell(Rect r, uint32_t cell_x, uint32_t cell_y) {
    int32_t px = (int32_t)cell_px(cell_x);
    int32_t py = (int32_t)cell_py(cell_y);
    return px >= r.x && py >= r.y && px < r.x + r.width && py < r.y + r.height;
}

UiState ui_state_from_hit(const UiHitContext *context, UiHitTarget target) {
    int hovered = 0;
    int active = 0;
    int focused = 0;

    if (context == 0) {
        return UI_STATE_DEFAULT;
    }

    if (target.use_rect_hit != 0u) {
        hovered = ui_rect_contains_cell(target.rect, context->cursor_x, context->cursor_y);
    } else if (context->hover_kind == target.hover_kind) {
        hovered = target.hover_glyph == 0xFFFFFFFFu || context->hover_glyph == target.hover_glyph;
    }

    active = hovered && ((context->pointer_buttons & 0x01u) != 0u);
    focused = target.focus_index != 0xFFFFFFFFu &&
        (context->focus_index == target.focus_index || context->logical_focus_id == target.focus_index);

    if (active) {
        return UI_STATE_ACTIVE;
    }
    if (hovered) {
        return UI_STATE_HOVER;
    }
    if (focused) {
        return UI_STATE_FOCUS;
    }
    return UI_STATE_DEFAULT;
}
