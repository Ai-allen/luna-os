#ifndef GRAPHICS_INTERNAL_H
#define GRAPHICS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"
#include "../include/luna_ui.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);

struct luna_window_record {
    uint32_t live;
    uint32_t minimized;
    uint32_t maximized;
    uint32_t window_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t restore_x;
    uint32_t restore_y;
    uint32_t restore_width;
    uint32_t restore_height;
    char title[16];
    uint8_t cells[78u * 23u];
    uint8_t attrs[78u * 23u];
};

typedef struct ui_hit_context {
    uint32_t hover_kind;
    uint32_t hover_glyph;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t pointer_buttons;
    uint32_t focus_index;
    uint32_t logical_focus_id;
} UiHitContext;

typedef struct ui_hit_target {
    uint32_t hover_kind;
    uint32_t hover_glyph;
    uint32_t focus_index;
    Rect rect;
    uint32_t use_rect_hit;
} UiHitTarget;

typedef struct desktop_grid {
    int32_t origin_x;
    int32_t origin_y;
    int32_t columns;
    int32_t cell_width;
    int32_t cell_height;
    int32_t gap_x;
    int32_t gap_y;
} DesktopGrid;

extern struct luna_cid g_device_write_cid;
extern struct luna_cid g_device_read_cid;
extern volatile struct luna_manifest *g_manifest;
extern uint16_t g_text_cells[80u * 25u];
extern uint32_t g_active_window;
extern uint32_t *g_framebuffer;
extern uint32_t g_fb_width;
extern uint32_t g_fb_height;
extern uint32_t g_fb_stride;
extern uint32_t g_fb_format;
extern uint64_t g_fb_buffer_bytes;
extern uint32_t g_cell_width;
extern uint32_t g_cell_height;
extern uint32_t g_launcher_open;
extern uint32_t g_launcher_index;
extern uint32_t g_cursor_x;
extern uint32_t g_cursor_y;
extern uint32_t g_control_open;
extern uint32_t g_render_hover_kind;
extern uint32_t g_render_hover_window;
extern struct luna_desktop_shell_state g_shell_state;
extern struct luna_window_record g_windows[8];

void zero_bytes(void *ptr, size_t len);

uint32_t request_capability(uint64_t domain_key, struct luna_cid *out);
uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint64_t caller_space, uint32_t target_gate);
void device_write(const char *text);
int device_read_display_info(struct luna_display_info *info);
void init_framebuffer_from_info(const struct luna_display_info *info);
void init_framebuffer(const struct luna_bootview *bootview);
void device_present_framebuffer(void);

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t palette_rgb(uint8_t index);
void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
uint32_t cell_px(uint32_t x);
uint32_t cell_py(uint32_t y);
void framebuffer_stroke_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_fill_round_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color);
void render_glyph(uint32_t px, uint32_t py, char ch, uint32_t fg, uint32_t bg);
void legacy_device_put(uint32_t x, uint32_t y, uint8_t glyph, uint8_t attr);
void put_cell(uint32_t x, uint32_t y, uint8_t glyph, uint8_t attr);
void draw_text(uint32_t x, uint32_t y, uint8_t attr, const char *text);
void fill_cells(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t attr);
void clear_screen(uint8_t attr);
void framebuffer_fill_cell_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_fill_cell_round_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, uint32_t color);
void framebuffer_stroke_cell_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_draw_progress_strip(uint32_t x, uint32_t y, uint32_t width, uint32_t filled, uint32_t rail, uint32_t glow);
void framebuffer_draw_surface(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t body, uint32_t border, uint32_t radius);
void framebuffer_draw_surface_row(uint32_t x, uint32_t y, uint32_t width, uint32_t body, uint32_t border, uint32_t accent, uint32_t selected);

uint32_t ui_rgb_color(uint32_t rgb_color);
uint32_t ui_token_rgb(UiColorRole role);
uint32_t ui_radius_px(UiRadiusToken token);
void ui_backend_fill_round_rect(Rect r, int32_t radius, uint32_t color);
void ui_backend_stroke_round_rect(Rect r, int32_t radius, int32_t stroke_width, uint32_t color);
void ui_backend_draw_text(Vec2 pos, uint32_t color, const char *text);
void ui_begin_frame(void);
int ui_rect_contains_cell(Rect r, uint32_t cell_x, uint32_t cell_y);
UiState ui_state_from_hit(const UiHitContext *context, UiHitTarget target);

DesktopGrid desktop_grid_default(void);
Rect desktop_grid_cell(const DesktopGrid *grid, int index);
void copy_title(char out[16], const char *in);
int title_equals(const char title[16], const char *text);
int title_is_native_surface(const char title[16]);
uint32_t shell_entry_count(void);
const struct luna_desktop_entry *shell_entry_at(uint32_t index);
uint8_t shell_desktop_attr(void);
uint8_t shell_chrome_attr(void);
uint8_t shell_panel_attr(void);
uint8_t shell_accent_attr(void);
uint8_t shell_muted_attr(void);
uint8_t shell_focus_attr(void);
void clear_window_cells(struct luna_window_record *window);
struct luna_window_record *active_window_record(void);
struct luna_window_record *resolve_window(uint32_t window_id);
void write_window_title(struct luna_graphics_gate *gate, const struct luna_window_record *window);
void copy_short_label(char out[6], const char *label);
uint32_t live_window_count(void);
struct luna_window_record *find_window(uint32_t window_id);
struct luna_window_record *cycle_window(int reverse);
struct luna_window_record *alloc_window(void);
int text_equal(const char *left, const char *right);
struct luna_window_record *window_for_entry(const struct luna_desktop_entry *entry);
struct luna_window_record *visible_window_for_entry(const struct luna_desktop_entry *entry);

void framebuffer_draw_desktop_icon(uint32_t x, uint32_t y, uint32_t width, uint32_t height, UiState state, const char *label);
UiState ui_window_frame_state(uint32_t window_id);
UiState ui_window_control_state(uint32_t window_id, uint32_t control_kind);
void draw_window_control(Rect rect, UiState state, const char *glyph);
void draw_window_frame(uint32_t window_id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char *title, uint8_t attr);
void draw_window_content_chrome(const struct luna_window_record *window);
void draw_window_contents(const struct luna_window_record *window);
void draw_boot_desktop(uint32_t hover_kind, uint32_t hover_glyph);
void draw_task_strip(uint32_t hover_kind, uint32_t hover_glyph);
void draw_launcher(uint32_t hover_kind, uint32_t hover_glyph);
void draw_control_center(void);
void draw_cursor(void);

void desktop_hit_test(struct luna_graphics_gate *gate);
void render_scene(void);

#endif
