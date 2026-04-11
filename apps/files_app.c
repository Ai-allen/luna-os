#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *app_write_fn_t)(const struct luna_bootview *bootview, const char *text);
typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);

static void app_write(const struct luna_bootview *bootview, const char *text) {
    uint64_t entry = 0u;
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;

    if (text == 0) {
        return;
    }
    if (bootview != 0) {
        entry = bootview->app_write_entry;
    }
    if (entry == 0u || entry == ~0ull) {
        entry = manifest->program_app_write_entry;
    }
    if (entry == 0u || entry == ~0ull) {
        return;
    }
    ((app_write_fn_t)(uintptr_t)entry)(bootview, text);
}

static void zero_bytes(void *ptr, uint64_t len) {
    uint8_t *out = (uint8_t *)ptr;
    uint64_t i = 0u;
    while (i < len) {
        out[i] = 0u;
        i += 1u;
    }
}

static void copy_bytes(char *out, const char *in, uint32_t size) {
    uint32_t i = 0u;
    while (i < size) {
        out[i] = in[i];
        i += 1u;
    }
}

static void append_u32(char *out, uint32_t value) {
    char digits[10];
    uint32_t count = 0u;
    uint32_t cursor = 0u;

    if (value == 0u) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value != 0u && count < 10u) {
        digits[count] = (char)('0' + (value % 10u));
        value /= 10u;
        count += 1u;
    }
    while (count != 0u) {
        count -= 1u;
        out[cursor] = digits[count];
        cursor += 1u;
    }
    out[cursor] = '\0';
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 96u;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_PROGRAM;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint64_t parse_hex_u64(const char *text) {
    uint64_t value = 0u;
    uint32_t i = 0u;

    while (text[i] != '\0') {
        char ch = text[i];
        uint64_t digit = 16u;
        if (ch >= '0' && ch <= '9') {
            digit = (uint64_t)(ch - '0');
        } else if (ch >= 'A' && ch <= 'F') {
            digit = 10u + (uint64_t)(ch - 'A');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = 10u + (uint64_t)(ch - 'a');
        } else {
            break;
        }
        value = (value << 4) | digit;
        i += 1u;
    }
    return value;
}

static uint64_t parse_u64_field(const char *text, const char *marker) {
    uint32_t i = 0u;
    uint32_t cursor = 0u;

    while (text[i] != '\0') {
        cursor = 0u;
        while (marker[cursor] != '\0' && text[i + cursor] == marker[cursor]) {
            cursor += 1u;
        }
        if (marker[cursor] == '\0') {
            return parse_hex_u64(text + i + cursor);
        }
        i += 1u;
    }
    return 0u;
}

static uint32_t gather_objects(struct luna_cid cid, struct luna_object_ref set_ref, struct luna_object_ref *out_refs, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes(out_refs, buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 98u;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = set_ref.low;
    gate->object_high = set_ref.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)out_refs;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return 0u;
    }
    return gate->result_count;
}

static uint32_t draw_object_bytes(struct luna_cid cid, struct luna_object_ref ref, char *out_text, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes(out_text, buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 99u;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = ref.low;
    gate->object_high = ref.high;
    gate->offset = 0u;
    gate->size = buffer_size - 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)out_text;
    gate->buffer_size = buffer_size - 1u;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return 0u;
    }
    return (uint32_t)gate->size;
}

static void sanitize_preview(char *text, uint32_t size) {
    uint32_t i = 0u;
    while (i < size) {
        char ch = text[i];
        if (ch == '\0') {
            break;
        }
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            text[i] = ' ';
        }
        i += 1u;
    }
}

