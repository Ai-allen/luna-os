#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define LUNA_MANIFEST_ADDR 0x39000ull

typedef void (SYSV_ABI *security_gate_fn_t)(struct luna_gate *gate);
typedef void (SYSV_ABI *lifecycle_gate_fn_t)(struct luna_lifecycle_gate *gate);
typedef void (SYSV_ABI *program_gate_fn_t)(struct luna_program_gate *gate);

static struct luna_cid g_shell_cid = {0, 0};
static struct luna_cid g_life_read_cid = {0, 0};
static struct luna_cid g_program_load_cid = {0, 0};
static struct luna_cid g_program_start_cid = {0, 0};

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_putc(char value) {
    while ((inb(0x3FD) & 0x20) == 0) {
    }
    outb(0x3F8, (uint8_t)value);
}

static int serial_can_read(void) {
    return (inb(0x3FD) & 0x01) != 0;
}

static char serial_getc(void) {
    while (!serial_can_read()) {
    }
    return (char)inb(0x3F8);
}

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void zero_bytes(void *ptr, size_t len) {
    uint8_t *out = (uint8_t *)ptr;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0;
    }
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

static const char *space_name(uint32_t space_id) {
    if (space_id == LUNA_SPACE_BOOT) return "BOOT";
    if (space_id == LUNA_SPACE_SECURITY) return "SECURITY";
    if (space_id == LUNA_SPACE_DATA) return "DATA";
    if (space_id == LUNA_SPACE_GRAPHICS) return "GRAPHICS";
    if (space_id == LUNA_SPACE_DEVICE) return "DEVICE";
    if (space_id == LUNA_SPACE_NETWORK) return "NETWORK";
    if (space_id == LUNA_SPACE_SYSTEM) return "SYSTEM";
    if (space_id == LUNA_SPACE_USER) return "USER";
    if (space_id == LUNA_SPACE_TIME) return "TIME";
    if (space_id == LUNA_SPACE_LIFECYCLE) return "LIFECYCLE";
    if (space_id == LUNA_SPACE_OBSERVABILITY) return "OBSERVE";
    if (space_id == LUNA_SPACE_AI) return "AI";
    if (space_id == LUNA_SPACE_PROGRAM) return "PROGRAM";
    if (space_id == LUNA_SPACE_PACKAGE) return "PACKAGE";
    if (space_id == LUNA_SPACE_UPDATE) return "UPDATE";
    if (space_id == LUNA_SPACE_TEST) return "TEST";
    return "UNKNOWN";
}

static void print_prompt(void) {
    serial_write("luna:~$ ");
}

static void print_help(void) {
    serial_write("help list-spaces run <app> exit\r\n");
}

static void print_spaces(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate =
        (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
    struct luna_unit_record *records =
        (struct luna_unit_record *)(uintptr_t)manifest->list_buffer_base;

    zero_bytes((void *)(uintptr_t)manifest->list_buffer_base, manifest->list_buffer_size);
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 60;
    gate->opcode = LUNA_LIFE_READ_UNITS;
    gate->cid_low = g_life_read_cid.low;
    gate->cid_high = g_life_read_cid.high;
    gate->buffer_addr = manifest->list_buffer_base;
    gate->buffer_size = manifest->list_buffer_size;
    ((lifecycle_gate_fn_t)(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );

    if (gate->status != LUNA_LIFE_OK) {
        serial_write("[USER] list-spaces failed\r\n");
        return;
    }

    serial_write("[USER] live spaces:");
    for (uint32_t i = 0; i < gate->result_count; ++i) {
        serial_putc(' ');
        serial_write(space_name(records[i].space_id));
    }
    serial_write("\r\n");
}

static void run_app(const char *name) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_program_gate *gate =
        (volatile struct luna_program_gate *)(uintptr_t)manifest->program_gate_base;

    zero_bytes((void *)(uintptr_t)manifest->program_gate_base, sizeof(struct luna_program_gate));
    gate->sequence = 70;
    gate->opcode = LUNA_PROGRAM_LOAD_APP;
    gate->cid_low = g_program_load_cid.low;
    gate->cid_high = g_program_load_cid.high;
    for (size_t i = 0; i < 16; ++i) {
        gate->name[i] = 0;
        if (name[i] == '\0') {
            break;
        }
        gate->name[i] = name[i];
    }
    ((program_gate_fn_t)(uintptr_t)manifest->program_gate_entry)(
        (struct luna_program_gate *)(uintptr_t)manifest->program_gate_base
    );
    if (gate->status != LUNA_PROGRAM_OK) {
        serial_write("[USER] run failed\r\n");
        return;
    }

    gate->sequence = 71;
    gate->opcode = LUNA_PROGRAM_START_APP;
    gate->cid_low = g_program_start_cid.low;
    gate->cid_high = g_program_start_cid.high;
    ((program_gate_fn_t)(uintptr_t)manifest->program_gate_entry)(
        (struct luna_program_gate *)(uintptr_t)manifest->program_gate_base
    );
    if (gate->status != LUNA_PROGRAM_OK) {
        serial_write("[USER] start failed\r\n");
    }
}

static int text_equal(const char *lhs, const char *rhs) {
    while (*lhs != '\0' || *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }
    return 1;
}

static void execute_command(char *line) {
    while (*line == ' ') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }
    if (text_equal(line, "help")) {
        print_help();
        return;
    }
    if (text_equal(line, "list-spaces")) {
        print_spaces();
        return;
    }
    if (line[0] == 'r' && line[1] == 'u' && line[2] == 'n' && line[3] == ' ') {
        run_app(line + 4);
        return;
    }
    serial_write("[USER] unknown command\r\n");
}

void SYSV_ABI user_entry_boot(const struct luna_bootview *bootview) {
    char line[64];
    size_t len = 0;

    (void)bootview;

    if (request_capability(LUNA_CAP_USER_SHELL, &g_shell_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_LIFE_READ, &g_life_read_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PROGRAM_LOAD, &g_program_load_cid) != LUNA_GATE_OK ||
        request_capability(LUNA_CAP_PROGRAM_START, &g_program_start_cid) != LUNA_GATE_OK) {
        serial_write("[USER] shell capability fail\r\n");
        return;
    }

    serial_write("[USER] shell ready\r\n");
    print_prompt();
    while (1) {
        char ch = serial_getc();
        if (ch == '\r' || ch == '\n') {
            serial_write("\r\n");
            line[len] = '\0';
            if (text_equal(line, "exit")) {
                serial_write("[USER] session closed\r\n");
                return;
            }
            execute_command(line);
            len = 0;
            line[0] = '\0';
            print_prompt();
            continue;
        }
        if (ch == '\b' || ch == 0x7F) {
            if (len != 0) {
                --len;
                serial_write("\b \b");
            }
            continue;
        }
        if (len + 1 < sizeof(line)) {
            line[len++] = ch;
            serial_putc(ch);
        }
    }
}
