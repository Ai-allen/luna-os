#include <stddef.h>
#include <stdint.h>

#include "../include/luna_ui.h"

static UiContext g_ui_context;

static const UiTokens g_ui_default_tokens = {
    .colors = {
        [UI_COLOR_BG_BASE] = 0x0E1116u,
        [UI_COLOR_BG_CANVAS] = 0x131923u,
        [UI_COLOR_SURFACE] = 0x1A212Cu,
        [UI_COLOR_PANEL] = 0x181E28u,
        [UI_COLOR_PANEL_BORDER] = 0x3A4352u,
        [UI_COLOR_ACCENT] = 0x5E9BFFu,
        [UI_COLOR_ACCENT_SOFT] = 0x243B61u,
        [UI_COLOR_TEXT_PRIMARY] = 0xF2F5FAu,
        [UI_COLOR_TEXT_SECONDARY] = 0xAAB3C0u,
        [UI_COLOR_TEXT_TERTIARY] = 0x727C8Cu,
    },
    .radii = {
        [UI_RADIUS_MD] = { 12 },
        [UI_RADIUS_LG] = { 16 },
    },
    .shadows = {
        [UI_SHADOW_NONE] = { .layer_count = 0u },
        [UI_SHADOW_SM] = {
            .layers = {
                { 0, 3, 2, 2, 0x0A0D13u },
                { 0, 1, 1, 1, 0x111722u },
            },
            .layer_count = 2u,
        },
        [UI_SHADOW_MD] = {
            .layers = {
                { 0, 6, 4, 3, 0x070A10u },
                { 0, 2, 2, 2, 0x0D121Bu },
            },
            .layer_count = 2u,
        },
        [UI_SHADOW_LG] = {
            .layers = {
                { 0, 10, 6, 4, 0x05080Du },
                { 0, 4, 3, 2, 0x0A1018u },
            },
            .layer_count = 2u,
        },
    },
    .spacing_unit = 8,
};

static int valid_rect(Rect r) {
    return r.width > 0 && r.height > 0;
}

static const UiTokens *active_tokens(void) {
    return g_ui_context.tokens != 0 ? g_ui_context.tokens : &g_ui_default_tokens;
}

static uint32_t resolve_color(UiColorRole role, UiState state) {
    const UiTokens *tokens = active_tokens();
    uint32_t color = tokens->colors[role];

    if (role == UI_COLOR_PANEL) {
        if (state == UI_STATE_HOVER) {
            return tokens->colors[UI_COLOR_SURFACE];
        }
        if (state == UI_STATE_ACTIVE) {
            return tokens->colors[UI_COLOR_ACCENT_SOFT];
        }
        if (state == UI_STATE_FOCUS) {
            return tokens->colors[UI_COLOR_SURFACE];
        }
    }

    if (role == UI_COLOR_PANEL_BORDER && state == UI_STATE_FOCUS) {
        return tokens->colors[UI_COLOR_ACCENT];
    }

    if (role == UI_COLOR_TEXT_PRIMARY) {
        if (state == UI_STATE_ACTIVE || state == UI_STATE_FOCUS) {
            return tokens->colors[UI_COLOR_TEXT_PRIMARY];
        }
        if (state == UI_STATE_HOVER) {
            return 0xFFFFFFu;
        }
    }

    if (role == UI_COLOR_TEXT_SECONDARY && state == UI_STATE_HOVER) {
        return tokens->colors[UI_COLOR_TEXT_PRIMARY];
    }

    return color;
}

static Rect inflate_rect(Rect r, int32_t expand, int32_t dx, int32_t dy) {
    Rect out = r;
    out.x -= expand;
    out.y -= expand;
    out.width += expand * 2;
    out.height += expand * 2;
    out.x += dx;
    out.y += dy;
    return out;
}

static void backend_fill_round_rect(Rect r, int32_t radius, uint32_t color) {
    if (!valid_rect(r) || g_ui_context.backend.fill_round_rect == 0) {
        return;
    }
    g_ui_context.backend.fill_round_rect(r, radius, color);
}

static void backend_stroke_round_rect(Rect r, int32_t radius, int32_t width, uint32_t color) {
    if (!valid_rect(r) || g_ui_context.backend.stroke_round_rect == 0) {
        return;
    }
    g_ui_context.backend.stroke_round_rect(r, radius, width, color);
}

