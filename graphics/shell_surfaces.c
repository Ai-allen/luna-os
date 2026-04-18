#include "graphics_internal.h"

DesktopGrid desktop_grid_default(void) {
    DesktopGrid grid;
    grid.origin_x = ui_space(3u);
    grid.origin_y = (int32_t)cell_py(4u) + ui_space(1u);
    grid.columns = 2;
    grid.cell_width = 104;
    grid.cell_height = 64;
    grid.gap_x = ui_space(2u);
    grid.gap_y = ui_space(2u);
    return grid;
}

Rect desktop_grid_cell(const DesktopGrid *grid, int index) {
    Rect rect = {0, 0, 0, 0};
    int32_t column;
    int32_t row;

    if (grid == 0 || grid->columns <= 0 || index < 0) {
        return rect;
    }

    column = index % grid->columns;
    row = index / grid->columns;
    rect.x = grid->origin_x + column * (grid->cell_width + grid->gap_x);
    rect.y = grid->origin_y + row * (grid->cell_height + grid->gap_y);
    rect.width = grid->cell_width;
    rect.height = grid->cell_height;
    return rect;
}

void framebuffer_draw_desktop_icon(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    UiState state,
    const char *label
) {
    Rect card = { (int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height };
    UiState title_state = state == UI_STATE_DEFAULT ? UI_STATE_DEFAULT : UI_STATE_FOCUS;
    Rect icon_slot = {
        (int32_t)x + ui_space(1u),
        (int32_t)y + ui_space(1u),
        ui_space(3u),
        ui_space(3u),
    };
    Rect title_slot = {
        (int32_t)x + ui_space(1u),
        (int32_t)y + ui_space(4u),
        (int32_t)width - ui_space(2u),
        ui_space(2u),
    };

    ui_card(card, state);
    ui_panel(
        icon_slot,
        (UiPanelStyle){
            .fill_role = state == UI_STATE_ACTIVE ? UI_COLOR_ACCENT_SOFT : UI_COLOR_SURFACE,
            .border_role = state != UI_STATE_DEFAULT ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = state,
        }
    );
    ui_panel(
        title_slot,
        (UiPanelStyle){
            .fill_role = UI_COLOR_BG_CANVAS,
            .border_role = UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = title_state,
        }
    );
    ui_text(
        (Vec2){ icon_slot.x + ui_space(1u), icon_slot.y + ui_space(1u) },
        (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = title_state },
        "[]"
    );
    ui_text(
        (Vec2){ title_slot.x + 8, title_slot.y + 3 },
        (UiTextStyle){
            .color_role = state == UI_STATE_DEFAULT ? UI_COLOR_TEXT_SECONDARY : UI_COLOR_TEXT_PRIMARY,
            .state = title_state
        },
        label
    );
}

UiState ui_window_frame_state(uint32_t window_id) {
    return window_id == g_active_window ? UI_STATE_ACTIVE : UI_STATE_DEFAULT;
}

UiState ui_window_control_state(uint32_t window_id, uint32_t control_kind) {
    if (g_render_hover_window == window_id && g_render_hover_kind == control_kind) {
        return (g_shell_state.last_pointer_buttons & 0x01u) != 0u ? UI_STATE_ACTIVE : UI_STATE_HOVER;
    }
    return window_id == g_active_window ? UI_STATE_FOCUS : UI_STATE_DEFAULT;
}

void draw_window_control(Rect rect, UiState state, const char *glyph) {
    UiState visual_state = state == UI_STATE_FOCUS ? UI_STATE_DEFAULT : state;
    UiColorRole border_role = state == UI_STATE_FOCUS ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER;

    ui_shadow(rect, visual_state == UI_STATE_DEFAULT ? UI_SHADOW_NONE : UI_SHADOW_SM);
    ui_panel(
        rect,
        (UiPanelStyle){
            .fill_role = UI_COLOR_SURFACE,
            .border_role = visual_state == UI_STATE_ACTIVE ? UI_COLOR_ACCENT : border_role,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = visual_state,
        }
    );
    ui_text(
        (Vec2){ rect.x + ui_space(1u), rect.y + ui_space(1u) },
        (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = visual_state },
        glyph
    );
}

void draw_window_frame(uint32_t window_id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char *title, uint8_t attr) {
    if (width < 3u || height < 3u) {
        return;
    }

    if (g_framebuffer != 0) {
        uint8_t title_attr = (uint8_t)((attr & 0x0Fu) | (shell_chrome_attr() & 0xF0u));
        Rect frame = { (int32_t)cell_px(x), (int32_t)cell_py(y), (int32_t)cell_px(width), (int32_t)cell_py(height) };
        Rect title_bar = { frame.x + ui_space(1u), frame.y + ui_space(1u), frame.width - ui_space(2u), ui_space(4u) };
        Rect content_shell = {
            frame.x + ui_space(1u),
            title_bar.y + title_bar.height + ui_space(1u),
            frame.width - ui_space(2u),
            frame.height - title_bar.height - ui_space(3u)
        };
        Rect title_chip = { title_bar.x + ui_space(1u), title_bar.y + ui_space(1u), ui_space(3u), ui_space(2u) };
        Rect controls_band = { title_bar.x + title_bar.width - ui_space(8u), title_bar.y + ui_space(1u), ui_space(7u), ui_space(2u) };
        Rect min_rect = { controls_band.x, controls_band.y, 16, 16 };
        Rect max_rect = { controls_band.x + ui_space(3u), controls_band.y, 16, 16 };
        Rect close_rect = { controls_band.x + ui_space(6u), controls_band.y, 16, 16 };
        UiState frame_state = ui_window_frame_state(window_id);
        UiState bar_state = window_id == g_active_window ? UI_STATE_ACTIVE : UI_STATE_DEFAULT;

        ui_card(frame, frame_state);
        ui_panel(
            content_shell,
            (UiPanelStyle){
                .fill_role = UI_COLOR_BG_CANVAS,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = UI_STATE_DEFAULT,
            }
        );
        ui_panel(
            title_bar,
            (UiPanelStyle){
                .fill_role = UI_COLOR_PANEL,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_LG,
                .shadow = UI_SHADOW_NONE,
                .state = bar_state,
            }
        );
        ui_panel(
            title_chip,
            (UiPanelStyle){
                .fill_role = UI_COLOR_SURFACE,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = frame_state,
            }
        );
        ui_text(
            (Vec2){ title_chip.x + ui_space(1u), title_chip.y + ui_space(1u) },
            (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = frame_state },
            (title != 0 && title[0] != '\0') ? "[]" : "--"
        );
        ui_text(
            (Vec2){ title_bar.x + ui_space(5u), title_bar.y + ui_space(1u) },
            (UiTextStyle){
                .color_role = window_id == g_active_window ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_SECONDARY,
                .state = frame_state
            },
            title
        );
        draw_window_control(min_rect, ui_window_control_state(window_id, 7u), "-");
        draw_window_control(max_rect, ui_window_control_state(window_id, 8u), "[]");
        draw_window_control(close_rect, ui_window_control_state(window_id, 5u), "X");
        fill_cells(x, y, width, height, shell_focus_attr());
        fill_cells(x, y, width, 1u, title_attr);
        return;
    }

    for (uint32_t col = 0; col < width; ++col) {
        put_cell(x + col, y, (uint8_t)'-', attr);
        put_cell(x + col, y + height - 1u, (uint8_t)'-', attr);
    }
    for (uint32_t row = 0; row < height; ++row) {
        put_cell(x, y + row, (uint8_t)'|', attr);
        put_cell(x + width - 1u, y + row, (uint8_t)'|', attr);
    }
    put_cell(x, y, (uint8_t)'+', attr);
    put_cell(x + width - 1u, y, (uint8_t)'+', attr);
    put_cell(x, y + height - 1u, (uint8_t)'+', attr);
    put_cell(x + width - 1u, y + height - 1u, (uint8_t)'+', attr);
    if (title != 0) {
        draw_text(x + 2u, y, attr, title);
    }
}

void draw_window_content_chrome(const struct luna_window_record *window) {
    Rect body;
    Rect toolbar;
    Rect footer;
    UiState state;

    if (window == 0 || g_framebuffer == 0) {
        return;
    }

    body.x = (int32_t)cell_px(window->x + 1u);
    body.y = (int32_t)cell_py(window->y + 5u);
    body.width = (int32_t)cell_px(window->width - 2u);
    body.height = (int32_t)cell_py(window->height - 6u);
    toolbar.x = body.x + ui_space(1u);
    toolbar.y = body.y + ui_space(1u);
    toolbar.width = body.width - ui_space(2u);
    toolbar.height = ui_space(2u);
    footer.x = body.x + ui_space(1u);
    footer.y = body.y + body.height - ui_space(2u);
    footer.width = body.width - ui_space(2u);
    footer.height = ui_space(1u);
    state = ui_window_frame_state(window->window_id);

    ui_panel(
        toolbar,
        (UiPanelStyle){
            .fill_role = UI_COLOR_SURFACE,
            .border_role = UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = state
        }
    );
    ui_panel(
        footer,
        (UiPanelStyle){
            .fill_role = UI_COLOR_SURFACE,
            .border_role = UI_COLOR_PANEL_BORDER,
            .radius = UI_RADIUS_MD,
            .shadow = UI_SHADOW_NONE,
            .state = UI_STATE_DEFAULT
        }
    );
}

void draw_window_contents(const struct luna_window_record *window) {
    uint32_t body_x;
    uint32_t body_y;
    uint32_t body_width;
    uint32_t body_height;
    uint8_t attr;

    if (window == 0) {
        return;
    }

    body_x = window->x + 1u;
    body_y = window->y + 5u;
    body_width = window->width > 2u ? window->width - 2u : 0u;
    body_height = window->height > 6u ? window->height - 6u : 0u;
    attr = shell_focus_attr();

    if (body_width == 0u || body_height == 0u) {
        return;
    }

    fill_cells(body_x, body_y, body_width, body_height, attr);
    if (g_framebuffer != 0) {
        draw_window_content_chrome(window);
    }

    if (title_equals(window->title, "Console")) {
        draw_text(body_x + 2u, body_y + 2u, shell_panel_attr(), "session console");
        draw_text(body_x + 2u, body_y + 4u, shell_accent_attr(), "theme / control");
        return;
    }

    for (uint32_t row = 0u; row + 2u < body_height && row < 23u; ++row) {
        uint32_t offset = row * 78u;
        for (uint32_t col = 0u; col < body_width && col < 78u; ++col) {
            put_cell(body_x + col, body_y + row, window->cells[offset + col], window->attrs[offset + col]);
        }
    }
}

void draw_boot_desktop(uint32_t hover_kind, uint32_t hover_glyph) {
    DesktopGrid grid = desktop_grid_default();
    uint8_t desktop_attr = shell_desktop_attr();
    uint8_t chrome_attr = shell_chrome_attr();

    clear_screen(desktop_attr);
    if (g_framebuffer != 0) {
        Rect top_bar = { 0, 0, (int32_t)g_fb_width, (int32_t)cell_py(3u) };
        Rect title_chip = { ui_space(1u), ui_space(1u), ui_space(6u), ui_space(2u) };
        Rect status_chip = { (int32_t)g_fb_width - ui_space(14u), ui_space(1u), ui_space(13u), ui_space(2u) };

        ui_begin_frame();
        ui_panel(
            top_bar,
            (UiPanelStyle){
                .fill_role = UI_COLOR_PANEL,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = UI_STATE_DEFAULT
            }
        );
        ui_panel(
            title_chip,
            (UiPanelStyle){
                .fill_role = UI_COLOR_SURFACE,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = hover_kind == 15u ? UI_STATE_HOVER : UI_STATE_DEFAULT
            }
        );
        ui_text((Vec2){ title_chip.x + ui_space(1u), title_chip.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = UI_STATE_DEFAULT }, "LunaOS");
        ui_panel(
            status_chip,
            (UiPanelStyle){
                .fill_role = UI_COLOR_SURFACE,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = hover_kind == 11u ? UI_STATE_HOVER : UI_STATE_DEFAULT
            }
        );
        ui_text((Vec2){ status_chip.x + ui_space(1u), status_chip.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_SECONDARY, .state = UI_STATE_DEFAULT }, "system");
    } else {
        fill_cells(0u, 0u, 80u, 1u, chrome_attr);
        draw_text(2u, 0u, chrome_attr, "LunaOS");
    }

    for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
        const struct luna_desktop_entry *entry = shell_entry_at(i);
        if (entry == 0) {
            continue;
        }
        if (g_framebuffer != 0) {
            Rect cell = desktop_grid_cell(&grid, (int)i);
            UiState state = hover_kind == 9u && hover_glyph == i ? UI_STATE_HOVER : UI_STATE_DEFAULT;
            framebuffer_draw_desktop_icon((uint32_t)cell.x, (uint32_t)cell.y, (uint32_t)cell.width, (uint32_t)cell.height, state, entry->label);
        } else {
            draw_text(entry->icon_x, entry->icon_y, desktop_attr, entry->label);
        }
    }
}

