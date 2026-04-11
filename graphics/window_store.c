#include "graphics_internal.h"

void copy_title(char out[16], const char *in) {
    for (size_t i = 0; i < 16; ++i) {
        out[i] = 0;
        if (in == 0 || in[i] == '\0') {
            break;
        }
        out[i] = in[i];
    }
}

int title_equals(const char title[16], const char *text) {
    size_t i = 0u;
    while (i < 16u && title[i] != '\0' && text[i] != '\0') {
        if (title[i] != text[i]) {
            return 0;
        }
        i += 1u;
    }
    return title[i] == '\0' && text[i] == '\0';
}

int title_is_native_surface(const char title[16]) {
    return title_equals(title, "Files") ||
        title_equals(title, "Notes") ||
        title_equals(title, "Guard") ||
        title_equals(title, "Console") ||
        title_equals(title, "Hello");
}

uint32_t shell_entry_count(void) {
    if (g_shell_state.entry_count > LUNA_DESKTOP_ENTRY_CAPACITY) {
        return LUNA_DESKTOP_ENTRY_CAPACITY;
    }
    return g_shell_state.entry_count;
}

const struct luna_desktop_entry *shell_entry_at(uint32_t index) {
    if (index >= shell_entry_count()) {
        return 0;
    }
    return &g_shell_state.entries[index];
}

uint8_t shell_desktop_attr(void) {
    return (uint8_t)(g_shell_state.desktop_attr != 0u ? g_shell_state.desktop_attr : 0x17u);
}

uint8_t shell_chrome_attr(void) {
    return (uint8_t)(g_shell_state.chrome_attr != 0u ? g_shell_state.chrome_attr : 0x71u);
}

uint8_t shell_panel_attr(void) {
    return (uint8_t)(g_shell_state.panel_attr != 0u ? g_shell_state.panel_attr : 0x1Bu);
}

uint8_t shell_accent_attr(void) {
    return (uint8_t)(g_shell_state.accent_attr != 0u ? g_shell_state.accent_attr : 0x1Fu);
}

uint8_t shell_muted_attr(void) {
    uint8_t panel = shell_panel_attr();
    uint8_t fg = (uint8_t)(panel & 0x0Fu);
    uint8_t bg = (uint8_t)((shell_desktop_attr() >> 4) & 0x0Fu);
    return (uint8_t)((bg << 4) | fg);
}

uint8_t shell_focus_attr(void) {
    uint8_t accent = shell_accent_attr();
    uint8_t fg = (uint8_t)(accent & 0x0Fu);
    uint8_t bg = (uint8_t)((shell_chrome_attr() >> 4) & 0x0Fu);
    return (uint8_t)((bg << 4) | fg);
}

void clear_window_cells(struct luna_window_record *window) {
    if (window == 0) {
        return;
    }
    for (size_t i = 0; i < sizeof(window->cells); ++i) {
        window->cells[i] = (uint8_t)' ';
        window->attrs[i] = shell_focus_attr();
    }
}

struct luna_window_record *find_window(uint32_t window_id) {
    for (size_t i = 0; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        if (g_windows[i].live && g_windows[i].window_id == window_id) {
            return &g_windows[i];
        }
    }
    return 0;
}

struct luna_window_record *active_window_record(void) {
    if (g_active_window == 0u) {
        return 0;
    }
    return find_window(g_active_window);
}

struct luna_window_record *resolve_window(uint32_t window_id) {
    if (window_id == 0u) {
        return active_window_record();
    }
    return find_window(window_id);
}

void write_window_title(struct luna_graphics_gate *gate, const struct luna_window_record *window) {
    char *out;

    if (gate == 0 || window == 0 || gate->buffer_addr == 0u || gate->buffer_size == 0u) {
        return;
    }
    out = (char *)(uintptr_t)gate->buffer_addr;
    for (uint64_t i = 0u; i < gate->buffer_size; ++i) {
        out[i] = '\0';
        if (i >= 15u || window->title[i] == '\0') {
            break;
        }
        out[i] = window->title[i];
    }
}

void copy_short_label(char out[6], const char *label) {
    size_t i = 0u;
    while (label != 0 && label[i] != '\0' && i < 5u) {
        out[i] = label[i];
        i += 1u;
    }
    out[i] = '\0';
}

uint32_t live_window_count(void) {
    uint32_t count = 0u;
    for (size_t i = 0; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        if (g_windows[i].live) {
            count += 1u;
        }
    }
    return count;
}

struct luna_window_record *cycle_window(int reverse) {
    size_t count = sizeof(g_windows) / sizeof(g_windows[0]);
    size_t start = 0u;

    if (g_active_window != 0u) {
        for (size_t i = 0; i < count; ++i) {
            if (g_windows[i].live && g_windows[i].window_id == g_active_window) {
                start = i;
                break;
            }
        }
    }

    for (size_t step = 1u; step <= count; ++step) {
        size_t index = reverse ? (start + count - step) % count : (start + step) % count;
        if (g_windows[index].live) {
            return &g_windows[index];
        }
    }

    for (size_t i = 0; i < count; ++i) {
        if (g_windows[i].live) {
            return &g_windows[i];
        }
    }
    return 0;
}

struct luna_window_record *alloc_window(void) {
    for (size_t i = 0; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        if (!g_windows[i].live) {
            return &g_windows[i];
        }
    }
    return 0;
}

int text_equal(const char *left, const char *right) {
    size_t i = 0u;
    if (left == 0 || right == 0) {
        return 0;
    }
    while (left[i] != '\0' || right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

struct luna_window_record *window_for_entry(const struct luna_desktop_entry *entry) {
    if (entry == 0) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(g_windows) / sizeof(g_windows[0]); ++i) {
        if (g_windows[i].live && text_equal(g_windows[i].title, entry->label)) {
            return &g_windows[i];
        }
    }
    return 0;
}

struct luna_window_record *visible_window_for_entry(const struct luna_desktop_entry *entry) {
    struct luna_window_record *window = window_for_entry(entry);
    if (window == 0 || window->minimized != 0u) {
        return 0;
    }
    return window;
}
