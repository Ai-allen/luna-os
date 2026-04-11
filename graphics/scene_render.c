#include "graphics_internal.h"

void render_scene(void) {
    struct luna_graphics_gate hover_gate;

    zero_bytes(&hover_gate, sizeof(hover_gate));
    hover_gate.x = g_cursor_x;
    hover_gate.y = g_cursor_y;
    desktop_hit_test(&hover_gate);
    g_render_hover_kind = hover_gate.result_count;
    g_render_hover_window = hover_gate.window_id;

    draw_boot_desktop(g_render_hover_kind, hover_gate.glyph);

    for (size_t i = 0u; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        struct luna_window_record *window = &g_windows[i];
        if (!window->live || window->minimized != 0u || window->window_id == g_active_window) {
            continue;
        }
        draw_window_frame(window->window_id, window->x, window->y, window->width, window->height, window->title, shell_chrome_attr());
        draw_window_contents(window);
    }

    {
        struct luna_window_record *window = active_window_record();
        if (window != 0 && window->minimized == 0u) {
            draw_window_frame(window->window_id, window->x, window->y, window->width, window->height, window->title, shell_chrome_attr());
            draw_window_contents(window);
        }
    }

    draw_task_strip(g_render_hover_kind, hover_gate.glyph);
    draw_launcher(g_render_hover_kind, hover_gate.glyph);
    draw_control_center();
    draw_cursor();
    device_present_framebuffer();
}