static Rect task_strip_rect(void) {
    Rect rect;
    rect.x = ui_space(1u);
    rect.y = (int32_t)g_fb_height - ui_space(4u);
    rect.width = (int32_t)g_fb_width - ui_space(2u);
    rect.height = ui_space(3u);
    return rect;
}

static Rect task_start_rect(Rect strip) {
    Rect rect = { strip.x + ui_space(1u), strip.y + ui_space(1u), ui_space(5u), ui_space(2u) };
    return rect;
}

static Rect task_slot_rect(Rect strip, uint32_t index) {
    Rect rect = { strip.x + ui_space(7u) + (int32_t)index * ui_space(6u), strip.y + ui_space(1u), ui_space(5u), ui_space(2u) };
    return rect;
}

void draw_task_strip(uint32_t hover_kind, uint32_t hover_glyph) {
    uint8_t chrome_attr = shell_chrome_attr();

    if (g_framebuffer == 0) {
        fill_cells(0u, 22u, 80u, 3u, chrome_attr);
        draw_text(2u, 23u, chrome_attr, "[menu]");
        return;
    }

    {
        Rect strip = task_strip_rect();
        Rect start = task_start_rect(strip);
        Rect status = { strip.x + strip.width - ui_space(10u), strip.y + ui_space(1u), ui_space(9u), ui_space(2u) };
        UiState start_state = hover_kind == 3u ? ((g_shell_state.last_pointer_buttons & 0x01u) != 0u ? UI_STATE_ACTIVE : UI_STATE_HOVER) : UI_STATE_DEFAULT;

        ui_panel(
            strip,
            (UiPanelStyle){
                .fill_role = UI_COLOR_PANEL,
                .border_role = UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_LG,
                .shadow = UI_SHADOW_SM,
                .state = UI_STATE_DEFAULT
            }
        );
        ui_panel(
            start,
            (UiPanelStyle){
                .fill_role = UI_COLOR_SURFACE,
                .border_role = start_state == UI_STATE_ACTIVE ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = start_state
            }
        );
        ui_text((Vec2){ start.x + ui_space(1u), start.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = start_state }, "Start");

        for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
            const struct luna_desktop_entry *entry = shell_entry_at(i);
            struct luna_window_record *window = visible_window_for_entry(entry);
            Rect slot = task_slot_rect(strip, i);
            UiState slot_state = hover_kind == 4u && hover_glyph == i ? UI_STATE_HOVER : UI_STATE_DEFAULT;
            if (window != 0 && window->window_id == g_active_window) {
                slot_state = UI_STATE_ACTIVE;
            }
            ui_panel(
                slot,
                (UiPanelStyle){
                    .fill_role = window != 0 ? UI_COLOR_SURFACE : UI_COLOR_BG_CANVAS,
                    .border_role = slot_state == UI_STATE_ACTIVE ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER,
                    .radius = UI_RADIUS_MD,
                    .shadow = UI_SHADOW_NONE,
                    .state = slot_state
                }
            );
            if (entry != 0) {
                char short_label[6];
                copy_short_label(short_label, entry->label);
                ui_text((Vec2){ slot.x + ui_space(1u), slot.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = slot_state }, short_label);
            }
        }

        ui_panel(
            status,
            (UiPanelStyle){
                .fill_role = UI_COLOR_SURFACE,
                .border_role = hover_kind == 11u ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = hover_kind == 11u ? UI_STATE_HOVER : UI_STATE_DEFAULT
            }
        );
        ui_text((Vec2){ status.x + ui_space(1u), status.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_SECONDARY, .state = UI_STATE_DEFAULT }, "control");
    }
}

