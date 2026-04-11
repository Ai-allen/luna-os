#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_NOTE_OBJECT_TYPE 0x4C4E4F54u

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

static uint32_t gather_objects(struct luna_cid cid, struct luna_object_ref *out_refs, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes(out_refs, buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 102u;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
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

static int starts_with_text(const char *text, const char *prefix) {
    uint32_t i = 0u;
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

static void copy_line_value(char *out, uint32_t out_size, const char *src) {
    uint32_t i = 0u;
    if (out_size == 0u) {
        return;
    }
    while (i + 1u < out_size && src[i] != '\0' && src[i] != '\r' && src[i] != '\n') {
        out[i] = src[i];
        i += 1u;
    }
    out[i] = '\0';
}

static void parse_note_preview(const char *preview, char *out_title, uint32_t title_size, char *out_body, uint32_t body_size) {
    uint32_t i = 0u;

    copy_line_value(out_title, title_size, "workspace");
    copy_line_value(out_body, body_size, "Luna note seed");
    while (preview[i] != '\0') {
        if (starts_with_text(preview + i, "title: ")) {
            copy_line_value(out_title, title_size, preview + i + 7u);
        } else if (starts_with_text(preview + i, "body: ")) {
            copy_line_value(out_body, body_size, preview + i + 6u);
        }
        i += 1u;
    }
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 98u;
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

static uint32_t seed_note_object(struct luna_cid cid, struct luna_object_ref *out_ref) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 99u;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_type = LUNA_NOTE_OBJECT_TYPE;
    gate->object_flags = 0u;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return gate->status;
    }
    out_ref->low = gate->object_low;
    out_ref->high = gate->object_high;
    return LUNA_DATA_OK;
}

static uint32_t pour_note_bytes(struct luna_cid cid, struct luna_object_ref object_ref, const char *text, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 100u;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    return gate->status;
}

static uint32_t draw_note_bytes(
    struct luna_cid cid,
    struct luna_object_ref object_ref,
    char *out_text,
    uint64_t buffer_size
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes(out_text, buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 101u;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = buffer_size - 1u;
    gate->buffer_addr = (uint64_t)(uintptr_t)out_text;
    gate->buffer_size = buffer_size - 1u;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return gate->status;
    }
    return LUNA_DATA_OK;
}

void SYSV_ABI notes_app_entry(const struct luna_bootview *bootview) {
    static const char note_seed[] =
        "LUNA-NOTE\r\n"
        "title: firstlight\r\n"
        "body: Luna note seed\r\n";
    struct luna_cid seed_cid = {0u, 0u};
    struct luna_cid pour_cid = {0u, 0u};
    struct luna_cid draw_cid = {0u, 0u};
    struct luna_cid gather_cid = {0u, 0u};
    struct luna_object_ref note_ref = {0u, 0u};
    struct luna_object_ref refs[8];
    char preview[96];
    char title_line[48];
    char body_line[96];
    uint32_t count = 0u;
    uint32_t i = 0u;
    uint32_t found_existing = 0u;

    app_write(bootview, "[NOTES] Luna Notes live\r\n");
    if (request_capability(LUNA_CAP_DATA_SEED, &seed_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_POUR, &pour_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_DRAW, &draw_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_GATHER, &gather_cid) != LUNA_GATE_OK) {
        app_write(bootview, "workspace unavailable\r\n");
        return;
    }

    count = gather_objects(gather_cid, refs, sizeof(refs));
    while (i < count) {
        if (draw_note_bytes(draw_cid, refs[i], preview, sizeof(preview)) == LUNA_DATA_OK &&
            starts_with_note(preview)) {
            note_ref = refs[i];
            found_existing = 1u;
            break;
        }
        i += 1u;
    }

    if (found_existing == 0u) {
        if (seed_note_object(seed_cid, &note_ref) != LUNA_DATA_OK) {
            app_write(bootview, "workspace unavailable\r\n");
            return;
        }

        if (pour_note_bytes(pour_cid, note_ref, note_seed, sizeof(note_seed) - 1u) != LUNA_DATA_OK) {
            app_write(bootview, "save failed\r\n");
            return;
        }
        app_write(bootview, "workspace created\r\n");
    } else {
        app_write(bootview, "workspace opened\r\n");
    }

    if (draw_note_bytes(draw_cid, note_ref, preview, sizeof(preview)) != LUNA_DATA_OK) {
        app_write(bootview, "workspace open failed\r\n");
        return;
    }

    parse_note_preview(preview, title_line, sizeof(title_line), body_line, sizeof(body_line));
    app_write(bootview, "title ");
    app_write(bootview, title_line);
    app_write(bootview, "\r\n");
    app_write(bootview, "text ");
    app_write(bootview, body_line);
    app_write(bootview, "\r\n");
    app_write(bootview, "status ready for editing\r\n");
    app_write(bootview, "next type in Notes or use desktop.note <text>\r\n");
}