static int starts_with_note(const char *text) {
    static const char marker[] = "LUNA-NOTE";
    uint32_t i = 0u;
    while (marker[i] != '\0') {
        if (text[i] != marker[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

static int starts_with_theme(const char *text) {
    static const char marker[] = "LUNA-THEME";
    uint32_t i = 0u;
    while (marker[i] != '\0') {
        if (text[i] != marker[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

static int starts_with_files_view(const char *text) {
    static const char marker[] = "LUNA-FILES";
    uint32_t i = 0u;
    while (marker[i] != '\0') {
        if (text[i] != marker[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

struct luna_host_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    struct luna_object_ref primary_user;
    char hostname[32];
};

struct luna_user_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    struct luna_object_ref secret_object;
    struct luna_object_ref home_root;
    struct luna_object_ref profile_object;
    char username[16];
};

struct luna_user_home_root_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    struct luna_object_ref user_object;
    struct luna_object_ref documents_set;
    struct luna_object_ref desktop_set;
    struct luna_object_ref downloads_set;
};

#define LUNA_HOST_RECORD_MAGIC 0x484F5354u
#define LUNA_USER_RECORD_MAGIC 0x55534552u
#define LUNA_USER_HOME_MAGIC 0x484F4D45u
#define LUNA_USER_RECORD_VERSION 1u

static int same_object_ref(struct luna_object_ref left, struct luna_object_ref right) {
    return left.low == right.low && left.high == right.high;
}

static int load_current_user_identity(
    struct luna_cid gather_cid,
    struct luna_cid draw_cid,
    struct luna_object_ref documents_set,
    char *out_username,
    uint32_t out_username_size,
    char *out_hostname,
    uint32_t out_hostname_size
) {
    struct luna_object_ref refs[32];
    struct luna_host_record host;
    struct luna_user_record user;
    struct luna_user_home_root_record home;
    uint32_t count = gather_objects(gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs));
    uint32_t i = 0u;
    uint32_t j = 0u;

    if (out_username_size != 0u) {
        out_username[0] = '\0';
    }
    if (out_hostname_size != 0u) {
        out_hostname[0] = '\0';
    }
    while (i < count && i < 32u) {
        if (draw_object_bytes(draw_cid, refs[i], (char *)&host, sizeof(host)) >= sizeof(host) &&
            host.magic == LUNA_HOST_RECORD_MAGIC &&
            host.version == LUNA_USER_RECORD_VERSION) {
            copy_bytes(out_hostname, host.hostname, out_hostname_size);
            if (out_hostname_size != 0u) {
                out_hostname[out_hostname_size - 1u] = '\0';
            }
            break;
        }
        i += 1u;
    }
    j = 0u;
    while (j < count && j < 32u) {
        if (draw_object_bytes(draw_cid, refs[j], (char *)&home, sizeof(home)) >= sizeof(home) &&
            home.magic == LUNA_USER_HOME_MAGIC &&
            home.version == LUNA_USER_RECORD_VERSION &&
            same_object_ref(home.documents_set, documents_set)) {
            uint32_t k = 0u;
            while (k < count && k < 32u) {
                if (draw_object_bytes(draw_cid, refs[k], (char *)&user, sizeof(user)) >= sizeof(user) &&
                    user.magic == LUNA_USER_RECORD_MAGIC &&
                    user.version == LUNA_USER_RECORD_VERSION &&
                    same_object_ref(user.home_root, refs[j])) {
                    copy_bytes(out_username, user.username, out_username_size);
                    if (out_username_size != 0u) {
                        out_username[out_username_size - 1u] = '\0';
                    }
                    return 1;
                }
                k += 1u;
            }
        }
        j += 1u;
    }
    if (documents_set.low == 0u && documents_set.high == 0u) {
        j = 0u;
        while (j < count && j < 32u) {
            if (draw_object_bytes(draw_cid, refs[j], (char *)&user, sizeof(user)) >= sizeof(user) &&
                user.magic == LUNA_USER_RECORD_MAGIC &&
                user.version == LUNA_USER_RECORD_VERSION) {
                copy_bytes(out_username, user.username, out_username_size);
                if (out_username_size != 0u) {
                    out_username[out_username_size - 1u] = '\0';
                }
                return 1;
            }
            j += 1u;
        }
    }
    return 0;
}

static uint32_t collect_preview_refs(
    struct luna_cid gather_cid,
    struct luna_cid draw_cid,
    struct luna_object_ref documents_set,
    struct luna_object_ref *out_refs,
    uint64_t buffer_size
) {
    struct luna_object_ref all_refs[16];
    char preview_raw[160];
    uint32_t count = 0u;
    uint32_t capacity = (uint32_t)(buffer_size / sizeof(struct luna_object_ref));

    zero_bytes(out_refs, buffer_size);
    if (capacity == 0u) {
        return 0u;
    }
    if (documents_set.low != 0u && documents_set.high != 0u) {
        count = gather_objects(gather_cid, documents_set, out_refs, buffer_size);
        if (count != 0u) {
            return count;
        }
    }
    if (draw_cid.low == 0u) {
        return 0u;
    }
    count = 0u;
    {
        uint32_t total = gather_objects(gather_cid, (struct luna_object_ref){0u, 0u}, all_refs, sizeof(all_refs));
        for (uint32_t i = 0u; i < total && count < capacity; ++i) {
            uint32_t size = draw_object_bytes(draw_cid, all_refs[i], preview_raw, sizeof(preview_raw));
            if (size == 0u || (!starts_with_note(preview_raw) && !starts_with_theme(preview_raw))) {
                continue;
            }
            out_refs[count++] = all_refs[i];
        }
    }
    return count;
}

static uint32_t parse_selected_index(const char *text) {
    uint32_t i = 0u;
    while (text[i] != '\0') {
        if (text[i] == 's' &&
            text[i + 1u] == 'e' &&
            text[i + 2u] == 'l' &&
            text[i + 3u] == 'e' &&
            text[i + 4u] == 'c' &&
            text[i + 5u] == 't' &&
            text[i + 6u] == 'e' &&
            text[i + 7u] == 'd' &&
            text[i + 8u] == ':' &&
            text[i + 9u] == ' ') {
            uint32_t value = 0u;
            i += 10u;
            while (text[i] >= '0' && text[i] <= '9') {
                value = (value * 10u) + (uint32_t)(text[i] - '0');
                i += 1u;
            }
            return value;
        }
        i += 1u;
    }
    return 0u;
}

static int load_files_state(struct luna_cid gather_cid, struct luna_cid draw_cid, struct luna_object_ref *out_documents, uint32_t *out_selected) {
    struct luna_object_ref refs[16];
    char preview_raw[160];
    uint32_t count = gather_objects(gather_cid, (struct luna_object_ref){0u, 0u}, refs, sizeof(refs));
    struct luna_object_ref best_view = {0u, 0u};
    struct luna_object_ref best_documents = {0u, 0u};
    uint32_t best_selected = 0u;
    uint32_t best_member_count = 0u;

    out_documents->low = 0u;
    out_documents->high = 0u;
    *out_selected = 0u;
    if (draw_cid.low == 0u) {
        return 0;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t size = draw_object_bytes(draw_cid, refs[i], preview_raw, sizeof(preview_raw));
        if (size == 0u || !starts_with_files_view(preview_raw)) {
            continue;
        }
        {
            struct luna_object_ref candidate_documents = {
                parse_u64_field(preview_raw, "documents-low: "),
                parse_u64_field(preview_raw, "documents-high: ")
            };
            uint32_t candidate_valid = (candidate_documents.low != 0u && candidate_documents.high != 0u) ? 1u : 0u;
            uint32_t best_valid = (best_documents.low != 0u && best_documents.high != 0u) ? 1u : 0u;
            uint32_t candidate_members = 0u;
            if (candidate_valid != 0u) {
                struct luna_object_ref candidate_refs[8];
                candidate_members = gather_objects(gather_cid, candidate_documents, candidate_refs, sizeof(candidate_refs));
            }
            if (best_view.low == 0u ||
                (candidate_members != 0u && best_member_count == 0u) ||
                (candidate_valid != 0u && best_valid == 0u) ||
                (candidate_members == best_member_count &&
                 candidate_valid == best_valid &&
                 refs[i].high > best_view.high)) {
                best_view = refs[i];
                best_selected = parse_selected_index(preview_raw);
                best_documents = candidate_documents;
                best_member_count = candidate_members;
            }
        }
    }
    if (best_view.low != 0u && best_view.high != 0u) {
        *out_selected = best_selected;
        *out_documents = best_documents;
        return 1;
    }
    return 0;
}

static const char *find_note_title(const char *text) {
    uint32_t i = 0u;
    while (text[i] != '\0') {
        if (text[i] == 't' &&
            text[i + 1u] == 'i' &&
            text[i + 2u] == 't' &&
            text[i + 3u] == 'l' &&
            text[i + 4u] == 'e' &&
            text[i + 5u] == ':' &&
            text[i + 6u] == ' ') {
            return &text[i + 7u];
        }
        i += 1u;
    }
    return 0;
}

static const char *find_note_body(const char *text) {
    uint32_t i = 0u;
    while (text[i] != '\0') {
        if (text[i] == 'b' &&
            text[i + 1u] == 'o' &&
            text[i + 2u] == 'd' &&
            text[i + 3u] == 'y' &&
            text[i + 4u] == ':' &&
            text[i + 5u] == ' ') {
            return &text[i + 6u];
        }
        i += 1u;
    }
    return 0;
}

static void copy_field_excerpt(char *out, const char *text, uint32_t limit) {
    uint32_t i = 0u;
    while (i + 1u < limit && text[i] != '\0') {
        char ch = text[i];
        if (ch == '\r' || ch == '\n') {
            break;
        }
        if (ch == '\t') {
            ch = ' ';
        }
        out[i] = ch;
        i += 1u;
    }
    out[i] = '\0';
}

static void preview_objects(const struct luna_bootview *bootview, struct luna_cid gather_cid, struct luna_cid draw_cid) {
    struct luna_object_ref refs[8];
    char preview_raw[96];
    char preview[96];
    char title_excerpt[25];
    char body_excerpt[25];
    struct luna_object_ref documents_set = {0u, 0u};
    uint32_t selected = 0u;
    uint32_t count = 0u;
    uint32_t shown = 0u;
    uint32_t i = 0u;

    if (draw_cid.low != 0u) {
        (void)load_files_state(gather_cid, draw_cid, &documents_set, &selected);
        count = collect_preview_refs(gather_cid, draw_cid, documents_set, refs, sizeof(refs));
    }
    if (count != 0u && draw_cid.low != 0u) {
        i = 0u;
        shown = 0u;
        i = 0u;
        while (i < count && shown < 3u) {
            uint32_t size = draw_object_bytes(draw_cid, refs[i], preview_raw, sizeof(preview_raw));
            if (size != 0u) {
                copy_bytes(preview, preview_raw, sizeof(preview));
                sanitize_preview(preview, size);
                app_write(bootview, shown == selected ? "> " : "  ");
                app_write(bootview, "item ");
                if (starts_with_note(preview_raw)) {
                    const char *title = find_note_title(preview_raw);
                    const char *body = find_note_body(preview_raw);
                    app_write(bootview, "[note] ");
                    if (title != 0) {
                        copy_field_excerpt(title_excerpt, title, sizeof(title_excerpt));
                        app_write(bootview, title_excerpt);
                        app_write(bootview, " | ");
                    }
                    if (body != 0) {
                        copy_field_excerpt(body_excerpt, body, sizeof(body_excerpt));
                        app_write(bootview, body_excerpt);
                    }
                } else {
                    app_write(bootview, preview);
                }
                app_write(bootview, "\r\n");
                shown += 1u;
            }
            i += 1u;
        }
    }
}

void SYSV_ABI files_app_entry(const struct luna_bootview *bootview) {
    struct luna_cid gather_cid = {0u, 0u};
    struct luna_cid draw_cid = {0u, 0u};
    struct luna_object_ref refs[8];
    struct luna_object_ref documents_set = {0u, 0u};
    char preview_raw[96];
    char line[96];
    char username[16];
    char hostname[32];
    uint32_t count = 0u;
    uint32_t selected = 0u;
    uint32_t visible = 0u;
    uint32_t selected_visible = 0u;

    app_write(bootview, "[FILES] Luna Files preview\r\n");
    if (request_capability(LUNA_CAP_DATA_GATHER, &gather_cid) == LUNA_GATE_OK &&
        request_capability(LUNA_CAP_DATA_DRAW, &draw_cid) == LUNA_GATE_OK) {
        (void)load_files_state(gather_cid, draw_cid, &documents_set, &selected);
        zero_bytes(username, sizeof(username));
        zero_bytes(hostname, sizeof(hostname));
        if (load_current_user_identity(gather_cid, draw_cid, documents_set, username, sizeof(username), hostname, sizeof(hostname))) {
            app_write(bootview, "library ");
            app_write(bootview, username);
            app_write(bootview, "@");
            app_write(bootview, hostname[0] != '\0' ? hostname : "luna");
            app_write(bootview, " Documents\r\n");
        } else {
            app_write(bootview, "library current user Documents\r\n");
        }
        count = collect_preview_refs(gather_cid, draw_cid, documents_set, refs, sizeof(refs));
        for (uint32_t i = 0u; i < count; ++i) {
            uint32_t size = draw_object_bytes(draw_cid, refs[i], preview_raw, sizeof(preview_raw));
            if (size == 0u) {
                continue;
            }
            visible += 1u;
            if (i == selected) {
                selected_visible = 1u;
            }
        }
        append_u32(line, visible);
        app_write(bootview, "items visible ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
        append_u32(line, selected);
        app_write(bootview, "selection ");
        app_write(bootview, line);
        app_write(bootview, "\r\n");
        app_write(bootview, "current item ");
        if (count == 0u || selected >= count) {
            app_write(bootview, "empty\r\n");
        } else if (selected_visible == 0u) {
            app_write(bootview, "hidden\r\n");
        } else if (draw_object_bytes(draw_cid, refs[selected], preview_raw, sizeof(preview_raw)) != 0u) {
            if (starts_with_note(preview_raw)) {
                const char *title = find_note_title(preview_raw);
                app_write(bootview, "note");
                if (title != 0 && title[0] != '\0') {
                    app_write(bootview, " ");
                    app_write(bootview, title);
                }
                app_write(bootview, "\r\n");
            } else if (starts_with_theme(preview_raw)) {
                app_write(bootview, "theme\r\n");
            } else {
                sanitize_preview(preview_raw, sizeof(preview_raw));
                app_write(bootview, preview_raw);
                app_write(bootview, "\r\n");
            }
        } else {
            app_write(bootview, "unavailable\r\n");
        }
        app_write(bootview, "access current session\r\n");
        app_write(bootview, "next browse the list and open the current item\r\n");
        preview_objects(bootview, gather_cid, draw_cid);
    } else {
        app_write(bootview, "library unavailable\r\n");
    }
    app_write(bootview, "controls arrows browse  enter open\r\n");
    app_write(bootview, "browser surface online\r\n");
}