void draw_launcher(uint32_t hover_kind, uint32_t hover_glyph) {
    if (g_launcher_open == 0u) {
        return;
    }

    if (g_framebuffer == 0) {
        fill_cells(1u, 8u, 26u, 12u, shell_panel_attr());
        for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
            const struct luna_desktop_entry *entry = shell_entry_at(i);
            if (entry != 0) {
                draw_text(3u, 10u + i, shell_panel_attr(), entry->label);
            }
        }
        return;
    }

    {
        Rect panel = { ui_space(2u), (int32_t)g_fb_height - ui_space(18u), ui_space(22u), ui_space(14u) };
        Rect header = { panel.x + ui_space(1u), panel.y + ui_space(1u), panel.width - ui_space(2u), ui_space(2u) };
        ui_card(panel, UI_STATE_DEFAULT);
        ui_panel(header, (UiPanelStyle){ .fill_role = UI_COLOR_SURFACE, .border_role = UI_COLOR_PANEL_BORDER, .radius = UI_RADIUS_MD, .shadow = UI_SHADOW_NONE, .state = UI_STATE_DEFAULT });
        ui_text((Vec2){ header.x + ui_space(1u), header.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = UI_STATE_DEFAULT }, "Launcher");

        for (uint32_t i = 0u; i < shell_entry_count(); ++i) {
            const struct luna_desktop_entry *entry = shell_entry_at(i);
            Rect row = { panel.x + ui_space(1u), panel.y + ui_space(4u) + (int32_t)i * ui_space(2u), panel.width - ui_space(2u), ui_space(2u) };
            UiState state = hover_kind == 2u && hover_glyph == i ? UI_STATE_HOVER : (g_launcher_index == i ? UI_STATE_FOCUS : UI_STATE_DEFAULT);
            ui_panel(row, (UiPanelStyle){ .fill_role = UI_COLOR_BG_CANVAS, .border_role = state == UI_STATE_HOVER ? UI_COLOR_ACCENT : UI_COLOR_PANEL_BORDER, .radius = UI_RADIUS_MD, .shadow = UI_SHADOW_NONE, .state = state });
            if (entry != 0) {
                ui_text((Vec2){ row.x + ui_space(1u), row.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = state }, entry->label);
            }
        }
    }
}

