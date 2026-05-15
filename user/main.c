#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *device_gate_fn_t)(struct luna_device_gate *gate);
typedef void (SYSV_ABI *network_gate_fn_t)(struct luna_network_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *program_gate_fn_t)(struct luna_program_gate *gate);
typedef void (SYSV_ABI *data_gate_fn_t)(struct luna_data_gate *gate);
typedef void (SYSV_ABI *system_gate_fn_t)(struct luna_system_gate *gate);
typedef void (SYSV_ABI *graphics_gate_fn_t)(struct luna_graphics_gate *gate);
typedef void (SYSV_ABI *package_gate_fn_t)(struct luna_package_gate *gate);
typedef void (SYSV_ABI *observe_gate_fn_t)(struct luna_observe_gate *gate);

static void device_write(const char *text);
static void device_write_bytes(const char *text, size_t size);
static uint32_t graphics_render_desktop_state(uint32_t attr, uint32_t selection, uint32_t x, uint32_t y, uint32_t *kind, uint32_t *window_id, uint32_t *glyph);
static uint32_t graphics_query_window(uint32_t window_id, uint32_t *out_window_id, uint32_t *out_x, uint32_t *out_y, uint32_t *out_width, uint32_t *out_height, uint32_t *out_minimized, uint32_t *out_maximized, char *out_title, uint64_t out_title_size);
static int active_window_matches(const char *title);
static uint32_t graphics_create_window_native(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char *title, uint32_t *out_window_id);
static uint32_t graphics_draw_window_char(uint32_t window_id, uint32_t x, uint32_t y, char ch, uint8_t attr);
static uint32_t render_settings_window(void);
static uint32_t render_files_window(void);
static uint32_t render_notes_window(void);
static uint32_t render_console_window(void);
static void append_u32_decimal(char *out, uint32_t value);
static void append_u32_hex_fixed(char *out, uint32_t value, uint32_t digits);
static void append_u64_hex_fixed(char *out, uint64_t value, uint32_t digits);
static uint32_t file_visible_object_count(void);
static void refresh_files_app(void);
static void debug_input_byte(const char *prefix, uint32_t value);
static const char *package_label_by_index(uint32_t index);
static const char *package_display_name(const char *name, size_t len, uint32_t *out_index);
static void copy_name16(volatile char out[16], const char *text, size_t len);
static int chars_equal(const char *left, const char *right, size_t len);
static const char *canonical_package_name(const char *name, size_t len);
static void append_u64_decimal(char *out, uint64_t value);
static void print_desktop_status(void);
static uint32_t network_pair_peer(const char *name, struct luna_link_peer *out);
static uint32_t network_open_session(uint32_t peer_id, struct luna_link_session *out);
static uint32_t network_open_channel(uint32_t session_id, uint32_t kind, uint32_t transfer_class, struct luna_link_channel *out);
static uint32_t network_send_channel(uint32_t channel_id, const void *payload, uint64_t size);
static uint32_t network_recv_channel(uint32_t channel_id, void *out, uint64_t size, uint64_t *out_size);
static uint32_t network_send_packet(const void *payload, uint64_t size);
static uint32_t network_recv_packet(void *out, uint64_t size, uint64_t *out_size);
static uint32_t network_get_info(struct luna_net_info *out);
static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out);
static void print_lasql_catalog(void);
static void print_lasql_files(void);
static void print_lasql_logs(void);
static int file_query_row_visible(const struct luna_query_row *row);
static uint32_t query_visible_file_rows(struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count);
static int query_selected_file_row(struct luna_query_row *out_row, uint32_t *out_count);
static int prepare_files_surface(void);
static void print_files_status(void);
static void print_package_feedback(const char *action, const char *name, size_t len, uint32_t status);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_putc(char value) {
    while ((inb(0x3FD) & 0x20u) == 0u) {
    }
    outb(0x3F8, (uint8_t)value);
}

static void serial_write_debug(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void debug_input_byte(const char *prefix, uint32_t value) {
    char hex[3];
    static const char digits[] = "0123456789ABCDEF";

    hex[0] = digits[(value >> 4) & 0x0Fu];
    hex[1] = digits[value & 0x0Fu];
    hex[2] = '\0';
    serial_write_debug(prefix);
    serial_write_debug(hex);
    serial_write_debug("\r\n");
}

static void debug_input_text(const char *prefix, const char *text, size_t len) {
    serial_write_debug(prefix);
    for (size_t i = 0u; i < len; ++i) {
        char ch = text[i];
        if (ch == '\0') {
            break;
        }
        serial_putc(ch);
    }
    serial_write_debug("\r\n");
}

static struct luna_cid g_shell_cid = {0, 0};
static struct luna_cid g_life_read_cid = {0, 0};
static struct luna_cid g_program_load_cid = {0, 0};
static struct luna_cid g_program_start_cid = {0, 0};
static struct luna_cid g_data_seed_cid = {0, 0};
static struct luna_cid g_data_pour_cid = {0, 0};
static struct luna_cid g_data_draw_cid = {0, 0};
static struct luna_cid g_data_gather_cid = {0, 0};
static struct luna_cid g_data_query_cid = {0, 0};
static struct luna_cid g_observe_read_cid = {0, 0};
static struct luna_cid g_device_list_cid = {0, 0};
static struct luna_cid g_device_read_cid = {0, 0};
static struct luna_cid g_device_write_cid = {0, 0};
static struct luna_cid g_network_send_cid = {0, 0};
static struct luna_cid g_network_recv_cid = {0, 0};
static struct luna_cid g_network_pair_cid = {0, 0};
static struct luna_cid g_network_session_cid = {0, 0};
static struct luna_cid g_graphics_draw_cid = {0, 0};
static struct luna_cid g_package_list_cid = {0, 0};
static struct luna_cid g_package_install_cid = {0, 0};
static struct luna_cid g_update_check_cid = {0, 0};
static struct luna_cid g_update_apply_cid = {0, 0};
static struct luna_cid g_user_auth_cid = {0, 0};
static uint32_t g_launcher_open = 0u;
static uint32_t g_launcher_index = 0u;
static uint32_t g_control_open = 0u;
static uint32_t g_pointer_x = 40u;
static uint32_t g_pointer_y = 12u;
static uint32_t g_pointer_buttons = 0u;
static uint32_t g_drag_window = 0u;
static uint32_t g_resize_window = 0u;
static uint32_t g_pointer_armed = 0u;
static uint32_t g_pointer_active = 0u;
static uint32_t g_desktop_mode = 0u;
static uint32_t g_desktop_booted = 0u;
static uint32_t g_user_input_debug_budget = 64u;
static uint32_t g_user_command_debug_budget = 32u;
static uint32_t g_session_script_debug_budget = 64u;
static uint64_t device_read_block(uint32_t device_id, void *out, uint64_t size);
static struct luna_desktop_shell_state g_desktop_shell = {
    .version = 1u,
    .entry_count = 5u,
    .desktop_attr = 0x17u,
    .chrome_attr = 0x71u,
    .panel_attr = 0x1Bu,
    .accent_attr = 0x1Fu,
    .entries = {
        { "files.luna", "Files", 3u, 4u, 0u },
        { "notes.luna", "Notes", 3u, 8u, 0u },
        { "guard.luna", "Guard", 3u, 12u, 0u },
        { "console.luna", "Console", 3u, 16u, 0u },
        { "hello.luna", "Settings", 3u, 20u, 0u },
    },
};
static struct luna_package_record g_package_catalog[LUNA_PACKAGE_CAPACITY];
static uint32_t g_package_count = 0u;
static uint32_t g_theme_variant = 0u;
static struct luna_object_ref g_theme_object = {0u, 0u};
static struct luna_object_ref g_note_object = {0u, 0u};
static uint32_t g_settings_window_id = 0u;
static uint32_t g_files_window_id = 0u;
static uint32_t g_notes_window_id = 0u;
static uint32_t g_console_window_id = 0u;
static const char g_note_marker[] = "LUNA-NOTE\r\n";
static const char g_theme_marker[] = "LUNA-THEME\r\n";
static char g_note_body[97] = "Luna note seed";
static struct luna_object_ref g_files_view_object = {0u, 0u};
static struct luna_object_ref g_user_documents_set = {0u, 0u};
static struct luna_object_ref g_host_object = {0u, 0u};
static struct luna_object_ref g_user_object = {0u, 0u};
static struct luna_object_ref g_user_secret_object = {0u, 0u};
static struct luna_object_ref g_user_home_object = {0u, 0u};
static struct luna_object_ref g_user_profile_object = {0u, 0u};
static struct luna_object_ref g_user_desktop_set = {0u, 0u};
static struct luna_object_ref g_user_downloads_set = {0u, 0u};
static uint32_t g_files_selection = 0u;
static struct luna_object_ref g_files_candidate_refs[LUNA_DATA_OBJECT_CAPACITY];
static struct {
    struct luna_query_request request;
    struct luna_query_row rows[LUNA_DATA_OBJECT_CAPACITY];
} g_lasql_data_payload;
static struct {
    struct luna_query_request request;
    struct luna_query_row rows[LUNA_OBSERVE_LOG_CAPACITY];
} g_lasql_observe_payload;
static struct luna_cid g_system_query_cid = {0, 0};
static uint64_t g_device_lifecycle_flags = 0u;
static uint32_t g_input_recovery_actions = 0u;
static char g_hostname[32] = "luna";
static char g_username[16] = "guest";
static uint32_t g_user_setup_required = 1u;
static uint32_t g_user_logged_in = 0u;
static uint64_t g_user_session_low = 0u;
static uint64_t g_user_session_high = 0u;
static struct {
    uint32_t ready;
    uint32_t peer_id;
    uint32_t session_id;
    uint32_t channel_id;
    uint32_t last_status;
    uint32_t last_phase;
    uint64_t tx_messages;
    uint64_t rx_messages;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    char peer_name[16];
} g_external_stack = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, "external"};
static const char g_msg_shell_ready[] = "[USER] shell ready\r\n";
static const char g_msg_input_lane[] = "[USER] input lane ready\r\n";
static const char g_msg_newline[] = "\r\n";
static const char g_msg_help[] =
    "core: setup.status  setup.init <hostname> <username> <password>  login <username> <password>  logout  whoami  hostname  id  home.status\r\n"
    "apps: list-apps  run <app>  package.install <app>  package.remove <app>\r\n"
    "settings: settings.status  update.status  update.apply  update.rollback  pairing.status  net.status  net.connect [peer]  net.send <text>  net.recv  net.info\r\n"
    "lasql: lasql.catalog  lasql.files  lasql.logs\r\n"
    "desktop: desktop.boot  desktop.menu  desktop.settings  desktop.launch <app>  desktop.files.prev  desktop.files.next  desktop.files.open  desktop.note <text>  desktop.minimize  desktop.maximize  desktop.close  exit\r\n"
    "diagnostics: net.external  net.loop  net.inbound  desktop.status  cap-count  cap-list  seal-list  list-spaces  space-map  space-log  store-info  store-check  list-devices  lane-census  pci-scan  revoke-cap <domain>\r\n";
static const char g_msg_exit[] = "session locked\r\n";
static const char g_msg_unknown[] = "unknown command; run help\r\n";
static const char g_msg_login_required[] = "login required: session is locked\r\n";
static const char g_msg_setup_required[] = "first-setup required: no hostname or user configured\r\n";
static const char g_msg_setup_hint[] = "setup.init <hostname> <username> <password>\r\n";
static const char g_msg_login_hint[] = "login <username> <password>\r\n";
static const char g_msg_login_ok[] = "login ok: session active\r\n";
static const char g_msg_login_fail[] = "login fail: invalid username or password\r\n";
static const char g_msg_login_usage[] = "login usage: login <username> <password>\r\n";
static const char g_msg_logout_ok[] = "logout ok\r\n";
static const char g_msg_setup_ok[] = "setup.init ok: host and first user created\r\n";
static const char g_msg_setup_fail[] = "setup.init fail: setup not applied\r\n";
static const char g_msg_setup_usage[] = "setup.init usage: setup.init <hostname> <username> <password>\r\n";
static const char g_msg_setup_already[] = "setup.init skipped: system already initialized\r\n";
static const char g_msg_run_prefix[] = "launch request: ";
static const char g_msg_run_suffix[] = "\r\n";
static const char g_msg_run_fail[] = "launch failed\r\n";
static const char g_msg_run_hint[] = "use help or list-apps to choose an installed app\r\n";
static const char g_msg_settings_ready[] = "settings surface ready\r\n";
static const char g_msg_files_ready[] = "files surface ready\r\n";
static const char g_msg_spaces_fail[] = "list-spaces failed\r\n";
static const char g_msg_caps_fail[] = "cap-count failed\r\n";
static const char g_msg_caplist_fail[] = "cap-list failed\r\n";
static const char g_msg_seallist_fail[] = "seal-list failed\r\n";
static const char g_msg_store_fail[] = "store-info failed\r\n";
static const char g_msg_storecheck_fail[] = "store-check failed\r\n";
static const char g_msg_revoke_fail[] = "revoke-cap failed\r\n";
static const char g_msg_revoke_prefix[] = "revoked: ";
static const char g_msg_revoke_mid[] = " (";
static const char g_msg_revoke_suffix[] = ")\r\n";
static const char g_msg_devices_fail[] = "list-devices failed\r\n";
static const char g_msg_lanes_fail[] = "lane-census failed\r\n";
static const char g_msg_pci_fail[] = "pci-scan failed\r\n";
static const char g_msg_net_fail[] = "net.loop failed\r\n";
static const char g_msg_net_external_fail[] = "net.external failed\r\n";
static const char g_msg_spacemap_fail[] = "space-map failed\r\n";
static const char g_msg_spacelog_fail[] = "space-log failed\r\n";
static const char g_msg_id_prefix[] = "uid=0(";
static const char g_msg_id_mid[] = ") host=";
static const char g_msg_id_suffix[] = "\r\n";
static const char g_msg_desktop_ready[] = "[USER] desktop session online\r\n";
static const char g_msg_desktop_prefix[] = "[DESKTOP] ";
static const char g_msg_ok_suffix[] = " ok\r\n";
static const char g_msg_fail_suffix[] = " fail\r\n";
static const char g_msg_input_recovery_minimal[] = "[USER] input recovery=keyboard-minimal\r\n";
static const char g_msg_input_recovery_shell[] = "[USER] input recovery=operator-shell\r\n";
static const char g_msg_status_launcher[] = "status launcher=";
static const char g_msg_status_control[] = " control=";
static const char g_msg_status_theme[] = " theme=";
static const char g_msg_status_selected[] = " selected=";
static const char g_msg_status_open[] = "open";
static const char g_msg_status_closed[] = "closed";
static const char g_msg_status_update[] = " update=";
static const char g_msg_status_pair[] = " pair=";
static const char g_msg_status_policy[] = " policy=";
static const char g_msg_hit_kind[] = "hit kind=";
static const char g_msg_hit_window[] = " window=";
static const char g_msg_hit_glyph[] = " glyph=";
static const char g_msg_window_id[] = "window id=";
static const char g_msg_window_title[] = " title=";
static const char g_msg_window_x[] = " x=";
static const char g_msg_window_y[] = " y=";
static const char g_msg_window_w[] = " w=";
static const char g_msg_window_h[] = " h=";
static const char g_msg_window_min[] = " min=";
static const char g_msg_window_max[] = " max=";

struct luna_session_script_header {
    uint32_t magic;
    uint32_t command_bytes;
};

#define LUNA_DEV_BUNDLE_STAGE_MAGIC 0x4244564Cu
#define LUNA_PROGRAM_BUNDLE_MAGIC 0x4C554E42u
#define LUNA_LEGACY_APP_MAGIC 0x50414E55u

struct luna_dev_bundle_stage_header {
    uint32_t magic;
    uint32_t bundle_bytes;
};

struct luna_bundle_header {
    uint32_t magic;
    uint32_t version;
    uint64_t bundle_id;
    uint64_t source_id;
    uint64_t app_version;
    uint64_t integrity_check;
    uint64_t header_bytes;
    uint64_t entry_offset;
    uint64_t signer_id;
    uint64_t signature_check;
    uint32_t capability_count;
    uint32_t flags;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t sdk_major;
    uint16_t sdk_minor;
    uint32_t min_proto_version;
    uint32_t max_proto_version;
    char name[16];
    uint64_t capability_keys[4];
};

struct luna_app_header {
    uint32_t magic;
    uint32_t version;
    uint64_t entry_offset;
    uint32_t capability_count;
    uint32_t reserved0;
    char name[16];
    uint64_t capability_keys[4];
};

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

struct luna_user_secret_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    struct luna_object_ref user_object;
    uint64_t salt;
    uint64_t password_fold;
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

struct luna_user_profile_record {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t theme_variant;
    struct luna_object_ref user_object;
    struct luna_object_ref home_root;
    struct luna_object_ref note_object;
    struct luna_object_ref theme_object;
    struct luna_object_ref files_view_object;
};

#define LUNA_THEME_OBJECT_TYPE 0x4C54484Du
#define LUNA_NOTE_OBJECT_TYPE 0x4C4E4F54u
#define LUNA_FILES_VIEW_OBJECT_TYPE 0x4C46564Du
#define LUNA_SET_OBJECT_TYPE 0x4C534554u
#define LUNA_SET_MAGIC 0x5345544Cu
#define LUNA_SET_VERSION 1u

struct luna_set_header {
    uint32_t magic;
    uint32_t version;
    uint32_t member_count;
    uint32_t flags;
};

static uint8_t g_set_payload_buffer[sizeof(struct luna_set_header) + (sizeof(struct luna_object_ref) * LUNA_DATA_OBJECT_CAPACITY)];

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
}

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        out[i] = in[i];
    }
}

static void *data_stage_buffer(uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    if (manifest->data_buffer_base == 0u || manifest->data_buffer_size < size) {
        return 0;
    }
    return (void *)(uintptr_t)manifest->data_buffer_base;
}

static int object_ref_is_null(struct luna_object_ref ref) {
    return ref.low == 0u && ref.high == 0u;
}

static int object_ref_equal(struct luna_object_ref left, struct luna_object_ref right) {
    return left.low == right.low && left.high == right.high;
}

static size_t fixed_text_len(const char *text, size_t limit) {
    size_t len = 0u;
    while (len < limit && text[len] != '\0') {
        len += 1u;
    }
    return len;
}

static void copy_fixed_text(char *out, size_t out_size, const char *text, size_t len) {
    size_t i = 0u;
    if (out_size == 0u) {
        return;
    }
    while (i + 1u < out_size && i < len && text[i] != '\0') {
        out[i] = text[i];
        i += 1u;
    }
    while (i < out_size) {
        out[i++] = '\0';
    }
}

static uint64_t fold_user_secret(const char *username, size_t username_len, const char *password, size_t password_len, uint64_t salt) {
    uint64_t value = 0x9E3779B97F4A7C15ull ^ salt;
    for (size_t i = 0u; i < username_len; ++i) {
        value ^= (uint8_t)username[i];
        value = ((value << 7u) | (value >> 57u)) * 0x100000001B3ull;
    }
    for (size_t i = 0u; i < password_len; ++i) {
        value ^= (uint8_t)password[i];
        value = ((value << 11u) | (value >> 53u)) * 0x100000001B3ull;
    }
    value ^= (((uint64_t)username_len) << 32u) | (uint64_t)password_len;
    return ((value << 17u) | (value >> 47u)) ^ 0xA5A55A5AC3C33C3Cull;
}

static int split_token(const char *text, size_t len, size_t *cursor, const char **out_ptr, size_t *out_len) {
    size_t start = *cursor;
    while (start < len && text[start] == ' ') {
        start += 1u;
    }
    if (start >= len) {
        return 0;
    }
    size_t end = start;
    while (end < len && text[end] != ' ') {
        end += 1u;
    }
    *cursor = end;
    *out_ptr = text + start;
    *out_len = end - start;
    return 1;
}

static int starts_with_text(const char *text, const char *prefix) {
    size_t i = 0u;
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

static uint32_t text_length(const char *text) {
    uint32_t len = 0u;
    while (text[len] != '\0') {
        len += 1u;
    }
    return len;
}

static int text_equals(const char *left, const char *right) {
    uint32_t i = 0u;
    while (left[i] != '\0' || right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i += 1u;
    }
    return 1;
}

static uint32_t gather_object_refs(struct luna_object_ref *out_refs, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    void *stage = data_stage_buffer(buffer_size);
    void *target = stage != 0 ? stage : out_refs;

    zero_bytes(target, (size_t)buffer_size);
    if (target != out_refs) {
        zero_bytes(out_refs, (size_t)buffer_size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 78u;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = g_data_gather_cid.low;
    gate->cid_high = g_data_gather_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return 0u;
    }
    if (target != out_refs && gate->result_count != 0u) {
        uint64_t copy_size = (uint64_t)gate->result_count * sizeof(struct luna_object_ref);
        if (copy_size > buffer_size) {
            copy_size = buffer_size;
        }
        copy_bytes(out_refs, target, (size_t)copy_size);
    }
    return gate->result_count;
}

static uint32_t gather_set_refs(struct luna_object_ref set_ref, struct luna_object_ref *out_refs, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    void *stage = data_stage_buffer(buffer_size);
    void *target = stage != 0 ? stage : out_refs;

    zero_bytes(target, (size_t)buffer_size);
    if (target != out_refs) {
        zero_bytes(out_refs, (size_t)buffer_size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 78u;
    gate->opcode = LUNA_DATA_GATHER_SET;
    gate->cid_low = g_data_gather_cid.low;
    gate->cid_high = g_data_gather_cid.high;
    gate->object_low = set_ref.low;
    gate->object_high = set_ref.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        return 0u;
    }
    if (target != out_refs && gate->result_count != 0u) {
        uint64_t copy_size = (uint64_t)gate->result_count * sizeof(struct luna_object_ref);
        if (copy_size > buffer_size) {
            copy_size = buffer_size;
        }
        copy_bytes(out_refs, target, (size_t)copy_size);
    }
    return gate->result_count;
}

static uint32_t seed_theme_object(struct luna_object_ref *out_ref) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 79u;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = g_data_seed_cid.low;
    gate->cid_high = g_data_seed_cid.high;
    gate->object_type = LUNA_THEME_OBJECT_TYPE;
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

static uint32_t seed_named_object(uint32_t object_type, struct luna_object_ref *out_ref) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 79u;
    gate->opcode = LUNA_DATA_SEED_OBJECT;
    gate->cid_low = g_data_seed_cid.low;
    gate->cid_high = g_data_seed_cid.high;
    gate->object_type = object_type;
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

static uint32_t draw_theme_bytes(struct luna_object_ref object_ref, char *out_text, uint64_t buffer_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    uint64_t io_size = buffer_size == 0u ? 0u : buffer_size - 1u;
    void *stage = data_stage_buffer(io_size);
    void *target = stage != 0 ? stage : out_text;

    zero_bytes(out_text, (size_t)buffer_size);
    if (target != out_text) {
        zero_bytes(target, (size_t)io_size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 80u;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = g_data_draw_cid.low;
    gate->cid_high = g_data_draw_cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = io_size;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = io_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status == LUNA_DATA_OK && target != out_text && gate->size != 0u) {
        copy_bytes(out_text, target, (size_t)gate->size);
    }
    return gate->status;
}

static uint32_t pour_theme_bytes(struct luna_object_ref object_ref, const char *text, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    void *stage = data_stage_buffer(size);
    const void *source = stage != 0 ? stage : text;

    if (stage != 0 && text != 0 && size != 0u) {
        copy_bytes(stage, text, (size_t)size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 81u;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = g_data_pour_cid.low;
    gate->cid_high = g_data_pour_cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)source;
    gate->buffer_size = size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    return gate->status;
}

static uint32_t pour_object_bytes(struct luna_object_ref object_ref, const void *bytes, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    void *stage = data_stage_buffer(size);
    const void *source = stage != 0 ? stage : bytes;

    if (stage != 0 && bytes != 0 && size != 0u) {
        copy_bytes(stage, bytes, (size_t)size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 81u;
    gate->opcode = LUNA_DATA_POUR_SPAN;
    gate->cid_low = g_data_pour_cid.low;
    gate->cid_high = g_data_pour_cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = size;
    gate->buffer_addr = (uint64_t)(uintptr_t)source;
    gate->buffer_size = size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    return gate->status;
}

static uint32_t draw_object_bytes(struct luna_object_ref object_ref, void *out_bytes, uint64_t buffer_size, uint64_t *out_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    void *stage = data_stage_buffer(buffer_size);
    void *target = stage != 0 ? stage : out_bytes;

    zero_bytes(target, (size_t)buffer_size);
    if (target != out_bytes) {
        zero_bytes(out_bytes, (size_t)buffer_size);
    }
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 82u;
    gate->opcode = LUNA_DATA_DRAW_SPAN;
    gate->cid_low = g_data_draw_cid.low;
    gate->cid_high = g_data_draw_cid.high;
    gate->object_low = object_ref.low;
    gate->object_high = object_ref.high;
    gate->offset = 0u;
    gate->size = buffer_size;
    gate->buffer_addr = (uint64_t)(uintptr_t)target;
    gate->buffer_size = buffer_size;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (out_size != 0) {
        *out_size = gate->size;
    }
    if (gate->status == LUNA_DATA_OK && target != out_bytes && gate->size != 0u) {
        copy_bytes(out_bytes, target, (size_t)gate->size);
    }
    return gate->status;
}

static void copy_single_line(char *out, uint32_t out_size, const char *src) {
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

static void reset_active_user_projection(void) {
    g_user_logged_in = 0u;
    g_user_session_low = 0u;
    g_user_session_high = 0u;
    g_user_object.low = 0u;
    g_user_object.high = 0u;
    g_user_secret_object.low = 0u;
    g_user_secret_object.high = 0u;
    g_user_home_object.low = 0u;
    g_user_home_object.high = 0u;
    g_user_profile_object.low = 0u;
    g_user_profile_object.high = 0u;
    g_user_documents_set.low = 0u;
    g_user_documents_set.high = 0u;
    g_user_desktop_set.low = 0u;
    g_user_desktop_set.high = 0u;
    g_user_downloads_set.low = 0u;
    g_user_downloads_set.high = 0u;
    copy_single_line(g_username, sizeof(g_username), "guest");
}

static int load_host_record(struct luna_host_record *out_record) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = gather_object_refs(refs, sizeof(refs));
    uint64_t size = 0u;

    zero_bytes(out_record, sizeof(*out_record));
    g_host_object.low = 0u;
    g_host_object.high = 0u;
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        if (draw_object_bytes(refs[i], out_record, sizeof(*out_record), &size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(*out_record) ||
            out_record->magic != LUNA_HOST_RECORD_MAGIC ||
            out_record->version != LUNA_USER_RECORD_VERSION) {
            continue;
        }
        g_host_object = refs[i];
        copy_fixed_text(g_hostname, sizeof(g_hostname), out_record->hostname, sizeof(out_record->hostname));
        return 1;
    }
    copy_single_line(g_hostname, sizeof(g_hostname), "luna");
    return 0;
}

static int load_user_record_by_name(const char *username, size_t username_len, struct luna_user_record *out_record, struct luna_object_ref *out_ref) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = gather_object_refs(refs, sizeof(refs));
    uint64_t size = 0u;

    zero_bytes(out_record, sizeof(*out_record));
    if (out_ref != 0) {
        out_ref->low = 0u;
        out_ref->high = 0u;
    }
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        size_t candidate_len;
        if (draw_object_bytes(refs[i], out_record, sizeof(*out_record), &size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(*out_record) ||
            out_record->magic != LUNA_USER_RECORD_MAGIC ||
            out_record->version != LUNA_USER_RECORD_VERSION) {
            continue;
        }
        candidate_len = fixed_text_len(out_record->username, sizeof(out_record->username));
        if (candidate_len == username_len && chars_equal(out_record->username, username, username_len)) {
            if (out_ref != 0) {
                *out_ref = refs[i];
            }
            return 1;
        }
    }
    return 0;
}

static int load_first_user_record(struct luna_user_record *out_record, struct luna_object_ref *out_ref) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = gather_object_refs(refs, sizeof(refs));
    uint64_t size = 0u;

    zero_bytes(out_record, sizeof(*out_record));
    if (out_ref != 0) {
        out_ref->low = 0u;
        out_ref->high = 0u;
    }
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        if (draw_object_bytes(refs[i], out_record, sizeof(*out_record), &size) != LUNA_DATA_OK) {
            continue;
        }
        if (size < sizeof(*out_record) ||
            out_record->magic != LUNA_USER_RECORD_MAGIC ||
            out_record->version != LUNA_USER_RECORD_VERSION) {
            continue;
        }
        if (out_ref != 0) {
            *out_ref = refs[i];
        }
        return 1;
    }
    return 0;
}

static int load_user_home_record(struct luna_object_ref object_ref, struct luna_user_home_root_record *out_record) {
    uint64_t size = 0u;
    zero_bytes(out_record, sizeof(*out_record));
    if (object_ref.low == 0u || object_ref.high == 0u) {
        return 0;
    }
    if (draw_object_bytes(object_ref, out_record, sizeof(*out_record), &size) != LUNA_DATA_OK ||
        size < sizeof(*out_record) ||
        out_record->magic != LUNA_USER_HOME_MAGIC ||
        out_record->version != LUNA_USER_RECORD_VERSION) {
        return 0;
    }
    return 1;
}

static int load_user_profile_record(struct luna_object_ref object_ref, struct luna_user_profile_record *out_record) {
    uint64_t size = 0u;
    zero_bytes(out_record, sizeof(*out_record));
    if (object_ref.low == 0u || object_ref.high == 0u) {
        return 0;
    }
    if (draw_object_bytes(object_ref, out_record, sizeof(*out_record), &size) != LUNA_DATA_OK ||
        size < sizeof(*out_record) ||
        out_record->magic != LUNA_USER_PROFILE_MAGIC ||
        out_record->version != LUNA_USER_RECORD_VERSION) {
        return 0;
    }
    return 1;
}

static int persist_user_profile_record(const struct luna_user_profile_record *profile) {
    return g_user_profile_object.low != 0u &&
        g_user_profile_object.high != 0u &&
        pour_object_bytes(g_user_profile_object, profile, sizeof(*profile)) == LUNA_DATA_OK;
}

static int auth_login_secret(struct luna_object_ref secret_object, const char *username, size_t username_len, const char *password, size_t password_len) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_user_auth_request request;

    zero_bytes(&request, sizeof(request));
    request.secret_low = secret_object.low;
    request.secret_high = secret_object.high;
    copy_fixed_text(request.username, sizeof(request.username), username, username_len);
    copy_fixed_text(request.password, sizeof(request.password), password, password_len);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 51u;
    gate->opcode = LUNA_GATE_AUTH_LOGIN;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = LUNA_CAP_USER_AUTH;
    gate->cid_low = g_user_auth_cid.low;
    gate->cid_high = g_user_auth_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    return gate->status == LUNA_GATE_OK;
}

static int security_derive_user_secret(struct luna_object_ref user_ref, uint64_t salt, const char *username, size_t username_len, const char *password, size_t password_len, uint64_t *out_fold) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_crypto_secret_request request;

    if (out_fold == 0) {
        return 0;
    }
    zero_bytes(&request, sizeof(request));
    request.key_class = LUNA_CRYPTO_KEY_USER_AUTH;
    request.flags = LUNA_USER_SECRET_FLAG_SECURITY_OWNED | LUNA_USER_SECRET_FLAG_INSTALL_BOUND;
    request.subject_low = user_ref.low;
    request.subject_high = user_ref.high;
    request.salt = salt;
    copy_fixed_text(request.username, sizeof(request.username), username, username_len);
    copy_fixed_text(request.secret, sizeof(request.secret), password, password_len);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 52u;
    gate->opcode = LUNA_GATE_CRYPTO_SECRET;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = LUNA_CAP_USER_AUTH;
    gate->cid_low = g_user_auth_cid.low;
    gate->cid_high = g_user_auth_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return 0;
    }
    *out_fold = request.output_low;
    return 1;
}

static int security_open_user_session(struct luna_object_ref user_ref, struct luna_object_ref secret_ref) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_auth_session_request request;

    zero_bytes(&request, sizeof(request));
    request.user_low = user_ref.low;
    request.user_high = user_ref.high;
    request.secret_low = secret_ref.low;
    request.secret_high = secret_ref.high;
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 53u;
    gate->opcode = LUNA_GATE_AUTH_SESSION_OPEN;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = LUNA_CAP_USER_AUTH;
    gate->cid_low = g_user_auth_cid.low;
    gate->cid_high = g_user_auth_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        return 0;
    }
    g_user_session_low = request.session_low;
    g_user_session_high = request.session_high;
    return 1;
}

