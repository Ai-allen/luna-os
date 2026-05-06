#include "graphics_internal.h"

static uint32_t next_window_id(void) {
    uint32_t highest = 0u;
    for (size_t i = 0u; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        if (g_windows[i].live && g_windows[i].window_id > highest) {
            highest = g_windows[i].window_id;
        }
    }
    return highest + 1u;
}

static void import_shell_state(const struct luna_graphics_gate *gate) {
    const uint8_t *src;
    uint8_t *dst;

    if (gate == 0 || gate->buffer_addr == 0u || gate->buffer_size < sizeof(struct luna_desktop_shell_state)) {
        return;
    }
    src = (const uint8_t *)(uintptr_t)gate->buffer_addr;
    dst = (uint8_t *)&g_shell_state;
    for (size_t i = 0u; i < sizeof(struct luna_desktop_shell_state); ++i) {
        dst[i] = src[i];
    }
}

static void clamp_window(struct luna_window_record *window) {
    if (window == 0) {
        return;
    }
    if (window->width < 6u) {
        window->width = 6u;
    }
    if (window->height < 6u) {
        window->height = 6u;
    }
    if (window->width > 80u) {
        window->width = 80u;
    }
    if (window->height > 23u) {
        window->height = 23u;
    }
    if (window->x + window->width > 80u) {
        window->x = 80u - window->width;
    }
    if (window->y + window->height > 23u) {
        window->y = 23u - window->height;
    }
}

void SYSV_ABI graphics_entry_gate(struct luna_graphics_gate *gate) {
    struct luna_window_record *window;
    uint64_t caller_space;

    if (gate == 0) {
        return;
    }

    gate->status = LUNA_GRAPHICS_OK;
    gate->result_count = 0u;
    caller_space = gate->caller_space != 0u ? gate->caller_space : LUNA_SPACE_GRAPHICS;

    if (validate_capability(LUNA_CAP_GRAPHICS_DRAW, gate->cid_low, gate->cid_high, caller_space, gate->opcode) != LUNA_GATE_OK) {
        gate->status = LUNA_GRAPHICS_ERR_INVALID_CAP;
        return;
    }

    switch (gate->opcode) {
        case LUNA_GRAPHICS_DRAW_CHAR:
            window = resolve_window(gate->window_id);
            if (window == 0 || gate->x >= 78u || gate->y >= 23u) {
                gate->status = LUNA_GRAPHICS_ERR_RANGE;
                return;
            }
            window->cells[gate->y * 78u + gate->x] = (uint8_t)gate->glyph;
            window->attrs[gate->y * 78u + gate->x] = (uint8_t)gate->attr;
            render_scene();
            return;

        case LUNA_GRAPHICS_CREATE_WINDOW:
            window = alloc_window();
            if (window == 0) {
                gate->status = LUNA_GRAPHICS_ERR_NO_ROOM;
                return;
            }
            zero_bytes(window, sizeof(*window));
            window->live = 1u;
            window->window_id = next_window_id();
            window->x = gate->x;
            window->y = gate->y;
            window->width = gate->width;
            window->height = gate->height;
            window->restore_x = window->x;
            window->restore_y = window->y;
            window->restore_width = window->width;
            window->restore_height = window->height;
            clamp_window(window);
            clear_window_cells(window);
            copy_title(window->title, gate->buffer_addr != 0u ? (const char *)(uintptr_t)gate->buffer_addr : "");
            g_active_window = window->window_id;
            gate->window_id = window->window_id;
            render_scene();
            return;

        case LUNA_GRAPHICS_SET_ACTIVE_WINDOW:
            if (gate->window_id == 0u) {
                window = active_window_record();
                if (window == 0) {
                    window = cycle_window(0);
                }
                if (window == 0) {
                    gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                    return;
                }
                g_active_window = window->window_id;
            } else {
                window = find_window(gate->window_id);
                if (window == 0) {
                    gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                    return;
                }
                window->minimized = 0u;
                g_active_window = window->window_id;
            }
            render_scene();
            return;

        case LUNA_GRAPHICS_CLOSE_WINDOW:
            window = resolve_window(gate->window_id);
            if (window == 0) {
                gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                return;
            }
            zero_bytes(window, sizeof(*window));
            if (g_active_window == gate->window_id || (gate->window_id == 0u && g_active_window != 0u)) {
                struct luna_window_record *next = cycle_window(0);
                g_active_window = next != 0 ? next->window_id : 0u;
            }
            render_scene();
            return;

        case LUNA_GRAPHICS_MOVE_WINDOW:
            window = resolve_window(gate->window_id);
            if (window == 0) {
                gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                return;
            }
            window->x = (uint32_t)((int32_t)window->x + (int32_t)gate->x);
            window->y = (uint32_t)((int32_t)window->y + (int32_t)gate->y);
            clamp_window(window);
            if (window->maximized == 0u) {
                window->restore_x = window->x;
                window->restore_y = window->y;
            }
            render_scene();
            return;

        case LUNA_GRAPHICS_RENDER_DESKTOP:
            import_shell_state(gate);
            g_launcher_index = gate->glyph;
            g_cursor_x = gate->x;
            g_cursor_y = gate->y;

            if (gate->attr == 0x11u) {
                g_launcher_open = 1u;
            } else if (gate->attr == 0x12u) {
                g_launcher_open = 0u;
            } else if (gate->attr == 0x40u) {
                window = resolve_window(gate->window_id);
                if (window == 0) {
                    gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                    return;
                }
                window->minimized = 1u;
                if (window->window_id == g_active_window) {
                    struct luna_window_record *next = cycle_window(0);
                    g_active_window = next != 0 ? next->window_id : 0u;
                }
            } else if (gate->attr == 0x80u) {
                window = resolve_window(gate->window_id);
                if (window == 0) {
                    gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                    return;
                }
                if (window->maximized == 0u) {
                    window->restore_x = window->x;
                    window->restore_y = window->y;
                    window->restore_width = window->width;
                    window->restore_height = window->height;
                    window->x = 0u;
                    window->y = 0u;
                    window->width = 80u;
                    window->height = 23u;
                    window->maximized = 1u;
                } else {
                    window->x = window->restore_x;
                    window->y = window->restore_y;
                    window->width = window->restore_width;
                    window->height = window->restore_height;
                    window->maximized = 0u;
                }
            } else if (gate->attr == 0x100u) {
                window = resolve_window(gate->window_id);
                if (window == 0) {
                    gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                    return;
                }
                window->width = (uint32_t)((int32_t)window->width + (int32_t)gate->x);
                window->height = (uint32_t)((int32_t)window->height + (int32_t)gate->y);
                clamp_window(window);
                if (window->maximized == 0u) {
                    window->restore_width = window->width;
                    window->restore_height = window->height;
                }
            } else if (gate->attr == 0x200u) {
                g_control_open = 1u;
            } else if (gate->attr == 0x400u) {
                g_control_open = 0u;
            }

            if (gate->attr == 0x30u) {
                desktop_hit_test(gate);
            }
            render_scene();
            return;

        case LUNA_GRAPHICS_QUERY_WINDOW:
            window = resolve_window(gate->window_id);
            if (window == 0) {
                gate->status = LUNA_GRAPHICS_ERR_NOT_FOUND;
                return;
            }
            gate->window_id = window->window_id;
            gate->x = window->x;
            gate->y = window->y;
            gate->width = window->width;
            gate->height = window->height;
            gate->glyph = window->minimized;
            gate->attr = window->maximized;
            write_window_title(gate, window);
            return;

        default:
            gate->status = LUNA_GRAPHICS_ERR_RANGE;
            return;
    }
}