void ui_set_context(UiContext context) {
    g_ui_context = context;
    if (g_ui_context.tokens == 0) {
        g_ui_context.tokens = &g_ui_default_tokens;
    }
}

void ui_reset_context(void) {
    g_ui_context.backend.fill_round_rect = 0;
    g_ui_context.backend.stroke_round_rect = 0;
    g_ui_context.backend.draw_text = 0;
    g_ui_context.tokens = &g_ui_default_tokens;
}

const UiTokens *ui_default_tokens(void) {
    return &g_ui_default_tokens;
}

uint32_t ui_color(UiColorRole role) {
    return active_tokens()->colors[role];
}

int32_t ui_radius(UiRadiusToken token) {
    return active_tokens()->radii[token].radius;
}

int32_t ui_space(uint32_t steps) {
    return (int32_t)steps * active_tokens()->spacing_unit;
}

void ui_shadow(Rect r, UiShadow level) {
    const UiTokens *tokens = active_tokens();
    const UiShadowDef *shadow;
    uint32_t i;

    if (level == UI_SHADOW_NONE || level > UI_SHADOW_LG || !valid_rect(r)) {
        return;
    }

    shadow = &tokens->shadows[level];
    for (i = 0u; i < shadow->layer_count; ++i) {
        UiShadowLayer layer = shadow->layers[i];
        Rect layer_rect = inflate_rect(r, layer.expand, layer.offset_x, layer.offset_y);
        int32_t radius = ui_radius(UI_RADIUS_MD) + layer.radius_bias;
        backend_fill_round_rect(layer_rect, radius, layer.color);
    }
}

void ui_panel(Rect r, UiPanelStyle style) {
    int32_t radius = ui_radius(style.radius);
    uint32_t fill = resolve_color(style.fill_role, style.state);
    uint32_t border = resolve_color(style.border_role, style.state);

    if (!valid_rect(r)) {
        return;
    }

    ui_shadow(r, style.shadow);
    backend_fill_round_rect(r, radius, fill);
    backend_stroke_round_rect(r, radius, 1, border);
}

void ui_card(Rect r, UiState state) {
    UiPanelStyle style;
    style.fill_role = UI_COLOR_PANEL;
    style.border_role = UI_COLOR_PANEL_BORDER;
    style.radius = UI_RADIUS_LG;
    style.shadow = state == UI_STATE_ACTIVE ? UI_SHADOW_LG : UI_SHADOW_MD;
    style.state = state;
    ui_panel(r, style);
}

void ui_text(Vec2 pos, UiTextStyle style, const char *text) {
    uint32_t color;

    if (text == 0 || g_ui_context.backend.draw_text == 0) {
        return;
    }

    color = resolve_color(style.color_role, style.state);
    g_ui_context.backend.draw_text(pos, color, text);
}

void ui_example_draw_card(Rect r, const char *title) {
    Rect title_band;
    Rect body_band;
    UiTextStyle title_style;
    UiTextStyle body_style;

    if (!valid_rect(r)) {
        return;
    }

    ui_card(r, UI_STATE_DEFAULT);

    title_band = r;
    title_band.x += ui_space(2u);
    title_band.y += ui_space(2u);
    title_band.width -= ui_space(4u);
    title_band.height = ui_space(4u);

    body_band = r;
    body_band.x += ui_space(2u);
    body_band.y += ui_space(6u);
    body_band.width -= ui_space(4u);
    body_band.height = ui_space(3u);

    ui_panel(
        title_band,
        (UiPanelStyle){
            .fill_role = UI_COLOR_SURFACE,
            .border_role = UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = UI_STATE_DEFAULT,
        }
    );

    ui_panel(
        body_band,
        (UiPanelStyle){
            .fill_role = UI_COLOR_BG_CANVAS,
            .border_role = UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = UI_STATE_HOVER,
        }
    );

    title_style.color_role = UI_COLOR_TEXT_PRIMARY;
    title_style.state = UI_STATE_DEFAULT;
    ui_text((Vec2){ title_band.x + ui_space(1u), title_band.y + ui_space(1u) }, title_style, title);

    body_style.color_role = UI_COLOR_TEXT_SECONDARY;
    body_style.state = UI_STATE_DEFAULT;
    ui_text((Vec2){ body_band.x + ui_space(1u), body_band.y + ui_space(1u) }, body_style, "Card body");
}