static void security_close_user_session(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_auth_session_request request;

    if (g_user_session_low == 0u && g_user_session_high == 0u) {
        return;
    }
    zero_bytes(&request, sizeof(request));
    request.session_low = g_user_session_low;
    request.session_high = g_user_session_high;
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 54u;
    gate->opcode = LUNA_GATE_AUTH_SESSION_CLOSE;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = LUNA_CAP_USER_AUTH;
    gate->buffer_addr = (uint64_t)(uintptr_t)&request;
    gate->buffer_size = sizeof(request);
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
}

static void load_profile_state(void) {
    struct luna_user_profile_record profile;

    if (!load_user_profile_record(g_user_profile_object, &profile)) {
        return;
    }
    if (profile.theme_object.low != 0u || profile.theme_object.high != 0u) {
        g_theme_object = profile.theme_object;
    }
    if (profile.note_object.low != 0u || profile.note_object.high != 0u) {
        g_note_object = profile.note_object;
    }
    if (profile.files_view_object.low != 0u || profile.files_view_object.high != 0u) {
        g_files_view_object = profile.files_view_object;
    }
    g_theme_variant = profile.theme_variant & 1u;
}

static void sync_profile_projection(void) {
    struct luna_user_profile_record profile;

    if (!load_user_profile_record(g_user_profile_object, &profile)) {
        return;
    }
    profile.theme_variant = g_theme_variant & 1u;
    profile.theme_object = g_theme_object;
    profile.note_object = g_note_object;
    profile.files_view_object = g_files_view_object;
    (void)persist_user_profile_record(&profile);
}

static int activate_user_session(const struct luna_user_record *user_record, struct luna_object_ref user_ref) {
    struct luna_user_home_root_record home;
    struct luna_user_profile_record profile;

    reset_active_user_projection();
    g_user_object = user_ref;
    g_user_secret_object = user_record->secret_object;
    g_user_home_object = user_record->home_root;
    g_user_profile_object = user_record->profile_object;
    copy_fixed_text(g_username, sizeof(g_username), user_record->username, sizeof(user_record->username));
    if (!load_user_home_record(g_user_home_object, &home)) {
        reset_active_user_projection();
        return 0;
    }
    g_user_documents_set = home.documents_set;
    g_user_desktop_set = home.desktop_set;
    g_user_downloads_set = home.downloads_set;
    if (load_user_profile_record(g_user_profile_object, &profile)) {
        g_theme_variant = profile.theme_variant & 1u;
        g_note_object = profile.note_object;
        g_theme_object = profile.theme_object;
        g_files_view_object = profile.files_view_object;
    }
    g_user_logged_in = 1u;
    g_user_setup_required = 0u;
    return 1;
}

static void refresh_user_system_state(void) {
    struct luna_host_record host;
    struct luna_user_record user;
    struct luna_object_ref user_ref;

    g_user_setup_required = load_host_record(&host) ? 0u : 1u;
    if (!load_first_user_record(&user, &user_ref)) {
        g_user_setup_required = 1u;
    }
    if (g_user_setup_required != 0u) {
        reset_active_user_projection();
    }
}

static void parse_note_body(const char *blob, char *out_body, uint32_t out_size) {
    static const char body_marker[] = "body: ";
    uint32_t i = 0u;

    if (out_size == 0u) {
        return;
    }
    out_body[0] = '\0';
    while (blob[i] != '\0') {
        if (starts_with_text(blob + i, body_marker)) {
            copy_single_line(out_body, out_size, blob + i + 6u);
            return;
        }
        i += 1u;
    }
}

static void load_theme_state(void) {
    static const char theme_marker[] = "LUNA-THEME\r\nvariant: ";
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    char theme_blob[48];
    uint32_t count = gather_object_refs(refs, sizeof(refs));

    if (g_theme_object.low != 0u || g_theme_object.high != 0u) {
        if (draw_theme_bytes(g_theme_object, theme_blob, sizeof(theme_blob)) == LUNA_DATA_OK &&
            starts_with_text(theme_blob, theme_marker)) {
            g_theme_variant = theme_blob[21] == '1' ? 1u : 0u;
            return;
        }
    }
    g_theme_object.low = 0u;
    g_theme_object.high = 0u;
    g_theme_variant = 0u;
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        if (draw_theme_bytes(refs[i], theme_blob, sizeof(theme_blob)) != LUNA_DATA_OK) {
            continue;
        }
        if (!starts_with_text(theme_blob, theme_marker)) {
            continue;
        }
        g_theme_object = refs[i];
        if (theme_blob[21] == '1') {
            g_theme_variant = 1u;
        } else {
            g_theme_variant = 0u;
        }
        return;
    }
}

static void persist_theme_state(void) {
    char theme_blob[32];

    zero_bytes(theme_blob, sizeof(theme_blob));
    theme_blob[0] = 'L';
    theme_blob[1] = 'U';
    theme_blob[2] = 'N';
    theme_blob[3] = 'A';
    theme_blob[4] = '-';
    theme_blob[5] = 'T';
    theme_blob[6] = 'H';
    theme_blob[7] = 'E';
    theme_blob[8] = 'M';
    theme_blob[9] = 'E';
    theme_blob[10] = '\r';
    theme_blob[11] = '\n';
    theme_blob[12] = 'v';
    theme_blob[13] = 'a';
    theme_blob[14] = 'r';
    theme_blob[15] = 'i';
    theme_blob[16] = 'a';
    theme_blob[17] = 'n';
    theme_blob[18] = 't';
    theme_blob[19] = ':';
    theme_blob[20] = ' ';
    theme_blob[21] = (char)('0' + (g_theme_variant & 1u));
    theme_blob[22] = '\r';
    theme_blob[23] = '\n';

    if (g_theme_object.low == 0u || g_theme_object.high == 0u) {
        if (seed_theme_object(&g_theme_object) != LUNA_DATA_OK) {
            return;
        }
    }
    (void)pour_theme_bytes(g_theme_object, theme_blob, 24u);
    sync_profile_projection();
}

static void load_note_state(void) {
    static const char note_marker[] = "LUNA-NOTE\r\n";
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    char note_blob[160];
    uint32_t count = gather_object_refs(refs, sizeof(refs));

    copy_single_line(g_note_body, sizeof(g_note_body), "Luna note seed");
    if (g_note_object.low != 0u || g_note_object.high != 0u) {
        if (draw_theme_bytes(g_note_object, note_blob, sizeof(note_blob)) == LUNA_DATA_OK &&
            starts_with_text(note_blob, note_marker)) {
            parse_note_body(note_blob, g_note_body, sizeof(g_note_body));
            if (g_note_body[0] == '\0') {
                copy_single_line(g_note_body, sizeof(g_note_body), "Luna note seed");
            }
            return;
        }
    }
    g_note_object.low = 0u;
    g_note_object.high = 0u;
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        if (draw_theme_bytes(refs[i], note_blob, sizeof(note_blob)) != LUNA_DATA_OK) {
            continue;
        }
        if (!starts_with_text(note_blob, note_marker)) {
            continue;
        }
        g_note_object = refs[i];
        parse_note_body(note_blob, g_note_body, sizeof(g_note_body));
        if (g_note_body[0] == '\0') {
            copy_single_line(g_note_body, sizeof(g_note_body), "Luna note seed");
        }
        return;
    }
}

static void persist_note_state(void) {
    static const char prefix[] = "LUNA-NOTE\r\ntitle: workspace\r\nbody: ";
    char note_blob[160];
    uint32_t cursor = 0u;

    zero_bytes(note_blob, sizeof(note_blob));
    for (uint32_t i = 0u; prefix[i] != '\0' && cursor + 1u < sizeof(note_blob); ++i) {
        note_blob[cursor++] = prefix[i];
    }
    for (uint32_t i = 0u; g_note_body[i] != '\0' && cursor + 1u < sizeof(note_blob); ++i) {
        note_blob[cursor++] = g_note_body[i];
    }
    if (cursor + 2u < sizeof(note_blob)) {
        note_blob[cursor++] = '\r';
        note_blob[cursor++] = '\n';
    }

    if (g_note_object.low == 0u || g_note_object.high == 0u) {
        if (seed_named_object(LUNA_NOTE_OBJECT_TYPE, &g_note_object) != LUNA_DATA_OK) {
            return;
        }
    }
    (void)pour_theme_bytes(g_note_object, note_blob, cursor);
    sync_profile_projection();
}

static uint32_t parse_files_selection(const char *blob) {
    static const char marker[] = "selected: ";
    uint32_t i = 0u;

    while (blob[i] != '\0') {
        if (starts_with_text(blob + i, marker)) {
            uint32_t value = 0u;
            i += 10u;
            while (blob[i] >= '0' && blob[i] <= '9') {
                value = (value * 10u) + (uint32_t)(blob[i] - '0');
                i += 1u;
            }
            return value;
        }
        i += 1u;
    }
    return 0u;
}

static uint64_t parse_hex_u64(const char *text) {
    uint64_t value = 0u;
    uint32_t i = 0u;

    while (text[i] != '\0') {
        char ch = text[i];
        uint32_t digit = 16u;
        if (ch >= '0' && ch <= '9') {
            digit = (uint32_t)(ch - '0');
        } else if (ch >= 'A' && ch <= 'F') {
            digit = 10u + (uint32_t)(ch - 'A');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = 10u + (uint32_t)(ch - 'a');
        } else {
            break;
        }
        value = (value << 4) | digit;
        i += 1u;
    }
    return value;
}

static uint64_t parse_u64_field(const char *blob, const char *marker) {
    uint32_t i = 0u;

    while (blob[i] != '\0') {
        if (starts_with_text(blob + i, marker)) {
            i += text_length(marker);
            return parse_hex_u64(blob + i);
        }
        i += 1u;
    }
    return 0u;
}

static uint32_t persist_documents_set_members(const struct luna_object_ref *refs, uint32_t count) {
    struct luna_set_header *header = (struct luna_set_header *)g_set_payload_buffer;

    zero_bytes(g_set_payload_buffer, sizeof(g_set_payload_buffer));
    header->magic = LUNA_SET_MAGIC;
    header->version = LUNA_SET_VERSION;
    header->member_count = count;
    header->flags = 0u;
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        ((struct luna_object_ref *)(g_set_payload_buffer + sizeof(struct luna_set_header)))[i] = refs[i];
    }
    return pour_object_bytes(
        g_user_documents_set,
        g_set_payload_buffer,
        sizeof(struct luna_set_header) + ((uint64_t)count * sizeof(struct luna_object_ref))
    );
}

static uint32_t ensure_user_documents_set(void) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint32_t count = 0u;

    if (g_user_documents_set.low == 0u || g_user_documents_set.high == 0u) {
        if (seed_named_object(LUNA_SET_OBJECT_TYPE, &g_user_documents_set) != LUNA_DATA_OK) {
            return LUNA_DATA_ERR_NO_SPACE;
        }
    }

    if (!object_ref_is_null(g_note_object)) {
        refs[count++] = g_note_object;
    }
    if (!object_ref_is_null(g_theme_object)) {
        uint32_t duplicate = 0u;
        for (uint32_t i = 0u; i < count; ++i) {
            if (object_ref_equal(refs[i], g_theme_object)) {
                duplicate = 1u;
                break;
            }
        }
        if (duplicate == 0u) {
            refs[count++] = g_theme_object;
        }
    }
    return persist_documents_set_members(refs, count);
}

static void load_files_view_state(void) {
    static const char files_marker[] = "LUNA-FILES\r\n";
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    char files_blob[160];
    uint32_t count = gather_object_refs(refs, sizeof(refs));
    struct luna_object_ref best_view = {0u, 0u};
    struct luna_object_ref best_documents = {0u, 0u};
    uint32_t best_selection = 0u;
    uint32_t best_member_count = 0u;

    if (g_files_view_object.low != 0u || g_files_view_object.high != 0u) {
        if (draw_theme_bytes(g_files_view_object, files_blob, sizeof(files_blob)) == LUNA_DATA_OK &&
            starts_with_text(files_blob, files_marker)) {
            g_files_selection = parse_files_selection(files_blob);
            if (g_user_documents_set.low == 0u && g_user_documents_set.high == 0u) {
                g_user_documents_set.low = parse_u64_field(files_blob, "documents-low: ");
                g_user_documents_set.high = parse_u64_field(files_blob, "documents-high: ");
            }
            return;
        }
    }
    g_files_view_object.low = 0u;
    g_files_view_object.high = 0u;
    if (g_user_logged_in == 0u) {
        g_user_documents_set.low = 0u;
        g_user_documents_set.high = 0u;
    }
    g_files_selection = 0u;
    for (uint32_t i = 0u; i < count && i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        if (draw_theme_bytes(refs[i], files_blob, sizeof(files_blob)) != LUNA_DATA_OK) {
            continue;
        }
        if (!starts_with_text(files_blob, files_marker)) {
            continue;
        }
        {
            struct luna_object_ref candidate_documents = {
                parse_u64_field(files_blob, "documents-low: "),
                parse_u64_field(files_blob, "documents-high: ")
            };
            uint32_t candidate_valid = (candidate_documents.low != 0u && candidate_documents.high != 0u) ? 1u : 0u;
            uint32_t best_valid = (best_documents.low != 0u && best_documents.high != 0u) ? 1u : 0u;
            uint32_t candidate_members = 0u;
            if (candidate_valid != 0u) {
                candidate_members = gather_set_refs(candidate_documents, g_files_candidate_refs, sizeof(g_files_candidate_refs));
                if (candidate_members != 0u) {
                    uint32_t valid_members = 0u;
                    char candidate_blob[96];
                    for (uint32_t member = 0u; member < candidate_members; ++member) {
                        if (draw_theme_bytes(g_files_candidate_refs[member], candidate_blob, sizeof(candidate_blob)) != LUNA_DATA_OK) {
                            continue;
                        }
                        if (!starts_with_text(candidate_blob, g_note_marker) &&
                            !starts_with_text(candidate_blob, g_theme_marker)) {
                            continue;
                        }
                        valid_members += 1u;
                    }
                    candidate_members = valid_members;
                }
            }
            if (best_view.low == 0u ||
                (candidate_members != 0u && best_member_count == 0u) ||
                (candidate_valid != 0u && best_valid == 0u) ||
                (candidate_members == best_member_count &&
                 candidate_valid == best_valid &&
                 refs[i].high > best_view.high)) {
                best_view = refs[i];
                best_selection = parse_files_selection(files_blob);
                best_documents = candidate_documents;
                best_member_count = candidate_members;
            }
        }
    }
    if (best_view.low != 0u && best_view.high != 0u) {
        g_files_view_object = best_view;
        g_files_selection = best_selection;
        g_user_documents_set = best_documents;
    }
}

