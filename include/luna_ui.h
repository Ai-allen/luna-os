#ifndef LUNA_UI_H
#define LUNA_UI_H

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
} Vec2;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} Rect;

typedef enum ui_state {
    UI_STATE_DEFAULT = 0,
    UI_STATE_HOVER = 1,
    UI_STATE_ACTIVE = 2,
    UI_STATE_FOCUS = 3,
} UiState;

typedef enum ui_shadow {
    UI_SHADOW_NONE = 0,
    UI_SHADOW_SM = 1,
    UI_SHADOW_MD = 2,
    UI_SHADOW_LG = 3,
} UiShadow;

typedef enum ui_color_role {
    UI_COLOR_BG_BASE = 0,
    UI_COLOR_BG_CANVAS = 1,
    UI_COLOR_SURFACE = 2,
    UI_COLOR_PANEL = 3,
    UI_COLOR_PANEL_BORDER = 4,
    UI_COLOR_ACCENT = 5,
    UI_COLOR_ACCENT_SOFT = 6,
    UI_COLOR_TEXT_PRIMARY = 7,
    UI_COLOR_TEXT_SECONDARY = 8,
    UI_COLOR_TEXT_TERTIARY = 9,
    UI_COLOR_COUNT = 10,
} UiColorRole;

typedef enum ui_radius_token {
    UI_RADIUS_MD = 0,
    UI_RADIUS_LG = 1,
    UI_RADIUS_COUNT = 2,
} UiRadiusToken;

typedef enum ui_space_token {
    UI_SPACE_1 = 8,
    UI_SPACE_2 = 16,
    UI_SPACE_3 = 24,
    UI_SPACE_4 = 32,
} UiSpaceToken;

typedef struct {
    int32_t radius;
} UiRadiusDef;

typedef struct {
    int32_t offset_x;
    int32_t offset_y;
    int32_t expand;
    int32_t radius_bias;
    uint32_t color;
} UiShadowLayer;

typedef struct {
    UiShadowLayer layers[2];
    uint32_t layer_count;
} UiShadowDef;

typedef struct {
    uint32_t colors[UI_COLOR_COUNT];
    UiRadiusDef radii[UI_RADIUS_COUNT];
    UiShadowDef shadows[4];
    int32_t spacing_unit;
} UiTokens;

typedef struct {
    UiColorRole fill_role;
    UiColorRole border_role;
    UiRadiusToken radius;
    UiShadow shadow;
    UiState state;
} UiPanelStyle;

typedef struct {
    UiRadiusToken radius;
    UiShadow shadow;
    UiState state;
} UiCardStyle;

typedef struct {
    UiColorRole color_role;
    UiState state;
} UiTextStyle;

typedef struct ui_backend {
    void (*fill_round_rect)(Rect r, int32_t radius, uint32_t color);
    void (*stroke_round_rect)(Rect r, int32_t radius, int32_t stroke_width, uint32_t color);
    void (*draw_text)(Vec2 pos, uint32_t color, const char *text);
} UiBackend;

typedef struct ui_context {
    UiBackend backend;
    const UiTokens *tokens;
} UiContext;

void ui_set_context(UiContext context);
void ui_reset_context(void);

const UiTokens *ui_default_tokens(void);
uint32_t ui_color(UiColorRole role);
int32_t ui_radius(UiRadiusToken token);
int32_t ui_space(uint32_t steps);

void ui_shadow(Rect r, UiShadow level);
void ui_panel(Rect r, UiPanelStyle style);
void ui_card(Rect r, UiState state);
void ui_text(Vec2 pos, UiTextStyle style, const char *text);

void ui_example_draw_card(Rect r, const char *title);

#endif