void draw_control_center(void) {
    if (g_control_open == 0u) {
        return;
    }

    if (g_framebuffer == 0) {
        fill_cells(56u, 4u, 22u, 16u, shell_panel_attr());
        draw_text(58u, 6u, shell_panel_attr(), "Control");
        return;
    }

    {
        Rect panel = { (int32_t)g_fb_width - ui_space(18u), ui_space(4u), ui_space(16u), ui_space(14u) };
        ui_card(panel, UI_STATE_DEFAULT);
        ui_text((Vec2){ panel.x + ui_space(1u), panel.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = UI_STATE_DEFAULT }, "Control");

        {
            const char *labels[6] = { "theme", "control", "console", "files", "notes", "accent" };
            for (uint32_t i = 0u; i < 6u; ++i) {
                Rect row = { panel.x + ui_space(1u), panel.y + ui_space(3u) + (int32_t)i * ui_space(2u), panel.width - ui_space(2u), ui_space(2u) };
                ui_panel(row, (UiPanelStyle){ .fill_role = UI_COLOR_BG_CANVAS, .border_role = UI_COLOR_PANEL_BORDER, .radius = UI_RADIUS_MD, .shadow = UI_SHADOW_NONE, .state = UI_STATE_DEFAULT });
                ui_text((Vec2){ row.x + ui_space(1u), row.y + ui_space(1u) }, (UiTextStyle){ .color_role = UI_COLOR_TEXT_PRIMARY, .state = UI_STATE_DEFAULT }, labels[i]);
            }
        }
    }
}

void draw_cursor(void) {
    if (g_framebuffer != 0) {
        Rect cursor = { (int32_t)cell_px(g_cursor_x), (int32_t)cell_py(g_cursor_y), (int32_t)g_cell_width, (int32_t)g_cell_height };
        ui_panel(
            cursor,
            (UiPanelStyle){
                .fill_role = UI_COLOR_ACCENT,
                .border_role = UI_COLOR_TEXT_PRIMARY,
                .radius = UI_RADIUS_MD,
                .shadow = UI_SHADOW_NONE,
                .state = UI_STATE_ACTIVE
            }
        );
        return;
    }

    put_cell(g_cursor_x, g_cursor_y, (uint8_t)'X', shell_accent_attr());
}
