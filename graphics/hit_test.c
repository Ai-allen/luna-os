#include "graphics_internal.h"

static int point_in_window(const struct luna_window_record *window, uint32_t cell_x, uint32_t cell_y) {
    return window != 0 &&
        cell_x >= window->x &&
        cell_x < window->x + window->width &&
        cell_y >= window->y &&
        cell_y < window->y + window->height;
}

static int point_in_rect_cells(Rect rect, uint32_t cell_x, uint32_t cell_y) {
    return ui_rect_contains_cell(rect, cell_x, cell_y);
}

void desktop_hit_test(struct luna_graphics_gate *gate) {
    uint32_t cell_x;
    uint32_t cell_y;
    DesktopGrid grid;

    if (gate == 0) {
        return;
    }

    gate->result_count = 0u;
    gate->window_id = 0u;

    cell_x = gate->x;
    cell_y = gate->y;
    grid = desktop_grid_default();

    if (g_framebuffer != 0) {
        Rect strip = { ui_space(1u), (int32_t)g_fb_height - ui_space(4u), (int32_t)g_fb_width - ui_space(2u), ui_space(3u) };
        Rect start = { strip.x + ui_space(1u), strip.y + ui_space(1u), ui_space(5u), ui_space(2u) };
        Rect control = { strip.x + strip.width - ui_space(10u), strip.y + ui_space(1u), ui_space(9u), ui_space(2u) };
        Rect top_theme = { ui_space(1u), ui_space(1u), ui_space(6u), ui_space(2u) };

        if (point_in_rect_cells(top_theme, cell_x, cell_y)) {
            gate->result_count = 15u;
            return;
        }
        if (point_in_rect_cells(control, cell_x, cell_y)) {
            gate->result_count = 11u;
            return;
        }
        if (point_in_rect_cells(start, cell_x, cell_y)) {
            gate->result_count = 3u;
            return;
        }
        for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
            Rect slot = { strip.x + ui_space(7u) + (int32_t)i * ui_space(6u), strip.y + ui_space(1u), ui_space(5u), ui_space(2u) };
            if (point_in_rect_cells(slot, cell_x, cell_y)) {
                gate->result_count = 4u;
                gate->glyph = i;
                if (shell_entry_at(i) != 0) {
                    struct luna_window_record *window = visible_window_for_entry(shell_entry_at(i));
                    if (window != 0) {
                        gate->window_id = window->window_id;
                    }
                }
                return;
            }
        }

        if (g_launcher_open != 0u) {
            Rect panel = { ui_space(2u), (int32_t)g_fb_height - ui_space(18u), ui_space(22u), ui_space(14u) };
            if (!point_in_rect_cells(panel, cell_x, cell_y)) {
                gate->result_count = 16u;
                return;
            }
            for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
                Rect row = { panel.x + ui_space(1u), panel.y + ui_space(4u) + (int32_t)i * ui_space(2u), panel.width - ui_space(2u), ui_space(2u) };
                if (point_in_rect_cells(row, cell_x, cell_y)) {
                    gate->result_count = 2u;
                    gate->glyph = i;
                    return;
                }
            }
        }

        if (g_control_open != 0u) {
            Rect panel = { (int32_t)g_fb_width - ui_space(18u), ui_space(4u), ui_space(16u), ui_space(14u) };
            if (point_in_rect_cells(panel, cell_x, cell_y)) {
                uint32_t row = 0u;
                if (cell_y > (uint32_t)(panel.y / (int32_t)g_cell_height + 3u)) {
                    row = (cell_y - (uint32_t)(panel.y / (int32_t)g_cell_height + 3u)) / 2u;
                }
                switch (row) {
                    case 0u: gate->result_count = 17u; return;
                    case 1u: gate->result_count = 18u; return;
                    case 2u: gate->result_count = 19u; return;
                    case 3u: gate->result_count = 23u; return;
                    case 4u: gate->result_count = 24u; return;
                    default: gate->result_count = 22u; return;
                }
            }
        }

        for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
            Rect icon = desktop_grid_cell(&grid, (int)i);
            if (point_in_rect_cells(icon, cell_x, cell_y)) {
                gate->result_count = 9u;
                gate->glyph = i;
                return;
            }
        }
    }

    for (int index = (int)(sizeof(g_windows) / sizeof(g_windows[0])) - 1; index >= 0; --index) {
        struct luna_window_record *window = &g_windows[index];
        if (!window->live || window->minimized != 0u || !point_in_window(window, cell_x, cell_y)) {
            continue;
        }
        gate->window_id = window->window_id;
        if (cell_y == window->y + window->height - 1u && cell_x >= window->x + window->width - 2u) {
            gate->result_count = 10u;
            return;
        }
        if (cell_y == window->y + 1u) {
            if (cell_x >= window->x + window->width - 4u && cell_x < window->x + window->width - 3u) {
                gate->result_count = 7u;
                return;
            }
            if (cell_x >= window->x + window->width - 3u && cell_x < window->x + window->width - 2u) {
                gate->result_count = 8u;
                return;
            }
            if (cell_x >= window->x + window->width - 2u) {
                gate->result_count = 5u;
                return;
            }
            gate->result_count = 6u;
            return;
        }
        if (title_equals(window->title, "Files") && cell_y >= window->y + 7u && cell_y < window->y + 15u) {
            gate->result_count = 12u;
            gate->glyph = (cell_y - (window->y + 7u)) / 2u;
            return;
        }
        if (title_equals(window->title, "Notes") && cell_y == window->y + 9u) {
            gate->result_count = 20u;
            return;
        }
        if (title_equals(window->title, "Console")) {
            if (cell_y == window->y + 9u) {
                gate->result_count = 21u;
                return;
            }
            if (cell_y == window->y + 11u) {
                gate->result_count = 22u;
                return;
            }
        }
        gate->result_count = 1u;
        return;
    }
}