static void persist_files_view_state(void) {
    static const char prefix[] = "LUNA-FILES\r\nselected: ";
    static const char documents_low_prefix[] = "\r\ndocuments-low: ";
    static const char documents_high_prefix[] = "\r\ndocuments-high: ";
    char files_blob[160];
    char digits[12];
    char documents_low[17];
    char documents_high[17];
    uint32_t cursor = 0u;

    zero_bytes(files_blob, sizeof(files_blob));
    append_u32_decimal(digits, g_files_selection);
    append_u64_hex_fixed(documents_low, g_user_documents_set.low, 16u);
    append_u64_hex_fixed(documents_high, g_user_documents_set.high, 16u);
    for (uint32_t i = 0u; prefix[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = prefix[i];
    }
    for (uint32_t i = 0u; digits[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = digits[i];
    }
    for (uint32_t i = 0u; documents_low_prefix[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = documents_low_prefix[i];
    }
    for (uint32_t i = 0u; documents_low[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = documents_low[i];
    }
    for (uint32_t i = 0u; documents_high_prefix[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = documents_high_prefix[i];
    }
    for (uint32_t i = 0u; documents_high[i] != '\0' && cursor + 1u < sizeof(files_blob); ++i) {
        files_blob[cursor++] = documents_high[i];
    }
    if (cursor + 2u < sizeof(files_blob)) {
        files_blob[cursor++] = '\r';
        files_blob[cursor++] = '\n';
    }

    if (g_files_view_object.low == 0u || g_files_view_object.high == 0u) {
        if (seed_named_object(LUNA_FILES_VIEW_OBJECT_TYPE, &g_files_view_object) != LUNA_DATA_OK) {
            return;
        }
    }
    (void)pour_theme_bytes(g_files_view_object, files_blob, cursor);
    sync_profile_projection();
}

static void sync_desktop_shell_state(void) {
    g_desktop_shell.version = 1u;
    if (g_theme_variant == 0u) {
        g_desktop_shell.desktop_attr = 0x30u;
        g_desktop_shell.chrome_attr = 0x70u;
        g_desktop_shell.panel_attr = 0x3Fu;
        g_desktop_shell.accent_attr = 0x1Fu;
    } else {
        g_desktop_shell.desktop_attr = 0x40u;
        g_desktop_shell.chrome_attr = 0x4Fu;
        g_desktop_shell.panel_attr = 0x47u;
        g_desktop_shell.accent_attr = 0x6Fu;
    }
    g_desktop_shell.last_key &= 0xFFu;
    g_desktop_shell.last_pointer_buttons &= 0xFFu;
    g_desktop_shell.entry_count = g_package_count;
    if (g_desktop_shell.entry_count > LUNA_DESKTOP_ENTRY_CAPACITY) {
        g_desktop_shell.entry_count = LUNA_DESKTOP_ENTRY_CAPACITY;
    }
    for (uint32_t i = 0u; i < LUNA_DESKTOP_ENTRY_CAPACITY; ++i) {
        zero_bytes(&g_desktop_shell.entries[i], sizeof(g_desktop_shell.entries[i]));
    }
    for (uint32_t i = 0u; i < g_desktop_shell.entry_count; ++i) {
        const char *label = package_label_by_index(i);
        zero_bytes(g_desktop_shell.entries[i].name, sizeof(g_desktop_shell.entries[i].name));
        zero_bytes(g_desktop_shell.entries[i].label, sizeof(g_desktop_shell.entries[i].label));
        for (uint32_t j = 0u; j < 15u && g_package_catalog[i].name[j] != '\0'; ++j) {
            g_desktop_shell.entries[i].name[j] = g_package_catalog[i].name[j];
        }
        if (label != 0) {
            for (uint32_t j = 0u; j < 15u && label[j] != '\0'; ++j) {
                g_desktop_shell.entries[i].label[j] = label[j];
            }
        }
        g_desktop_shell.entries[i].icon_x = g_package_catalog[i].icon_x;
        g_desktop_shell.entries[i].icon_y = g_package_catalog[i].icon_y;
        g_desktop_shell.entries[i].flags = g_package_catalog[i].flags;
        g_desktop_shell.entries[i].preferred_x = g_package_catalog[i].preferred_x;
        g_desktop_shell.entries[i].preferred_y = g_package_catalog[i].preferred_y;
        g_desktop_shell.entries[i].preferred_width = g_package_catalog[i].preferred_width;
        g_desktop_shell.entries[i].preferred_height = g_package_catalog[i].preferred_height;
        g_desktop_shell.entries[i].window_role = g_package_catalog[i].window_role;
        g_desktop_shell.entries[i].reserved0 = 0u;
    }
}

static void cycle_theme(void) {
    g_theme_variant = (g_theme_variant + 1u) & 1u;
    persist_theme_state();
    sync_desktop_shell_state();
    (void)graphics_render_desktop_state(0x10u, g_launcher_index, g_pointer_x, g_pointer_y, 0, 0, 0);
}

static uint32_t refresh_package_catalog(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_package_gate *gate =
        (volatile struct luna_package_gate *)(uintptr_t)manifest->package_gate_base;

    zero_bytes(g_package_catalog, sizeof(g_package_catalog));
    zero_bytes((void *)(uintptr_t)manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 77;
    gate->opcode = LUNA_PACKAGE_LIST;
    gate->cid_low = g_package_list_cid.low;
    gate->cid_high = g_package_list_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)g_package_catalog;
    gate->buffer_size = sizeof(g_package_catalog);
    ((package_gate_fn_t)(uintptr_t)manifest->package_gate_entry)(
        (struct luna_package_gate *)(uintptr_t)manifest->package_gate_base
    );
    if (gate->status != LUNA_PACKAGE_OK) {
        g_package_count = 0u;
        return gate->status;
    }
    g_package_count = gate->result_count;
    if (g_package_count > LUNA_PACKAGE_CAPACITY) {
        g_package_count = LUNA_PACKAGE_CAPACITY;
    }
    return LUNA_PACKAGE_OK;
}

static uint32_t data_query_rows(uint32_t caller_space, struct luna_query_request *request, struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    if (capacity > LUNA_DATA_OBJECT_CAPACITY) {
        capacity = LUNA_DATA_OBJECT_CAPACITY;
    }
    zero_bytes(&g_lasql_data_payload, sizeof(g_lasql_data_payload));
    copy_bytes(&g_lasql_data_payload.request, request, sizeof(g_lasql_data_payload.request));
    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 88;
    gate->opcode = LUNA_DATA_QUERY;
    gate->cid_low = g_data_query_cid.low;
    gate->cid_high = g_data_query_cid.high;
    gate->writer_space = caller_space;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_lasql_data_payload;
    gate->buffer_size = sizeof(g_lasql_data_payload.request) + ((uint64_t)capacity * sizeof(struct luna_query_row));
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        if (out_count != 0) {
            *out_count = 0u;
        }
        return gate->status;
    }
    if (gate->result_count > capacity) {
        gate->result_count = capacity;
    }
    copy_bytes(out_rows, g_lasql_data_payload.rows, (size_t)gate->result_count * sizeof(struct luna_query_row));
    if (out_count != 0) {
        *out_count = gate->result_count;
    }
    return LUNA_DATA_OK;
}

static int file_query_row_visible(const struct luna_query_row *row) {
    if (row == 0) {
        return 0;
    }
    return row->object_type == LUNA_NOTE_OBJECT_TYPE ||
           row->object_type == LUNA_THEME_OBJECT_TYPE;
}

static uint32_t query_visible_file_rows(struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count) {
    struct luna_query_request request;
    uint32_t count = 0u;
    uint32_t visible = 0u;

    if (out_count != 0) {
        *out_count = 0u;
    }
    if (out_rows == 0 || capacity == 0u) {
        return LUNA_DATA_ERR_RANGE;
    }
    if (g_user_documents_set.low == 0u || g_user_documents_set.high == 0u) {
        if (ensure_user_documents_set() != LUNA_DATA_OK) {
            return LUNA_DATA_ERR_NOT_FOUND;
        }
    }

    zero_bytes(&request, sizeof(request));
    request.target = LUNA_QUERY_TARGET_USER_FILES;
    request.filter_flags = LUNA_QUERY_FILTER_NAMESPACE | LUNA_QUERY_FILTER_OWNER;
    request.projection_flags =
        LUNA_QUERY_PROJECT_NAME |
        LUNA_QUERY_PROJECT_REF |
        LUNA_QUERY_PROJECT_TYPE |
        LUNA_QUERY_PROJECT_STATE |
        LUNA_QUERY_PROJECT_OWNER |
        LUNA_QUERY_PROJECT_CREATED |
        LUNA_QUERY_PROJECT_UPDATED;
    request.sort_mode = LUNA_QUERY_SORT_UPDATED_DESC;
    request.limit = capacity;
    request.namespace_id = LUNA_QUERY_NAMESPACE_USER;
    request.owner_low = g_user_object.low;
    request.owner_high = g_user_object.high;
    request.scope_low = g_user_documents_set.low;
    request.scope_high = g_user_documents_set.high;
    if (data_query_rows(LUNA_SPACE_USER, &request, out_rows, capacity, &count) != LUNA_DATA_OK) {
        return LUNA_DATA_ERR_NOT_FOUND;
    }

    for (uint32_t i = 0u; i < count; ++i) {
        if (!file_query_row_visible(&out_rows[i])) {
            continue;
        }
        if (visible != i) {
            out_rows[visible] = out_rows[i];
        }
        visible += 1u;
    }
    if (out_count != 0) {
        *out_count = visible;
    }
    return LUNA_DATA_OK;
}

static int query_selected_file_row(struct luna_query_row *out_row, uint32_t *out_count) {
    uint32_t count = 0u;
    if (query_visible_file_rows(g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count) != LUNA_DATA_OK) {
        if (out_count != 0) {
            *out_count = 0u;
        }
        return 0;
    }
    if (out_count != 0) {
        *out_count = count;
    }
    if (out_row == 0 || count == 0u || g_files_selection >= count) {
        return 0;
    }
    *out_row = g_lasql_data_payload.rows[g_files_selection];
    return 1;
}

static uint32_t observe_query_rows(uint32_t caller_space, struct luna_query_request *request, struct luna_query_row *out_rows, uint32_t capacity, uint32_t *out_count) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_observe_gate *gate =
        (volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base;

    if (capacity > LUNA_OBSERVE_LOG_CAPACITY) {
        capacity = LUNA_OBSERVE_LOG_CAPACITY;
    }
    zero_bytes(&g_lasql_observe_payload, sizeof(g_lasql_observe_payload));
    copy_bytes(&g_lasql_observe_payload.request, request, sizeof(g_lasql_observe_payload.request));
    zero_bytes((void *)(uintptr_t)manifest->observe_gate_base, sizeof(struct luna_observe_gate));
    gate->sequence = 89;
    gate->opcode = LUNA_OBSERVE_QUERY;
    gate->space_id = caller_space;
    gate->cid_low = g_observe_read_cid.low;
    gate->cid_high = g_observe_read_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_lasql_observe_payload;
    gate->buffer_size = sizeof(g_lasql_observe_payload.request) + ((uint64_t)capacity * sizeof(struct luna_query_row));
    ((observe_gate_fn_t)(uintptr_t)manifest->observe_gate_entry)(
        (struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base
    );
    if (gate->status != LUNA_OBSERVE_OK) {
        if (out_count != 0) {
            *out_count = 0u;
        }
        return gate->status;
    }
    if (gate->result_count > capacity) {
        gate->result_count = capacity;
    }
    copy_bytes(out_rows, g_lasql_observe_payload.rows, (size_t)gate->result_count * sizeof(struct luna_query_row));
    if (out_count != 0) {
        *out_count = gate->result_count;
    }
    return LUNA_OBSERVE_OK;
}

static void print_query_rows(struct luna_query_row *rows, uint32_t count) {
    char digits[24];
    for (uint32_t i = 0u; i < count; ++i) {
        device_write("row ");
        append_u32_decimal(digits, i + 1u);
        device_write(digits);
        device_write(": ");
        if (rows[i].name[0] != '\0') {
            device_write(rows[i].name);
        } else if (rows[i].message[0] != '\0') {
            device_write(rows[i].message);
        } else {
            device_write("object");
        }
        device_write(" type=");
        append_u32_hex_fixed(digits, rows[i].object_type, 8u);
        device_write(digits);
        device_write(" state=");
        append_u32_decimal(digits, rows[i].state);
        device_write(digits);
        if (rows[i].version != 0u) {
            device_write(" version=");
            append_u64_decimal(digits, rows[i].version);
            device_write(digits);
        }
        if (rows[i].object_low != 0u || rows[i].object_high != 0u) {
            char hex[17];
            device_write(" ref=0x");
            append_u64_hex_fixed(hex, rows[i].object_low, 16u);
            device_write(hex);
        }
        device_write(g_msg_newline);
    }
}

static const char *files_row_name(const struct luna_query_row *row) {
    if (row != 0 && row->name[0] != '\0') {
        return row->name;
    }
    if (row != 0 && row->label[0] != '\0') {
        return row->label;
    }
    return "object";
}

static const char *files_row_kind(uint32_t object_type) {
    if (object_type == LUNA_NOTE_OBJECT_TYPE) {
        return "note";
    }
    if (object_type == LUNA_THEME_OBJECT_TYPE) {
        return "theme";
    }
    if (object_type == LUNA_FILES_VIEW_OBJECT_TYPE) {
        return "files-view";
    }
    if (object_type == LUNA_SET_OBJECT_TYPE) {
        return "set";
    }
    if (object_type == LUNA_DATA_OBJECT_TYPE_PACKAGE_INSTALL) {
        return "package";
    }
    if (object_type == LUNA_DATA_OBJECT_TYPE_LSON_RECORD) {
        return "lson";
    }
    return "object";
}

static const char *files_row_state(uint32_t state) {
    if (state == 1u) {
        return "normal";
    }
    if (state == 2u) {
        return "readonly";
    }
    if (state == 3u) {
        return "recovery";
    }
    if (state == 4u) {
        return "missing";
    }
    if (state == 5u) {
        return "denied";
    }
    return "unknown";
}

static void append_text_limited(char *out, uint32_t out_size, uint32_t *cursor, const char *text) {
    if (out == 0 || cursor == 0 || text == 0 || out_size == 0u) {
        return;
    }
    while (*cursor + 1u < out_size && text[0] != '\0') {
        out[*cursor] = text[0];
        *cursor += 1u;
        text += 1;
    }
    out[*cursor] = '\0';
}

static void build_files_window_row(char *out, uint32_t out_size, const struct luna_query_row *row, uint32_t index, uint32_t selected) {
    char digits[12];
    char hex[17];
    uint32_t cursor = 0u;

    if (out == 0 || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    append_text_limited(out, out_size, &cursor, selected ? "> " : "  ");
    append_u32_decimal(digits, index + 1u);
    append_text_limited(out, out_size, &cursor, digits);
    append_text_limited(out, out_size, &cursor, " ");
    append_text_limited(out, out_size, &cursor, files_row_kind(row != 0 ? row->object_type : 0u));
    append_text_limited(out, out_size, &cursor, " ");
    append_u64_hex_fixed(hex, row != 0 ? row->object_low : 0u, 16u);
    append_text_limited(out, out_size, &cursor, hex);
}

static void build_files_window_meta(char *out, uint32_t out_size, const struct luna_query_row *row) {
    uint32_t cursor = 0u;

    if (out == 0 || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    append_text_limited(out, out_size, &cursor, "meta ");
    append_text_limited(out, out_size, &cursor, files_row_name(row));
    append_text_limited(out, out_size, &cursor, " ");
    append_text_limited(out, out_size, &cursor, files_row_state(row != 0 ? row->state : 4u));
}

static void build_files_window_ref(char *out, uint32_t out_size, const struct luna_query_row *row) {
    char hex[17];
    uint32_t cursor = 0u;

    if (out == 0 || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    append_text_limited(out, out_size, &cursor, "ref ");
    append_u64_hex_fixed(hex, row != 0 ? row->object_low : 0u, 8u);
    append_text_limited(out, out_size, &cursor, hex);
}

static void print_files_lafs_rows(struct luna_query_row *rows, uint32_t count) {
    char digits[24];
    char hex[17];

    device_write("files.source=lafs target=user-documents query=lasql owner=DATA consumer=USER\r\n");
    for (uint32_t i = 0u; i < count; ++i) {
        device_write("files.entry ");
        append_u32_decimal(digits, i + 1u);
        device_write(digits);
        device_write(" name=");
        device_write(files_row_name(&rows[i]));
        device_write(" kind=");
        device_write(files_row_kind(rows[i].object_type));
        device_write(" type=");
        append_u32_hex_fixed(digits, rows[i].object_type, 8u);
        device_write(digits);
        device_write(" state=");
        device_write(files_row_state(rows[i].state));
        device_write(" source=lafs query=lasql ref=0x");
        append_u64_hex_fixed(hex, rows[i].object_low, 16u);
        device_write(hex);
        device_write(g_msg_newline);
    }
}

static void print_files_selected_metadata(struct luna_query_row *rows, uint32_t count) {
    char digits[24];
    char hex[17];
    const struct luna_query_row *row = 0;

    if (rows != 0 && count != 0u && g_files_selection < count) {
        row = &rows[g_files_selection];
    }

    if (row == 0) {
        device_write("files.meta selected=none source=LaFS owner-space=DATA consumer=USER query=LaSQL status=missing\r\n");
        device_write("files.status selected=none state=missing normal=no readonly=no recovery=no denied=no missing=yes\r\n");
        return;
    }

    device_write("files.meta selected=");
    append_u32_decimal(digits, g_files_selection);
    device_write(digits);
    device_write(" name=");
    device_write(files_row_name(row));
    device_write(" kind=");
    device_write(files_row_kind(row->object_type));
    device_write(" source=LaFS owner-space=DATA consumer=USER query=LaSQL\r\n");

    device_write("files.meta ref=0x");
    append_u64_hex_fixed(hex, row->object_low, 16u);
    device_write(hex);
    device_write(" type=");
    append_u32_hex_fixed(digits, row->object_type, 8u);
    device_write(digits);
    device_write(" state=");
    device_write(files_row_state(row->state));
    device_write(" flags=");
    append_u32_hex_fixed(digits, row->flags, 8u);
    device_write(digits);
    device_write(" owner-ref=0x");
    append_u64_hex_fixed(hex, row->owner_low, 16u);
    device_write(hex);
    device_write(g_msg_newline);

    device_write("files.meta created=");
    append_u64_decimal(digits, row->created_at);
    device_write(digits);
    device_write(" updated=");
    append_u64_decimal(digits, row->updated_at);
    device_write(digits);
    device_write(" version=");
    append_u64_decimal(digits, row->version);
    device_write(digits);
    device_write(" size=unavailable record-count=unavailable\r\n");

    device_write("files.status selected=");
    append_u32_decimal(digits, g_files_selection);
    device_write(digits);
    device_write(" state=");
    device_write(files_row_state(row->state));
    device_write(" normal=");
    device_write(row->state == 1u ? "yes" : "no");
    device_write(" readonly=");
    device_write(row->state == 2u ? "yes" : "no");
    device_write(" recovery=");
    device_write(row->state == 3u ? "yes" : "no");
    device_write(" denied=");
    device_write(row->state == 5u ? "yes" : "no");
    device_write(" missing=");
    device_write(row->state == 4u ? "yes" : "no");
    device_write(g_msg_newline);
}

static void print_lasql_catalog(void) {
    struct luna_query_request request;
    uint32_t status;
    uint32_t count;
    zero_bytes(&request, sizeof(request));
    request.target = LUNA_QUERY_TARGET_PACKAGE_CATALOG;
    request.filter_flags = LUNA_QUERY_FILTER_NAMESPACE;
    request.projection_flags = LUNA_QUERY_PROJECT_NAME | LUNA_QUERY_PROJECT_LABEL | LUNA_QUERY_PROJECT_REF | LUNA_QUERY_PROJECT_TYPE | LUNA_QUERY_PROJECT_STATE | LUNA_QUERY_PROJECT_VERSION;
    request.sort_mode = LUNA_QUERY_SORT_NAME_ASC;
    request.limit = LUNA_DATA_OBJECT_CAPACITY;
    request.namespace_id = LUNA_QUERY_NAMESPACE_PACKAGE;
    status = data_query_rows(LUNA_SPACE_USER, &request, g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count);
    if (status != LUNA_DATA_OK) {
        device_write("lasql.catalog fail\r\n");
        return;
    }
    device_write("lasql.catalog ok\r\n");
    print_query_rows(g_lasql_data_payload.rows, count);
}

static void print_lasql_files(void) {
    struct luna_query_request request;
    uint32_t status;
    uint32_t count;
    zero_bytes(&request, sizeof(request));
    request.target = LUNA_QUERY_TARGET_USER_FILES;
    request.filter_flags = LUNA_QUERY_FILTER_NAMESPACE | LUNA_QUERY_FILTER_OWNER;
    request.projection_flags = LUNA_QUERY_PROJECT_NAME | LUNA_QUERY_PROJECT_REF | LUNA_QUERY_PROJECT_TYPE | LUNA_QUERY_PROJECT_STATE | LUNA_QUERY_PROJECT_OWNER | LUNA_QUERY_PROJECT_CREATED | LUNA_QUERY_PROJECT_UPDATED;
    request.sort_mode = LUNA_QUERY_SORT_UPDATED_DESC;
    request.limit = LUNA_DATA_OBJECT_CAPACITY;
    request.namespace_id = LUNA_QUERY_NAMESPACE_USER;
    request.owner_low = g_user_object.low;
    request.owner_high = g_user_object.high;
    request.scope_low = g_user_documents_set.low;
    request.scope_high = g_user_documents_set.high;
    status = data_query_rows(LUNA_SPACE_USER, &request, g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count);
    if (status != LUNA_DATA_OK) {
        device_write("lasql.files fail\r\n");
        return;
    }
    device_write("lasql.files ok\r\n");
    print_query_rows(g_lasql_data_payload.rows, count);
}

static void print_file_rows_status(struct luna_query_row *rows, uint32_t count) {
    char digits[12];
    char hex[17];

    device_write("files.user ");
    device_write(g_username);
    device_write("@");
    device_write(g_hostname);
    device_write(g_msg_newline);
    device_write("files.documents=0x");
    append_u64_hex_fixed(hex, g_user_documents_set.low, 16u);
    device_write(hex);
    device_write(g_msg_newline);
    device_write("files.visible=");
    append_u32_decimal(digits, count);
    device_write(digits);
    device_write(" selected=");
    append_u32_decimal(digits, g_files_selection);
    device_write(digits);
    device_write(g_msg_newline);
    device_write("files.current=");
    if (count == 0u || g_files_selection >= count) {
        device_write("empty");
    } else if (rows[g_files_selection].object_type == LUNA_THEME_OBJECT_TYPE) {
        device_write("theme");
    } else if (rows[g_files_selection].object_type == LUNA_NOTE_OBJECT_TYPE) {
        device_write("notes");
    } else {
        device_write("unavailable");
    }
    device_write(g_msg_newline);
    print_files_lafs_rows(rows, count);
    print_files_selected_metadata(rows, count);
}

static void print_lasql_logs(void) {
    struct luna_query_request request;
    uint32_t status;
    uint32_t count;
    zero_bytes(&request, sizeof(request));
    request.target = LUNA_QUERY_TARGET_OBSERVE_LOGS;
    request.projection_flags = LUNA_QUERY_PROJECT_MESSAGE | LUNA_QUERY_PROJECT_STATE | LUNA_QUERY_PROJECT_OWNER | LUNA_QUERY_PROJECT_CREATED;
    request.sort_mode = LUNA_QUERY_SORT_STAMP_DESC;
    request.limit = LUNA_OBSERVE_LOG_CAPACITY;
    status = observe_query_rows(LUNA_SPACE_USER, &request, g_lasql_observe_payload.rows, LUNA_OBSERVE_LOG_CAPACITY, &count);
    if (status != LUNA_OBSERVE_OK) {
        device_write("lasql.logs fail\r\n");
        return;
    }
    device_write("lasql.logs ok\r\n");
    print_query_rows(g_lasql_observe_payload.rows, count);
}

static uint32_t package_remove_named(const char *name, size_t len) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_package_gate *gate =
        (volatile struct luna_package_gate *)(uintptr_t)manifest->package_gate_base;
    uint32_t index = 0u;
    zero_bytes((void *)(uintptr_t)manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 78;
    gate->opcode = LUNA_PACKAGE_REMOVE;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    if (package_display_name(name, len, &index) != 0 && index < g_package_count) {
        copy_name16(gate->name, g_package_catalog[index].name, 15u);
    } else if (canonical_package_name(name, len) != 0) {
        const char *canonical = canonical_package_name(name, len);
        size_t canonical_len = 0u;
        while (canonical[canonical_len] != '\0') {
            canonical_len += 1u;
        }
        copy_name16(gate->name, canonical, canonical_len);
    } else {
        copy_name16(gate->name, name, len);
    }
    ((package_gate_fn_t)(uintptr_t)manifest->package_gate_entry)(
        (struct luna_package_gate *)(uintptr_t)manifest->package_gate_base
    );
    if (gate->status == LUNA_PACKAGE_OK) {
        (void)refresh_package_catalog();
    }
    return gate->status;
}

static size_t app_header_name_len(const char name[16]) {
    size_t len = 0u;
    while (len < 16u && name[len] != '\0') {
        len += 1u;
    }
    return len;
}

static int bundle_name_matches_request(const char bundle_name[16], const char *name, size_t len) {
    size_t bundle_len = app_header_name_len(bundle_name);

    if (bundle_len == 0u) {
        return 0;
    }
    if (len == bundle_len && chars_equal(bundle_name, name, len)) {
        return 1;
    }
    if (len == bundle_len + 5u &&
        chars_equal(bundle_name, name, bundle_len) &&
        chars_equal(name + bundle_len, ".luna", 5u)) {
        return 1;
    }
    return 0;
}

static int bundle_blob_matches_request(const void *blob, uint64_t blob_size, const char *name, size_t len) {
    const struct luna_bundle_header *bundle = (const struct luna_bundle_header *)blob;
    const struct luna_app_header *legacy = (const struct luna_app_header *)blob;

    if (blob == 0 || blob_size < sizeof(struct luna_app_header)) {
        return 0;
    }
    if (blob_size >= sizeof(struct luna_bundle_header) &&
        bundle->magic == LUNA_PROGRAM_BUNDLE_MAGIC &&
        bundle->header_bytes >= sizeof(struct luna_bundle_header) &&
        bundle->header_bytes <= blob_size) {
        return bundle_name_matches_request(bundle->name, name, len);
    }
    if (legacy->magic == LUNA_LEGACY_APP_MAGIC) {
        return bundle_name_matches_request(legacy->name, name, len);
    }
    return 0;
}

static int staged_bundle_for_name(const char *name, size_t len, uint64_t *out_base, uint64_t *out_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_session_script_header *script =
        (volatile struct luna_session_script_header *)(uintptr_t)manifest->session_script_base;
    uint64_t stage_base = 0u;
    uint64_t stage_size = 0u;
    struct luna_dev_bundle_stage_header stage;

    if (manifest->session_script_size <= sizeof(struct luna_session_script_header) + sizeof(stage)) {
        return 0;
    }
    if (script->magic != LUNA_SESSION_SCRIPT_MAGIC) {
        return 0;
    }
    if (script->command_bytes > manifest->session_script_size - sizeof(struct luna_session_script_header) - sizeof(stage)) {
        return 0;
    }

    stage_base = manifest->session_script_base + sizeof(struct luna_session_script_header) + script->command_bytes;
    if (stage_base + sizeof(stage) > manifest->session_script_base + manifest->session_script_size) {
        return 0;
    }
    copy_bytes(&stage, (const void *)(uintptr_t)stage_base, sizeof(stage));
    if (stage.magic != LUNA_DEV_BUNDLE_STAGE_MAGIC || stage.bundle_bytes == 0u) {
        return 0;
    }
    stage_base += sizeof(stage);
    stage_size = stage.bundle_bytes;
    if (stage_base + stage_size > manifest->session_script_base + manifest->session_script_size) {
        return 0;
    }
    if (!bundle_blob_matches_request((const void *)(uintptr_t)stage_base, stage_size, name, len)) {
        return 0;
    }
    *out_base = stage_base;
    *out_size = stage_size;
    return 1;
}

static int embedded_bundle_for_name(const char *name, size_t len, uint64_t *out_base, uint64_t *out_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;

    if (staged_bundle_for_name(name, len, out_base, out_size)) {
        return 1;
    }
    if ((len == 8u && chars_equal(name, "Settings", 8u)) ||
        (len == 5u && chars_equal(name, "Hello", 5u)) ||
        (len == 10u && chars_equal(name, "hello.luna", 10u))) {
        *out_base = manifest->app_hello_base;
        *out_size = manifest->app_hello_size;
        return 1;
    }
    if ((len == 5u && chars_equal(name, "Files", 5u)) ||
        (len == 10u && chars_equal(name, "files.luna", 10u))) {
        *out_base = manifest->app_files_base;
        *out_size = manifest->app_files_size;
        return 1;
    }
    if ((len == 5u && chars_equal(name, "Notes", 5u)) ||
        (len == 10u && chars_equal(name, "notes.luna", 10u))) {
        *out_base = manifest->app_notes_base;
        *out_size = manifest->app_notes_size;
        return 1;
    }
    if ((len == 5u && chars_equal(name, "Guard", 5u)) ||
        (len == 10u && chars_equal(name, "guard.luna", 10u))) {
        *out_base = manifest->app_guard_base;
        *out_size = manifest->app_guard_size;
        return 1;
    }
    if ((len == 7u && chars_equal(name, "Console", 7u)) ||
        (len == 12u && chars_equal(name, "console.luna", 12u))) {
        *out_base = manifest->app_console_base;
        *out_size = manifest->app_console_size;
        return 1;
    }
    if (bundle_blob_matches_request((const void *)(uintptr_t)manifest->app_hello_base, manifest->app_hello_size, name, len)) {
        *out_base = manifest->app_hello_base;
        *out_size = manifest->app_hello_size;
        return 1;
    }
    if (bundle_blob_matches_request((const void *)(uintptr_t)manifest->app_files_base, manifest->app_files_size, name, len)) {
        *out_base = manifest->app_files_base;
        *out_size = manifest->app_files_size;
        return 1;
    }
    if (bundle_blob_matches_request((const void *)(uintptr_t)manifest->app_notes_base, manifest->app_notes_size, name, len)) {
        *out_base = manifest->app_notes_base;
        *out_size = manifest->app_notes_size;
        return 1;
    }
    if (bundle_blob_matches_request((const void *)(uintptr_t)manifest->app_guard_base, manifest->app_guard_size, name, len)) {
        *out_base = manifest->app_guard_base;
        *out_size = manifest->app_guard_size;
        return 1;
    }
    if (bundle_blob_matches_request((const void *)(uintptr_t)manifest->app_console_base, manifest->app_console_size, name, len)) {
        *out_base = manifest->app_console_base;
        *out_size = manifest->app_console_size;
        return 1;
    }
    return 0;
}

static const char *canonical_package_name(const char *name, size_t len) {
    if ((len == 8u && chars_equal(name, "Settings", 8u)) ||
        (len == 5u && chars_equal(name, "Hello", 5u)) ||
        (len == 10u && chars_equal(name, "hello.luna", 10u))) {
        return "hello.luna";
    }
    if ((len == 5u && chars_equal(name, "Files", 5u)) ||
        (len == 10u && chars_equal(name, "files.luna", 10u))) {
        return "files.luna";
    }
    if ((len == 5u && chars_equal(name, "Notes", 5u)) ||
        (len == 10u && chars_equal(name, "notes.luna", 10u))) {
        return "notes.luna";
    }
    if ((len == 5u && chars_equal(name, "Guard", 5u)) ||
        (len == 10u && chars_equal(name, "guard.luna", 10u))) {
        return "guard.luna";
    }
    if ((len == 7u && chars_equal(name, "Console", 7u)) ||
        (len == 12u && chars_equal(name, "console.luna", 12u))) {
        return "console.luna";
    }
    return 0;
}

static uint32_t package_install_named(const char *name, size_t len) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_package_gate *gate =
        (volatile struct luna_package_gate *)(uintptr_t)manifest->package_gate_base;
    uint64_t app_base = 0u;
    uint64_t app_size = 0u;
    uint64_t install_base = 0u;

    if (!embedded_bundle_for_name(name, len, &app_base, &app_size) || app_base == 0u || app_size == 0u) {
        return LUNA_PACKAGE_ERR_NOT_FOUND;
    }
    install_base = app_base;
    if (manifest->data_buffer_base != 0u && manifest->data_buffer_size >= app_size) {
        copy_bytes((void *)(uintptr_t)manifest->data_buffer_base, (const void *)(uintptr_t)app_base, (size_t)app_size);
        install_base = manifest->data_buffer_base;
    }
    zero_bytes((void *)(uintptr_t)manifest->package_gate_base, sizeof(struct luna_package_gate));
    gate->sequence = 78;
    gate->opcode = LUNA_PACKAGE_INSTALL;
    gate->cid_low = g_package_install_cid.low;
    gate->cid_high = g_package_install_cid.high;
    gate->buffer_addr = install_base;
    gate->buffer_size = app_size;
    ((package_gate_fn_t)(uintptr_t)manifest->package_gate_entry)(
        (struct luna_package_gate *)(uintptr_t)manifest->package_gate_base
    );
    if (gate->status == LUNA_PACKAGE_OK) {
        (void)refresh_package_catalog();
    }
    return gate->status;
}

static uint32_t update_check_status(uint64_t *out_current, uint64_t *out_target, uint64_t *out_flags) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_update_gate *gate =
        (volatile struct luna_update_gate *)(uintptr_t)manifest->update_gate_base;
    zero_bytes((void *)(uintptr_t)manifest->update_gate_base, sizeof(struct luna_update_gate));
    gate->sequence = 79;
    gate->opcode = LUNA_UPDATE_CHECK;
    gate->cid_low = g_update_check_cid.low;
    gate->cid_high = g_update_check_cid.high;
    ((void (SYSV_ABI *)(struct luna_update_gate *))(uintptr_t)manifest->update_gate_entry)(
        (struct luna_update_gate *)(uintptr_t)manifest->update_gate_base
    );
    if (out_current != 0) {
        *out_current = gate->current_version;
    }
    if (out_target != 0) {
        *out_target = gate->target_version;
    }
    if (out_flags != 0) {
        *out_flags = gate->flags;
    }
    return gate->status;
}

static uint32_t update_apply_status(uint64_t request_flags, uint64_t *out_current, uint64_t *out_target, uint64_t *out_flags) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_update_gate *gate =
        (volatile struct luna_update_gate *)(uintptr_t)manifest->update_gate_base;
    uint32_t status;
    uint64_t current;
    uint64_t target;
    uint64_t flags;
    if ((g_update_apply_cid.low == 0u && g_update_apply_cid.high == 0u) &&
        request_capability(LUNA_CAP_UPDATE_APPLY, &g_update_apply_cid) != LUNA_GATE_OK) {
        return LUNA_UPDATE_ERR_INVALID_CAP;
    }
    zero_bytes((void *)(uintptr_t)manifest->update_gate_base, sizeof(struct luna_update_gate));
    gate->sequence = 80;
    gate->opcode = LUNA_UPDATE_APPLY;
    gate->cid_low = g_update_apply_cid.low;
    gate->cid_high = g_update_apply_cid.high;
    gate->flags = request_flags;
    ((void (SYSV_ABI *)(struct luna_update_gate *))(uintptr_t)manifest->update_gate_entry)(
        (struct luna_update_gate *)(uintptr_t)manifest->update_gate_base
    );
    status = gate->status;
    current = gate->current_version;
    target = gate->target_version;
    flags = gate->flags;
    if (status == LUNA_UPDATE_OK &&
        flags == LUNA_UPDATE_TXN_STATE_EMPTY &&
        (current != 0u || target != 0u)) {
        uint64_t checked_current = 0u;
        uint64_t checked_target = 0u;
        uint64_t checked_flags = 0u;
        if (update_check_status(&checked_current, &checked_target, &checked_flags) == LUNA_UPDATE_OK &&
            checked_flags != LUNA_UPDATE_TXN_STATE_EMPTY) {
            current = checked_current;
            target = checked_target;
            flags = checked_flags;
        } else if ((request_flags & LUNA_UPDATE_FLAG_ROLLBACK) != 0u) {
            flags = LUNA_UPDATE_TXN_STATE_ROLLED_BACK;
        } else {
            flags = LUNA_UPDATE_TXN_STATE_COMMITTED;
        }
    }
    if (out_current != 0) {
        *out_current = current;
    }
    if (out_target != 0) {
        *out_target = target;
    }
    if (out_flags != 0) {
        *out_flags = flags;
    }
    return status;
}

static const char *update_state_label(uint64_t state) {
    if (state == LUNA_UPDATE_TXN_STATE_EMPTY) {
        return "idle";
    }
    if (state == LUNA_UPDATE_TXN_STATE_STAGED) {
        return "staged";
    }
    if (state == LUNA_UPDATE_TXN_STATE_COMMITTED) {
        return "committed";
    }
    if (state == LUNA_UPDATE_TXN_STATE_ACTIVATED) {
        return "active";
    }
    if (state == LUNA_UPDATE_TXN_STATE_ROLLED_BACK) {
        return "rolled_back";
    }
    if (state == LUNA_UPDATE_TXN_STATE_FAILED) {
        return "failed";
    }
    return "unknown";
}

static const char *pair_state_label(uint32_t flags) {
    if ((flags & 0x2u) != 0u) {
        return "trusted";
    }
    if ((flags & 0x1u) != 0u) {
        return "pending";
    }
    return "none";
}

#define LUNA_EXTERNAL_PHASE_IDLE 0u
#define LUNA_EXTERNAL_PHASE_CONNECT 1u
#define LUNA_EXTERNAL_PHASE_SEND 2u
#define LUNA_EXTERNAL_PHASE_RECV 3u
#define LUNA_EXTERNAL_STATUS_TIMEOUT 0xFFFFFFFFu

static const char *external_phase_label(uint32_t phase) {
    if (phase == LUNA_EXTERNAL_PHASE_CONNECT) {
        return "connect";
    }
    if (phase == LUNA_EXTERNAL_PHASE_SEND) {
        return "send";
    }
    if (phase == LUNA_EXTERNAL_PHASE_RECV) {
        return "recv";
    }
    return "idle";
}

static const char *surface_mode_label(uint32_t mode) {
    if (mode == 1u) {
        return "run";
    }
    if (mode == 2u) {
        return "read-only";
    }
    if (mode == 3u) {
        return "diagnostics";
    }
    return "not-ready";
}

static const char *update_action_label(uint64_t flags) {
    if (flags == LUNA_UPDATE_TXN_STATE_STAGED) {
        return "apply-ready";
    }
    if (flags == LUNA_UPDATE_TXN_STATE_COMMITTED) {
        return "reboot-confirm";
    }
    if (flags == LUNA_UPDATE_TXN_STATE_ACTIVATED) {
        return "applied";
    }
    if (flags == LUNA_UPDATE_TXN_STATE_FAILED) {
        return "failed";
    }
    return "check-only";
}

static const char *external_status_label(uint32_t status) {
    if (status == LUNA_NETWORK_OK) {
        return "ok";
    }
    if (status == LUNA_NETWORK_ERR_EMPTY) {
        return "empty";
    }
    if (status == LUNA_NETWORK_ERR_RANGE) {
        return "range";
    }
    if (status == LUNA_NETWORK_ERR_NOT_FOUND) {
        return "not_found";
    }
    if (status == LUNA_NETWORK_ERR_BAD_STATE) {
        return "bad_state";
    }
    if (status == LUNA_NETWORK_ERR_INVALID_CAP) {
        return "invalid_cap";
    }
    if (status == LUNA_EXTERNAL_STATUS_TIMEOUT) {
        return "timeout";
    }
    return "unknown";
}

static uint32_t external_transport_ready(struct luna_net_info *out_info) {
    struct luna_net_info info;
    zero_bytes(&info, sizeof(info));
    if (network_get_info(&info) != LUNA_NETWORK_OK ||
        (info.state_flags & LUNA_LANE_FLAG_READY) == 0u ||
        info.mmio_base == 0u ||
        (info.status & 0x00000002u) == 0u) {
        if (out_info != 0) {
            zero_bytes(out_info, sizeof(*out_info));
        }
        return 0u;
    }
    if (out_info != 0) {
        copy_bytes(out_info, &info, sizeof(info));
    }
    return 1u;
}

static void external_stack_reset_runtime(void) {
    g_external_stack.ready = 0u;
    g_external_stack.peer_id = 0u;
    g_external_stack.session_id = 0u;
    g_external_stack.channel_id = 0u;
}

static void run_net_status(void) {
    struct luna_net_info info;
    char digits[24];
    uint32_t transport_ready = external_transport_ready(&info);

    device_write("net.status state=");
    if (transport_ready == 0u) {
        device_write("unavailable");
    } else if (g_external_stack.ready != 0u) {
        device_write("ready");
    } else {
        device_write("idle");
    }
    device_write(" transport=");
    device_write(transport_ready != 0u ? "external-message" : "unavailable");
    device_write(" peer=");
    device_write(g_external_stack.peer_name);
    device_write(" peer_id=");
    append_u32_decimal(digits, g_external_stack.peer_id);
    device_write(digits);
    device_write(" session=");
    append_u32_decimal(digits, g_external_stack.session_id);
    device_write(digits);
    device_write(" channel=");
    append_u32_decimal(digits, g_external_stack.channel_id);
    device_write(digits);
    device_write(g_msg_newline);

    device_write("net.status phase=");
    device_write(external_phase_label(g_external_stack.last_phase));
    device_write(" last=");
    device_write(external_status_label(g_external_stack.last_status));
    device_write(" tx_messages=");
    append_u64_decimal(digits, g_external_stack.tx_messages);
    device_write(digits);
    device_write(" tx_bytes=");
    append_u64_decimal(digits, g_external_stack.tx_bytes);
    device_write(digits);
    device_write(" rx_messages=");
    append_u64_decimal(digits, g_external_stack.rx_messages);
    device_write(digits);
    device_write(" rx_bytes=");
    append_u64_decimal(digits, g_external_stack.rx_bytes);
    device_write(digits);
    device_write(g_msg_newline);

    device_write("net.status entry=net.connect send=net.send recv=net.recv inspect=net.info\r\n");
    device_write("net.default mode=");
    device_write(surface_mode_label(transport_ready != 0u ? 1u : 0u));
    device_write(" action=");
    if (transport_ready == 0u) {
        device_write("wait-net");
    } else if (g_external_stack.ready != 0u) {
        device_write("send-or-recv");
    } else {
        device_write("connect");
    }
    device_write(" path=connect-send-recv\r\n");
}

static void run_net_connect(const char *peer_name) {
    struct luna_link_peer peer;
    struct luna_link_session session;
    struct luna_link_channel channel;
    struct luna_net_info info;
    uint32_t status = LUNA_NETWORK_OK;
    char digits[24];

    zero_bytes(&peer, sizeof(peer));
    zero_bytes(&session, sizeof(session));
    zero_bytes(&channel, sizeof(channel));
    g_external_stack.last_phase = LUNA_EXTERNAL_PHASE_CONNECT;
    if (external_transport_ready(&info) == 0u) {
        external_stack_reset_runtime();
        g_external_stack.last_status = LUNA_NETWORK_ERR_BAD_STATE;
        device_write("net.connect failed state=transport-unavailable\r\n");
        return;
    }
    status = network_pair_peer(peer_name, &peer);
    if (status != LUNA_NETWORK_OK) {
        external_stack_reset_runtime();
        g_external_stack.last_status = status;
        device_write("net.connect failed state=pair-");
        device_write(external_status_label(status));
        device_write(g_msg_newline);
        return;
    }
    status = network_open_session(peer.peer_id, &session);
    if (status != LUNA_NETWORK_OK) {
        external_stack_reset_runtime();
        g_external_stack.last_status = status;
        device_write("net.connect failed state=session-");
        device_write(external_status_label(status));
        device_write(g_msg_newline);
        return;
    }
    status = network_open_channel(
        session.session_id,
        LUNA_LINK_CHANNEL_KIND_MESSAGE,
        LUNA_LINK_TRANSFER_CLASS_MESSAGE,
        &channel
    );
    if (status != LUNA_NETWORK_OK) {
        external_stack_reset_runtime();
        g_external_stack.last_status = status;
        device_write("net.connect failed state=channel-");
        device_write(external_status_label(status));
        device_write(g_msg_newline);
        return;
    }
    copy_single_line(g_external_stack.peer_name, sizeof(g_external_stack.peer_name), peer_name);
    g_external_stack.ready = 1u;
    g_external_stack.peer_id = peer.peer_id;
    g_external_stack.session_id = session.session_id;
    g_external_stack.channel_id = channel.channel_id;
    g_external_stack.last_status = LUNA_NETWORK_OK;

    device_write("net.connect state=ready scope=external-message\r\n");
    device_write("net.connect peer=");
    device_write(g_external_stack.peer_name);
    device_write(" peer_id=");
    append_u32_decimal(digits, g_external_stack.peer_id);
    device_write(digits);
    device_write(" session=");
    append_u32_decimal(digits, g_external_stack.session_id);
    device_write(digits);
    device_write(" channel=");
    append_u32_decimal(digits, g_external_stack.channel_id);
    device_write(digits);
    device_write(g_msg_newline);
    device_write("net.connect mode=run action=send-or-recv\r\n");
}

static void run_net_send(const char *payload, size_t size) {
    char digits[24];

    g_external_stack.last_phase = LUNA_EXTERNAL_PHASE_SEND;
    if (g_external_stack.ready == 0u || g_external_stack.channel_id == 0u) {
        g_external_stack.last_status = LUNA_NETWORK_ERR_BAD_STATE;
        device_write("net.send failed state=not-connected\r\n");
        return;
    }
    if (payload == 0 || size == 0u) {
        g_external_stack.last_status = LUNA_NETWORK_ERR_RANGE;
        device_write("net.send failed state=usage text required\r\n");
        return;
    }
    g_external_stack.last_status = network_send_channel(g_external_stack.channel_id, payload, size);
    if (g_external_stack.last_status != LUNA_NETWORK_OK) {
        device_write("net.send failed state=");
        device_write(external_status_label(g_external_stack.last_status));
        device_write(g_msg_newline);
        return;
    }
    g_external_stack.tx_messages += 1u;
    g_external_stack.tx_bytes += size;
    device_write("net.send state=ready bytes=");
    append_u64_decimal(digits, size);
    device_write(digits);
    device_write(" channel=");
    append_u32_decimal(digits, g_external_stack.channel_id);
    device_write(digits);
    device_write(" data=");
    device_write_bytes(payload, size);
    device_write(g_msg_newline);
    device_write("net.send next=net.recv or net.status\r\n");
}

static void run_net_recv(void) {
    uint8_t rx[LUNA_NETWORK_PACKET_BYTES];
    uint64_t got = 0u;
    uint32_t status = LUNA_NETWORK_ERR_EMPTY;
    char digits[24];

    zero_bytes(rx, sizeof(rx));
    g_external_stack.last_phase = LUNA_EXTERNAL_PHASE_RECV;
    if (g_external_stack.ready == 0u || g_external_stack.channel_id == 0u) {
        g_external_stack.last_status = LUNA_NETWORK_ERR_BAD_STATE;
        device_write("net.recv failed state=not-connected\r\n");
        return;
    }
    for (uint32_t spins = 0u; spins < 65536u; ++spins) {
        status = network_recv_channel(g_external_stack.channel_id, rx, sizeof(rx), &got);
        if (status == LUNA_NETWORK_OK && got != 0u) {
            break;
        }
        if (status != LUNA_NETWORK_ERR_EMPTY) {
            break;
        }
    }
    if (status == LUNA_NETWORK_ERR_EMPTY || got == 0u) {
        g_external_stack.last_status = LUNA_EXTERNAL_STATUS_TIMEOUT;
        device_write("net.recv timeout channel=");
        append_u32_decimal(digits, g_external_stack.channel_id);
        device_write(digits);
        device_write(g_msg_newline);
        return;
    }
    g_external_stack.last_status = status;
    if (status != LUNA_NETWORK_OK) {
        device_write("net.recv failed state=");
        device_write(external_status_label(status));
        device_write(g_msg_newline);
        return;
    }
    g_external_stack.rx_messages += 1u;
    g_external_stack.rx_bytes += got;
    device_write("net.recv state=ready bytes=");
    append_u64_decimal(digits, got);
    device_write(digits);
    device_write(" channel=");
    append_u32_decimal(digits, g_external_stack.channel_id);
    device_write(digits);
    device_write(" data=");
    device_write_bytes((const char *)rx, (size_t)got);
    device_write(g_msg_newline);
    device_write("net.recv next=net.send or net.status\r\n");
}

static uint32_t settings_pair_status(struct luna_link_peer *out) {
    if (out == 0) {
        return 0u;
    }
    zero_bytes(out, sizeof(*out));
    if (g_desktop_mode == 0u || g_desktop_booted == 0u) {
        return 0u;
    }
    return network_pair_peer("loop", out) == LUNA_NETWORK_OK;
}

static void print_package_list(void) {
    for (uint32_t i = 0u; i < g_package_count; ++i) {
        const char *label = package_label_by_index(i);
        device_write(label != 0 ? label : g_package_catalog[i].name);
        device_write(g_msg_newline);
    }
}

static void print_prompt(void) {
    device_write(g_username);
    device_write("@");
    device_write(g_hostname);
    device_write(":~$ ");
}

static void print_setup_status(void) {
    device_write("setup state=");
    device_write(g_user_setup_required != 0u ? "required" : "ready");
    device_write(" login=");
    device_write(g_user_logged_in != 0u ? "active" : "locked");
    device_write(" user=");
    device_write(g_username);
    device_write(" host=");
    device_write(g_hostname);
    device_write(g_msg_newline);
    device_write("setup.next=");
    if (g_user_setup_required != 0u) {
        device_write("setup.init");
    } else if (g_user_logged_in != 0u) {
        device_write("desktop or apps");
    } else {
        device_write("login");
    }
    device_write(g_msg_newline);
}

static void print_home_status(void) {
    char hex[17];
    device_write("home owner=");
    device_write(g_username);
    device_write("@");
    device_write(g_hostname);
    device_write(g_msg_newline);

    device_write("home.root=0x");
    append_u64_hex_fixed(hex, g_user_home_object.low, 16u);
    device_write(hex);
    device_write(g_msg_newline);

    device_write("home.documents=0x");
    append_u64_hex_fixed(hex, g_user_documents_set.low, 16u);
    device_write(hex);
    device_write(" home.desktop=0x");
    append_u64_hex_fixed(hex, g_user_desktop_set.low, 16u);
    device_write(hex);
    device_write(" home.downloads=0x");
    append_u64_hex_fixed(hex, g_user_downloads_set.low, 16u);
    device_write(hex);
    device_write(g_msg_newline);
}

static void print_settings_status(void) {
    uint64_t current = 0u;
    uint64_t target = 0u;
    uint64_t flags = 0u;
    struct luna_net_info net_info;
    struct luna_link_peer peer;
    char digits[24];
    char hex[17];
    uint32_t pair_ok = 0u;
    uint32_t inbound_ready = 0u;

    print_desktop_status();
    device_write("settings.user account=");
    device_write(g_username);
    device_write(" host=");
    device_write(g_hostname);
    device_write(" session=");
    device_write(g_user_logged_in != 0u ? "active" : "locked");
    device_write(g_msg_newline);
    device_write("settings.home root=0x");
    append_u64_hex_fixed(hex, g_user_home_object.low, 16u);
    device_write(hex);
    device_write(" documents=0x");
    append_u64_hex_fixed(hex, g_user_documents_set.low, 16u);
    device_write(hex);
    device_write(" desktop=0x");
    append_u64_hex_fixed(hex, g_user_desktop_set.low, 16u);
    device_write(hex);
    device_write(" downloads=0x");
    append_u64_hex_fixed(hex, g_user_downloads_set.low, 16u);
    device_write(hex);
    device_write(g_msg_newline);
    pair_ok = settings_pair_status(&peer);
    if (update_check_status(&current, &target, &flags) == LUNA_UPDATE_OK) {
        device_write("settings.update mode=");
        device_write(flags == LUNA_UPDATE_TXN_STATE_STAGED ? surface_mode_label(1u) : surface_mode_label(2u));
        device_write(" action=");
        device_write(update_action_label(flags));
        device_write(g_msg_newline);
        device_write("settings.update state=");
        device_write(update_state_label(flags));
        device_write(" current=");
        append_u64_decimal(digits, current);
        device_write(digits);
        device_write(" target=");
        append_u64_decimal(digits, target);
        device_write(digits);
        device_write(" user=");
        device_write(g_username);
        device_write(g_msg_newline);
        if (flags == LUNA_UPDATE_TXN_STATE_STAGED) {
            device_write("settings.update entry=update.apply action=apply-ready\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_COMMITTED) {
            device_write("settings.update entry=update.status action=reboot-confirm\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_ACTIVATED) {
            device_write("settings.update entry=update.status action=applied\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_FAILED) {
            device_write("settings.update entry=update.apply action=failed\r\n");
        } else {
            device_write("settings.update entry=update.status action=check-only\r\n");
        }
    } else {
        device_write("settings.update mode=not-ready action=not ready\r\n");
        device_write("settings.update unavailable\r\n");
        device_write("settings.update entry=update.status action=not ready\r\n");
    }
    device_write("settings.pairing mode=");
    device_write(surface_mode_label(pair_ok != 0u ? 2u : 0u));
    device_write(" action=");
    device_write(pair_ok != 0u ? "trust loop peer" : "not ready");
    device_write(g_msg_newline);
    device_write("settings.pairing state=");
    device_write(pair_ok != 0u ? pair_state_label(peer.flags) : "unavailable");
    device_write(" peer=");
    if (pair_ok != 0u) {
        append_u32_decimal((char *)digits, peer.peer_id);
        device_write((char *)digits);
    } else {
        device_write("0");
    }
    device_write(" user=");
    device_write(g_username);
    device_write(g_msg_newline);
    device_write("settings.pairing entry=pairing.status action=trust loop peer\r\n");
    device_write("settings.policy state=");
    device_write(pair_ok != 0u && (peer.flags & 0x2u) != 0u ? "allow" : "deny");
    device_write(" user=");
    device_write(g_username);
    device_write(g_msg_newline);
    device_write("settings.network mode=read-only action=inspect adapter\r\n");
    device_write("settings.network entry=net.info action=inspect adapter  verify=net.external diagnostics=net.loop\r\n");
    zero_bytes(&net_info, sizeof(net_info));
    if (network_get_info(&net_info) == LUNA_NETWORK_OK &&
        (net_info.state_flags & LUNA_LANE_FLAG_READY) != 0u &&
        net_info.mmio_base != 0u &&
        (net_info.status & 0x00000002u) != 0u) {
        inbound_ready = 1u;
    }
    device_write("settings.network diagnostics=");
    device_write(surface_mode_label(inbound_ready != 0u ? 3u : 0u));
    device_write(" action=net.inbound");
    if (inbound_ready == 0u) {
        device_write(" state=not ready");
    }
    device_write(g_msg_newline);
    device_write("settings.network inbound=");
    device_write(inbound_ready != 0u ? "ready" : "unavailable");
    device_write(" action=net.inbound filter=dst-mac|broadcast");
    if (inbound_ready != 0u && net_info.rx_packets != 0u) {
        device_write(" event=seen rx_packets=");
        append_u64_decimal(digits, net_info.rx_packets);
        device_write(digits);
    }
    device_write(g_msg_newline);
    device_write("settings.network messaging mode=");
    device_write(surface_mode_label(inbound_ready != 0u ? 1u : 0u));
    device_write(" action=");
    if (inbound_ready == 0u) {
        device_write("wait-net");
    } else if (g_external_stack.ready != 0u) {
        device_write("send-or-recv");
    } else {
        device_write("connect");
    }
    device_write(g_msg_newline);
    device_write("settings.network stack=");
    if (inbound_ready != 0u && g_external_stack.ready != 0u) {
        device_write("ready");
    } else if (inbound_ready != 0u) {
        device_write("available");
    } else {
        device_write("unavailable");
    }
    device_write(" entry=net.status connect=net.connect send=net.send recv=net.recv");
    if (g_external_stack.ready != 0u) {
        device_write(" peer=");
        device_write(g_external_stack.peer_name);
        device_write(" channel=");
        append_u32_decimal(digits, g_external_stack.channel_id);
        device_write(digits);
    }
    device_write(g_msg_newline);
    device_write("settings.readonly update network pairing policy when desktop is closed\r\n");
    device_write("settings.entry=");
    device_write(g_desktop_booted != 0u ? "desktop" : "shell");
    device_write(" launcher=");
    device_write(g_launcher_open != 0u ? "open" : "closed");
    device_write(" control=");
    device_write(g_control_open != 0u ? "open" : "closed");
    device_write(g_msg_newline);
    device_write("settings.available account host home status\r\n");
}

static void print_files_status(void) {
    uint32_t count = 0u;

    if (!prepare_files_surface()) {
        device_write("files scope unavailable\r\n");
        return;
    }
    if (query_visible_file_rows(g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count) != LUNA_DATA_OK) {
        device_write("files scope unavailable\r\n");
        return;
    }
    print_file_rows_status(g_lasql_data_payload.rows, count);
}

static void print_package_feedback(const char *action, const char *name, size_t len, uint32_t status) {
    uint32_t index = LUNA_PACKAGE_CAPACITY;
    const char *display = package_display_name(name, len, &index);
    const char *canonical = canonical_package_name(name, len);
    const char *shown_name = display != 0 ? display : 0;

    device_write("package.");
    device_write(action);
    device_write(" app=");
    if (shown_name != 0) {
        device_write(shown_name);
    } else {
        device_write_bytes(name, len);
    }
    device_write(" status=");
    if (status == LUNA_PACKAGE_OK) {
        device_write(chars_equal(action, "install", 7u) ? "installed" : "removed");
    } else {
        device_write("failed");
    }
    device_write(g_msg_newline);

    device_write("package.");
    device_write(action);
    device_write(" package=");
    if (canonical != 0) {
        device_write(canonical);
    } else if (display != 0 && index < g_package_count) {
        device_write(g_package_catalog[index].name);
    } else {
        device_write_bytes(name, len);
    }
    device_write(g_msg_newline);

    if (status == LUNA_PACKAGE_OK) {
        if (chars_equal(action, "install", 7u)) {
            device_write("package.install result=visible in Apps and ready to open\r\n");
            device_write("package.install next=run ");
            if (shown_name != 0) {
                device_write(shown_name);
            } else {
                device_write_bytes(name, len);
            }
            device_write(g_msg_newline);
        } else {
            device_write("package.remove result=removed from Apps and launch blocked\r\n");
        }
    } else if (chars_equal(action, "install", 7u)) {
        device_write("package.install result=app not installed\r\n");
    } else {
        device_write("package.remove result=app still present or not found\r\n");
    }
}

static int create_first_user_system(const char *hostname, size_t hostname_len, const char *username, size_t username_len, const char *password, size_t password_len) {
    struct luna_object_ref host_ref = {0u, 0u};
    struct luna_object_ref user_ref = {0u, 0u};
    struct luna_object_ref secret_ref = {0u, 0u};
    struct luna_object_ref home_ref = {0u, 0u};
    struct luna_object_ref profile_ref = {0u, 0u};
    struct luna_object_ref documents_ref = {0u, 0u};
    struct luna_object_ref desktop_ref = {0u, 0u};
    struct luna_object_ref downloads_ref = {0u, 0u};
    struct luna_host_record host;
    struct luna_user_record user;
    struct luna_user_secret_record secret;
    struct luna_user_home_root_record home;
    struct luna_user_profile_record profile;
    uint64_t salt;
    uint32_t status;

    if (hostname_len == 0u || username_len == 0u || password_len == 0u) {
        return 0;
    }
    status = seed_named_object(LUNA_DATA_OBJECT_TYPE_HOST_RECORD, &host_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_DATA_OBJECT_TYPE_USER_RECORD, &user_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_DATA_OBJECT_TYPE_USER_SECRET, &secret_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_DATA_OBJECT_TYPE_USER_HOME_ROOT, &home_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_DATA_OBJECT_TYPE_USER_PROFILE, &profile_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_SET_OBJECT_TYPE, &documents_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_SET_OBJECT_TYPE, &desktop_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = seed_named_object(LUNA_SET_OBJECT_TYPE, &downloads_ref);
    if (status != LUNA_DATA_OK) {
        return 0;
    }

    zero_bytes(&host, sizeof(host));
    host.magic = LUNA_HOST_RECORD_MAGIC;
    host.version = LUNA_USER_RECORD_VERSION;
    host.primary_user = user_ref;
    copy_fixed_text(host.hostname, sizeof(host.hostname), hostname, hostname_len);

    zero_bytes(&user, sizeof(user));
    user.magic = LUNA_USER_RECORD_MAGIC;
    user.version = LUNA_USER_RECORD_VERSION;
    user.secret_object = secret_ref;
    user.home_root = home_ref;
    user.profile_object = profile_ref;
    copy_fixed_text(user.username, sizeof(user.username), username, username_len);

    zero_bytes(&secret, sizeof(secret));
    secret.magic = LUNA_USER_SECRET_MAGIC;
    secret.version = LUNA_USER_RECORD_VERSION;
    secret.flags = LUNA_USER_SECRET_FLAG_SECURITY_OWNED | LUNA_USER_SECRET_FLAG_INSTALL_BOUND;
    secret.user_object = user_ref;
    salt = user_ref.low ^ home_ref.high ^ 0x13579BDF2468ACE0ull;
    secret.salt = salt;
    if (!security_derive_user_secret(user_ref, salt, username, username_len, password, password_len, &secret.password_fold)) {
        return 0;
    }
    copy_fixed_text(secret.username, sizeof(secret.username), username, username_len);

    zero_bytes(&home, sizeof(home));
    home.magic = LUNA_USER_HOME_MAGIC;
    home.version = LUNA_USER_RECORD_VERSION;
    home.user_object = user_ref;
    home.documents_set = documents_ref;
    home.desktop_set = desktop_ref;
    home.downloads_set = downloads_ref;

    zero_bytes(&profile, sizeof(profile));
    profile.magic = LUNA_USER_PROFILE_MAGIC;
    profile.version = LUNA_USER_RECORD_VERSION;
    profile.user_object = user_ref;
    profile.home_root = home_ref;
    profile.theme_variant = g_theme_variant & 1u;

    status = pour_object_bytes(host_ref, &host, sizeof(host));
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = pour_object_bytes(user_ref, &user, sizeof(user));
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = pour_object_bytes(secret_ref, &secret, sizeof(secret));
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = pour_object_bytes(home_ref, &home, sizeof(home));
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    status = pour_object_bytes(profile_ref, &profile, sizeof(profile));
    if (status != LUNA_DATA_OK) {
        return 0;
    }

    g_host_object = host_ref;
    copy_fixed_text(g_hostname, sizeof(g_hostname), hostname, hostname_len);
    if (!security_open_user_session(user_ref, secret_ref)) {
        return 0;
    }
    if (!activate_user_session(&user, user_ref)) {
        security_close_user_session();
        return 0;
    }
    load_profile_state();
    status = ensure_user_documents_set();
    if (status != LUNA_DATA_OK) {
        return 0;
    }
    persist_note_state();
    persist_theme_state();
    persist_files_view_state();
    sync_desktop_shell_state();
    return 1;
}

static int login_user_session(const char *username, size_t username_len, const char *password, size_t password_len) {
    struct luna_user_record user;
    struct luna_object_ref user_ref;

    if (!load_user_record_by_name(username, username_len, &user, &user_ref)) {
        return 0;
    }
    if (!auth_login_secret(user.secret_object, username, username_len, password, password_len)) {
        return 0;
    }
    if (!security_open_user_session(user_ref, user.secret_object)) {
        return 0;
    }
    if (!activate_user_session(&user, user_ref)) {
        security_close_user_session();
        return 0;
    }
    load_profile_state();
    load_theme_state();
    load_note_state();
    load_files_view_state();
    if (ensure_user_documents_set() != LUNA_DATA_OK) {
        return 0;
    }
    persist_files_view_state();
    sync_desktop_shell_state();
    return 1;
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 50;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = domain_key;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static void device_write(const char *text) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    uint64_t size = 0;

    while (text[size] != '\0') {
        size += 1u;
    }

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 52;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
}

static uint64_t device_read_byte(uint32_t device_id, uint8_t *out) {
    return device_read_block(device_id, out, 1u);
}

static uint64_t device_input_read_key(uint8_t *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 55;
    gate->opcode = LUNA_DEVICE_INPUT_READ;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_INPUT0;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = 1u;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->size;
}

static uint64_t device_input_read_pointer(struct luna_pointer_event *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 56;
    gate->opcode = LUNA_DEVICE_INPUT_READ;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = LUNA_DEVICE_ID_POINTER0;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = sizeof(*out);
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->size;
}

enum {
    USER_INPUT_SOURCE_NONE = 0u,
    USER_INPUT_SOURCE_KEYBOARD = 1u,
    USER_INPUT_SOURCE_OPERATOR = 2u,
};

static const char *user_input_source_name(uint32_t source) {
    switch (source) {
        case USER_INPUT_SOURCE_KEYBOARD:
            return "keyboard";
        case USER_INPUT_SOURCE_OPERATOR:
            return "operator";
        default:
            return "none";
    }
}

static void log_user_input_event(uint32_t source, uint8_t value) {
    const char *prefix = "[USER] input lane src=none key=";
    if (g_user_input_debug_budget == 0u) {
        return;
    }
    if (source == USER_INPUT_SOURCE_KEYBOARD) {
        prefix = "[USER] input lane src=keyboard key=";
    } else if (source == USER_INPUT_SOURCE_OPERATOR) {
        prefix = "[USER] input lane src=operator key=";
    }
    debug_input_byte(prefix, value);
    g_user_input_debug_budget -= 1u;
}

static void log_shell_command(const char *stage, uint32_t source, const char *line, size_t len) {
    char prefix[64];
    size_t prefix_len = 0u;
    const char *source_name = user_input_source_name(source);

    if (g_user_command_debug_budget == 0u) {
        return;
    }
    zero_bytes(prefix, sizeof(prefix));
    for (const char *p = "[USER] shell "; *p != '\0' && prefix_len + 1u < sizeof(prefix); ++p) {
        prefix[prefix_len++] = *p;
    }
    for (const char *p = stage; *p != '\0' && prefix_len + 1u < sizeof(prefix); ++p) {
        prefix[prefix_len++] = *p;
    }
    for (const char *p = " src="; *p != '\0' && prefix_len + 1u < sizeof(prefix); ++p) {
        prefix[prefix_len++] = *p;
    }
    for (const char *p = source_name; *p != '\0' && prefix_len + 1u < sizeof(prefix); ++p) {
        prefix[prefix_len++] = *p;
    }
    for (const char *p = " cmd="; *p != '\0' && prefix_len + 1u < sizeof(prefix); ++p) {
        prefix[prefix_len++] = *p;
    }
    prefix[prefix_len] = '\0';
    debug_input_text(prefix, line, len);
    g_user_command_debug_budget -= 1u;
}

static void log_session_script_command(const char *line, size_t len) {
    if (g_session_script_debug_budget == 0u) {
        return;
    }
    debug_input_text("[USER] session script cmd=", line, len);
    g_session_script_debug_budget -= 1u;
}

static uint32_t lifecycle_read_units(struct luna_unit_record *out, uint32_t capacity) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;

    if (out == 0 || capacity == 0u || g_life_read_cid.low == 0u || g_life_read_cid.high == 0u) {
        return 0u;
    }

    zero_bytes(out, sizeof(struct luna_unit_record) * capacity);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 57u;
    gate->opcode = LUNA_LIFE_READ_UNITS;
    gate->cid_low = g_life_read_cid.low;
    gate->cid_high = g_life_read_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = sizeof(struct luna_unit_record) * capacity;
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );
    if (gate->status != LUNA_LIFE_OK) {
        return 0u;
    }
    return gate->result_count;
}

static uint64_t device_driver_lifecycle_flags(void) {
    struct luna_unit_record records[16];
    uint32_t count = lifecycle_read_units(records, 16u);

    for (uint32_t i = 0u; i < count && i < 16u; ++i) {
        if (records[i].space_id == LUNA_SPACE_DEVICE && records[i].state == LUNA_UNIT_LIVE) {
            return records[i].flags;
        }
    }
    return 0u;
}

static uint32_t system_query_space_state(uint32_t space_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_system_gate *gate =
        (volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base;
    struct luna_system_record record;

    if (g_system_query_cid.low == 0u || g_system_query_cid.high == 0u) {
        return 0u;
    }

    zero_bytes(&record, sizeof(record));
    zero_bytes((void *)(uintptr_t)manifest->system_gate_base, sizeof(struct luna_system_gate));
    gate->sequence = 69u + space_id;
    gate->opcode = LUNA_SYSTEM_QUERY_SPACE;
    gate->space_id = space_id;
    gate->cid_low = g_system_query_cid.low;
    gate->cid_high = g_system_query_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&record;
    gate->buffer_size = sizeof(record);
    ((system_gate_fn_t)(uintptr_t)manifest->system_gate_entry)(
        (struct luna_system_gate *)(uintptr_t)manifest->system_gate_base
    );
    if (gate->status != LUNA_SYSTEM_OK || gate->result_count == 0u || record.space_id != space_id) {
        return 0u;
    }
    return record.state;
}

static uint32_t input_recovery_action_flags(uint64_t lifecycle_flags) {
    return ((uint32_t)(lifecycle_flags >> LUNA_DRIVER_ACTION_LIFECYCLE_SHIFT))
        & (LUNA_DRIVER_ACTION_INPUT_MINIMAL | LUNA_DRIVER_ACTION_INPUT_OPERATOR_RECOVERY);
}

static void refresh_input_recovery_state(void) {
    if (system_query_space_state(LUNA_SPACE_DEVICE) == LUNA_SYSTEM_STATE_DEGRADED) {
        g_device_lifecycle_flags = device_driver_lifecycle_flags();
        g_input_recovery_actions = input_recovery_action_flags(g_device_lifecycle_flags);
        return;
    }
    g_device_lifecycle_flags = 0u;
    g_input_recovery_actions = 0u;
}

static uint32_t input_minimal_recovery_active(void) {
    return (g_input_recovery_actions & LUNA_DRIVER_ACTION_INPUT_MINIMAL) != 0u;
}

static uint32_t input_operator_recovery_active(void) {
    return (g_input_recovery_actions & LUNA_DRIVER_ACTION_INPUT_OPERATOR_RECOVERY) != 0u;
}

static uint32_t input_no_local_recovery_active(void) {
    return (g_device_lifecycle_flags & LUNA_DRIVER_LIFE_INPUT_DEGRADED) != 0u;
}

static uint32_t input_pointer_path_allowed(void) {
    return input_minimal_recovery_active() == 0u && input_no_local_recovery_active() == 0u;
}

static uint32_t input_desktop_keyboard_allowed(void) {
    return g_desktop_mode != 0u && input_no_local_recovery_active() == 0u;
}

static uint32_t input_desktop_text_entry_allowed(void) {
    return g_desktop_mode != 0u
        && input_no_local_recovery_active() == 0u
        && input_minimal_recovery_active() == 0u;
}

static uint32_t input_shell_prompt_needed(void) {
    return g_desktop_mode == 0u
        || input_operator_recovery_active() != 0u
        || input_no_local_recovery_active() != 0u;
}

static void announce_input_recovery_mode(void) {
    if (input_no_local_recovery_active() != 0u) {
        device_write(g_msg_input_recovery_shell);
        return;
    }
    if (input_minimal_recovery_active() != 0u || input_operator_recovery_active() != 0u) {
        device_write(g_msg_input_recovery_minimal);
    }
}

static uint64_t read_user_input_key(uint8_t *out, uint32_t *out_source) {
    uint64_t got = 0u;

    if (out == 0) {
        return 0u;
    }
    if (out_source != 0) {
        *out_source = USER_INPUT_SOURCE_NONE;
    }

    got = device_input_read_key(out);
    if (got != 0u) {
        if (out_source != 0) {
            *out_source = USER_INPUT_SOURCE_KEYBOARD;
        }
        return got;
    }

    if (input_operator_recovery_active() != 0u) {
        got = device_read_byte(LUNA_DEVICE_ID_INPUT0, out);
        if (got != 0u) {
            if (out_source != 0) {
                *out_source = USER_INPUT_SOURCE_OPERATOR;
            }
            return got;
        }
    }

    return 0u;
}

static uint64_t device_read_block(uint32_t device_id, void *out, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 53;
    gate->opcode = LUNA_DEVICE_READ;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = device_id;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = size;
    gate->size = 0u;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->size;
}

static __attribute__((unused)) uint32_t device_write_block(uint32_t device_id, const void *src, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 53;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = device_id;
    gate->buffer_addr = (uint64_t)(uintptr_t)src;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->status;
}

static __attribute__((unused)) uint32_t device_query(uint32_t device_id, void *out, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 54;
    gate->opcode = LUNA_DEVICE_QUERY;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_read_cid.low;
    gate->cid_high = g_device_read_cid.high;
    gate->device_id = device_id;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    return gate->status;
}

static uint32_t network_pair_peer(const char *name, struct luna_link_peer *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    struct luna_link_peer peer;
    struct luna_link_pair_request request;
    struct luna_link_peer *stage = (struct luna_link_peer *)data_stage_buffer(sizeof(struct luna_link_peer));

    zero_bytes(&peer, sizeof(peer));
    zero_bytes(&request, sizeof(request));
    copy_single_line(request.name, sizeof(request.name), name);
    if (stage == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, sizeof(*stage));
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 57;
    gate->opcode = LUNA_NETWORK_PAIR_PEER;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_network_pair_cid.low;
    gate->cid_high = g_network_pair_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = sizeof(*stage);
    copy_bytes(stage, &request, sizeof(request));
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    copy_bytes(&peer, stage, sizeof(peer));
    if (gate->status == LUNA_NETWORK_OK && out != 0) {
        copy_bytes(out, &peer, sizeof(peer));
    }
    if (gate->status != LUNA_NETWORK_OK) {
        return gate->status;
    }
    if ((peer.flags & 0x2u) != 0u) {
        if (out != 0) {
            copy_bytes(out, &peer, sizeof(peer));
        }
        return LUNA_NETWORK_OK;
    }
    if ((peer.flags & 0x4u) != 0u && peer.challenge != 0u) {
        zero_bytes(&request, sizeof(request));
        copy_single_line(request.name, sizeof(request.name), name);
        request.flags = 1u;
        request.challenge = peer.challenge;
        request.response = peer.challenge ^ ((uint64_t)peer.peer_id << 32) ^ 0x4C4C5052u;
        zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
        gate->sequence = 58;
        gate->opcode = LUNA_NETWORK_PAIR_PEER;
        gate->caller_space = LUNA_SPACE_USER;
        gate->actor_space = LUNA_SPACE_USER;
        gate->cid_low = g_network_pair_cid.low;
        gate->cid_high = g_network_pair_cid.high;
        zero_bytes(stage, sizeof(*stage));
        gate->buffer_addr = (uint64_t)(uintptr_t)stage;
        gate->buffer_size = sizeof(*stage);
        copy_bytes(stage, &request, sizeof(request));
        ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
            (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
        );
        copy_bytes(&peer, stage, sizeof(peer));
        if (gate->status == LUNA_NETWORK_OK && out != 0) {
            copy_bytes(out, &peer, sizeof(peer));
        }
    }
    return gate->status;
}

static uint32_t network_open_session(uint32_t peer_id, struct luna_link_session *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    struct luna_link_session session;
    struct luna_link_session_request request;
    struct luna_link_session *stage =
        (struct luna_link_session *)data_stage_buffer(sizeof(struct luna_link_session));

    zero_bytes(&session, sizeof(session));
    zero_bytes(&request, sizeof(request));
    request.peer_id = peer_id;
    if (stage == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, sizeof(*stage));
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 58;
    gate->opcode = LUNA_NETWORK_OPEN_SESSION;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_network_session_cid.low;
    gate->cid_high = g_network_session_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = sizeof(*stage);
    copy_bytes(stage, &request, sizeof(request));
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    copy_bytes(&session, stage, sizeof(session));
    if (gate->status == LUNA_NETWORK_OK && out != 0) {
        copy_bytes(out, &session, sizeof(session));
    }
    return gate->status;
}

static uint32_t network_open_channel(uint32_t session_id, uint32_t kind, uint32_t transfer_class, struct luna_link_channel *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    struct luna_link_channel channel;
    struct luna_link_channel_request request;
    struct luna_link_channel *stage =
        (struct luna_link_channel *)data_stage_buffer(sizeof(struct luna_link_channel));

    zero_bytes(&channel, sizeof(channel));
    zero_bytes(&request, sizeof(request));
    request.session_id = session_id;
    request.kind = kind;
    request.transfer_class = transfer_class;
    if (stage == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, sizeof(*stage));
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 59;
    gate->opcode = LUNA_NETWORK_OPEN_CHANNEL;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->session_id = session_id;
    gate->cid_low = g_network_session_cid.low;
    gate->cid_high = g_network_session_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = sizeof(*stage);
    copy_bytes(stage, &request, sizeof(request));
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    copy_bytes(&channel, stage, sizeof(channel));
    if (gate->status == LUNA_NETWORK_OK && out != 0) {
        copy_bytes(out, &channel, sizeof(channel));
    }
    return gate->status;
}

static uint32_t network_send_channel(uint32_t channel_id, const void *payload, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    struct luna_link_send_request request;
    struct luna_link_send_request *request_stage =
        (struct luna_link_send_request *)data_stage_buffer(sizeof(struct luna_link_send_request) + size);
    uint8_t *payload_stage = 0;

    zero_bytes(&request, sizeof(request));
    if (request_stage == 0) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    payload_stage = ((uint8_t *)request_stage) + sizeof(struct luna_link_send_request);
    request.channel_id = channel_id;
    request.payload_addr = (uint64_t)(uintptr_t)payload_stage;
    request.payload_size = size;
    zero_bytes(request_stage, sizeof(*request_stage) + (size_t)size);
    copy_bytes(payload_stage, payload, (size_t)size);
    copy_bytes(request_stage, &request, sizeof(request));
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 60;
    gate->opcode = LUNA_NETWORK_SEND_CHANNEL;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->channel_id = channel_id;
    gate->cid_low = g_network_send_cid.low;
    gate->cid_high = g_network_send_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)request_stage;
    gate->buffer_size = sizeof(*request_stage);
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    return gate->status;
}

static uint32_t network_recv_channel(uint32_t channel_id, void *out, uint64_t size, uint64_t *out_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    void *stage = data_stage_buffer(size);

    zero_bytes(out, (size_t)size);
    if (stage == 0) {
        if (out_size != 0) {
            *out_size = 0u;
        }
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, (size_t)size);
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 61;
    gate->opcode = LUNA_NETWORK_RECV_CHANNEL;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->channel_id = channel_id;
    gate->cid_low = g_network_recv_cid.low;
    gate->cid_high = g_network_recv_cid.high;
    gate->flags = channel_id;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = size;
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    if (gate->status == LUNA_NETWORK_OK && gate->size != 0u) {
        copy_bytes(out, stage, (size_t)gate->size);
    }
    if (out_size != 0) {
        *out_size = gate->size;
    }
    return gate->status;
}

static uint32_t network_send_packet(const void *payload, uint64_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    void *stage = data_stage_buffer(size);

    if (stage == 0 || payload == 0 || size == 0u || size > manifest->data_buffer_size) {
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, (size_t)size);
    copy_bytes(stage, payload, (size_t)size);
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 62;
    gate->opcode = LUNA_NETWORK_SEND_PACKET;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_network_send_cid.low;
    gate->cid_high = g_network_send_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = size;
    gate->size = size;
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    return gate->status;
}

static uint32_t network_recv_packet(void *out, uint64_t size, uint64_t *out_size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;
    void *stage = data_stage_buffer(size);

    zero_bytes(out, (size_t)size);
    if (stage == 0 || out == 0 || size == 0u) {
        if (out_size != 0) {
            *out_size = 0u;
        }
        return LUNA_NETWORK_ERR_RANGE;
    }
    zero_bytes(stage, (size_t)size);
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 64;
    gate->opcode = LUNA_NETWORK_RECV_PACKET;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_network_recv_cid.low;
    gate->cid_high = g_network_recv_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)stage;
    gate->buffer_size = size;
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    if (gate->status == LUNA_NETWORK_OK && gate->size != 0u) {
        copy_bytes(out, stage, (size_t)gate->size);
    }
    if (out_size != 0) {
        *out_size = gate->size;
    }
    return gate->status;
}

static uint32_t network_get_info(struct luna_net_info *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_network_gate *gate =
        (volatile struct luna_network_gate *)(uintptr_t)manifest->network_gate_base;

    zero_bytes(out, sizeof(*out));
    zero_bytes((void *)(uintptr_t)manifest->network_gate_base, sizeof(struct luna_network_gate));
    gate->sequence = 63;
    gate->opcode = LUNA_NETWORK_GET_INFO;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_network_recv_cid.low;
    gate->cid_high = g_network_recv_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)out;
    gate->buffer_size = sizeof(*out);
    ((network_gate_fn_t)(uintptr_t)manifest->network_gate_entry)(
        (struct luna_network_gate *)(uintptr_t)manifest->network_gate_base
    );
    return gate->status;
}

static uint32_t graphics_set_active_window(uint32_t window_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 68;
    gate->opcode = LUNA_GRAPHICS_SET_ACTIVE_WINDOW;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_create_window_native(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    const char *title,
    uint32_t *out_window_id
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 67u;
    gate->opcode = LUNA_GRAPHICS_CREATE_WINDOW;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->x = x;
    gate->y = y;
    gate->width = width;
    gate->height = height;
    gate->buffer_addr = (uint64_t)(uintptr_t)title;
    gate->buffer_size = title != 0 ? text_length(title) : 0u;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    if (gate->status == LUNA_GRAPHICS_OK && out_window_id != 0) {
        *out_window_id = gate->window_id;
    }
    return gate->status;
}

static uint32_t graphics_draw_window_char(uint32_t window_id, uint32_t x, uint32_t y, char ch, uint8_t attr) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 66u;
    gate->opcode = LUNA_GRAPHICS_DRAW_CHAR;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    gate->x = x;
    gate->y = y;
    gate->glyph = (uint32_t)(uint8_t)ch;
    gate->attr = attr;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_nudge_window(uint32_t window_id, int32_t dx, int32_t dy) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 69;
    gate->opcode = LUNA_GRAPHICS_MOVE_WINDOW;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    gate->x = (uint32_t)dx;
    gate->y = (uint32_t)dy;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_close_window(uint32_t window_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 70;
    gate->opcode = LUNA_GRAPHICS_CLOSE_WINDOW;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_minimize_window(uint32_t window_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 73;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = 0x40u;
    gate->window_id = window_id;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_toggle_maximize_window(uint32_t window_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 74;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = 0x80u;
    gate->window_id = window_id;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_resize_window(uint32_t window_id, int32_t dw, int32_t dh) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 75;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = 0x100u;
    gate->window_id = window_id;
    gate->x = (uint32_t)dw;
    gate->y = (uint32_t)dh;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_render_desktop(uint32_t attr, uint32_t selection) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 71;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = attr;
    gate->glyph = selection;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_desktop_shell;
    gate->buffer_size = sizeof(g_desktop_shell);
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    return gate->status;
}

static uint32_t graphics_toggle_control_center(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 76;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = g_control_open == 0u ? 0x200u : 0x400u;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_desktop_shell;
    gate->buffer_size = sizeof(g_desktop_shell);
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    g_control_open = g_control_open == 0u ? 1u : 0u;
    return gate->status;
}

static uint32_t graphics_render_desktop_state(uint32_t attr, uint32_t selection, uint32_t x, uint32_t y, uint32_t *kind, uint32_t *window_id, uint32_t *glyph) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 72;
    gate->opcode = LUNA_GRAPHICS_RENDER_DESKTOP;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->attr = attr;
    gate->glyph = selection;
    gate->x = x;
    gate->y = y;
    gate->buffer_addr = (uint64_t)(uintptr_t)&g_desktop_shell;
    gate->buffer_size = sizeof(g_desktop_shell);
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    if (kind != 0) {
        *kind = gate->result_count;
    }
    if (window_id != 0) {
        *window_id = gate->window_id;
    }
    if (glyph != 0) {
        *glyph = gate->glyph;
    }
    return gate->status;
}

static void drain_input_noise(uint32_t device_id, uint32_t limit) {
    uint32_t i = 0u;
    while (i < limit) {
        uint8_t drop = 0u;
        if (device_read_byte(device_id, &drop) == 0u) {
            break;
        }
        i += 1u;
    }
}

static void drain_pointer_noise(uint32_t limit) {
    uint32_t i = 0u;
    while (i < limit) {
        struct luna_pointer_event drop;
        if (device_input_read_pointer(&drop) == 0u) {
            break;
        }
        i += 1u;
    }
}

static int chars_equal(const char *left, const char *right, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}

static int parse_u32_token(const char *text, size_t len, uint32_t *value_out, size_t *used_out) {
    uint32_t value = 0u;
    size_t used = 0u;

    while (used < len && text[used] >= '0' && text[used] <= '9') {
        value = (value * 10u) + (uint32_t)(text[used] - '0');
        used += 1u;
    }
    if (used == 0u) {
        return 0;
    }
    if (value_out != 0) {
        *value_out = value;
    }
    if (used_out != 0) {
        *used_out = used;
    }
    return 1;
}

static void append_u32_decimal(char *out, uint32_t value) {
    char digits[10];
    size_t count = 0u;
    size_t cursor = 0u;

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

static void append_u32_hex_fixed(char *out, uint32_t value, uint32_t digits) {
    static const char hex_digits[] = "0123456789ABCDEF";
    for (uint32_t i = 0u; i < digits; ++i) {
        uint32_t shift = (digits - 1u - i) * 4u;
        out[i] = hex_digits[(value >> shift) & 0xFu];
    }
    out[digits] = '\0';
}

static void append_u64_hex_fixed(char *out, uint64_t value, uint32_t digits) {
    static const char hex_digits[] = "0123456789ABCDEF";
    for (uint32_t i = 0u; i < digits; ++i) {
        uint32_t shift = (digits - 1u - i) * 4u;
        out[i] = hex_digits[(uint32_t)((value >> shift) & 0xFu)];
    }
    out[digits] = '\0';
}

static void append_u64_decimal(char *out, uint64_t value) {
    char digits[20];
    size_t count = 0u;
    size_t cursor = 0u;

    if (value == 0u) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value != 0u && count < 20u) {
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

static int event_word_ascii(uint64_t value, char out[9]) {
    for (size_t i = 0; i < 8u; ++i) {
        uint8_t ch = (uint8_t)(value >> ((7u - i) * 8u));
        if (ch < 33u || ch > 126u) {
            out[0] = '\0';
            return 0;
        }
        out[i] = (char)ch;
    }
    out[8] = '\0';
    return 1;
}

static void device_write_bytes(const char *text, size_t size) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 54;
    gate->opcode = LUNA_DEVICE_WRITE;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_write_cid.low;
    gate->cid_high = g_device_write_cid.high;
    gate->device_id = LUNA_DEVICE_ID_SERIAL0;
    gate->buffer_addr = (uint64_t)(uintptr_t)text;
    gate->buffer_size = size;
    gate->size = size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
}

static const char *space_name(uint32_t space_id) {
    switch (space_id) {
        case LUNA_SPACE_BOOT: return "BOOT";
        case LUNA_SPACE_SYSTEM: return "SYSTEM";
        case LUNA_SPACE_DATA: return "DATA";
        case LUNA_SPACE_SECURITY: return "SECURITY";
        case LUNA_SPACE_GRAPHICS: return "GRAPHICS";
        case LUNA_SPACE_DEVICE: return "DEVICE";
        case LUNA_SPACE_NETWORK: return "NETWORK";
        case LUNA_SPACE_USER: return "USER";
        case LUNA_SPACE_TIME: return "TIME";
        case LUNA_SPACE_LIFECYCLE: return "LIFECYCLE";
        case LUNA_SPACE_OBSERVABILITY: return "OBSERVABILITY";
        case LUNA_SPACE_AI: return "AI";
        case LUNA_SPACE_PROGRAM: return "PROGRAM";
        case LUNA_SPACE_PACKAGE: return "PACKAGE";
        case LUNA_SPACE_UPDATE: return "UPDATE";
        default: return 0;
    }
}

static const char *space_state_name(uint32_t state) {
    switch (state) {
        case LUNA_SYSTEM_STATE_ACTIVE: return "active";
        case LUNA_SYSTEM_STATE_PAUSED: return "paused";
        case LUNA_SYSTEM_STATE_DEGRADED: return "degraded";
        case LUNA_SYSTEM_STATE_BOOTING: return "booting";
        default: return "void";
    }
}

static void print_space_list(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
    struct luna_unit_record *records =
        (struct luna_unit_record *)(uintptr_t)manifest->list_buffer_base;
    uint32_t count = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 55;
    gate->opcode = LUNA_LIFE_READ_UNITS;
    gate->cid_low = g_life_read_cid.low;
    gate->cid_high = g_life_read_cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );
    if (gate->status != LUNA_LIFE_OK) {
        device_write(g_msg_spaces_fail);
        return;
    }

    count = gate->result_count;
    for (uint32_t i = 0; i < count; ++i) {
        const char *name = space_name(records[i].space_id);
        if (name == 0 || records[i].state != LUNA_UNIT_LIVE) {
            continue;
        }
        device_write(name);
        device_write(g_msg_newline);
    }
}

static void print_cap_count(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    char line[] = "caps: 000\r\n";

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 58;
    gate->opcode = LUNA_GATE_LIST_CAPS;
    gate->caller_space = LUNA_SPACE_USER;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        device_write(g_msg_caps_fail);
        return;
    }

    line[6] = (char)('0' + ((gate->count / 100u) % 10u));
    line[7] = (char)('0' + ((gate->count / 10u) % 10u));
    line[8] = (char)('0' + (gate->count % 10u));
    device_write(line);
}

static const char *cap_name(uint64_t domain_key) {
    switch (domain_key) {
        case LUNA_CAP_DATA_SEED: return "data.seed";
        case LUNA_CAP_DATA_POUR: return "data.pour";
        case LUNA_CAP_DATA_DRAW: return "data.draw";
        case LUNA_CAP_DATA_GATHER: return "data.gather";
        case LUNA_CAP_LIFE_READ: return "life.read";
        case LUNA_CAP_USER_SHELL: return "user.shell";
        case LUNA_CAP_TIME_PULSE: return "time.pulse";
        case LUNA_CAP_DEVICE_LIST: return "device.list";
        case LUNA_CAP_DEVICE_READ: return "device.read";
        case LUNA_CAP_DEVICE_WRITE: return "device.write";
        case LUNA_CAP_GRAPHICS_DRAW: return "graphics.draw";
        case LUNA_CAP_OBSERVE_READ: return "observe.read";
        case LUNA_CAP_OBSERVE_STATS: return "observe.stats";
        case LUNA_CAP_NETWORK_SEND: return "network.send";
        case LUNA_CAP_NETWORK_RECV: return "network.recv";
        case LUNA_CAP_NETWORK_PAIR: return "network.pair";
        case LUNA_CAP_NETWORK_SESSION: return "network.session";
        case LUNA_CAP_PROGRAM_LOAD: return "program.load";
        case LUNA_CAP_PROGRAM_START: return "program.start";
        case LUNA_CAP_SYSTEM_QUERY: return "system.query";
        default: return "cap";
    }
}

static void print_cap_list(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_cap_record *records =
        (struct luna_cap_record *)(uintptr_t)manifest->list_buffer_base;
    char line[32];
    uint32_t count = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 66;
    gate->opcode = LUNA_GATE_LIST_CAPS;
    gate->caller_space = LUNA_SPACE_USER;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        device_write(g_msg_caplist_fail);
        return;
    }
    count = gate->count;

    for (uint32_t i = 0; i < count; ++i) {
        device_write(cap_name(records[i].domain_key));
        device_write(" uses=");
        append_u32_decimal(line, records[i].uses_left);
        device_write(line);
        device_write(" ttl=");
        append_u32_decimal(line, records[i].ttl);
        device_write(line);
        if (records[i].target_space != 0u) {
            device_write(" target=");
            if (space_name(records[i].target_space) != 0) {
                device_write(space_name(records[i].target_space));
            } else {
                append_u32_decimal(line, records[i].target_space);
                device_write(line);
            }
            device_write(":");
            append_u32_decimal(line, records[i].target_gate);
            device_write(line);
        }
        device_write(g_msg_newline);
    }
}

static void print_seal_list(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_seal_record *records =
        (struct luna_seal_record *)(uintptr_t)manifest->list_buffer_base;
    char line[32];
    uint32_t count = 0u;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 67;
    gate->opcode = LUNA_GATE_LIST_SEALS;
    gate->caller_space = LUNA_SPACE_USER;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        device_write(g_msg_seallist_fail);
        return;
    }
    count = gate->count;

    if (count == 0u) {
        device_write("seals: 0\r\n");
        return;
    }

    for (uint32_t i = 0; i < count; ++i) {
        device_write("seal target=");
        if (records[i].target_space != 0u && space_name(records[i].target_space) != 0) {
            device_write(space_name(records[i].target_space));
        } else if (records[i].target_space != 0u) {
            append_u32_decimal(line, records[i].target_space);
            device_write(line);
        } else {
            device_write("ANY");
        }
        device_write(":");
        append_u32_decimal(line, records[i].target_gate);
        device_write(line);
        device_write(g_msg_newline);
    }
}

static void print_space_map(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_system_gate *gate =
        (volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base;
    struct luna_system_record record;
    char line[32];
    char token[9];
    for (uint32_t space_id = 0u; space_id < 15u; ++space_id) {
        zero_bytes(&record, sizeof(record));
        zero_bytes((void *)(uintptr_t)manifest->system_gate_base, sizeof(struct luna_system_gate));
        gate->sequence = 64u + space_id;
        gate->opcode = LUNA_SYSTEM_QUERY_SPACE;
        gate->space_id = space_id;
        gate->cid_low = g_system_query_cid.low;
        gate->cid_high = g_system_query_cid.high;
        gate->buffer_addr = (uint64_t)(uintptr_t)&record;
        gate->buffer_size = sizeof(record);
        ((system_gate_fn_t)(uintptr_t)manifest->system_gate_entry)(
            (struct luna_system_gate *)(uintptr_t)manifest->system_gate_base
        );
        if (gate->status != LUNA_SYSTEM_OK || gate->result_count == 0u || record.state == 0u) {
            continue;
        }
        device_write(record.name);
        device_write(" state=");
        device_write(space_state_name(record.state));
        device_write(" mem=");
        append_u32_decimal(line, record.resource_memory);
        device_write(line);
        device_write(" time=");
        append_u32_decimal(line, record.resource_time);
        device_write(line);
        device_write(" event=");
        if (record.last_event == 0u) {
            device_write("0");
        } else if (event_word_ascii(record.last_event, token)) {
            device_write(token);
        } else {
            append_u64_decimal(line, record.last_event);
            device_write(line);
        }
        device_write(g_msg_newline);
    }
}

static void print_space_log(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_system_gate *gate =
        (volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base;
    struct luna_system_event_record *records =
        (struct luna_system_event_record *)(uintptr_t)manifest->list_buffer_base;
    char line[32];
    char token[9];

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->system_gate_base, sizeof(struct luna_system_gate));
    gate->sequence = 65;
    gate->opcode = LUNA_SYSTEM_QUERY_EVENTS;
    gate->cid_low = g_system_query_cid.low;
    gate->cid_high = g_system_query_cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((system_gate_fn_t)(uintptr_t)manifest->system_gate_entry)(
        (struct luna_system_gate *)(uintptr_t)manifest->system_gate_base
    );
    if (gate->status != LUNA_SYSTEM_OK) {
        device_write(g_msg_spacelog_fail);
        return;
    }

    uint32_t shown = gate->result_count;
    if (shown > 8u) {
        shown = 8u;
    }

    for (uint32_t i = 0; i < shown; ++i) {
        append_u64_decimal(line, records[i].sequence);
        device_write("#");
        device_write(line);
        device_write(" ");
        device_write(records[i].name);
        device_write(" ");
        device_write(space_state_name(records[i].state));
        device_write(" ");
        if (event_word_ascii(records[i].event_word, token)) {
            device_write(token);
        } else {
            append_u64_decimal(line, records[i].event_word);
            device_write(line);
        }
        device_write(g_msg_newline);
    }
}

static void print_store_info(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    char line[32];

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 62;
    gate->opcode = LUNA_DATA_STAT_STORE;
    gate->cid_low = g_data_gather_cid.low;
    gate->cid_high = g_data_gather_cid.high;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        device_write(g_msg_store_fail);
        return;
    }

    device_write("lafs.version: ");
    append_u32_decimal(line, gate->object_type);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.objects: ");
    append_u32_decimal(line, gate->result_count);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.state: ");
    device_write(gate->object_flags != 0u ? "dirty" : "clean");
    device_write(g_msg_newline);

    device_write("lafs.mounts: ");
    append_u64_decimal(line, gate->content_size);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.formats: ");
    append_u64_decimal(line, gate->created_at);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.nonce: ");
    append_u64_decimal(line, gate->size);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.last-repair: ");
    if (gate->object_low == 1u) {
        device_write("replay");
    } else if (gate->object_low == 2u) {
        device_write("metadata");
    } else if (gate->object_low == 3u) {
        device_write("scrub");
    } else {
        device_write("none");
    }
    device_write(g_msg_newline);

    device_write("lafs.last-scrubbed: ");
    append_u64_decimal(line, gate->object_high);
    device_write(line);
    device_write(g_msg_newline);
}

static void print_store_check(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;
    char line[32];
    uint32_t flags = 0u;

    zero_bytes((void *)(uintptr_t)manifest->data_gate_base, sizeof(struct luna_data_gate));
    gate->sequence = 63;
    gate->opcode = LUNA_DATA_VERIFY_STORE;
    gate->cid_low = g_data_gather_cid.low;
    gate->cid_high = g_data_gather_cid.high;
    ((data_gate_fn_t)(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (gate->status != LUNA_DATA_OK) {
        device_write(g_msg_storecheck_fail);
        return;
    }

    flags = gate->object_flags;
    device_write("lafs.health: ");
    if (flags == 0u) {
        device_write("ok");
    } else {
        if ((flags & 1u) != 0u) {
            device_write("dirty ");
        }
        if ((flags & 2u) != 0u) {
            device_write("layout ");
        }
        if ((flags & 4u) != 0u) {
            device_write("checksum ");
        }
        if ((flags & 8u) != 0u) {
            device_write("records ");
        }
    }
    device_write(g_msg_newline);

    device_write("lafs.invalid: ");
    append_u64_decimal(line, gate->size);
    device_write(line);
    device_write(g_msg_newline);

    device_write("lafs.objects: ");
    append_u32_decimal(line, gate->result_count);
    device_write(line);
    device_write(g_msg_newline);
}

static uint64_t cap_domain_from_name(const char *text, size_t len) {
    if (len == 9u && chars_equal(text, "data.seed", 9u)) return LUNA_CAP_DATA_SEED;
    if (len == 9u && chars_equal(text, "data.pour", 9u)) return LUNA_CAP_DATA_POUR;
    if (len == 9u && chars_equal(text, "data.draw", 9u)) return LUNA_CAP_DATA_DRAW;
    if (len == 11u && chars_equal(text, "data.gather", 11u)) return LUNA_CAP_DATA_GATHER;
    if (len == 9u && chars_equal(text, "life.read", 9u)) return LUNA_CAP_LIFE_READ;
    if (len == 11u && chars_equal(text, "device.list", 11u)) return LUNA_CAP_DEVICE_LIST;
    if (len == 11u && chars_equal(text, "device.read", 11u)) return LUNA_CAP_DEVICE_READ;
    if (len == 12u && chars_equal(text, "device.write", 12u)) return LUNA_CAP_DEVICE_WRITE;
    if (len == 12u && chars_equal(text, "observe.read", 12u)) return LUNA_CAP_OBSERVE_READ;
    if (len == 13u && chars_equal(text, "observe.stats", 13u)) return LUNA_CAP_OBSERVE_STATS;
    if (len == 12u && chars_equal(text, "network.send", 12u)) return LUNA_CAP_NETWORK_SEND;
    if (len == 12u && chars_equal(text, "network.recv", 12u)) return LUNA_CAP_NETWORK_RECV;
    if (len == 12u && chars_equal(text, "network.pair", 12u)) return LUNA_CAP_NETWORK_PAIR;
    if (len == 15u && chars_equal(text, "network.session", 15u)) return LUNA_CAP_NETWORK_SESSION;
    if (len == 12u && chars_equal(text, "program.load", 12u)) return LUNA_CAP_PROGRAM_LOAD;
    if (len == 13u && chars_equal(text, "program.start", 13u)) return LUNA_CAP_PROGRAM_START;
    return 0u;
}

static void revoke_cap_domain(const char *text, size_t len) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    char count_line[12];
    uint64_t domain = cap_domain_from_name(text, len);

    if (domain == 0u) {
        device_write(g_msg_revoke_fail);
        return;
    }

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 60;
    gate->opcode = LUNA_GATE_REVOKE_DOMAIN;
    gate->caller_space = LUNA_SPACE_USER;
    gate->domain_key = domain;
    ((security_gate_fn_t)(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (gate->status != LUNA_GATE_OK) {
        device_write(g_msg_revoke_fail);
        return;
    }

    device_write(g_msg_revoke_prefix);
    device_write_bytes(text, len);
    device_write(g_msg_revoke_mid);
    append_u32_decimal(count_line, gate->count);
    device_write(count_line);
    device_write(g_msg_revoke_suffix);
}

static void print_device_list(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    struct luna_device_record *records =
        (struct luna_device_record *)(uintptr_t)manifest->list_buffer_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
    gate->sequence = 59;
    gate->opcode = LUNA_DEVICE_LIST;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_device_list_cid.low;
    gate->cid_high = g_device_list_cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
        (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
    );
    if (gate->status != LUNA_DEVICE_OK || gate->result_count == 0u) {
        device_write(g_msg_devices_fail);
        return;
    }

    for (uint32_t i = 0; i < gate->result_count; ++i) {
        device_write(records[i].name);
        device_write(g_msg_newline);
    }
}

static const char *device_kind_name(uint32_t kind) {
    switch (kind) {
        case LUNA_DEVICE_KIND_SERIAL: return "serial";
        case LUNA_DEVICE_KIND_DISK: return "disk";
        case LUNA_DEVICE_KIND_DISPLAY: return "display";
        case LUNA_DEVICE_KIND_CLOCK: return "clock";
        case LUNA_DEVICE_KIND_INPUT: return "input";
        case LUNA_DEVICE_KIND_NETWORK: return "network";
        default: return "unknown";
    }
}

static const char *lane_class_name(uint32_t lane_class) {
    switch (lane_class) {
        case LUNA_LANE_CLASS_STREAM: return "stream";
        case LUNA_LANE_CLASS_BLOCK: return "block";
        case LUNA_LANE_CLASS_SCANOUT: return "scanout";
        case LUNA_LANE_CLASS_CLOCK: return "clock";
        case LUNA_LANE_CLASS_INPUT: return "input";
        case LUNA_LANE_CLASS_LINK: return "link";
        default: return "unknown";
    }
}

static const char *pci_class_name(uint32_t class_code, uint32_t subclass) {
    if (class_code == 0x01u && subclass == 0x01u) return "ide";
    if (class_code == 0x01u && subclass == 0x06u) return "sata";
    if (class_code == 0x01u && subclass == 0x08u) return "nvme";
    if (class_code == 0x02u && subclass == 0x00u) return "ethernet";
    if (class_code == 0x03u && subclass == 0x00u) return "vga";
    if (class_code == 0x06u && subclass == 0x00u) return "host-bridge";
    if (class_code == 0x06u && subclass == 0x01u) return "isa-bridge";
    if (class_code == 0x06u && subclass == 0x04u) return "pci-bridge";
    return "other";
}

static void print_lane_flags(uint32_t flags) {
    uint32_t wrote = 0u;
    if ((flags & LUNA_LANE_FLAG_PRESENT) != 0u) {
        device_write("present");
        wrote = 1u;
    }
    if ((flags & LUNA_LANE_FLAG_READY) != 0u) {
        device_write(wrote != 0u ? "|ready" : "ready");
        wrote = 1u;
    }
    if ((flags & LUNA_LANE_FLAG_BOOT) != 0u) {
        device_write(wrote != 0u ? "|boot" : "boot");
        wrote = 1u;
    }
    if ((flags & LUNA_LANE_FLAG_FALLBACK) != 0u) {
        device_write(wrote != 0u ? "|fallback" : "fallback");
        wrote = 1u;
    }
    if ((flags & LUNA_LANE_FLAG_POLLING) != 0u) {
        device_write(wrote != 0u ? "|polling" : "polling");
        wrote = 1u;
    }
    if (wrote == 0u) {
        device_write("none");
    }
}

static void print_lane_census(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    struct luna_lane_record *records =
        (struct luna_lane_record *)(uintptr_t)manifest->data_buffer_base;
    char digits[12];
    uint32_t cursor = 0u;

    while (1) {
        uint32_t count = 0u;
        zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
        zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
        gate->sequence = 60;
        gate->opcode = LUNA_DEVICE_CENSUS;
        gate->caller_space = LUNA_SPACE_USER;
        gate->actor_space = LUNA_SPACE_USER;
        gate->flags = cursor;
        gate->cid_low = g_device_list_cid.low;
        gate->cid_high = g_device_list_cid.high;
        gate->buffer_addr = manifest->data_buffer_base;
        gate->buffer_size = manifest->data_buffer_size;
        ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
            (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
        );
        if (gate->status != LUNA_DEVICE_OK) {
            device_write(g_msg_lanes_fail);
            return;
        }
        count = gate->result_count;
        if (count == 0u) {
            return;
        }

        for (uint32_t i = 0; i < count; ++i) {
            device_write(records[i].lane_name);
            device_write(" kind=");
            device_write(device_kind_name(records[i].device_kind));
            device_write(" class=");
            device_write(lane_class_name(records[i].lane_class));
            device_write(" driver=");
            if (records[i].driver_name[0] != '\0') {
                device_write(records[i].driver_name);
            } else {
                device_write("unknown");
            }
            device_write(" flags=");
            print_lane_flags(records[i].state_flags);
            device_write(" id=");
            append_u32_decimal(digits, records[i].device_id);
            device_write(digits);
            device_write(g_msg_newline);
        }
        cursor += count;
        if (cursor >= 64u) {
            return;
        }
    }
}

static void print_pci_scan(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_device_gate *gate =
        (volatile struct luna_device_gate *)(uintptr_t)manifest->device_gate_base;
    struct luna_pci_record *records =
        (struct luna_pci_record *)(uintptr_t)manifest->data_buffer_base;
    char hex4[5];
    char hex2[3];
    uint32_t cursor = 0u;

    while (1) {
        uint32_t count = 0u;
        zero_bytes((void *)(uintptr_t)manifest->data_buffer_base, manifest->data_buffer_size);
        zero_bytes((void *)(uintptr_t)manifest->device_gate_base, sizeof(struct luna_device_gate));
        gate->sequence = 61;
        gate->opcode = LUNA_DEVICE_PCI_SCAN;
        gate->caller_space = LUNA_SPACE_USER;
        gate->actor_space = LUNA_SPACE_USER;
        gate->flags = cursor;
        gate->cid_low = g_device_list_cid.low;
        gate->cid_high = g_device_list_cid.high;
        gate->buffer_addr = manifest->data_buffer_base;
        gate->buffer_size = manifest->data_buffer_size;
        ((device_gate_fn_t)(uintptr_t)manifest->device_gate_entry)(
            (struct luna_device_gate *)(uintptr_t)manifest->device_gate_base
        );
        if (gate->status != LUNA_DEVICE_OK) {
            device_write(g_msg_pci_fail);
            return;
        }
        count = gate->result_count;
        if (count == 0u) {
            return;
        }
        for (uint32_t i = 0u; i < count; ++i) {
            append_u32_hex_fixed(hex2, records[i].bus, 2u);
            device_write(hex2);
            device_write(":");
            append_u32_hex_fixed(hex2, records[i].slot, 2u);
            device_write(hex2);
            device_write(".");
            append_u32_hex_fixed(hex2, records[i].function, 1u);
            device_write(hex2);
            device_write(" ");
            device_write(pci_class_name(records[i].class_code, records[i].subclass));
            device_write(" vendor=");
            append_u32_hex_fixed(hex4, records[i].vendor_id, 4u);
            device_write(hex4);
            device_write(" device=");
            append_u32_hex_fixed(hex4, records[i].device_id, 4u);
            device_write(hex4);
            device_write(" class=");
            append_u32_hex_fixed(hex2, records[i].class_code, 2u);
            device_write(hex2);
            device_write(":");
            append_u32_hex_fixed(hex2, records[i].subclass, 2u);
            device_write(hex2);
            device_write(":");
            append_u32_hex_fixed(hex2, records[i].prog_if, 2u);
            device_write(hex2);
            device_write(g_msg_newline);
        }
        cursor += count;
        if (cursor >= 256u) {
            return;
        }
    }
}

static void run_net_loopback(void) {
    static const char payload[] = "luna-net";
    struct luna_link_peer peer;
    struct luna_link_session session;
    struct luna_link_channel channel;
    uint8_t rx[LUNA_NETWORK_PACKET_BYTES];
    char digits[12];
    uint64_t got = 0u;

    zero_bytes(&peer, sizeof(peer));
    zero_bytes(&session, sizeof(session));
    zero_bytes(&channel, sizeof(channel));
    if (network_pair_peer("loop", &peer) != LUNA_NETWORK_OK ||
        network_open_session(peer.peer_id, &session) != LUNA_NETWORK_OK ||
        network_open_channel(session.session_id, LUNA_LINK_CHANNEL_KIND_MESSAGE, LUNA_LINK_TRANSFER_CLASS_MESSAGE, &channel) != LUNA_NETWORK_OK ||
        network_send_channel(channel.channel_id, payload, sizeof(payload) - 1u) != LUNA_NETWORK_OK) {
        device_write(g_msg_net_fail);
        return;
    }
    for (uint32_t spins = 0u; spins < 4096u; ++spins) {
        if (network_recv_channel(channel.channel_id, rx, sizeof(rx), &got) == LUNA_NETWORK_OK && got != 0u) {
            break;
        }
    }
    if (got != sizeof(payload) - 1u || !chars_equal((const char *)rx, payload, sizeof(payload) - 1u)) {
        device_write(g_msg_net_fail);
        return;
    }
    device_write("net.loop state=ready scope=local-only\r\n");
    device_write("net.loop bytes=");
    append_u64_decimal(digits, got);
    device_write(digits);
    device_write(" data=");
    device_write_bytes((const char *)rx, (size_t)got);
    device_write(g_msg_newline);
}

static void run_net_external(void) {
    static const char payload[] = "luna-ext";
    struct luna_net_info before;
    struct luna_net_info after;
    char digits[21];

    zero_bytes(&before, sizeof(before));
    zero_bytes(&after, sizeof(after));
    if (network_get_info(&before) != LUNA_NETWORK_OK ||
        (before.state_flags & LUNA_LANE_FLAG_READY) == 0u ||
        before.mmio_base == 0u ||
        (before.status & 0x00000002u) == 0u) {
        device_write(g_msg_net_external_fail);
        return;
    }
    if (network_send_packet(payload, sizeof(payload) - 1u) != LUNA_NETWORK_OK ||
        network_get_info(&after) != LUNA_NETWORK_OK ||
        after.tx_packets <= before.tx_packets) {
        device_write(g_msg_net_external_fail);
        return;
    }
    device_write("net.external state=ready scope=outbound-only\r\n");
    device_write("net.external bytes=");
    append_u64_decimal(digits, sizeof(payload) - 1u);
    device_write(digits);
    device_write(" data=");
    device_write_bytes(payload, sizeof(payload) - 1u);
    device_write(g_msg_newline);
    device_write("net.external tx_packets=");
    append_u64_decimal(digits, before.tx_packets);
    device_write(digits);
    device_write("->");
    append_u64_decimal(digits, after.tx_packets);
    device_write(digits);
    device_write(g_msg_newline);
}

static void run_net_inbound(void) {
    uint8_t frame[LUNA_NETWORK_PACKET_BYTES];
    struct luna_net_info before;
    struct luna_net_info after;
    char digits[21];
    char hex4[5];
    char hex2[3];
    uint64_t got = 0u;

    zero_bytes(frame, sizeof(frame));
    zero_bytes(&before, sizeof(before));
    zero_bytes(&after, sizeof(after));
    if (network_get_info(&before) != LUNA_NETWORK_OK ||
        (before.state_flags & LUNA_LANE_FLAG_READY) == 0u ||
        before.mmio_base == 0u ||
        (before.status & 0x00000002u) == 0u) {
        device_write("net.inbound failed\r\n");
        return;
    }
    for (uint32_t spins = 0u; spins < 32u; ++spins) {
        uint32_t status = network_recv_packet(frame, sizeof(frame), &got);
        if (status == LUNA_NETWORK_OK && got >= 14u) {
            uint32_t dst_match = 1u;
            for (uint32_t i = 0u; i < 6u; ++i) {
                uint8_t expected = before.mac[i];
                if (frame[i] != expected && frame[i] != 0xFFu) {
                    dst_match = 0u;
                    break;
                }
            }
            if (dst_match != 0u) {
                break;
            }
        }
        got = 0u;
    }
    if (got < 14u || network_get_info(&after) != LUNA_NETWORK_OK || after.rx_packets <= before.rx_packets) {
        device_write("net.inbound failed\r\n");
        return;
    }
    device_write("net.inbound state=ready scope=external-raw\r\n");
    device_write("net.inbound bytes=");
    append_u64_decimal(digits, got - 14u);
    device_write(digits);
    device_write(" src=");
    for (uint32_t i = 0u; i < 6u; ++i) {
        append_u32_hex_fixed(hex2, frame[6u + i], 2u);
        device_write(hex2);
        if (i + 1u != 6u) {
            device_write(":");
        }
    }
    device_write(" dst=");
    for (uint32_t i = 0u; i < 6u; ++i) {
        append_u32_hex_fixed(hex2, frame[i], 2u);
        device_write(hex2);
        if (i + 1u != 6u) {
            device_write(":");
        }
    }
    device_write(" ethertype=0x");
    append_u32_hex_fixed(hex4, ((uint32_t)frame[12] << 8) | frame[13], 4u);
    device_write(hex4);
    device_write(g_msg_newline);
    device_write("net.inbound data=");
    device_write_bytes((const char *)(frame + 14u), (size_t)(got - 14u));
    device_write(g_msg_newline);
    device_write("net.inbound rx_packets=");
    append_u64_decimal(digits, before.rx_packets);
    device_write(digits);
    device_write("->");
    append_u64_decimal(digits, after.rx_packets);
    device_write(digits);
    device_write(" filter=dst-mac|broadcast\r\n");
}

static void print_net_info(void) {
    struct luna_net_info info;
    char digits[21];
    char hex16[17];
    char hex4[5];
    char hex2[3];

    zero_bytes(&info, sizeof(info));
    if (network_get_info(&info) != LUNA_NETWORK_OK) {
        device_write("net.info failed\r\n");
        return;
    }

    device_write("network state=");
    device_write((info.state_flags & 0x1u) != 0u ? "ready" : "limited");
    device_write(" loopback=available external=");
    if ((info.state_flags & LUNA_LANE_FLAG_READY) != 0u &&
        info.mmio_base != 0u &&
        (info.status & 0x00000002u) != 0u) {
        device_write("outbound-only");
    } else {
        device_write("not-ready");
    }
    device_write(" user=");
    device_write(g_username);
    device_write(" host=");
    device_write(g_hostname);
    device_write(g_msg_newline);
    device_write("network.entry inspect=net.info verify=net.external diagnostics=net.loop\r\n");
    device_write("network.default mode=");
    if ((info.state_flags & LUNA_LANE_FLAG_READY) != 0u &&
        info.mmio_base != 0u &&
        (info.status & 0x00000002u) != 0u) {
        device_write(surface_mode_label(1u));
    } else {
        device_write(surface_mode_label(0u));
    }
    device_write(" status=net.status connect=net.connect send=net.send recv=net.recv\r\n");
    device_write("network.inbound state=");
    if ((info.state_flags & LUNA_LANE_FLAG_READY) != 0u &&
        info.mmio_base != 0u &&
        (info.status & 0x00000002u) != 0u) {
        device_write("ready");
    } else {
        device_write("unavailable");
    }
    device_write(" entry=net.inbound filter=dst-mac|broadcast");
    if (info.rx_packets != 0u) {
        device_write(" event=seen rx_packets=");
        append_u64_decimal(digits, info.rx_packets);
        device_write(digits);
    }
    device_write(g_msg_newline);
    device_write("network.stack state=");
    if ((info.state_flags & LUNA_LANE_FLAG_READY) == 0u || info.mmio_base == 0u || (info.status & 0x00000002u) == 0u) {
        device_write("unavailable");
    } else if (g_external_stack.ready != 0u) {
        device_write("ready");
    } else {
        device_write("idle");
    }
    device_write(" entry=net.status connect=net.connect send=net.send recv=net.recv");
    if (g_external_stack.ready != 0u) {
        device_write(" peer=");
        device_write(g_external_stack.peer_name);
        device_write(" session=");
        append_u32_decimal(digits, g_external_stack.session_id);
        device_write(digits);
        device_write(" channel=");
        append_u32_decimal(digits, g_external_stack.channel_id);
        device_write(digits);
    }
    device_write(g_msg_newline);
    device_write("net.info driver=");
    append_u32_decimal(digits, info.driver_family);
    device_write(digits);
    device_write(" flags=");
    append_u32_decimal(digits, info.state_flags);
    device_write(digits);
    device_write(" vendor=");
    append_u32_hex_fixed(hex4, info.vendor_id, 4u);
    device_write(hex4);
    device_write(" device=");
    append_u32_hex_fixed(hex4, info.device_id, 4u);
    device_write(hex4);
    device_write(g_msg_newline);

    device_write("net.info ctrl=0x");
    append_u32_hex_fixed(hex4, info.ctrl, 4u);
    device_write(hex4);
    device_write(" status=0x");
    append_u32_hex_fixed(hex4, info.status, 4u);
    device_write(hex4);
    device_write(" rctl=0x");
    append_u32_hex_fixed(hex16, info.rctl, 8u);
    device_write(hex16);
    device_write(" tctl=0x");
    append_u32_hex_fixed(hex16, info.tctl, 8u);
    device_write(hex16);
    device_write(g_msg_newline);

    device_write("net.info mac=");
    for (uint32_t i = 0u; i < 6u; ++i) {
        append_u32_hex_fixed(hex2, info.mac[i], 2u);
        device_write(hex2);
        if (i + 1u != 6u) {
            device_write(":");
        }
    }
    device_write(" mmio=0x");
    append_u64_hex_fixed(hex16, info.mmio_base, 16u);
    device_write(hex16);
    device_write(g_msg_newline);

    device_write("net.info rx_head=");
    append_u32_decimal(digits, info.rx_head);
    device_write(digits);
    device_write(" rx_tail=");
    append_u32_decimal(digits, info.rx_tail);
    device_write(digits);
    device_write(" tx_head=");
    append_u32_decimal(digits, info.tx_head);
    device_write(digits);
    device_write(" tx_tail=");
    append_u32_decimal(digits, info.tx_tail);
    device_write(digits);
    device_write(g_msg_newline);

    device_write("net.info hw_rx_head=");
    append_u32_decimal(digits, info.hw_rx_head);
    device_write(digits);
    device_write(" hw_rx_tail=");
    append_u32_decimal(digits, info.hw_rx_tail);
    device_write(digits);
    device_write(" hw_tx_head=");
    append_u32_decimal(digits, info.hw_tx_head);
    device_write(digits);
    device_write(" hw_tx_tail=");
    append_u32_decimal(digits, info.hw_tx_tail);
    device_write(digits);
    device_write(g_msg_newline);

    device_write("net.info tx_packets=");
    append_u64_decimal(digits, info.tx_packets);
    device_write(digits);
    device_write(" rx_packets=");
    append_u64_decimal(digits, info.rx_packets);
    device_write(digits);
    device_write(g_msg_newline);
}

static void print_whoami(void) {
    device_write(g_username);
    device_write(g_msg_newline);
}

static void print_hostname(void) {
    device_write(g_hostname);
    device_write(g_msg_newline);
}

static void print_identity(void) {
    device_write(g_msg_id_prefix);
    device_write(g_username);
    device_write(g_msg_id_mid);
    device_write(g_hostname);
    device_write(g_msg_id_suffix);
}

static void copy_name16(volatile char out[16], const char *text, size_t len) {
    size_t i = 0;
    while (i < 16u) {
        out[i] = '\0';
        i += 1u;
    }
    i = 0u;
    while (i < len && i < 15u) {
        out[i] = text[i];
        i += 1u;
    }
}

static const char *package_label_by_index(uint32_t index) {
    const char *label;
    if (index >= g_package_count) {
        return 0;
    }
    label = g_package_catalog[index].label[0] != '\0' ? g_package_catalog[index].label : g_package_catalog[index].name;
    if (text_equals(label, "hello.luna") || text_equals(label, "hello")) {
        return "Settings";
    }
    if (text_equals(label, "files.luna") || text_equals(label, "files")) {
        return "Files";
    }
    if (text_equals(label, "notes.luna") || text_equals(label, "notes")) {
        return "Notes";
    }
    if (text_equals(label, "guard.luna") || text_equals(label, "guard")) {
        return "Guard";
    }
    if (text_equals(label, "console.luna") || text_equals(label, "console")) {
        return "Console";
    }
    if (text_equals(g_package_catalog[index].name, "hello.luna") ||
        text_equals(g_package_catalog[index].name, "hello")) {
        return "Settings";
    }
    if (text_equals(g_package_catalog[index].name, "files.luna") ||
        text_equals(g_package_catalog[index].name, "files")) {
        return "Files";
    }
    if (text_equals(g_package_catalog[index].name, "notes.luna") ||
        text_equals(g_package_catalog[index].name, "notes")) {
        return "Notes";
    }
    if (text_equals(g_package_catalog[index].name, "guard.luna") ||
        text_equals(g_package_catalog[index].name, "guard")) {
        return "Guard";
    }
    if (text_equals(g_package_catalog[index].name, "console.luna") ||
        text_equals(g_package_catalog[index].name, "console")) {
        return "Console";
    }
    return 0;
}

static const char *package_display_name(const char *name, size_t len, uint32_t *out_index) {
    for (uint32_t i = 0u; i < g_package_count; ++i) {
        const char *label = package_label_by_index(i);
        size_t cursor = 0u;
        size_t name_len = 0u;
        while (label != 0 &&
               cursor < len &&
               label[cursor] != '\0' &&
               label[cursor] == name[cursor]) {
            cursor += 1u;
        }
        if (label != 0 && cursor == len && label[cursor] == '\0') {
            if (out_index != 0) {
                *out_index = i;
            }
            return label;
        }
        cursor = 0u;
        while (cursor < len &&
               g_package_catalog[i].name[cursor] != '\0' &&
               g_package_catalog[i].name[cursor] == name[cursor]) {
            cursor += 1u;
        }
        if (cursor == len && g_package_catalog[i].name[cursor] == '\0') {
            if (out_index != 0) {
                *out_index = i;
            }
            return label != 0 ? label : g_package_catalog[i].name;
        }
        name_len = fixed_text_len(g_package_catalog[i].name, 16u);
        if (name_len == len + 5u &&
            chars_equal(g_package_catalog[i].name, name, len) &&
            chars_equal(g_package_catalog[i].name + len, ".luna", 5u)) {
            if (out_index != 0) {
                *out_index = i;
            }
            return label != 0 ? label : g_package_catalog[i].name;
        }
        {
            const char *canonical = canonical_package_name(name, len);
            if (canonical != 0 && text_equals(g_package_catalog[i].name, canonical)) {
                if (out_index != 0) {
                    *out_index = i;
                }
                return label != 0 ? label : g_package_catalog[i].name;
            }
        }
    }
    return 0;
}

static int open_settings_surface(void) {
    if (g_desktop_booted != 0u) {
        return render_settings_window();
    }
    if (g_control_open == 0u) {
        return graphics_toggle_control_center() == LUNA_GRAPHICS_OK;
    }
    (void)graphics_render_desktop_state(0x18u, g_launcher_index, g_pointer_x, g_pointer_y, 0, 0, 0);
    return 1;
}

static int prepare_files_surface(void) {
    uint32_t count = 0u;

    if (ensure_user_documents_set() != LUNA_DATA_OK) {
        if (g_user_logged_in == 0u) {
            return 0;
        }
        g_files_selection = 0u;
        return 1;
    }
    count = file_visible_object_count();
    if (count == 0u) {
        g_files_selection = 0u;
    } else if (g_files_selection >= count) {
        g_files_selection = 0u;
    }
    persist_files_view_state();
    return 1;
}

static int run_app(const char *name, size_t len) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_program_gate *gate =
        (volatile struct luna_program_gate *)(uintptr_t)manifest->program_gate_base;
    uint64_t ticket = 0u;
    uint32_t index = 0u;
    const char *display = 0;
    const char *program_name = 0;

    display = package_display_name(name, len, &index);
    if (display == 0) {
        device_write(g_msg_run_fail);
        device_write(g_msg_run_hint);
        return 0;
    }
    program_name = g_package_catalog[index].name;

    if ((len == 8u && chars_equal(name, "Settings", 8u)) ||
        (display != 0 && text_equals(display, "Settings"))) {
        g_launcher_index = index;
        if (g_desktop_booted == 0u) {
            device_write(g_msg_settings_ready);
            print_settings_status();
            return 1;
        }
        if (render_settings_window()) {
            device_write(g_msg_settings_ready);
            return 1;
        }
        device_write(g_msg_run_fail);
        device_write("settings entry unavailable\r\n");
        return 0;
    }
    if ((len == 5u && chars_equal(name, "Files", 5u)) ||
        (len == 10u && chars_equal(name, "files.luna", 10u)) ||
        (display != 0 && text_equals(display, "Files"))) {
        g_launcher_index = index;
        if (!prepare_files_surface()) {
            device_write(g_msg_run_fail);
            device_write("files scope unavailable\r\n");
            return 0;
        }
        device_write(g_msg_files_ready);
        if (g_desktop_booted == 0u) {
            print_files_status();
            device_write("files.next=run Notes or desktop.files.open\r\n");
        } else if (!render_files_window()) {
            device_write(g_msg_run_fail);
            device_write("files window unavailable\r\n");
            return 0;
        } else {
            print_files_status();
        }
        return 1;
    }
    if ((len == 5u && chars_equal(name, "Notes", 5u)) ||
        (len == 10u && chars_equal(name, "notes.luna", 10u)) ||
        (display != 0 && text_equals(display, "Notes"))) {
        if (g_desktop_booted != 0u && render_notes_window()) {
            device_write("workspace created\r\n");
            return 1;
        }
    }
    if ((len == 7u && chars_equal(name, "Console", 7u)) ||
        (len == 12u && chars_equal(name, "console.luna", 12u)) ||
        (display != 0 && text_equals(display, "Console"))) {
        if (g_desktop_booted != 0u && render_console_window()) {
            device_write("apps 1-5 or F/N/G/C/H\r\n");
            return 1;
        }
    }
    zero_bytes((void *)(uintptr_t)manifest->program_gate_base, sizeof(struct luna_program_gate));
    gate->sequence = 56;
    gate->opcode = LUNA_PROGRAM_LOAD_APP;
    gate->cid_low = g_program_load_cid.low;
    gate->cid_high = g_program_load_cid.high;
    copy_name16(gate->name, program_name, 15u);
    ((program_gate_fn_t)(uintptr_t)manifest->program_gate_entry)(
        (struct luna_program_gate *)(uintptr_t)manifest->program_gate_base
    );
    if (gate->status != LUNA_PROGRAM_OK) {
        serial_write_debug("[USERDBG] run load fail\r\n");
        device_write(g_msg_run_fail);
        device_write("app load path unavailable\r\n");
        return 0;
    }

    ticket = gate->ticket;
    zero_bytes((void *)(uintptr_t)manifest->program_gate_base, sizeof(struct luna_program_gate));
    gate->sequence = 57;
    gate->opcode = LUNA_PROGRAM_START_APP;
    gate->cid_low = g_program_start_cid.low;
    gate->cid_high = g_program_start_cid.high;
    gate->ticket = ticket;
    ((program_gate_fn_t)(uintptr_t)manifest->program_gate_entry)(
        (struct luna_program_gate *)(uintptr_t)manifest->program_gate_base
    );
    if (gate->status != LUNA_PROGRAM_OK) {
        serial_write_debug("[USERDBG] run start fail\r\n");
        device_write(g_msg_run_fail);
        device_write("app start handoff unavailable\r\n");
        return 0;
    }
    return 1;
}

static void sync_launcher(uint32_t open_flag) {
    g_launcher_open = open_flag;
    (void)graphics_render_desktop_state(
        open_flag != 0u ? 0x11u : 0x12u,
        g_launcher_index,
        g_pointer_x,
        g_pointer_y,
        0,
        0,
        0
    );
}

static void launch_selected_app(void) {
    if (g_launcher_index < g_package_count) {
        const char *name = g_package_catalog[g_launcher_index].name;
        size_t len = 0u;
        while (name[len] != '\0') {
            len += 1u;
        }
        (void)run_app(name, len);
    }
}

static __attribute__((unused)) int find_package_index_len(const char *name, size_t name_len, uint32_t *out_index) {
    for (uint32_t i = 0u; i < g_package_count; ++i) {
        if (chars_equal(g_package_catalog[i].label, name, name_len) &&
            g_package_catalog[i].label[name_len] == '\0') {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

static void launch_named_app(const char *name) {
    uint32_t index = 0u;
    size_t len = 0u;
    while (name[len] != '\0') {
        len += 1u;
    }
    if (package_display_name(name, len, &index) != 0) {
        g_launcher_index = index;
        launch_selected_app();
    }
}

static void boot_desktop_session(void) {
    if (g_desktop_booted != 0u) {
        return;
    }
    for (uint32_t i = 0u; i < g_package_count; ++i) {
        if ((g_package_catalog[i].flags & LUNA_PACKAGE_FLAG_STARTUP) == 0u) {
            continue;
        }
        if (text_equals(g_package_catalog[i].name, "console.luna") ||
            text_equals(g_package_catalog[i].name, "console")) {
            continue;
        }
        g_launcher_index = i;
        launch_selected_app();
    }
    g_desktop_booted = 1u;
}

static int session_script_present(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_session_script_header *header =
        (volatile struct luna_session_script_header *)(uintptr_t)manifest->session_script_base;
    uint64_t payload_limit = 0u;

    if (manifest->session_script_size <= sizeof(struct luna_session_script_header)) {
        return 0;
    }
    if (header->magic != LUNA_SESSION_SCRIPT_MAGIC) {
        return 0;
    }
    payload_limit = manifest->session_script_size - sizeof(struct luna_session_script_header);
    if (header->command_bytes == 0u || header->command_bytes > payload_limit) {
        return 0;
    }
    return 1;
}

static void write_desktop_result(const char *action, uint32_t ok) {
    device_write(g_msg_desktop_prefix);
    device_write(action);
    device_write(ok != 0u ? g_msg_ok_suffix : g_msg_fail_suffix);
}

static void write_desktop_named_result(const char *action, const char *name, size_t len, uint32_t ok) {
    uint32_t index = 0u;
    const char *display = package_display_name(name, len, &index);
    device_write(g_msg_desktop_prefix);
    device_write(action);
    device_write(" ");
    if (display != 0) {
        device_write(display);
    } else {
        device_write_bytes(name, len);
    }
    device_write(ok != 0u ? g_msg_ok_suffix : g_msg_fail_suffix);
}

static void print_desktop_status(void) {
    char digits[12];
    uint64_t update_current = 0u;
    uint64_t update_target = 0u;
    uint64_t update_flags = 0u;
    struct luna_link_peer peer;
    uint32_t pair_ok = 0u;

    device_write(g_msg_desktop_prefix);
    device_write(g_msg_status_launcher);
    device_write(g_launcher_open != 0u ? g_msg_status_open : g_msg_status_closed);
    device_write(g_msg_status_control);
    device_write(g_control_open != 0u ? g_msg_status_open : g_msg_status_closed);
    device_write(g_msg_status_theme);
    append_u32_decimal(digits, g_theme_variant);
    device_write(digits);
    device_write(g_msg_status_selected);
    if (g_launcher_index < g_package_count) {
        const char *label = package_label_by_index(g_launcher_index);
        device_write(label != 0 ? label : g_package_catalog[g_launcher_index].name);
    } else {
        device_write("none");
    }
    device_write(g_msg_status_update);
    if (update_check_status(&update_current, &update_target, &update_flags) == LUNA_UPDATE_OK) {
        device_write(update_state_label(update_flags));
    } else {
        device_write("unavailable");
    }
    zero_bytes(&peer, sizeof(peer));
    pair_ok = settings_pair_status(&peer);
    device_write(g_msg_status_pair);
    device_write(pair_ok != 0u ? pair_state_label(peer.flags) : "unavailable");
    device_write(g_msg_status_policy);
    device_write(pair_ok != 0u && (peer.flags & 0x2u) != 0u ? "allow" : "deny");
    device_write(g_msg_newline);
}

static void print_desktop_hit(void) {
    char digits[12];
    uint32_t kind = 0u;
    uint32_t window_id = 0u;
    uint32_t glyph = 0u;

    (void)graphics_render_desktop_state(0x30u, g_launcher_index, g_pointer_x, g_pointer_y, &kind, &window_id, &glyph);
    device_write(g_msg_desktop_prefix);
    device_write(g_msg_hit_kind);
    append_u32_decimal(digits, kind);
    device_write(digits);
    device_write(g_msg_hit_window);
    append_u32_decimal(digits, window_id);
    device_write(digits);
    device_write(g_msg_hit_glyph);
    append_u32_decimal(digits, glyph);
    device_write(digits);
    device_write(g_msg_newline);
}

static void desktop_launch_named_app(const char *name, size_t len) {
    uint32_t index = 0u;
    uint32_t ok = 0u;

    if (!package_display_name(name, len, &index)) {
        write_desktop_named_result("launch", name, len, 0u);
        return;
    }
    g_launcher_index = index;
    if (g_launcher_open != 0u) {
        sync_launcher(0u);
    }
    ok = run_app(name, len);
    write_desktop_named_result("launch", name, len, ok);
}

static int notes_selected(void) {
    if (g_desktop_mode != 0u) {
        return active_window_matches("Notes");
    }
    if (g_launcher_index >= g_package_count) {
        return 0;
    }
    return text_equals(g_package_catalog[g_launcher_index].label, "Notes");
}

static void refresh_notes_app(void) {
    if (g_desktop_booted != 0u) {
        (void)render_notes_window();
        return;
    }
    {
        static const char notes_name[] = "Notes";
        (void)run_app(notes_name, sizeof(notes_name) - 1u);
    }
}

static void set_note_body_text(const char *text) {
    copy_single_line(g_note_body, sizeof(g_note_body), text);
    persist_note_state();
    refresh_notes_app();
}

static int active_window_matches(const char *title) {
    char active_title[16];

    if (graphics_query_window(0u, 0, 0, 0, 0, 0, 0, 0, active_title, sizeof(active_title)) != LUNA_GRAPHICS_OK) {
        return 0;
    }
    return text_equals(active_title, title);
}

static int files_selected(void) {
    const char *label = 0;

    if (g_desktop_mode != 0u) {
        if (active_window_matches("Files")) {
            return 1;
        }
    }
    if (g_launcher_index >= g_package_count) {
        return 0;
    }
    label = package_label_by_index(g_launcher_index);
    if (label != 0 && text_equals(label, "Files")) {
        return 1;
    }
    if (text_equals(g_package_catalog[g_launcher_index].label, "Files")) {
        return 1;
    }
    if (text_equals(g_package_catalog[g_launcher_index].name, "Files") ||
        text_equals(g_package_catalog[g_launcher_index].name, "files") ||
        text_equals(g_package_catalog[g_launcher_index].name, "files.luna")) {
        return 1;
    }
    return 0;
}

static int files_select_index(uint32_t index) {
    uint32_t count = file_visible_object_count();

    if (!files_selected() || g_launcher_open != 0u || g_control_open != 0u || index >= count) {
        return 0;
    }
    g_files_selection = index;
    persist_files_view_state();
    refresh_files_app();
    return 1;
}

static void refresh_files_app(void) {
    if (g_desktop_booted != 0u) {
        (void)render_files_window();
        return;
    }
    {
        static const char files_name[] = "Files";
        (void)run_app(files_name, sizeof(files_name) - 1u);
    }
}

static uint32_t file_visible_object_count(void) {
    uint32_t count = 0u;
    if (query_visible_file_rows(g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count) != LUNA_DATA_OK) {
        return 0u;
    }
    return count;
}

static int files_step_selection(int32_t delta) {
    uint32_t count = file_visible_object_count();

    if (!files_selected() || g_launcher_open != 0u || g_control_open != 0u) {
        return 0;
    }
    if (count == 0u) {
        g_files_selection = 0u;
        persist_files_view_state();
        return 1;
    }
    if (delta > 0) {
        g_files_selection = (g_files_selection + 1u) % count;
    } else if (delta < 0) {
        g_files_selection = g_files_selection == 0u ? (count - 1u) : (g_files_selection - 1u);
    }
    persist_files_view_state();
    refresh_files_app();
    return 1;
}

static int files_open_selection(void) {
    struct luna_query_row row;
    uint32_t count = 0u;
    int fallback_open = 0;

    if (!object_ref_is_null(g_note_object)) {
        fallback_open = 1;
    } else if (!object_ref_is_null(g_theme_object)) {
        fallback_open = 2;
    }

    if (!files_selected() || g_launcher_open != 0u || g_control_open != 0u) {
        return 0;
    }
    if (!prepare_files_surface()) {
        if (fallback_open == 1) {
            launch_named_app("Notes");
            return 1;
        }
        if (fallback_open == 2) {
            cycle_theme();
            refresh_files_app();
            return 1;
        }
        return 0;
    }
    zero_bytes(&row, sizeof(row));
    if (!query_selected_file_row(&row, &count)) {
        launch_named_app("Notes");
        return 1;
    }
    if (g_files_selection >= count) {
        if (fallback_open == 1) {
            launch_named_app("Notes");
            return 1;
        }
        if (fallback_open == 2) {
            cycle_theme();
            refresh_files_app();
            return 1;
        }
        return 0;
    }
    if (row.object_type == LUNA_NOTE_OBJECT_TYPE) {
        launch_named_app("Notes");
        return 1;
    }
    if (row.object_type == LUNA_THEME_OBJECT_TYPE) {
        cycle_theme();
        refresh_files_app();
        return 1;
    }
    if (fallback_open == 1) {
        launch_named_app("Notes");
        return 1;
    }
    if (fallback_open == 2) {
        cycle_theme();
        refresh_files_app();
        return 1;
    }
    return 0;
}

static int append_note_input(uint8_t ch) {
    uint32_t len = text_length(g_note_body);

    if (!notes_selected() || g_launcher_open != 0u || g_control_open != 0u) {
        return 0;
    }
    if (ch == 8u || ch == 127u) {
        if (len != 0u) {
            g_note_body[len - 1u] = '\0';
            persist_note_state();
            refresh_notes_app();
        }
        return 1;
    }
    if (ch == '\r' || ch == '\n') {
        if (len + 3u < sizeof(g_note_body)) {
            g_note_body[len++] = ' ';
            g_note_body[len++] = '/';
            g_note_body[len++] = ' ';
            g_note_body[len] = '\0';
            persist_note_state();
            refresh_notes_app();
        }
        return 1;
    }
    if (ch < 32u || ch > 126u) {
        return 0;
    }
    if (len + 1u < sizeof(g_note_body)) {
        g_note_body[len] = (char)ch;
        g_note_body[len + 1u] = '\0';
        persist_note_state();
        refresh_notes_app();
    }
    return 1;
}

static void sync_pointer(void) {
    (void)graphics_render_desktop_state(0x10u, g_launcher_index, g_pointer_x, g_pointer_y, 0, 0, 0);
}

static uint32_t graphics_query_window(
    uint32_t window_id,
    uint32_t *out_window_id,
    uint32_t *out_x,
    uint32_t *out_y,
    uint32_t *out_width,
    uint32_t *out_height,
    uint32_t *out_minimized,
    uint32_t *out_maximized,
    char *out_title,
    uint64_t out_title_size
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_graphics_gate *gate =
        (volatile struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base;

    if (out_title != 0 && out_title_size != 0u) {
        zero_bytes(out_title, (size_t)out_title_size);
    }
    zero_bytes((void *)(uintptr_t)manifest->graphics_gate_base, sizeof(struct luna_graphics_gate));
    gate->sequence = 110u;
    gate->opcode = LUNA_GRAPHICS_QUERY_WINDOW;
    gate->caller_space = LUNA_SPACE_USER;
    gate->actor_space = LUNA_SPACE_USER;
    gate->cid_low = g_graphics_draw_cid.low;
    gate->cid_high = g_graphics_draw_cid.high;
    gate->window_id = window_id;
    gate->buffer_addr = (uint64_t)(uintptr_t)out_title;
    gate->buffer_size = out_title_size;
    ((graphics_gate_fn_t)(uintptr_t)manifest->graphics_gate_entry)(
        (struct luna_graphics_gate *)(uintptr_t)manifest->graphics_gate_base
    );
    if (gate->status != LUNA_GRAPHICS_OK) {
        return gate->status;
    }
    if (out_window_id != 0) {
        *out_window_id = gate->window_id;
    }
    if (out_x != 0) {
        *out_x = gate->x;
    }
    if (out_y != 0) {
        *out_y = gate->y;
    }
    if (out_width != 0) {
        *out_width = gate->width;
    }
    if (out_height != 0) {
        *out_height = gate->height;
    }
    if (out_minimized != 0) {
        *out_minimized = gate->glyph;
    }
    if (out_maximized != 0) {
        *out_maximized = gate->attr;
    }
    return LUNA_GRAPHICS_OK;
}

static int window_is_live(uint32_t *window_id) {
    uint32_t live_id = 0u;
    if (window_id == 0 || *window_id == 0u) {
        return 0;
    }
    if (graphics_query_window(*window_id, &live_id, 0, 0, 0, 0, 0, 0, 0, 0u) != LUNA_GRAPHICS_OK) {
        *window_id = 0u;
        return 0;
    }
    *window_id = live_id;
    return 1;
}

static void window_fill_line(uint32_t window_id, uint32_t row, const char *text, uint8_t attr) {
    uint32_t col = 0u;
    while (col < 78u) {
        char ch = ' ';
        if (text != 0 && text[col] != '\0') {
            ch = text[col];
        }
        (void)graphics_draw_window_char(window_id, col, row, ch, attr);
        col += 1u;
    }
}

static uint32_t ensure_native_window(
    uint32_t *window_id,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    const char *title
) {
    if (!window_is_live(window_id)) {
        if (graphics_create_window_native(x, y, width, height, title, window_id) != LUNA_GRAPHICS_OK) {
            return 0u;
        }
    }
    return graphics_set_active_window(*window_id) == LUNA_GRAPHICS_OK;
}

static uint32_t render_settings_window(void) {
    if (!ensure_native_window(&g_settings_window_id, 40u, 14u, 17u, 7u, "Settings")) {
        return 0u;
    }
    window_fill_line(g_settings_window_id, 0u, "settings surface", 0x1Fu);
    window_fill_line(g_settings_window_id, 1u, "account host home", 0x1Fu);
    window_fill_line(g_settings_window_id, 2u, "update network pair", 0x1Fu);
    window_fill_line(g_settings_window_id, 3u, "entry desktop", 0x1Fu);
    return 1u;
}

static uint32_t render_files_window(void) {
    char line0[48];
    char line1[48];
    char line2[48];
    char line3[48];
    char line4[48];
    char line5[48];
    char line6[48];
    char line7[48];
    char digits[16];
    uint32_t count = 0u;

    if (!prepare_files_surface()) {
        return 0u;
    }
    if (query_visible_file_rows(g_lasql_data_payload.rows, LUNA_DATA_OBJECT_CAPACITY, &count) != LUNA_DATA_OK) {
        return 0u;
    }
    if (!ensure_native_window(&g_files_window_id, 2u, 2u, 26u, 9u, "Files")) {
        return 0u;
    }
    zero_bytes(line0, sizeof(line0));
    zero_bytes(line1, sizeof(line1));
    zero_bytes(line2, sizeof(line2));
    zero_bytes(line3, sizeof(line3));
    zero_bytes(line4, sizeof(line4));
    zero_bytes(line5, sizeof(line5));
    zero_bytes(line6, sizeof(line6));
    zero_bytes(line7, sizeof(line7));
    zero_bytes(digits, sizeof(digits));
    append_u32_decimal(digits, count);
    copy_single_line(line0, sizeof(line0), "files surface ready");
    copy_single_line(line1, sizeof(line1), "source LaFS DATA");
    copy_single_line(line2, sizeof(line2), "entries ");
    {
        uint32_t cursor = 8u;
        append_text_limited(line2, sizeof(line2), &cursor, digits);
    }
    if (count == 0u) {
        copy_single_line(line3, sizeof(line3), "empty");
    } else {
        build_files_window_row(line3, sizeof(line3), &g_lasql_data_payload.rows[0], 0u, g_files_selection == 0u);
    }
    if (count > 1u) {
        build_files_window_row(line4, sizeof(line4), &g_lasql_data_payload.rows[1], 1u, g_files_selection == 1u);
    }
    if (count > 2u) {
        build_files_window_row(line5, sizeof(line5), &g_lasql_data_payload.rows[2], 2u, g_files_selection == 2u);
    } else {
        copy_single_line(line5, sizeof(line5), "enter opens selected");
    }
    if (count == 0u || g_files_selection >= count) {
        copy_single_line(line6, sizeof(line6), "meta missing");
        copy_single_line(line7, sizeof(line7), "state missing");
    } else {
        build_files_window_meta(line6, sizeof(line6), &g_lasql_data_payload.rows[g_files_selection]);
        build_files_window_ref(line7, sizeof(line7), &g_lasql_data_payload.rows[g_files_selection]);
    }
    window_fill_line(g_files_window_id, 0u, line0, 0x1Fu);
    window_fill_line(g_files_window_id, 1u, line1, 0x1Fu);
    window_fill_line(g_files_window_id, 2u, line2, 0x1Fu);
    window_fill_line(g_files_window_id, 3u, line3, 0x1Fu);
    window_fill_line(g_files_window_id, 4u, line4, 0x1Fu);
    window_fill_line(g_files_window_id, 5u, line5, 0x1Fu);
    window_fill_line(g_files_window_id, 6u, line6, 0x1Fu);
    window_fill_line(g_files_window_id, 7u, line7, 0x1Fu);
    return 1u;
}

static uint32_t render_notes_window(void) {
    if (!ensure_native_window(&g_notes_window_id, 29u, 2u, 27u, 9u, "Notes")) {
        return 0u;
    }
    window_fill_line(g_notes_window_id, 0u, "workspace created", 0x1Fu);
    window_fill_line(g_notes_window_id, 1u, "notes surface ready", 0x1Fu);
    window_fill_line(g_notes_window_id, 2u, g_note_body, 0x1Fu);
    window_fill_line(g_notes_window_id, 3u, "desktop.note edits text", 0x1Fu);
    return 1u;
}

static uint32_t render_console_window(void) {
    if (!ensure_native_window(&g_console_window_id, 2u, 12u, 37u, 11u, "Console")) {
        return 0u;
    }
    window_fill_line(g_console_window_id, 0u, "apps 1-5 or F/N/G/C/H", 0x1Fu);
    window_fill_line(g_console_window_id, 1u, "menu Esc   focus Tab", 0x1Fu);
    window_fill_line(g_console_window_id, 2u, "open Settings Files Notes", 0x1Fu);
    window_fill_line(g_console_window_id, 3u, "window keys close min max", 0x1Fu);
    return 1u;
}

static void print_desktop_window(void) {
    char line[16];
    char title[16];
    uint32_t window_id = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t minimized = 0u;
    uint32_t maximized = 0u;

    if (graphics_query_window(
            0u,
            &window_id,
            &x,
            &y,
            &width,
            &height,
            &minimized,
            &maximized,
            title,
            sizeof(title)
        ) != LUNA_GRAPHICS_OK) {
        device_write(g_msg_desktop_prefix);
        device_write("window none\r\n");
        return;
    }

    device_write(g_msg_desktop_prefix);
    device_write(g_msg_window_id);
    append_u32_decimal(line, window_id);
    device_write(line);
    device_write(g_msg_window_title);
    device_write(title);
    device_write(g_msg_window_x);
    append_u32_decimal(line, x);
    device_write(line);
    device_write(g_msg_window_y);
    append_u32_decimal(line, y);
    device_write(line);
    device_write(g_msg_window_w);
    append_u32_decimal(line, width);
    device_write(line);
    device_write(g_msg_window_h);
    append_u32_decimal(line, height);
    device_write(line);
    device_write(g_msg_window_min);
    append_u32_decimal(line, minimized);
    device_write(line);
    device_write(g_msg_window_max);
    append_u32_decimal(line, maximized);
    device_write(line);
    device_write(g_msg_newline);
}

static int move_pointer_absolute(uint32_t x, uint32_t y) {
    if (x > 79u || y > 24u) {
        return 0;
    }
    g_pointer_x = x;
    g_pointer_y = y;
    g_pointer_active = 1u;
    sync_pointer();
    return 1;
}

static void pointer_nudge(int32_t dx, int32_t dy) {
    int32_t next_x = (int32_t)g_pointer_x + dx;
    int32_t next_y = (int32_t)g_pointer_y + dy;

    if (next_x < 0) {
        next_x = 0;
    }
    if (next_y < 0) {
        next_y = 0;
    }
    if (next_x > 79) {
        next_x = 79;
    }
    if (next_y > 24) {
        next_y = 24;
    }

    if (g_drag_window != 0u) {
        (void)graphics_nudge_window(g_drag_window, next_x - (int32_t)g_pointer_x, next_y - (int32_t)g_pointer_y);
    } else if (g_resize_window != 0u) {
        (void)graphics_resize_window(g_resize_window, next_x - (int32_t)g_pointer_x, next_y - (int32_t)g_pointer_y);
    }
    g_pointer_x = (uint32_t)next_x;
    g_pointer_y = (uint32_t)next_y;
    sync_pointer();
}

static void handle_pointer_click(void) {
    uint32_t kind = 0u;
    uint32_t window_id = 0u;
    uint32_t glyph = 0u;

    (void)graphics_render_desktop_state(0x30u, g_launcher_index, g_pointer_x, g_pointer_y, &kind, &window_id, &glyph);

    if (kind == 3u) {
        sync_launcher(g_launcher_open == 0u ? 1u : 0u);
        return;
    }
    if (kind == 11u) {
        (void)graphics_toggle_control_center();
        return;
    }
    if (kind == 15u) {
        cycle_theme();
        return;
    }
    if (kind == 4u && glyph < g_package_count) {
        if (window_id != 0u) {
            (void)graphics_set_active_window(window_id);
        } else {
            g_launcher_index = glyph;
            launch_selected_app();
        }
        return;
    }
    if (kind == 2u && glyph < g_package_count) {
        g_launcher_index = glyph;
        launch_selected_app();
        sync_launcher(0u);
        return;
    }
    if (kind == 9u && glyph < g_package_count) {
        g_launcher_index = glyph;
        launch_selected_app();
        if (g_launcher_open != 0u) {
            sync_launcher(0u);
        } else {
            sync_pointer();
        }
        return;
    }
    if (kind == 12u) {
        if (glyph == g_files_selection) {
            (void)files_open_selection();
        } else {
            (void)files_select_index(glyph);
        }
        return;
    }
    if (kind == 13u) {
        cycle_theme();
        return;
    }
    if (kind == 14u) {
        sync_launcher(g_launcher_open == 0u ? 1u : 0u);
        return;
    }
    if (kind == 16u) {
        sync_launcher(0u);
        return;
    }
    if (kind == 20u) {
        set_note_body_text("workspace pinned");
        return;
    }
    if (kind == 21u) {
        (void)graphics_toggle_control_center();
        return;
    }
    if (kind == 22u) {
        cycle_theme();
        return;
    }
    if (kind == 23u) {
        static const char files_name[] = "Files";
        run_app(files_name, sizeof(files_name) - 1u);
        return;
    }
    if (kind == 24u) {
        static const char notes_name[] = "Notes";
        run_app(notes_name, sizeof(notes_name) - 1u);
        return;
    }
    if (kind == 17u) {
        cycle_theme();
        return;
    }
    if (kind == 18u) {
        (void)graphics_toggle_control_center();
        return;
    }
    if (kind == 19u) {
        static const char console_name[] = "Console";
        run_app(console_name, sizeof(console_name) - 1u);
        return;
    }
    if (kind == 5u && window_id != 0u) {
        (void)graphics_close_window(window_id);
        return;
    }
    if (kind == 7u && window_id != 0u) {
        (void)graphics_minimize_window(window_id);
        return;
    }
    if (kind == 8u && window_id != 0u) {
        (void)graphics_toggle_maximize_window(window_id);
        return;
    }
    if (kind == 6u && window_id != 0u) {
        g_resize_window = 0u;
        g_drag_window = window_id;
        (void)graphics_set_active_window(window_id);
        return;
    }
    if (kind == 10u && window_id != 0u) {
        g_drag_window = 0u;
        g_resize_window = window_id;
        (void)graphics_set_active_window(window_id);
        return;
    }
    if (kind == 1u && window_id != 0u) {
        (void)graphics_set_active_window(window_id);
        return;
    }
    if (g_launcher_open != 0u) {
        sync_launcher(0u);
    }
}

static void handle_pointer_secondary_click(void) {
    if (g_launcher_open != 0u) {
        sync_launcher(0u);
    } else {
        sync_launcher(1u);
    }
}

static void handle_pointer_event(const struct luna_pointer_event *event) {
    int32_t step_x = event->dx / 6;
    int32_t step_y = event->dy / 6;
    uint32_t buttons = event->buttons;

    g_desktop_shell.last_pointer_buttons = buttons & 0xFFu;
    g_desktop_shell.pointer_events += 1u;
    debug_input_byte("[USERDBG] ptr ", buttons & 0xFFu);

    if (event->dx != 0 && step_x == 0) {
        step_x = event->dx > 0 ? 1 : -1;
    }
    if (event->dy != 0 && step_y == 0) {
        step_y = event->dy > 0 ? 1 : -1;
    }
    if (step_x != 0 || step_y != 0) {
        g_pointer_active = 1u;
        pointer_nudge(step_x, -step_y);
    }
    if (g_pointer_armed == 0u) {
        if (buttons == 0u) {
            g_pointer_armed = 1u;
        }
        g_pointer_buttons = buttons;
        return;
    }
    if (g_pointer_active == 0u) {
        g_pointer_buttons = buttons;
        return;
    }
    if ((buttons & 0x01u) != 0u && (g_pointer_buttons & 0x01u) == 0u) {
        handle_pointer_click();
    }
    if ((buttons & 0x02u) != 0u && (g_pointer_buttons & 0x02u) == 0u) {
        handle_pointer_secondary_click();
    }
    if ((buttons & 0x01u) == 0u) {
        g_drag_window = 0u;
        g_resize_window = 0u;
    }
    g_pointer_buttons = buttons;
}

static int handle_desktop_key(uint8_t ch) {
    if (g_launcher_open != 0u) {
        if (ch == 0x1Bu) {
            sync_launcher(0u);
            return 1;
        }
        if (ch == 0x91u && g_launcher_index > 0u) {
            g_launcher_index -= 1u;
            (void)graphics_render_desktop(0x01u, g_launcher_index);
            return 1;
        }
        if (ch == 0x92u && g_launcher_index + 1u < g_package_count) {
            g_launcher_index += 1u;
            (void)graphics_render_desktop(0x01u, g_launcher_index);
            return 1;
        }
        if (ch >= '1' && ch < (char)('1' + g_package_count)) {
            g_launcher_index = (uint32_t)(ch - '1');
            launch_selected_app();
            sync_launcher(0u);
            return 1;
        }
        if (ch == '\n') {
            launch_selected_app();
            sync_launcher(0u);
            return 1;
        }
        return 1;
    }

    if (ch >= '1' && ch < (char)('1' + g_package_count)) {
        g_launcher_index = (uint32_t)(ch - '1');
        launch_selected_app();
        return 1;
    }
    if (ch == 'f' || ch == 'F') {
        static const char files_name[] = "Files";
        run_app(files_name, sizeof(files_name) - 1u);
        return 1;
    }
    if (ch == 'n' || ch == 'N') {
        static const char notes_name[] = "Notes";
        run_app(notes_name, sizeof(notes_name) - 1u);
        return 1;
    }
    if (ch == 'g' || ch == 'G') {
        static const char guard_name[] = "Guard";
        run_app(guard_name, sizeof(guard_name) - 1u);
        return 1;
    }
    if (ch == 'c' || ch == 'C') {
        static const char console_name[] = "Console";
        run_app(console_name, sizeof(console_name) - 1u);
        return 1;
    }
    if (ch == 'h' || ch == 'H') {
        static const char settings_name[] = "Settings";
        run_app(settings_name, sizeof(settings_name) - 1u);
        return 1;
    }
    if (ch == 0x1Bu) {
        sync_launcher(1u);
        return 1;
    }
    if (ch == 0x81u) {
        static const char files_name[] = "Files";
        run_app(files_name, sizeof(files_name) - 1u);
        return 1;
    }
    if (ch == 0x82u) {
        static const char notes_name[] = "Notes";
        run_app(notes_name, sizeof(notes_name) - 1u);
        return 1;
    }
    if (ch == 0x83u) {
        static const char guard_name[] = "Guard";
        run_app(guard_name, sizeof(guard_name) - 1u);
        return 1;
    }
    if (ch == 0x84u) {
        static const char console_name[] = "Console";
        run_app(console_name, sizeof(console_name) - 1u);
        return 1;
    }
    if (ch == 0x85u) {
        static const char settings_name[] = "Settings";
        run_app(settings_name, sizeof(settings_name) - 1u);
        return 1;
    }
    if (ch == 0x86u) {
        (void)graphics_close_window(0u);
        return 1;
    }
    if (ch == 0x89u) {
        (void)graphics_toggle_control_center();
        return 1;
    }
    if (ch == 0x8Au) {
        cycle_theme();
        return 1;
    }
    if (ch == 0x87u) {
        (void)graphics_minimize_window(0u);
        return 1;
    }
    if (ch == 0x88u) {
        (void)graphics_toggle_maximize_window(0u);
        return 1;
    }
    if (ch == '\t') {
        (void)graphics_set_active_window(0u);
        return 1;
    }
    if (ch == 0x91u) {
        if (files_step_selection(-1)) {
            return 1;
        }
        (void)graphics_nudge_window(0u, 0, -1);
        return 1;
    }
    if (ch == 0x92u) {
        if (files_step_selection(1)) {
            return 1;
        }
        (void)graphics_nudge_window(0u, 0, 1);
        return 1;
    }
    if (ch == '\n' || ch == '\r') {
        if (files_open_selection()) {
            return 1;
        }
        return 0;
    }
    if (ch == 0x93u) {
        (void)graphics_nudge_window(0u, -1, 0);
        return 1;
    }
    if (ch == 0x94u) {
        (void)graphics_nudge_window(0u, 1, 0);
        return 1;
    }
    return 0;
}

static void shell_execute(const char *line, size_t len) {
    size_t cursor = 0u;
    const char *arg0 = 0;
    const char *arg1 = 0;
    const char *arg2 = 0;
    size_t arg0_len = 0u;
    size_t arg1_len = 0u;
    size_t arg2_len = 0u;

    if (len == 0u) {
        return;
    }

    if (len == 4u && chars_equal(line, "help", 4u)) {
        device_write(g_msg_help);
        return;
    }

    if (len == 6u && chars_equal(line, "whoami", 6u)) {
        print_whoami();
        return;
    }

    if (len == 8u && chars_equal(line, "hostname", 8u)) {
        print_hostname();
        return;
    }

    if (len == 2u && chars_equal(line, "id", 2u)) {
        print_identity();
        return;
    }

    if (len == 12u && chars_equal(line, "setup.status", 12u)) {
        print_setup_status();
        return;
    }

    if (len == 11u && chars_equal(line, "home.status", 11u)) {
        print_home_status();
        return;
    }

    if (len > 11u && chars_equal(line, "setup.init ", 11u)) {
        cursor = 11u;
        if (!split_token(line, len, &cursor, &arg0, &arg0_len) ||
            !split_token(line, len, &cursor, &arg1, &arg1_len) ||
            !split_token(line, len, &cursor, &arg2, &arg2_len)) {
            device_write(g_msg_setup_usage);
            return;
        }
        if (g_user_setup_required == 0u) {
            device_write(g_msg_setup_already);
            return;
        }
        if (!create_first_user_system(arg0, arg0_len, arg1, arg1_len, arg2, arg2_len)) {
            device_write(g_msg_setup_fail);
            device_write(g_msg_setup_hint);
            return;
        }
        g_desktop_mode = session_script_present() ? 0u : 1u;
        device_write(g_msg_setup_ok);
        device_write(g_msg_login_ok);
        if (g_desktop_mode != 0u && input_no_local_recovery_active() == 0u) {
            device_write(g_msg_desktop_ready);
            boot_desktop_session();
        }
        announce_input_recovery_mode();
        return;
    }

    if (len > 6u && chars_equal(line, "login ", 6u)) {
        cursor = 6u;
        if (!split_token(line, len, &cursor, &arg0, &arg0_len) ||
            !split_token(line, len, &cursor, &arg1, &arg1_len)) {
            device_write(g_msg_login_usage);
            return;
        }
        if (g_user_setup_required != 0u) {
            device_write(g_msg_setup_required);
            device_write(g_msg_setup_hint);
            return;
        }
        if (!login_user_session(arg0, arg0_len, arg1, arg1_len)) {
            device_write(g_msg_login_fail);
            device_write(g_msg_login_hint);
            return;
        }
        g_desktop_mode = session_script_present() ? 0u : 1u;
        device_write(g_msg_login_ok);
        if (g_desktop_mode != 0u
            && g_desktop_booted == 0u
            && input_no_local_recovery_active() == 0u) {
            device_write(g_msg_desktop_ready);
            boot_desktop_session();
        }
        announce_input_recovery_mode();
        return;
    }

    if (len == 6u && chars_equal(line, "logout", 6u)) {
        security_close_user_session();
        reset_active_user_projection();
        g_desktop_mode = 0u;
        device_write(g_msg_logout_ok);
        return;
    }

    if (g_user_setup_required != 0u) {
        device_write(g_msg_setup_required);
        device_write(g_msg_setup_hint);
        return;
    }

    if (g_user_logged_in == 0u &&
        !(len == 6u && chars_equal(line, "whoami", 6u)) &&
        !(len == 8u && chars_equal(line, "hostname", 8u)) &&
        !(len == 2u && chars_equal(line, "id", 2u)) &&
        !(len == 12u && chars_equal(line, "setup.status", 12u)) &&
        !(len == 11u && chars_equal(line, "home.status", 11u))) {
        device_write(g_msg_login_required);
        device_write(g_msg_login_hint);
        return;
    }

    if (len == 11u && chars_equal(line, "list-spaces", 11u)) {
        print_space_list();
        return;
    }

    if (len == 12u && chars_equal(line, "list-devices", 12u)) {
        print_device_list();
        return;
    }

    if (len == 11u && chars_equal(line, "lane-census", 11u)) {
        print_lane_census();
        return;
    }

    if (len == 8u && chars_equal(line, "pci-scan", 8u)) {
        print_pci_scan();
        return;
    }

    if (len == 8u && chars_equal(line, "net.loop", 8u)) {
        run_net_loopback();
        return;
    }

    if (len == 10u && chars_equal(line, "net.status", 10u)) {
        run_net_status();
        return;
    }

    if (len == 11u && chars_equal(line, "net.connect", 11u)) {
        run_net_connect("external");
        return;
    }

    if (len > 12u && chars_equal(line, "net.connect ", 12u)) {
        run_net_connect(line + 12u);
        return;
    }

    if (len == 12u && chars_equal(line, "net.external", 12u)) {
        run_net_external();
        return;
    }

    if (len == 11u && chars_equal(line, "net.inbound", 11u)) {
        run_net_inbound();
        return;
    }

    if (len > 9u && chars_equal(line, "net.send ", 9u)) {
        run_net_send(line + 9u, len - 9u);
        return;
    }

    if (len == 8u && chars_equal(line, "net.recv", 8u)) {
        run_net_recv();
        return;
    }

    if (len >= 8u && chars_equal(line, "net.info", 8u)) {
        print_net_info();
        return;
    }

    if (len == 9u && chars_equal(line, "space-map", 9u)) {
        print_space_map();
        return;
    }

    if (len == 9u && chars_equal(line, "space-log", 9u)) {
        print_space_log();
        return;
    }

    if (len == 9u && chars_equal(line, "list-apps", 9u)) {
        print_package_list();
        return;
    }

    if (len == 13u && chars_equal(line, "lasql.catalog", 13u)) {
        print_lasql_catalog();
        return;
    }

    if (len == 11u && chars_equal(line, "lasql.files", 11u)) {
        print_lasql_files();
        return;
    }

    if (len == 10u && chars_equal(line, "lasql.logs", 10u)) {
        print_lasql_logs();
        return;
    }

    if (len == 9u && chars_equal(line, "cap-count", 9u)) {
        print_cap_count();
        return;
    }

    if (len == 8u && chars_equal(line, "cap-list", 8u)) {
        print_cap_list();
        return;
    }

    if (len == 9u && chars_equal(line, "seal-list", 9u)) {
        print_seal_list();
        return;
    }

    if (len == 10u && chars_equal(line, "store-info", 10u)) {
        print_store_info();
        return;
    }

    if (len == 15u && chars_equal(line, "settings.status", 15u)) {
        print_settings_status();
        return;
    }

    if (len == 13u && chars_equal(line, "update.status", 13u)) {
        uint64_t current = 0u;
        uint64_t target = 0u;
        uint64_t flags = 0u;
        char digits[24];
        if (update_check_status(&current, &target, &flags) != LUNA_UPDATE_OK) {
            device_write("update unavailable\r\n");
            device_write("update mode=not-ready action=not ready\r\n");
            device_write("update action=not ready\r\n");
            return;
        }
        device_write("update mode=");
        device_write(flags == LUNA_UPDATE_TXN_STATE_STAGED ? surface_mode_label(1u) : surface_mode_label(2u));
        device_write(" action=");
        device_write(update_action_label(flags));
        device_write(g_msg_newline);
        device_write("update state=");
        device_write(update_state_label(flags));
        device_write(" current=");
        append_u64_decimal(digits, current);
        device_write(digits);
        device_write(" target=");
        append_u64_decimal(digits, target);
        device_write(digits);
        device_write(" user=");
        device_write(g_username);
        device_write(" host=");
        device_write(g_hostname);
        device_write(g_msg_newline);
        if (flags == LUNA_UPDATE_TXN_STATE_STAGED) {
            device_write("update action=apply-ready\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_COMMITTED) {
            device_write("update action=reboot-confirm\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_ACTIVATED) {
            device_write("update action=applied\r\n");
        } else if (flags == LUNA_UPDATE_TXN_STATE_FAILED) {
            device_write("update action=failed\r\n");
        } else {
            device_write("update action=check-only\r\n");
        }
        return;
    }

    if (len == 12u && chars_equal(line, "update.apply", 12u)) {
        uint64_t current = 0u;
        uint64_t target = 0u;
        uint64_t flags = 0u;
        char digits[24];
        if (update_apply_status(LUNA_UPDATE_FLAG_NONE, &current, &target, &flags) != LUNA_UPDATE_OK) {
            device_write("update.apply fail\r\n");
            device_write("update.result=unavailable\r\n");
            return;
        }
        device_write("update.apply ");
        if (flags == LUNA_UPDATE_TXN_STATE_COMMITTED) {
            device_write("ok");
        } else if (flags == LUNA_UPDATE_TXN_STATE_FAILED) {
            device_write("fail");
        } else {
            device_write("noop");
        }
        device_write(g_msg_newline);
        device_write("update.result state=");
        device_write(update_state_label(flags));
        device_write(" current=");
        append_u64_decimal(digits, current);
        device_write(digits);
        device_write(" target=");
        append_u64_decimal(digits, target);
        device_write(digits);
        device_write(g_msg_newline);
        return;
    }

    if (len == 15u && chars_equal(line, "update.rollback", 15u)) {
        uint64_t current = 0u;
        uint64_t target = 0u;
        uint64_t flags = 0u;
        char digits[24];
        if (update_apply_status(LUNA_UPDATE_FLAG_ROLLBACK, &current, &target, &flags) != LUNA_UPDATE_OK) {
            device_write("update.rollback fail\r\n");
            device_write("update.result=unavailable\r\n");
            return;
        }
        device_write("update.rollback ");
        if (flags == LUNA_UPDATE_TXN_STATE_ROLLED_BACK) {
            device_write("ok");
        } else {
            device_write("noop");
        }
        device_write(g_msg_newline);
        device_write("update.result state=");
        device_write(update_state_label(flags));
        device_write(" current=");
        append_u64_decimal(digits, current);
        device_write(digits);
        device_write(" target=");
        append_u64_decimal(digits, target);
        device_write(digits);
        device_write(g_msg_newline);
        return;
    }

    if (len == 14u && chars_equal(line, "pairing.status", 14u)) {
        struct luna_link_peer peer;
        zero_bytes(&peer, sizeof(peer));
        if (network_pair_peer("loop", &peer) != LUNA_NETWORK_OK) {
            device_write("pairing unavailable\r\n");
            device_write("pairing mode=not-ready action=not ready\r\n");
            device_write("pairing action=not ready\r\n");
            return;
        }
        device_write("pairing mode=read-only action=trust loop peer\r\n");
        device_write("pairing state=");
        device_write(pair_state_label(peer.flags));
        device_write(" peer=");
        {
            char digits[12];
            append_u32_decimal(digits, peer.peer_id);
            device_write(digits);
        }
        device_write(" user=");
        device_write(g_username);
        device_write(" host=");
        device_write(g_hostname);
        device_write(g_msg_newline);
        device_write("pairing action=trust loop peer\r\n");
        return;
    }

    if (len == 11u && chars_equal(line, "store-check", 11u)) {
        print_store_check();
        return;
    }

    if (len > 11u && chars_equal(line, "revoke-cap ", 11u)) {
        revoke_cap_domain(line + 11u, len - 11u);
        return;
    }

    if (len > 16u &&
        chars_equal(line, "package.install", 15u) &&
        line[15] == ' ') {
        uint32_t status = package_install_named(line + 16u, len - 16u);
        device_write(status == LUNA_PACKAGE_OK ? "package.install ok\r\n" : "package.install fail\r\n");
        print_package_feedback("install", line + 16u, len - 16u, status);
        return;
    }

    if (len > 15u && chars_equal(line, "package.remove ", 15u)) {
        uint32_t status = package_remove_named(line + 15u, len - 15u);
        device_write(status == LUNA_PACKAGE_OK ? "package.remove ok\r\n" : "package.remove fail\r\n");
        print_package_feedback("remove", line + 15u, len - 15u, status);
        return;
    }

    if (len == 14u && chars_equal(line, "desktop.status", 14u)) {
        print_desktop_status();
        return;
    }

    if (len == 14u && chars_equal(line, "desktop.window", 14u)) {
        print_desktop_window();
        return;
    }

    if (len == 11u && chars_equal(line, "desktop.hit", 11u)) {
        print_desktop_hit();
        return;
    }

    if (len == 12u && chars_equal(line, "desktop.boot", 12u)) {
        boot_desktop_session();
        write_desktop_result("boot", 1u);
        return;
    }

    if (len == 13u && chars_equal(line, "desktop.focus", 13u)) {
        write_desktop_result("focus", graphics_set_active_window(0u) == LUNA_GRAPHICS_OK);
        return;
    }

    if (len == 12u && chars_equal(line, "desktop.menu", 12u)) {
        sync_launcher(g_launcher_open == 0u ? 1u : 0u);
        write_desktop_result("menu", 1u);
        return;
    }

    if (len == 15u && chars_equal(line, "desktop.control", 15u)) {
        write_desktop_result("control", graphics_toggle_control_center() == LUNA_GRAPHICS_OK);
        return;
    }

    if (len == 16u && chars_equal(line, "desktop.settings", 16u)) {
        write_desktop_result("settings", open_settings_surface());
        return;
    }

    if (len == 13u && chars_equal(line, "desktop.theme", 13u)) {
        cycle_theme();
        write_desktop_result("theme", 1u);
        return;
    }

    if (len > 16u && chars_equal(line, "desktop.pointer ", 16u)) {
        uint32_t x = 0u;
        uint32_t y = 0u;
        size_t used = 0u;
        size_t cursor = 16u;

        if (!parse_u32_token(line + cursor, len - cursor, &x, &used)) {
            write_desktop_result("pointer", 0u);
            return;
        }
        cursor += used;
        if (cursor >= len || line[cursor] != ' ') {
            write_desktop_result("pointer", 0u);
            return;
        }
        cursor += 1u;
        if (!parse_u32_token(line + cursor, len - cursor, &y, &used) || cursor + used != len) {
            write_desktop_result("pointer", 0u);
            return;
        }
        write_desktop_result("pointer", move_pointer_absolute(x, y));
        return;
    }

    if (len == 13u && chars_equal(line, "desktop.click", 13u)) {
        handle_pointer_click();
        write_desktop_result("click", 1u);
        return;
    }

    if (len == 18u && chars_equal(line, "desktop.files.prev", 18u)) {
        write_desktop_result("files.prev", files_step_selection(-1));
        return;
    }

    if (len == 18u && chars_equal(line, "desktop.files.next", 18u)) {
        write_desktop_result("files.next", files_step_selection(1));
        return;
    }

    if (len == 18u && chars_equal(line, "desktop.files.open", 18u)) {
        write_desktop_result("files.open", files_open_selection());
        return;
    }

    if (len > 13u && chars_equal(line, "desktop.note ", 13u)) {
        copy_single_line(g_note_body, sizeof(g_note_body), line + 13u);
        persist_note_state();
        refresh_notes_app();
        write_desktop_result("note", 1u);
        return;
    }

    if (len == 16u && chars_equal(line, "desktop.minimize", 16u)) {
        write_desktop_result("minimize", graphics_minimize_window(0u) == LUNA_GRAPHICS_OK);
        return;
    }

    if (len == 16u && chars_equal(line, "desktop.maximize", 16u)) {
        write_desktop_result("maximize", graphics_toggle_maximize_window(0u) == LUNA_GRAPHICS_OK);
        return;
    }

    if (len == 13u && chars_equal(line, "desktop.close", 13u)) {
        uint32_t ok = graphics_set_active_window(0u) == LUNA_GRAPHICS_OK &&
            graphics_close_window(0u) == LUNA_GRAPHICS_OK;
        write_desktop_result("close", ok);
        return;
    }

    if (len > 15u && chars_equal(line, "desktop.launch ", 15u)) {
        desktop_launch_named_app(line + 15u, len - 15u);
        return;
    }

    if (len == 4u && chars_equal(line, "exit", 4u)) {
        reset_active_user_projection();
        g_desktop_mode = 0u;
        device_write(g_msg_exit);
        device_write(g_msg_login_hint);
        return;
    }

    if (len > 4u && chars_equal(line, "run ", 4u)) {
        uint32_t index = 0u;
        const char *display = package_display_name(line + 4u, len - 4u, &index);
        const char *resolved_name = line + 4u;
        size_t resolved_len = len - 4u;
        device_write(g_msg_run_prefix);
        if (display != 0) {
            device_write(display);
            if (index < g_package_count) {
                resolved_name = g_package_catalog[index].name;
                resolved_len = fixed_text_len(g_package_catalog[index].name, 16u);
            }
        } else {
            device_write_bytes(line + 4u, len - 4u);
        }
        device_write(g_msg_run_suffix);
        run_app(resolved_name, resolved_len);
        return;
    }

    device_write(g_msg_unknown);
    device_write(g_msg_run_hint);
}

static void run_session_script(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_session_script_header *header =
        (volatile struct luna_session_script_header *)(uintptr_t)manifest->session_script_base;
    const char *script = (const char *)(uintptr_t)(manifest->session_script_base + sizeof(struct luna_session_script_header));
    uint64_t limit = 0u;
    char line[80];
    size_t len = 0u;

    if (manifest->session_script_size <= sizeof(struct luna_session_script_header)) {
        return;
    }
    if (header->magic != LUNA_SESSION_SCRIPT_MAGIC) {
        return;
    }

    limit = manifest->session_script_size - sizeof(struct luna_session_script_header);
    if (header->command_bytes == 0u || header->command_bytes > limit) {
        return;
    }

    for (uint64_t i = 0u; i < header->command_bytes; ++i) {
        char ch = script[i];

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (len == 0u) {
                continue;
            }
            line[len] = '\0';
            device_write_bytes(line, len);
            device_write(g_msg_newline);
            log_session_script_command(line, len);
            shell_execute(line, len);
            len = 0u;
            continue;
        }
        if ((uint8_t)ch < 32u || (uint8_t)ch > 126u) {
            continue;
        }
        if (len + 1u < sizeof(line)) {
            line[len] = ch;
            len += 1u;
        }
    }

    if (len != 0u) {
        line[len] = '\0';
        device_write_bytes(line, len);
        device_write(g_msg_newline);
        shell_execute(line, len);
    }
}

static __attribute__((unused)) void shell_loop(void) {
    char line[80];
    size_t len = 0u;
    uint32_t line_source = USER_INPUT_SOURCE_NONE;
    uint32_t suppress_boot_noise = 1u;
    uint32_t suppress_line = 0u;

    if (input_shell_prompt_needed() != 0u) {
        print_prompt();
    }
    for (;;) {
        struct luna_pointer_event pointer;
        uint8_t ch = 0u;
        uint32_t input_source = USER_INPUT_SOURCE_NONE;
        if (device_input_read_pointer(&pointer) != 0u && input_pointer_path_allowed() != 0u) {
            handle_pointer_event(&pointer);
        }
        uint64_t got = read_user_input_key(&ch, &input_source);

        if (got == 0u) {
            continue;
        }
        g_desktop_shell.last_key = ch;
        g_desktop_shell.key_events += 1u;
        debug_input_byte("[USERDBG] key ", ch);
        log_user_input_event(input_source, ch);
        if (g_desktop_mode != 0u && input_pointer_path_allowed() != 0u) {
            sync_pointer();
        }

        if (input_source == USER_INPUT_SOURCE_KEYBOARD
            && input_desktop_keyboard_allowed() != 0u
            && handle_desktop_key(ch)) {
            if (g_user_input_debug_budget != 0u) {
                debug_input_byte("[USER] desktop key=", ch);
                g_user_input_debug_budget -= 1u;
            }
            continue;
        }

        if (input_source == USER_INPUT_SOURCE_KEYBOARD
            && input_desktop_text_entry_allowed() != 0u
            && append_note_input(ch)) {
            continue;
        }

        if (input_source == USER_INPUT_SOURCE_KEYBOARD && input_desktop_keyboard_allowed() != 0u) {
            if (ch == '\r' || ch == '\n') {
                sync_launcher(g_launcher_open == 0u ? 1u : 0u);
            }
            continue;
        }

        if (suppress_line != 0u) {
            if (ch == '\r' || ch == '\n') {
                suppress_line = 0u;
                len = 0u;
                line_source = USER_INPUT_SOURCE_NONE;
            }
            continue;
        }

        if (suppress_boot_noise != 0u && len == 0u && ch == '[') {
            suppress_line = 1u;
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            suppress_boot_noise = 0u;
            device_write(g_msg_newline);
            line[len] = '\0';
            if (len != 0u) {
                uint32_t command_source = line_source != USER_INPUT_SOURCE_NONE ? line_source : input_source;
                log_shell_command("accept", command_source, line, len);
                log_shell_command("execute", command_source, line, len);
            }
            shell_execute(line, len);
            len = 0u;
            line_source = USER_INPUT_SOURCE_NONE;
            if (g_desktop_mode == 0u
                || input_source == USER_INPUT_SOURCE_OPERATOR
                || input_no_local_recovery_active() != 0u) {
                print_prompt();
            }
            continue;
        }

        if (ch == 8u || ch == 127u) {
            if (len != 0u) {
                static const char g_bs[] = "\b \b";
                len -= 1u;
                device_write(g_bs);
                if (len == 0u) {
                    line_source = USER_INPUT_SOURCE_NONE;
                }
            }
            continue;
        }

        if (ch < 32u || ch > 126u) {
            continue;
        }
        if (len + 1u >= sizeof(line)) {
            continue;
        }
        line[len] = (char)ch;
        len += 1u;
        line[len] = '\0';
        if (line_source == USER_INPUT_SOURCE_NONE) {
            line_source = input_source;
        }
        {
            char out[2];
            out[0] = (char)ch;
            out[1] = '\0';
            device_write(out);
        }
        continue;
    }
}

void SYSV_ABI user_entry_boot(const struct luna_bootview *bootview) {
    (void)bootview;

    serial_write_debug("[USERDBG] enter\r\n");
    if (request_capability(LUNA_CAP_USER_SHELL, &g_shell_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_USER_AUTH, &g_user_auth_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_LIFE_READ, &g_life_read_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_SYSTEM_QUERY, &g_system_query_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PROGRAM_LOAD, &g_program_load_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PROGRAM_START, &g_program_start_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_SEED, &g_data_seed_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_POUR, &g_data_pour_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_GATHER, &g_data_gather_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DATA_QUERY, &g_data_query_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DEVICE_LIST, &g_device_list_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DEVICE_READ, &g_device_read_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_DEVICE_WRITE, &g_device_write_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_OBSERVE_READ, &g_observe_read_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_NETWORK_SEND, &g_network_send_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_NETWORK_RECV, &g_network_recv_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_NETWORK_PAIR, &g_network_pair_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_NETWORK_SESSION, &g_network_session_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_GRAPHICS_DRAW, &g_graphics_draw_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PACKAGE_LIST, &g_package_list_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PACKAGE_INSTALL, &g_package_install_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_UPDATE_CHECK, &g_update_check_cid) != LUNA_GATE_OK) {
        serial_write_debug("[USERDBG] cap fail\r\n");
        return;
    }

    serial_write_debug("[USERDBG] caps ok\r\n");
    (void)refresh_package_catalog();
    refresh_user_system_state();
    load_theme_state();
    load_note_state();
    load_files_view_state();
    if (g_user_logged_in != 0u) {
        persist_note_state();
        persist_theme_state();
        (void)ensure_user_documents_set();
        persist_files_view_state();
    }
    sync_desktop_shell_state();
    refresh_input_recovery_state();
    g_user_input_debug_budget = 64u;
    g_user_command_debug_budget = 32u;
    g_session_script_debug_budget = 64u;
    g_desktop_mode = (g_user_logged_in != 0u && !session_script_present()) ? 1u : 0u;
    drain_input_noise(LUNA_DEVICE_ID_INPUT0, 64u);
    drain_pointer_noise(64u);
    serial_write_debug("[USERDBG] drain ok\r\n");
    device_write(g_msg_shell_ready);
    device_write(LUNA_OS_VERSION_TEXT);
    device_write(g_msg_newline);
    device_write(g_msg_input_lane);
    if (g_user_setup_required != 0u) {
        device_write(g_msg_setup_required);
        device_write(g_msg_setup_hint);
    } else if (g_user_logged_in == 0u) {
        device_write(g_msg_login_required);
        device_write(g_msg_login_hint);
    } else if (input_no_local_recovery_active() == 0u) {
        device_write(g_msg_desktop_ready);
    }
    announce_input_recovery_mode();
    serial_write_debug("[USERDBG] msg ok\r\n");
    if (input_pointer_path_allowed() != 0u) {
        sync_pointer();
    } else {
        g_pointer_active = 0u;
        g_pointer_buttons = 0u;
        g_drag_window = 0u;
        g_resize_window = 0u;
    }
    if (g_desktop_mode != 0u
        && g_user_logged_in != 0u
        && input_no_local_recovery_active() == 0u) {
        boot_desktop_session();
    }
    run_session_script();
    shell_loop();
}
