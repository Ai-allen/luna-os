#include <stddef.h>
#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))
#define MS_ABI __attribute__((ms_abi))

#define LUNA_UEFI_DISK_MAGIC 0x49464555u
#define PIT_HZ 1193182u
#define PIT_CALIBRATE_COUNTS 59659u

struct luna_uefi_disk_handoff {
    uint32_t magic;
    uint32_t version;
    uint64_t block_io_protocol;
    uint64_t media_id;
    uint64_t read_blocks_entry;
    uint64_t write_blocks_entry;
};

struct virtio_pci_cap {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed));

struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    uint32_t notify_off_multiplier;
} __attribute__((packed));

struct virtio_pci_common_cfg {
    volatile uint32_t device_feature_select;
    volatile uint32_t device_feature;
    volatile uint32_t driver_feature_select;
    volatile uint32_t driver_feature;
    volatile uint16_t msix_config;
    volatile uint16_t num_queues;
    volatile uint8_t device_status;
    volatile uint8_t config_generation;
    volatile uint16_t queue_select;
    volatile uint16_t queue_size;
    volatile uint16_t queue_msix_vector;
    volatile uint16_t queue_enable;
    volatile uint16_t queue_notify_off;
    volatile uint16_t reserved0;
    volatile uint64_t queue_desc;
    volatile uint64_t queue_driver;
    volatile uint64_t queue_device;
    volatile uint16_t queue_notify_data;
    volatile uint16_t queue_reset;
    volatile uint16_t reserved1[2];
} __attribute__((packed));

struct virtio_queue_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtio_queue_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[8];
    uint16_t used_event;
} __attribute__((packed));

struct virtio_queue_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtio_queue_used {
    uint16_t flags;
    uint16_t idx;
    struct virtio_queue_used_elem ring[8];
    uint16_t avail_event;
} __attribute__((packed));

struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed));

typedef uint64_t (MS_ABI *efi_block_rw_fn_t)(
    void *self,
    uint32_t media_id,
    uint64_t lba,
    uint64_t buffer_size,
    void *buffer
);

static volatile struct luna_uefi_disk_handoff *uefi_disk_handoff(void);

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

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void serial_write_hex8(uint8_t value) {
    static const char digits[] = "0123456789ABCDEF";
    serial_putc(digits[(value >> 4) & 0x0Fu]);
    serial_putc(digits[value & 0x0Fu]);
}

static void serial_write_hex16(uint16_t value) {
    serial_write_hex8((uint8_t)(value >> 8));
    serial_write_hex8((uint8_t)(value & 0xFFu));
}

static void serial_write_hex32(uint32_t value) {
    serial_write_hex16((uint16_t)(value >> 16));
    serial_write_hex16((uint16_t)(value & 0xFFFFu));
}

static __attribute__((unused)) void serial_write_hex64(uint64_t value) {
    serial_write_hex32((uint32_t)(value >> 32));
    serial_write_hex32((uint32_t)(value & 0xFFFFFFFFu));
}

static void display_putc(uint32_t x, uint32_t y, uint8_t glyph, uint8_t attr) {
    volatile uint16_t *vga = (volatile uint16_t *)(uintptr_t)0xB8000u;
    if (x >= 80u || y >= 25u) {
        return;
    }
    vga[y * 80u + x] = (uint16_t)(((uint16_t)attr << 8) | glyph);
}

static uint64_t clock_ticks(void) {
    uint32_t lo;
    uint32_t hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint8_t g_net_buffer[LUNA_NETWORK_PACKET_BYTES];
static uint64_t g_net_size = 0;
static uint8_t g_kbd_e0 = 0u;
static uint8_t g_kbd_f0 = 0u;
static uint8_t g_keyboard_ready = 0u;
static uint8_t g_mouse_ready = 0u;
static uint8_t g_virtio_keyboard_ready = 0u;
static uint8_t g_ps2_controller_known = 0u;
static uint8_t g_ps2_controller_present = 0u;
static uint32_t g_ps2_debug_budget = 32u;
static uint8_t g_key_queue[32];
static uint32_t g_key_head = 0u;
static uint32_t g_key_tail = 0u;
static uint8_t g_mouse_queue[64];
static uint32_t g_mouse_head = 0u;
static uint32_t g_mouse_tail = 0u;
static uint8_t g_mouse_packet[3];
static uint32_t g_mouse_phase = 0u;
static uint64_t g_clock_tsc_base = 0u;
static uint64_t g_clock_tsc_hz = 0u;
static uint64_t g_display_framebuffer_base = 0u;
static uint64_t g_display_framebuffer_size = 0u;
static uint32_t g_display_framebuffer_width = 0u;
static uint32_t g_display_framebuffer_height = 0u;
static uint32_t g_display_framebuffer_stride = 0u;
static uint32_t g_display_framebuffer_format = 0u;
static uint32_t g_net_driver_family = LUNA_LANE_DRIVER_SOFT_LOOP;
static uint32_t g_disk_driver_family = LUNA_LANE_DRIVER_ATA_PIO;
static uint8_t g_ahci_ready = 0u;
static uint8_t g_ahci_port = 0u;
static uintptr_t g_ahci_abar = 0u;
static uint8_t g_net_ready = 0u;
static uintptr_t g_net_mmio = 0u;
static uint16_t g_net_vendor = 0u;
static uint16_t g_net_device = 0u;
static uint8_t g_net_mac[6];
static uint16_t g_net_rx_index = 0u;
static uint16_t g_net_rx_tail = 0u;
static uint16_t g_net_tx_index = 0u;
static uint16_t g_net_tx_tail = 0u;
static uint64_t g_net_tx_packets = 0u;
static uint64_t g_net_rx_packets = 0u;
static uint8_t g_virtio_kbd_bus = 0u;
static uint8_t g_virtio_kbd_slot = 0u;
static uint8_t g_virtio_kbd_function = 0u;
static uint16_t g_virtio_kbd_queue_size = 0u;
static uint16_t g_virtio_kbd_avail_idx = 0u;
static uint16_t g_virtio_kbd_used_idx = 0u;
static uint32_t g_virtio_kbd_notify_multiplier = 0u;
static uint32_t g_virtio_kbd_debug_budget = 32u;
static volatile struct virtio_pci_common_cfg *g_virtio_kbd_common = 0;
static volatile uint8_t *g_virtio_kbd_notify = 0;
static uint16_t g_virtio_kbd_notify_off = 0u;
static struct luna_cid g_observe_log_cid = {0u, 0u};
static uint8_t g_observe_logging_enabled = 0u;
static uint64_t g_seen_device_cids[16][2];
static uint32_t g_observe_invoke_budget = 64u;
static struct luna_cid g_data_seed_cid = {0u, 0u};
static struct luna_cid g_data_pour_cid = {0u, 0u};
static struct luna_cid g_data_draw_cid = {0u, 0u};
static struct luna_cid g_data_gather_cid = {0u, 0u};
static struct luna_cid g_driver_bind_cid = {0u, 0u};
static struct luna_cid g_lifecycle_wake_cid = {0u, 0u};
static struct luna_cid g_system_query_cid = {0u, 0u};
static uint64_t g_driver_lifecycle_flags = 0u;
static uint8_t g_driver_fatal_seen = 0u;
static uint8_t g_driver_flags_published = 0u;
static char g_pending_driver_logs[24][48];
static uint32_t g_pending_driver_log_count = 0u;
static struct luna_driver_bind_record g_pending_driver_binds[8];
static uint32_t g_pending_driver_bind_count = 0u;
static uint8_t g_driver_bindings_persisted = 0u;

#define LUNA_DRIVER_BIND_MAGIC 0x44525642u
#define LUNA_DRIVER_BIND_VERSION 1u

struct intel_rx_desc {
    uint32_t addr_low;
    uint32_t addr_high;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct intel_tx_desc {
    uint32_t addr_low;
    uint32_t addr_high;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct ahci_command_header {
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct ahci_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc;
} __attribute__((packed));

struct ahci_command_table {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    struct ahci_prdt_entry prdt[1];
} __attribute__((packed));

struct ahci_received_fis {
    uint8_t bytes[256];
} __attribute__((packed));

struct ahci_port_mmio {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed));

struct ahci_hba_mmio {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    struct ahci_port_mmio ports[32];
} __attribute__((packed));

static struct ahci_command_header g_ahci_cmd_list[32] __attribute__((aligned(1024)));
static struct ahci_received_fis g_ahci_fis __attribute__((aligned(256)));
static struct ahci_command_table g_ahci_cmd_table __attribute__((aligned(128)));
static uint8_t g_ahci_dma_sector[512] __attribute__((aligned(512)));
static uint8_t g_firmware_disk_sector[512] __attribute__((aligned(4096)));
static struct intel_rx_desc g_net_rx_ring[8] __attribute__((aligned(128)));
static struct intel_tx_desc g_net_tx_ring[8] __attribute__((aligned(128)));
static uint8_t g_net_rx_buffers[8][2048] __attribute__((aligned(16)));
static uint8_t g_net_tx_buffer[2048] __attribute__((aligned(16)));
static volatile struct virtio_queue_desc g_virtio_kbd_desc[8] __attribute__((aligned(16)));
static volatile struct virtio_queue_avail g_virtio_kbd_avail __attribute__((aligned(2)));
static volatile struct virtio_queue_used g_virtio_kbd_used __attribute__((aligned(4)));
static volatile struct virtio_input_event g_virtio_kbd_events[8] __attribute__((aligned(8)));

static uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function);
static uint8_t pci_header_type(uint8_t bus, uint8_t slot, uint8_t function);
static int pci_find_class_device(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if,
    uint16_t vendor_id,
    uint16_t device_id
);
static int pci_find_device(uint16_t vendor_id, uint16_t device_id);
static int pci_find_class_location(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if,
    uint16_t vendor_id,
    uint16_t device_id,
    uint8_t *bus_out,
    uint8_t *slot_out,
    uint8_t *function_out
);
static int pci_find_ahci_location(uint8_t *bus_out, uint8_t *slot_out, uint8_t *function_out);
static uintptr_t pci_find_mmio_bar(uint8_t bus, uint8_t slot, uint8_t function);
static uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
static uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
static uintptr_t pci_read_bar_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t bar_index);
static uint32_t disk_driver_family(void);
static uint32_t serial_driver_family(void);
static const char *serial_driver_name(void);
static const char *display_driver_name(void);
static uint32_t keyboard_driver_family(void);
static uint32_t pointer_driver_family(void);
static const char *input_driver_name(uint32_t family);
static int intel_net_init(void);
static int intel_net_read(uint8_t *out, uint64_t *size);
static int intel_net_write(const uint8_t *src, uint64_t size);
static void intel_net_fill_info(struct luna_net_info *info);
static int ahci_init(void);
static int ahci_read_sector(uint32_t lba, uint8_t *out);
static int ahci_write_sector(uint32_t lba, const uint8_t *src);
static int virtio_keyboard_init(void);
static uint8_t virtio_keyboard_poll_byte(void);
static void zero_bytes(void *dst, size_t len);

static uint32_t display_lane_flags(void) {
    uint32_t flags = LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT;
    if (g_display_framebuffer_base == 0u || g_display_framebuffer_width == 0u || g_display_framebuffer_height == 0u) {
        return flags;
    }
    return flags | LUNA_LANE_FLAG_FALLBACK;
}

static uint32_t serial_driver_family(void) {
    if (pci_find_device(0x8086u, 0x2918u)) {
        return LUNA_LANE_DRIVER_ICH9_UART;
    }
    if (pci_find_device(0x8086u, 0x7000u)) {
        return LUNA_LANE_DRIVER_PIIX_UART;
    }
    return LUNA_LANE_DRIVER_UART16550;
}

static const char *serial_driver_name(void) {
    uint32_t family = serial_driver_family();
    if (family == LUNA_LANE_DRIVER_ICH9_UART) {
        return "ich9-uart";
    }
    if (family == LUNA_LANE_DRIVER_PIIX_UART) {
        return "piix-uart";
    }
    return "uart16550";
}

static uint32_t display_driver_family(void) {
    if (pci_find_device(0x1234u, 0x1111u)) {
        return LUNA_LANE_DRIVER_QEMU_STD_VGA;
    }
    if (g_display_framebuffer_base == 0u || g_display_framebuffer_width == 0u || g_display_framebuffer_height == 0u) {
        return LUNA_LANE_DRIVER_VGA_TEXT;
    }
    return LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER;
}

static const char *display_driver_name(void) {
    uint32_t family = display_driver_family();
    if (family == LUNA_LANE_DRIVER_QEMU_STD_VGA) {
        if (g_display_framebuffer_base == 0u || g_display_framebuffer_width == 0u || g_display_framebuffer_height == 0u) {
            return "std-vga-text";
        }
        return "std-vga-fb";
    }
    if (family == LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER) {
        return "boot-fb";
    }
    return "vga-text";
}

static uint32_t disk_lane_flags(void) {
    uint32_t family = disk_driver_family();
    uint32_t flags = LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT;
    if (family == LUNA_LANE_DRIVER_AHCI) {
        return flags | LUNA_LANE_FLAG_POLLING;
    }
    if (family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        return flags;
    }
    return flags | LUNA_LANE_FLAG_FALLBACK | LUNA_LANE_FLAG_POLLING;
}

static uint32_t disk_driver_family(void) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    if (g_ahci_ready != 0u) {
        return LUNA_LANE_DRIVER_AHCI;
    }
    if (handoff->magic == LUNA_UEFI_DISK_MAGIC
        && handoff->block_io_protocol != 0u
        && handoff->read_blocks_entry != 0u) {
        return LUNA_LANE_DRIVER_UEFI_BLOCK_IO;
    }
    if (pci_find_class_device(0x01u, 0x01u, 0x80u, 0x8086u, 0x7010u)) {
        return LUNA_LANE_DRIVER_PCI_IDE;
    }
    return g_disk_driver_family;
}

static const char *disk_driver_name(void) {
    uint32_t family = disk_driver_family();
    if (family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        return "uefi-block";
    }
    if (family == LUNA_LANE_DRIVER_PCI_IDE) {
        return "piix-ide";
    }
    if (family == LUNA_LANE_DRIVER_AHCI) {
        return "ahci";
    }
    return "ata-pio";
}

static uint32_t net_lane_flags(void) {
    uint32_t flags = LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY;
    if (g_net_ready != 0u) {
        return flags | LUNA_LANE_FLAG_POLLING;
    }
    return flags | LUNA_LANE_FLAG_FALLBACK;
}

static uint32_t net_driver_family(void) {
    if (g_net_ready != 0u) {
        return g_net_driver_family;
    }
    if (pci_find_class_device(0x02u, 0x00u, 0x00u, 0x8086u, 0x100Eu)) {
        return LUNA_LANE_DRIVER_E1000;
    }
    if (pci_find_class_device(0x02u, 0x00u, 0x00u, 0x8086u, 0x10D3u)) {
        return LUNA_LANE_DRIVER_E1000E;
    }
    return g_net_driver_family;
}

static const char *net_driver_name(void) {
    uint32_t family = net_driver_family();
    if (family == LUNA_LANE_DRIVER_E1000) {
        return g_net_ready != 0u ? "e1000" : "e1000-stub";
    }
    if (family == LUNA_LANE_DRIVER_E1000E) {
        return g_net_ready != 0u ? "e1000e" : "e1000e-stub";
    }
    return "soft-loop";
}

static uint32_t keyboard_driver_family(void) {
    if (g_virtio_keyboard_ready != 0u) {
        return LUNA_LANE_DRIVER_VIRTIO_KEYBOARD;
    }
    return LUNA_LANE_DRIVER_I8042_KEYBOARD;
}

static uint32_t pointer_driver_family(void) {
    return LUNA_LANE_DRIVER_I8042_MOUSE;
}

static const char *input_driver_name(uint32_t family) {
    if (family == LUNA_LANE_DRIVER_VIRTIO_KEYBOARD) {
        return "virtio-kbd";
    }
    if (family == LUNA_LANE_DRIVER_I8042_KEYBOARD) {
        return "i8042-kbd";
    }
    if (family == LUNA_LANE_DRIVER_I8042_MOUSE) {
        return "i8042-mouse";
    }
    if (family == LUNA_LANE_DRIVER_PS2_MOUSE) {
        return "ps2-mouse";
    }
    return "ps2-kbd";
}

static void copy_bytes(void *dst, const void *src, size_t len) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static uint16_t pit_read_counter0(void) {
    uint16_t value;
    outb(0x43u, 0x00u);
    value = (uint16_t)inb(0x40u);
    value |= (uint16_t)((uint16_t)inb(0x40u) << 8);
    return value;
}

static uint64_t calibrate_tsc_hz(void) {
    const uint16_t reload = 0xFFFFu;
    const uint16_t target = (uint16_t)(reload - PIT_CALIBRATE_COUNTS);
    uint64_t start_tsc;
    uint64_t end_tsc;
    uint64_t elapsed_tsc;
    uint64_t elapsed_us;
    uint32_t spins = 0u;

    outb(0x43u, 0x30u);
    outb(0x40u, (uint8_t)(reload & 0xFFu));
    outb(0x40u, (uint8_t)(reload >> 8));
    start_tsc = clock_ticks();
    while (pit_read_counter0() > target && spins < 200000u) {
        spins += 1u;
    }
    end_tsc = clock_ticks();
    if (spins == 0u || spins >= 200000u || end_tsc <= start_tsc) {
        return 0u;
    }
    elapsed_tsc = end_tsc - start_tsc;
    elapsed_us = ((uint64_t)PIT_CALIBRATE_COUNTS * 1000000ull) / (uint64_t)PIT_HZ;
    if (elapsed_us == 0u) {
        return 0u;
    }
    return (elapsed_tsc * 1000000ull) / elapsed_us;
}

static uint64_t clock_microseconds(void) {
    uint64_t now = clock_ticks();
    if (now <= g_clock_tsc_base) {
        return 0u;
    }
    if (g_clock_tsc_hz == 0u) {
        return now - g_clock_tsc_base;
    }
    return ((now - g_clock_tsc_base) * 1000000ull) / g_clock_tsc_hz;
}

static uint8_t ps2_status(void) {
    return inb(0x64);
}

static int ps2_controller_available(void) {
    uint8_t sample = 0u;

    if (g_ps2_controller_known != 0u) {
        return g_ps2_controller_present != 0u;
    }
    sample = ps2_status();
    g_ps2_controller_known = 1u;
    g_ps2_controller_present = (sample == 0xFFu) ? 0u : 1u;
    if (g_ps2_controller_present == 0u) {
        serial_write("[PS2] controller missing\r\n");
    }
    return g_ps2_controller_present != 0u;
}

static int ps2_has_output(void) {
    return (ps2_status() & 0x01u) != 0u;
}

static int ps2_can_write(void) {
    return (ps2_status() & 0x02u) == 0u;
}

static void ps2_wait_write(void) {
    for (uint32_t i = 0; i < 100000u; ++i) {
        if (ps2_can_write()) {
            return;
        }
    }
}

static uint8_t ps2_read_data(void) {
    return inb(0x60);
}

static int ps2_wait_read(uint32_t limit) {
    while (limit-- != 0u) {
        if (ps2_has_output()) {
            return 1;
        }
    }
    return 0;
}

static uint8_t ps2_read_data_timeout(uint32_t limit, uint8_t *ok) {
    if (!ps2_wait_read(limit)) {
        if (ok != 0) {
            *ok = 0u;
        }
        return 0u;
    }
    if (ok != 0) {
        *ok = 1u;
    }
    return ps2_read_data();
}

static void ps2_write_command(uint8_t value) {
    ps2_wait_write();
    outb(0x64, value);
}

static void ps2_write_data(uint8_t value) {
    ps2_wait_write();
    outb(0x60, value);
}

static void ps2_flush_output(void) {
    for (uint32_t i = 0; i < 64u; ++i) {
        if (!ps2_has_output()) {
            break;
        }
        (void)ps2_read_data();
    }
}

static uint8_t ps2_read_config(uint8_t *ok) {
    ps2_write_command(0x20u);
    return ps2_read_data_timeout(100000u, ok);
}

static void ps2_write_config(uint8_t value) {
    ps2_write_command(0x60u);
    ps2_write_data(value);
}

static uint8_t ps2_write_keyboard_command(uint8_t value) {
    uint8_t ok = 0u;
    ps2_write_data(value);
    return ps2_read_data_timeout(100000u, &ok);
}

static uint8_t ps2_write_mouse_command(uint8_t value) {
    uint8_t ok = 0u;
    ps2_write_command(0xD4u);
    ps2_write_data(value);
    return ps2_read_data_timeout(100000u, &ok);
}

static int keyboard_init(void) {
    uint8_t ok = 0u;
    uint8_t config = 0u;

    if (g_keyboard_ready != 0u) {
        return 1;
    }
    if (!ps2_controller_available()) {
        return 0;
    }
    ps2_write_command(0xADu);
    ps2_write_command(0xA7u);
    ps2_flush_output();
    config = ps2_read_config(&ok);
    if (ok != 0u) {
        config = (uint8_t)((config | 0x03u) & (uint8_t)~0x30u);
        ps2_write_config(config);
    }
    ps2_write_command(0xAEu);
    (void)ps2_write_keyboard_command(0xF6u);
    (void)ps2_write_keyboard_command(0xF4u);
    g_keyboard_ready = 1u;
    return 1;
}

static int mouse_init(void) {
    uint8_t ok = 0u;
    uint8_t config = 0u;

    if (g_mouse_ready != 0u) {
        return 1;
    }
    if (!ps2_controller_available()) {
        return 0;
    }
    config = ps2_read_config(&ok);
    if (ok != 0u) {
        config = (uint8_t)((config | 0x02u) & (uint8_t)~0x20u);
        ps2_write_config(config);
    }
    ps2_write_command(0xA8u);
    (void)ps2_write_mouse_command(0xF6u);
    (void)ps2_write_mouse_command(0xF4u);
    g_mouse_ready = 1u;
    return 1;
}

static void enqueue_key(uint8_t value) {
    uint32_t next = (g_key_tail + 1u) % (uint32_t)(sizeof(g_key_queue));
    if (next == g_key_head) {
        return;
    }
    g_key_queue[g_key_tail] = value;
    g_key_tail = next;
}

static uint8_t dequeue_key(void) {
    uint8_t value;
    if (g_key_head == g_key_tail) {
        return 0u;
    }
    value = g_key_queue[g_key_head];
    g_key_head = (g_key_head + 1u) % (uint32_t)(sizeof(g_key_queue));
    return value;
}

static void enqueue_mouse(uint8_t value) {
    uint32_t next = (g_mouse_tail + 1u) % (uint32_t)(sizeof(g_mouse_queue));
    if (next == g_mouse_head) {
        return;
    }
    g_mouse_queue[g_mouse_tail] = value;
    g_mouse_tail = next;
}

static uint8_t dequeue_mouse(void) {
    uint8_t value;
    if (g_mouse_head == g_mouse_tail) {
        return 0u;
    }
    value = g_mouse_queue[g_mouse_head];
    g_mouse_head = (g_mouse_head + 1u) % (uint32_t)(sizeof(g_mouse_queue));
    return value;
}

static uint8_t translate_set1(uint8_t code, uint8_t extended) {
    if (extended != 0u) {
        switch (code) {
            case 0x48u: return 0x91u;
            case 0x50u: return 0x92u;
            case 0x4Bu: return 0x93u;
            case 0x4Du: return 0x94u;
            default: return 0u;
        }
    }
    switch (code) {
        case 0x02u: return '1';
        case 0x03u: return '2';
        case 0x04u: return '3';
        case 0x05u: return '4';
        case 0x06u: return '5';
        case 0x07u: return '6';
        case 0x08u: return '7';
        case 0x09u: return '8';
        case 0x0Au: return '9';
        case 0x0Bu: return '0';
        case 0x01u: return 0x1Bu;
        case 0x0Fu: return 0x09u;
        case 0x1Cu: return '\n';
        case 0x39u: return ' ';
        case 0x0Eu: return 8u;
        case 0x3Bu: return 0x81u;
        case 0x3Cu: return 0x82u;
        case 0x3Du: return 0x83u;
        case 0x3Eu: return 0x84u;
        case 0x3Fu: return 0x85u;
        case 0x40u: return 0x86u;
        case 0x41u: return 0x87u;
        case 0x42u: return 0x88u;
        case 0x43u: return 0x89u;
        case 0x10u: return 'q';
        case 0x11u: return 'w';
        case 0x12u: return 'e';
        case 0x13u: return 'r';
        case 0x14u: return 't';
        case 0x15u: return 'y';
        case 0x16u: return 'u';
        case 0x17u: return 'i';
        case 0x18u: return 'o';
        case 0x19u: return 'p';
        case 0x1Eu: return 'a';
        case 0x1Fu: return 's';
        case 0x20u: return 'd';
        case 0x21u: return 'f';
        case 0x22u: return 'g';
        case 0x23u: return 'h';
        case 0x24u: return 'j';
        case 0x25u: return 'k';
        case 0x26u: return 'l';
        case 0x2Cu: return 'z';
        case 0x2Du: return 'x';
        case 0x2Eu: return 'c';
        case 0x2Fu: return 'v';
        case 0x30u: return 'b';
        case 0x31u: return 'n';
        case 0x32u: return 'm';
        default: return 0u;
    }
}

static uint8_t translate_set2(uint8_t code, uint8_t extended) {
    if (extended != 0u) {
        switch (code) {
            case 0x75u: return 0x91u;
            case 0x72u: return 0x92u;
            case 0x6Bu: return 0x93u;
            case 0x74u: return 0x94u;
            default: return 0u;
        }
    }
    switch (code) {
        case 0x16u: return '1';
        case 0x1Eu: return '2';
        case 0x26u: return '3';
        case 0x25u: return '4';
        case 0x2Eu: return '5';
        case 0x36u: return '6';
        case 0x3Du: return '7';
        case 0x3Eu: return '8';
        case 0x46u: return '9';
        case 0x45u: return '0';
        case 0x76u: return 0x1Bu;
        case 0x0Du: return '\t';
        case 0x5Au: return '\n';
        case 0x29u: return ' ';
        case 0x66u: return 8u;
        case 0x05u: return 0x81u;
        case 0x06u: return 0x82u;
        case 0x04u: return 0x83u;
        case 0x0Cu: return 0x84u;
        case 0x03u: return 0x85u;
        case 0x0Bu: return 0x86u;
        case 0x83u: return 0x87u;
        case 0x0Au: return 0x88u;
        case 0x01u: return 0x89u;
        case 0x15u: return 'q';
        case 0x1Du: return 'w';
        case 0x24u: return 'e';
        case 0x2Du: return 'r';
        case 0x2Cu: return 't';
        case 0x35u: return 'y';
        case 0x3Cu: return 'u';
        case 0x43u: return 'i';
        case 0x44u: return 'o';
        case 0x4Du: return 'p';
        case 0x1Cu: return 'a';
        case 0x1Bu: return 's';
        case 0x23u: return 'd';
        case 0x2Bu: return 'f';
        case 0x34u: return 'g';
        case 0x33u: return 'h';
        case 0x3Bu: return 'j';
        case 0x42u: return 'k';
        case 0x4Bu: return 'l';
        case 0x1Au: return 'z';
        case 0x22u: return 'x';
        case 0x21u: return 'c';
        case 0x2Au: return 'v';
        case 0x32u: return 'b';
        case 0x31u: return 'n';
        case 0x3Au: return 'm';
        default: return 0u;
    }
}

static void ps2_pump(uint32_t limit) {
    while (limit-- != 0u) {
        uint8_t status;
        uint8_t code;
        if (!ps2_has_output()) {
            return;
        }
        status = ps2_status();
        code = ps2_read_data();
        if (g_ps2_debug_budget != 0u) {
            serial_write("[PS2] status=");
            serial_write_hex8(status);
            serial_write(" code=");
            serial_write_hex8(code);
            serial_write("\r\n");
            g_ps2_debug_budget -= 1u;
        }

        if ((status & 0x20u) != 0u) {
            if (code != 0xFAu && code != 0xAAu) {
                enqueue_mouse(code);
            }
            continue;
        }

        if (code == 0xFAu || code == 0xAAu) {
            continue;
        }
        if (code == 0xE0u) {
            g_kbd_e0 = 1u;
            continue;
        }
        if (code == 0xF0u) {
            g_kbd_f0 = 1u;
            continue;
        }
        if ((code & 0x80u) != 0u && g_kbd_f0 == 0u) {
            g_kbd_e0 = 0u;
            continue;
        }
        if (g_kbd_f0 != 0u) {
            g_kbd_f0 = 0u;
            g_kbd_e0 = 0u;
            continue;
        }

        {
            uint8_t translated = translate_set1(code, g_kbd_e0);
            if (translated == 0u) {
                translated = translate_set2(code, g_kbd_e0);
            }
            g_kbd_e0 = 0u;
            if (translated != 0u) {
                enqueue_key(translated);
            }
        }
    }
}

static uint8_t keyboard_poll_byte(void) {
    uint8_t virtio_value = virtio_keyboard_poll_byte();
    if (virtio_value != 0u) {
        return virtio_value;
    }
    uint8_t value = dequeue_key();
    if (value != 0u) {
        return value;
    }
    ps2_pump(32u);
    return dequeue_key();
}

static uint8_t mouse_poll_byte(void) {
    uint8_t value = dequeue_mouse();
    if (value != 0u) {
        return value;
    }
    ps2_pump(32u);
    return dequeue_mouse();
}

static int mouse_poll_event(struct luna_pointer_event *event) {
    while (g_mouse_phase < 3u) {
        uint8_t value = mouse_poll_byte();
        if (value == 0u) {
            return 0;
        }
        if (g_mouse_phase == 0u && (value & 0x08u) == 0u) {
            continue;
        }
        g_mouse_packet[g_mouse_phase] = value;
        g_mouse_phase += 1u;
    }
    g_mouse_phase = 0u;
    event->dx = (int32_t)(int8_t)g_mouse_packet[1];
    event->dy = (int32_t)(int8_t)g_mouse_packet[2];
    event->buttons = (uint32_t)(g_mouse_packet[0] & 0x07u);
    event->flags = 1u;
    return 1;
}

static uint32_t validate_capability(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)gate)[i] = 0;
    }
    gate->sequence = 91;
    gate->opcode = LUNA_GATE_VALIDATE_CAP;
    gate->caller_space = 0;
    gate->domain_key = domain_key;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_DEVICE;
    gate->target_gate = target_gate;

    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    return gate->status;
}

static uint32_t request_capability(uint64_t domain_key, struct luna_cid *out) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)gate)[i] = 0;
    }
    gate->sequence = 92;
    gate->opcode = LUNA_GATE_REQUEST_CAP;
    gate->caller_space = LUNA_SPACE_DEVICE;
    gate->domain_key = domain_key;

    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    out->low = gate->cid_low;
    out->high = gate->cid_high;
    return gate->status;
}

static uint32_t data_call(
    struct luna_cid cid,
    uint32_t opcode,
    uint64_t object_low,
    uint64_t object_high,
    uint32_t object_type,
    uint32_t object_flags,
    void *buffer,
    uint64_t buffer_size,
    uint64_t *out_size,
    uint64_t *out_content_size,
    uint32_t *out_object_type
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_data_gate *gate =
        (volatile struct luna_data_gate *)(uintptr_t)manifest->data_gate_base;

    for (size_t i = 0; i < sizeof(struct luna_data_gate); ++i) {
        ((volatile uint8_t *)gate)[i] = 0;
    }
    gate->sequence = 94;
    gate->opcode = opcode;
    gate->cid_low = cid.low;
    gate->cid_high = cid.high;
    gate->object_low = object_low;
    gate->object_high = object_high;
    gate->object_type = object_type;
    gate->object_flags = object_flags;
    gate->buffer_addr = (uint64_t)(uintptr_t)buffer;
    gate->buffer_size = buffer_size;
    gate->size = buffer_size;
    ((void (SYSV_ABI *)(struct luna_data_gate *))(uintptr_t)manifest->data_gate_entry)(
        (struct luna_data_gate *)(uintptr_t)manifest->data_gate_base
    );
    if (out_size != 0) {
        *out_size = gate->size;
    }
    if (out_content_size != 0) {
        *out_content_size = gate->content_size;
    }
    if (out_object_type != 0) {
        *out_object_type = gate->object_type;
    }
    return gate->status;
}

static uint32_t governance_approve(
    uint64_t domain_key,
    struct luna_cid cid,
    uint32_t object_type,
    uint32_t object_flags,
    uint32_t *out_volume_state,
    uint32_t *out_mode
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;
    struct luna_governance_query query;

    zero_bytes(&query, sizeof(query));
    query.action = LUNA_GOVERN_WRITE;
    query.object_type = object_type;
    query.object_flags = object_flags;
    query.domain_key = domain_key;
    query.cid_low = cid.low;
    query.cid_high = cid.high;
    query.install_low = manifest->install_uuid_low;
    query.install_high = manifest->install_uuid_high;

    zero_bytes((void *)(uintptr_t)manifest->security_gate_base, sizeof(struct luna_gate));
    gate->sequence = 95;
    gate->opcode = LUNA_GATE_GOVERN;
    gate->caller_space = LUNA_SPACE_DEVICE;
    gate->buffer_addr = (uint64_t)(uintptr_t)&query;
    gate->buffer_size = sizeof(query);
    ((void (SYSV_ABI *)(struct luna_gate *))(uintptr_t)manifest->security_gate_entry)(
        (struct luna_gate *)(uintptr_t)manifest->security_gate_base
    );
    if (out_volume_state != 0) {
        *out_volume_state = query.result_state;
    }
    if (out_mode != 0) {
        *out_mode = query.mode;
    }
    return gate->status;
}

static void queue_driver_log(const char *message) {
    size_t i;
    uint32_t slot;

    if (message == 0 || g_pending_driver_log_count >= 24u) {
        return;
    }
    slot = g_pending_driver_log_count++;
    for (i = 0u; i < sizeof(g_pending_driver_logs[slot]); ++i) {
        g_pending_driver_logs[slot][i] = 0;
        if (message[i] == '\0') {
            break;
        }
        g_pending_driver_logs[slot][i] = message[i];
    }
}

static void try_enable_observe_logging(void) {
    if (g_observe_logging_enabled != 0u) {
        return;
    }
    if ((g_observe_log_cid.low != 0u || g_observe_log_cid.high != 0u)) {
        g_observe_logging_enabled = 1u;
    }
}

static void observe_log(uint32_t level, const char *message) {
    volatile struct luna_manifest *manifest;
    volatile struct luna_observe_gate *gate;

    if (g_observe_logging_enabled == 0u
        || (g_observe_log_cid.low == 0u && g_observe_log_cid.high == 0u)
        || message == 0) {
        return;
    }
    manifest = (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    gate = (volatile struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base;
    for (size_t i = 0; i < sizeof(struct luna_observe_gate); ++i) {
        ((volatile uint8_t *)gate)[i] = 0;
    }
    gate->sequence = 93;
    gate->opcode = LUNA_OBSERVE_LOG;
    gate->space_id = LUNA_SPACE_DEVICE;
    gate->level = level;
    gate->cid_low = g_observe_log_cid.low;
    gate->cid_high = g_observe_log_cid.high;
    for (size_t i = 0; i < sizeof(gate->message) - 1u && message[i] != '\0'; ++i) {
        gate->message[i] = message[i];
    }
    ((void (SYSV_ABI *)(struct luna_observe_gate *))(uintptr_t)manifest->observe_gate_entry)(
        (struct luna_observe_gate *)(uintptr_t)manifest->observe_gate_base
    );
}

static void flush_pending_driver_logs(void) {
    if (g_observe_logging_enabled == 0u) {
        return;
    }
    for (uint32_t i = 0u; i < g_pending_driver_log_count; ++i) {
        observe_log(1u, g_pending_driver_logs[i]);
    }
    g_pending_driver_log_count = 0u;
}

static void driver_audit(const char *message) {
    serial_write(message);
    serial_write("\r\n");
    queue_driver_log(message);
    observe_log(1u, message);
}

static void driver_fail_audit(const char *message) {
    serial_write(message);
    serial_write("\r\n");
    queue_driver_log(message);
    observe_log(3u, message);
}

static uint8_t system_space_active(uint32_t space_id) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_system_gate *gate;
    struct luna_system_record record;

    if (g_system_query_cid.low == 0u && g_system_query_cid.high == 0u) {
        return 0u;
    }
    gate = (volatile struct luna_system_gate *)(uintptr_t)manifest->system_gate_base;
    zero_bytes((void *)(uintptr_t)manifest->system_gate_base, sizeof(struct luna_system_gate));
    zero_bytes(&record, sizeof(record));
    gate->sequence = 97;
    gate->opcode = LUNA_SYSTEM_QUERY_SPACE;
    gate->space_id = space_id;
    gate->cid_low = g_system_query_cid.low;
    gate->cid_high = g_system_query_cid.high;
    gate->buffer_addr = (uint64_t)(uintptr_t)&record;
    gate->buffer_size = sizeof(record);
    ((void (SYSV_ABI *)(struct luna_system_gate *))(uintptr_t)manifest->system_gate_entry)(
        (struct luna_system_gate *)(uintptr_t)manifest->system_gate_base
    );
    if (gate->status != LUNA_SYSTEM_OK || gate->result_count == 0u) {
        return 0u;
    }
    return record.space_id == space_id && record.state == 1u;
}

static void lifecycle_publish_driver_flags(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_lifecycle_gate *gate;

    if (g_driver_flags_published != 0u) {
        return;
    }
    if (g_lifecycle_wake_cid.low == 0u && g_lifecycle_wake_cid.high == 0u) {
        return;
    }
    gate = (volatile struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base;
    zero_bytes((void *)(uintptr_t)manifest->lifecycle_gate_base, sizeof(struct luna_lifecycle_gate));
    gate->sequence = 96;
    gate->opcode = LUNA_LIFE_WAKE_UNIT;
    gate->space_id = LUNA_SPACE_DEVICE;
    gate->state = 2u;
    gate->flags = g_driver_lifecycle_flags;
    gate->cid_low = g_lifecycle_wake_cid.low;
    gate->cid_high = g_lifecycle_wake_cid.high;
    ((void (SYSV_ABI *)(struct luna_lifecycle_gate *))(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)(uintptr_t)manifest->lifecycle_gate_base
    );
    if (gate->status == LUNA_LIFE_OK) {
        g_driver_flags_published = 1u;
    }
}

static void stage_driver_binding(
    uint32_t driver_class,
    uint32_t stage,
    uint32_t failure_class,
    uint32_t device_id,
    uint32_t lane_class,
    uint32_t driver_family,
    uint32_t state_flags,
    const char *lane_name,
    const char *driver_name
) {
    struct luna_driver_bind_record *record;

    if (g_pending_driver_bind_count >= 8u) {
        return;
    }
    record = &g_pending_driver_binds[g_pending_driver_bind_count++];
    zero_bytes(record, sizeof(*record));
    record->magic = LUNA_DRIVER_BIND_MAGIC;
    record->version = LUNA_DRIVER_BIND_VERSION;
    record->driver_class = driver_class;
    record->stage = stage;
    record->runtime_class = LUNA_DRIVER_RUNTIME_SYSTEM;
    record->failure_class = failure_class;
    record->device_id = device_id;
    record->lane_class = lane_class;
    record->driver_family = driver_family;
    record->state_flags = state_flags;
    record->bound_at = clock_microseconds();
    record->writer_space = LUNA_SPACE_DEVICE;
    record->authority_space = LUNA_SPACE_DEVICE;
    copy_bytes(record->lane_name, lane_name, 16u);
    copy_bytes(record->driver_name, driver_name, 16u);
}

static int find_driver_bind_object(uint32_t driver_class, struct luna_object_ref *out) {
    struct luna_object_ref refs[LUNA_DATA_OBJECT_CAPACITY];
    uint64_t size = 0u;
    uint64_t content_size = 0u;

    zero_bytes(refs, sizeof(refs));
    if (data_call(
            g_data_gather_cid,
            LUNA_DATA_GATHER_SET,
            0u,
            0u,
            0u,
            0u,
            refs,
            sizeof(refs),
            &size,
            &content_size,
            0
        ) != LUNA_DATA_OK) {
        return 0;
    }
    for (uint32_t i = 0u; i < LUNA_DATA_OBJECT_CAPACITY; ++i) {
        struct luna_driver_bind_record record;
        uint64_t draw_size = 0u;
        uint64_t draw_content = 0u;
        uint32_t object_type = 0u;
        if (refs[i].low == 0u && refs[i].high == 0u) {
            continue;
        }
        zero_bytes(&record, sizeof(record));
        if (data_call(
                g_data_draw_cid,
                LUNA_DATA_DRAW_SPAN,
                refs[i].low,
                refs[i].high,
                0u,
                0u,
                &record,
                sizeof(record),
                &draw_size,
                &draw_content,
                &object_type
            ) != LUNA_DATA_OK) {
            continue;
        }
        if (object_type != LUNA_DATA_OBJECT_TYPE_DRIVER_BIND ||
            record.magic != LUNA_DRIVER_BIND_MAGIC ||
            record.version != LUNA_DRIVER_BIND_VERSION ||
            record.driver_class != driver_class) {
            continue;
        }
        *out = refs[i];
        return 1;
    }
    return 0;
}

static void persist_driver_binding(
    const struct luna_driver_bind_record *record
) {
    struct luna_object_ref object = {0u, 0u};
    uint32_t volume_state = 0u;
    uint32_t mode = 0u;

    if ((g_driver_bind_cid.low == 0u && g_driver_bind_cid.high == 0u) ||
        (g_data_seed_cid.low == 0u && g_data_seed_cid.high == 0u) ||
        (g_data_pour_cid.low == 0u && g_data_pour_cid.high == 0u) ||
        (g_data_draw_cid.low == 0u && g_data_draw_cid.high == 0u) ||
        (g_data_gather_cid.low == 0u && g_data_gather_cid.high == 0u)) {
        return;
    }
    if (governance_approve(
            LUNA_CAP_DEVICE_BIND,
            g_driver_bind_cid,
            LUNA_DATA_OBJECT_TYPE_DRIVER_BIND,
            record->driver_class,
            &volume_state,
            &mode
        ) != LUNA_GATE_OK) {
        driver_fail_audit("audit driver.bind denied=SECURITY");
        return;
    }
    if (!find_driver_bind_object(record->driver_class, &object)) {
        uint64_t seed_size = 0u;
        uint64_t seed_content = 0u;
        if (data_call(
                g_data_seed_cid,
                LUNA_DATA_SEED_OBJECT,
                0u,
                0u,
                LUNA_DATA_OBJECT_TYPE_DRIVER_BIND,
                record->driver_class,
                0,
                0u,
                &seed_size,
                &seed_content,
                0
            ) != LUNA_DATA_OK) {
            driver_fail_audit("audit driver.bind persist=seed-fail");
            return;
        }
        object.low = ((volatile struct luna_data_gate *)(uintptr_t)((volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR)->data_gate_base)->object_low;
        object.high = ((volatile struct luna_data_gate *)(uintptr_t)((volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR)->data_gate_base)->object_high;
    }
    if (data_call(
            g_data_pour_cid,
            LUNA_DATA_POUR_SPAN,
            object.low,
            object.high,
            0u,
            0u,
            (void *)record,
            sizeof(*record),
            0,
            0,
            0
        ) == LUNA_DATA_OK) {
        driver_audit("audit driver.bind persisted=DATA authority=DEVICE");
    } else {
        driver_fail_audit("audit driver.bind persist=write-fail");
    }
}

static void persist_pending_driver_bindings(void) {
    if (g_driver_bindings_persisted != 0u) {
        return;
    }
    for (uint32_t i = 0u; i < g_pending_driver_bind_count; ++i) {
        persist_driver_binding(&g_pending_driver_binds[i]);
    }
    g_pending_driver_bind_count = 0u;
    g_driver_bindings_persisted = 1u;
}

static void observe_device_connection(uint64_t cid_low, uint64_t cid_high) {
    for (size_t i = 0u; i < sizeof(g_seen_device_cids) / sizeof(g_seen_device_cids[0]); ++i) {
        if (g_seen_device_cids[i][0] == cid_low && g_seen_device_cids[i][1] == cid_high) {
            return;
        }
    }
    for (size_t i = 0u; i < sizeof(g_seen_device_cids) / sizeof(g_seen_device_cids[0]); ++i) {
        if (g_seen_device_cids[i][0] == 0u && g_seen_device_cids[i][1] == 0u) {
            g_seen_device_cids[i][0] = cid_low;
            g_seen_device_cids[i][1] = cid_high;
            observe_log(1u, "device.conn active");
            return;
        }
    }
}

static int allow_device_call(uint64_t domain_key, uint64_t cid_low, uint64_t cid_high, uint32_t target_gate) {
    if (validate_capability(domain_key, cid_low, cid_high, target_gate) != LUNA_GATE_OK) {
        observe_log(3u, "device.call denied");
        return 0;
    }
    observe_device_connection(cid_low, cid_high);
    if (g_observe_invoke_budget != 0u) {
        g_observe_invoke_budget -= 1u;
        observe_log(1u, "device.call invoke");
    }
    return 1;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static void io_delay(void) {
    for (uint32_t i = 0; i < 4u; ++i) {
        (void)inb(0x3F6);
    }
}

static uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFCu);
    outl(0xCF8u, address);
    return inl(0xCFCu);
}

static void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFCu);
    outl(0xCF8u, address);
    outw((uint16_t)(0xCFCu + (offset & 0x02u)), value);
}

static uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t word = pci_config_read32(bus, slot, function, (uint8_t)(offset & 0xFCu));
    uint32_t shift = (uint32_t)(offset & 0x03u) * 8u;
    return (uint8_t)((word >> shift) & 0xFFu);
}

static uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t word = pci_config_read32(bus, slot, function, (uint8_t)(offset & 0xFCu));
    uint32_t shift = (uint32_t)(offset & 0x02u) * 8u;
    return (uint16_t)((word >> shift) & 0xFFFFu);
}

static uintptr_t pci_read_bar_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t bar_index) {
    uint8_t offset = (uint8_t)(0x10u + bar_index * 4u);
    uint32_t low = pci_config_read32(bus, slot, function, offset);
    uint32_t type;
    if ((low & 0x01u) != 0u || low == 0u || low == 0xFFFFFFFFu) {
        return 0u;
    }
    type = (low >> 1) & 0x03u;
    if (type == 0x02u && bar_index + 1u < 6u) {
        uint32_t high = pci_config_read32(bus, slot, function, (uint8_t)(offset + 4u));
        return (uintptr_t)(((uint64_t)high << 32) | (uint64_t)(low & 0xFFFFFFF0u));
    }
    return (uintptr_t)(low & 0xFFFFFFF0u);
}

static int pci_find_class_device(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if,
    uint16_t vendor_id,
    uint16_t device_id
) {
    for (uint32_t bus = 0u; bus < 256u; ++bus) {
        for (uint32_t slot = 0u; slot < 32u; ++slot) {
            uint8_t function_limit = 8u;
            for (uint32_t function = 0u; function < function_limit; ++function) {
                uint32_t id_word;
                uint32_t class_word;
                uint16_t vendor;
                uint8_t header;
                vendor = pci_vendor_id((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (vendor == 0xFFFFu) {
                    if (function == 0u) {
                        break;
                    }
                    continue;
                }
                header = pci_header_type((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (function == 0u && (header & 0x80u) == 0u) {
                    function_limit = 1u;
                }
                id_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x00u);
                class_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x08u);
                if (vendor_id != 0xFFFFu && (uint16_t)(id_word & 0xFFFFu) != vendor_id) {
                    continue;
                }
                if (device_id != 0xFFFFu && (uint16_t)(id_word >> 16) != device_id) {
                    continue;
                }
                if ((uint8_t)(class_word >> 24) != class_code ||
                    (uint8_t)(class_word >> 16) != subclass ||
                    (uint8_t)(class_word >> 8) != prog_if) {
                    continue;
                }
                return 1;
            }
        }
    }
    return 0;
}

static int pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t bus = 0u; bus < 256u; ++bus) {
        for (uint32_t slot = 0u; slot < 32u; ++slot) {
            uint8_t function_limit = 8u;
            for (uint32_t function = 0u; function < function_limit; ++function) {
                uint32_t id_word;
                uint16_t vendor;
                uint8_t header;
                vendor = pci_vendor_id((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (vendor == 0xFFFFu) {
                    if (function == 0u) {
                        break;
                    }
                    continue;
                }
                header = pci_header_type((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (function == 0u && (header & 0x80u) == 0u) {
                    function_limit = 1u;
                }
                id_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x00u);
                if ((uint16_t)(id_word & 0xFFFFu) == vendor_id && (uint16_t)(id_word >> 16) == device_id) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int pci_find_class_location(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if,
    uint16_t vendor_id,
    uint16_t device_id,
    uint8_t *bus_out,
    uint8_t *slot_out,
    uint8_t *function_out
) {
    for (uint32_t bus = 0u; bus < 256u; ++bus) {
        for (uint32_t slot = 0u; slot < 32u; ++slot) {
            uint8_t function_limit = 8u;
            for (uint32_t function = 0u; function < function_limit; ++function) {
                uint32_t id_word;
                uint32_t class_word;
                uint16_t vendor;
                uint8_t header;
                vendor = pci_vendor_id((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (vendor == 0xFFFFu) {
                    if (function == 0u) {
                        break;
                    }
                    continue;
                }
                header = pci_header_type((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (function == 0u && (header & 0x80u) == 0u) {
                    function_limit = 1u;
                }
                id_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x00u);
                class_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x08u);
                if ((uint16_t)(id_word & 0xFFFFu) != vendor_id || (uint16_t)(id_word >> 16) != device_id) {
                    continue;
                }
                if ((uint8_t)(class_word >> 24) != class_code ||
                    (uint8_t)(class_word >> 16) != subclass ||
                    (uint8_t)(class_word >> 8) != prog_if) {
                    continue;
                }
                *bus_out = (uint8_t)bus;
                *slot_out = (uint8_t)slot;
                *function_out = (uint8_t)function;
                return 1;
            }
        }
    }
    return 0;
}

static int pci_find_ahci_location(uint8_t *bus_out, uint8_t *slot_out, uint8_t *function_out) {
    static const struct {
        uint16_t vendor_id;
        uint16_t device_id;
    } supported[] = {
        { 0x8086u, 0x2922u }, /* QEMU q35 ICH9 AHCI */
        { 0x15ADu, 0x07E0u }, /* VMware SATA AHCI */
    };

    for (uint32_t i = 0u; i < (sizeof(supported) / sizeof(supported[0])); ++i) {
        if (pci_find_class_location(
            0x01u,
            0x06u,
            0x01u,
            supported[i].vendor_id,
            supported[i].device_id,
            bus_out,
            slot_out,
            function_out
        )) {
            return 1;
        }
    }
    return 0;
}

static uintptr_t pci_find_mmio_bar(uint8_t bus, uint8_t slot, uint8_t function) {
    for (uint8_t bar = 0u; bar < 6u; ++bar) {
        uint8_t offset = (uint8_t)(0x10u + bar * 4u);
        uint32_t low = pci_config_read32(bus, slot, function, offset);
        uint32_t type;
        if ((low & 0x01u) != 0u || low == 0u || low == 0xFFFFFFFFu) {
            continue;
        }
        type = (low >> 1) & 0x03u;
        if (type == 0x02u && bar + 1u < 6u) {
            return (uintptr_t)(low & 0xFFFFFFF0u);
        }
        return (uintptr_t)(low & 0xFFFFFFF0u);
    }
    return 0u;
}

static uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function) {
    return (uint16_t)(pci_config_read32(bus, slot, function, 0x00u) & 0xFFFFu);
}

static uint8_t pci_header_type(uint8_t bus, uint8_t slot, uint8_t function) {
    return (uint8_t)((pci_config_read32(bus, slot, function, 0x0Cu) >> 16) & 0xFFu);
}

static void memory_barrier(void) {
    __asm__ volatile ("" : : : "memory");
}

static uint16_t virtio_used_idx(void) {
    memory_barrier();
    return g_virtio_kbd_used.idx;
}

static uint8_t virtio_translate_key(uint16_t type, uint16_t code, uint32_t value) {
    if (type != 0x0001u || value == 0u) {
        return 0u;
    }
    switch (code) {
        case 0x0001u: return 0x1Bu;
        case 0x0002u: return '1';
        case 0x0003u: return '2';
        case 0x0004u: return '3';
        case 0x0005u: return '4';
        case 0x0006u: return '5';
        case 0x0007u: return '6';
        case 0x0008u: return '7';
        case 0x0009u: return '8';
        case 0x000Au: return '9';
        case 0x000Bu: return '0';
        case 0x000Eu: return 8u;
        case 0x000Fu: return '\t';
        case 0x0010u: return 'q';
        case 0x0011u: return 'w';
        case 0x0012u: return 'e';
        case 0x0013u: return 'r';
        case 0x0014u: return 't';
        case 0x0015u: return 'y';
        case 0x0016u: return 'u';
        case 0x0017u: return 'i';
        case 0x0018u: return 'o';
        case 0x0019u: return 'p';
        case 0x001Cu: return '\n';
        case 0x001Eu: return 'a';
        case 0x001Fu: return 's';
        case 0x0020u: return 'd';
        case 0x0021u: return 'f';
        case 0x0022u: return 'g';
        case 0x0023u: return 'h';
        case 0x0024u: return 'j';
        case 0x0025u: return 'k';
        case 0x0026u: return 'l';
        case 0x002Cu: return 'z';
        case 0x002Du: return 'x';
        case 0x002Eu: return 'c';
        case 0x002Fu: return 'v';
        case 0x0030u: return 'b';
        case 0x0031u: return 'n';
        case 0x0032u: return 'm';
        case 0x0039u: return ' ';
        case 0x003Bu: return 0x81u;
        case 0x003Cu: return 0x82u;
        case 0x003Du: return 0x83u;
        case 0x003Eu: return 0x84u;
        case 0x003Fu: return 0x85u;
        case 0x0040u: return 0x86u;
        case 0x0041u: return 0x87u;
        case 0x0042u: return 0x88u;
        case 0x0043u: return 0x89u;
        case 0x0067u: return 0x91u;
        case 0x0069u: return 0x93u;
        case 0x006Au: return 0x94u;
        case 0x006Cu: return 0x92u;
        default: return 0u;
    }
}

static void virtio_keyboard_notify(void) {
    volatile uint16_t *doorbell;
    uintptr_t notify_addr;
    if (g_virtio_keyboard_ready == 0u || g_virtio_kbd_notify == 0 || g_virtio_kbd_common == 0) {
        return;
    }
    notify_addr = (uintptr_t)g_virtio_kbd_notify
        + (uintptr_t)g_virtio_kbd_notify_off * (uintptr_t)g_virtio_kbd_notify_multiplier;
    doorbell = (volatile uint16_t *)notify_addr;
    *doorbell = 0u;
}

static uint8_t virtio_keyboard_poll_byte(void) {
    uint8_t translated = 0u;
    if (g_virtio_keyboard_ready == 0u || g_virtio_kbd_queue_size == 0u) {
        return 0u;
    }
    while (g_virtio_kbd_used_idx != virtio_used_idx()) {
        uint16_t used_slot = (uint16_t)(g_virtio_kbd_used_idx % g_virtio_kbd_queue_size);
        uint16_t desc_id = (uint16_t)(g_virtio_kbd_used.ring[used_slot].id % g_virtio_kbd_queue_size);
        struct virtio_input_event event;
        event.type = g_virtio_kbd_events[desc_id].type;
        event.code = g_virtio_kbd_events[desc_id].code;
        event.value = g_virtio_kbd_events[desc_id].value;
        if (g_virtio_kbd_debug_budget != 0u) {
            serial_write("[VIRTKBD] type=");
            serial_write_hex16(event.type);
            serial_write(" code=");
            serial_write_hex16(event.code);
            serial_write(" value=");
            serial_write_hex32(event.value);
            serial_write("\r\n");
            g_virtio_kbd_debug_budget -= 1u;
        }
        translated = virtio_translate_key(event.type, event.code, event.value);
        g_virtio_kbd_avail.ring[g_virtio_kbd_avail_idx % g_virtio_kbd_queue_size] = desc_id;
        memory_barrier();
        g_virtio_kbd_avail_idx += 1u;
        g_virtio_kbd_avail.idx = g_virtio_kbd_avail_idx;
        g_virtio_kbd_used_idx += 1u;
        virtio_keyboard_notify();
        if (translated != 0u) {
            return translated;
        }
    }
    return 0u;
}

static int virtio_keyboard_init(void) {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t command;
    uint8_t status;
    uint8_t cap_ptr;
    uintptr_t common_base = 0u;
    uintptr_t notify_base = 0u;
    uint32_t notify_multiplier = 0u;
    uint32_t device_features_lo = 0u;
    uint32_t device_features_hi = 0u;
    if (g_virtio_keyboard_ready != 0u) {
        return 1;
    }
    if (!pci_find_class_location(0x09u, 0x00u, 0x00u, 0x1AF4u, 0x1052u, &bus, &slot, &function)) {
        serial_write("[VIRTKBD] pci missing\r\n");
        return 0;
    }

    command = pci_config_read16(bus, slot, function, 0x04u);
    if ((command & 0x0006u) != 0x0006u) {
        uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)function << 8) | 0x04u;
        uint32_t word = pci_config_read32(bus, slot, function, 0x04u) | 0x00000006u;
        outl(0xCF8u, address);
        outl(0xCFCu, word);
    }

    status = pci_config_read8(bus, slot, function, 0x06u);
    if ((status & 0x10u) == 0u) {
        serial_write("[VIRTKBD] caps missing\r\n");
        return 0;
    }
    cap_ptr = pci_config_read8(bus, slot, function, 0x34u);
    while (cap_ptr >= 0x40u && cap_ptr != 0u) {
        if (pci_config_read8(bus, slot, function, cap_ptr) == 0x09u) {
            struct virtio_pci_notify_cap cap;
            cap.cap.cap_vndr = pci_config_read8(bus, slot, function, cap_ptr);
            cap.cap.cap_next = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 1u));
            cap.cap.cap_len = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 2u));
            cap.cap.cfg_type = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 3u));
            cap.cap.bar = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 4u));
            cap.cap.id = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 5u));
            cap.cap.offset = pci_config_read32(bus, slot, function, (uint8_t)(cap_ptr + 8u));
            cap.cap.length = pci_config_read32(bus, slot, function, (uint8_t)(cap_ptr + 12u));
            cap.notify_off_multiplier = pci_config_read32(bus, slot, function, (uint8_t)(cap_ptr + 16u));
            if (cap.cap.cfg_type == 1u) {
                uintptr_t bar = pci_read_bar_address(bus, slot, function, cap.cap.bar);
                if (bar != 0u) {
                    common_base = bar + cap.cap.offset;
                }
            } else if (cap.cap.cfg_type == 2u) {
                uintptr_t bar = pci_read_bar_address(bus, slot, function, cap.cap.bar);
                if (bar != 0u) {
                    notify_base = bar + cap.cap.offset;
                    notify_multiplier = cap.notify_off_multiplier;
                }
            }
        }
        cap_ptr = pci_config_read8(bus, slot, function, (uint8_t)(cap_ptr + 1u));
    }
    if (common_base == 0u || notify_base == 0u || notify_multiplier == 0u) {
        serial_write("[VIRTKBD] cfg missing ");
        serial_write_hex32((uint32_t)common_base);
        serial_putc(' ');
        serial_write_hex32((uint32_t)notify_base);
        serial_putc(' ');
        serial_write_hex32(notify_multiplier);
        serial_write("\r\n");
        return 0;
    }

    g_virtio_kbd_common = (volatile struct virtio_pci_common_cfg *)common_base;
    g_virtio_kbd_notify = (volatile uint8_t *)notify_base;
    g_virtio_kbd_notify_multiplier = notify_multiplier;
    g_virtio_kbd_bus = bus;
    g_virtio_kbd_slot = slot;
    g_virtio_kbd_function = function;

    g_virtio_kbd_common->device_status = 0u;
    memory_barrier();
    g_virtio_kbd_common->device_status = 1u;
    g_virtio_kbd_common->device_status |= 2u;
    g_virtio_kbd_common->device_feature_select = 0u;
    device_features_lo = g_virtio_kbd_common->device_feature;
    g_virtio_kbd_common->device_feature_select = 1u;
    device_features_hi = g_virtio_kbd_common->device_feature;
    g_virtio_kbd_common->driver_feature_select = 0u;
    g_virtio_kbd_common->driver_feature = 0u;
    g_virtio_kbd_common->driver_feature_select = 1u;
    g_virtio_kbd_common->driver_feature = (device_features_hi & 0x00000001u);
    serial_write("[VIRTKBD] feat ");
    serial_write_hex32(device_features_hi);
    serial_putc(':');
    serial_write_hex32(device_features_lo);
    serial_write("\r\n");
    g_virtio_kbd_common->queue_select = 0u;
    if (g_virtio_kbd_common->queue_size == 0u) {
        g_virtio_kbd_common->device_status = 0u;
        serial_write("[VIRTKBD] queue zero\r\n");
        return 0;
    }

    g_virtio_kbd_queue_size = g_virtio_kbd_common->queue_size;
    if (g_virtio_kbd_queue_size > 8u) {
        g_virtio_kbd_queue_size = 8u;
    }
    g_virtio_kbd_notify_off = g_virtio_kbd_common->queue_notify_off;

    for (uint16_t i = 0u; i < g_virtio_kbd_queue_size; ++i) {
        g_virtio_kbd_desc[i].addr = (uint64_t)(uintptr_t)&g_virtio_kbd_events[i];
        g_virtio_kbd_desc[i].len = sizeof(struct virtio_input_event);
        g_virtio_kbd_desc[i].flags = 2u;
        g_virtio_kbd_desc[i].next = 0u;
        g_virtio_kbd_events[i].type = 0u;
        g_virtio_kbd_events[i].code = 0u;
        g_virtio_kbd_events[i].value = 0u;
        g_virtio_kbd_avail.ring[i] = i;
        g_virtio_kbd_used.ring[i].id = 0u;
        g_virtio_kbd_used.ring[i].len = 0u;
    }
    g_virtio_kbd_avail.flags = 0u;
    g_virtio_kbd_avail.idx = g_virtio_kbd_queue_size;
    g_virtio_kbd_avail.used_event = 0u;
    g_virtio_kbd_used.flags = 0u;
    g_virtio_kbd_used.idx = 0u;
    g_virtio_kbd_avail_idx = g_virtio_kbd_queue_size;
    g_virtio_kbd_used_idx = 0u;

    g_virtio_kbd_common->queue_select = 0u;
    g_virtio_kbd_common->queue_size = g_virtio_kbd_queue_size;
    g_virtio_kbd_common->queue_desc = (uint64_t)(uintptr_t)g_virtio_kbd_desc;
    g_virtio_kbd_common->queue_driver = (uint64_t)(uintptr_t)&g_virtio_kbd_avail;
    g_virtio_kbd_common->queue_device = (uint64_t)(uintptr_t)&g_virtio_kbd_used;
    g_virtio_kbd_common->queue_enable = 1u;
    memory_barrier();
    g_virtio_kbd_common->device_status |= 8u;
    if ((g_virtio_kbd_common->device_status & 8u) == 0u) {
        g_virtio_kbd_common->device_status = 0u;
        serial_write("[VIRTKBD] features reject\r\n");
        return 0;
    }
    g_virtio_kbd_common->device_status |= 4u;
    g_virtio_keyboard_ready = 1u;
    serial_write("[VIRTKBD] ready q=");
    serial_write_hex16(g_virtio_kbd_queue_size);
    serial_write(" bus=");
    serial_write_hex8(g_virtio_kbd_bus);
    serial_putc(':');
    serial_write_hex8(g_virtio_kbd_slot);
    serial_putc('.');
    serial_write_hex8(g_virtio_kbd_function);
    serial_write("\r\n");
    virtio_keyboard_notify();
    return 1;
}

static uint32_t pci_emit_scan(
    struct luna_pci_record *records,
    uint32_t capacity,
    uint32_t start_index
) {
    uint32_t copied = 0u;
    uint32_t ordinal = 0u;

    for (uint32_t bus = 0u; bus < 256u; ++bus) {
        for (uint32_t slot = 0u; slot < 32u; ++slot) {
            uint8_t function_limit = 8u;
            for (uint32_t function = 0u; function < function_limit; ++function) {
                uint16_t vendor = pci_vendor_id((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                if (vendor == 0xFFFFu) {
                    if (function == 0u) {
                        break;
                    }
                    continue;
                }
                if (function == 0u && (pci_header_type((uint8_t)bus, (uint8_t)slot, 0u) & 0x80u) == 0u) {
                    function_limit = 1u;
                }
                if (ordinal >= start_index && copied < capacity) {
                    uint32_t id_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x00u);
                    uint32_t class_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x08u);
                    uint8_t header = pci_header_type((uint8_t)bus, (uint8_t)slot, (uint8_t)function);
                    records[copied].vendor_id = (uint16_t)(id_word & 0xFFFFu);
                    records[copied].device_id = (uint16_t)(id_word >> 16);
                    records[copied].bus = (uint8_t)bus;
                    records[copied].slot = (uint8_t)slot;
                    records[copied].function = (uint8_t)function;
                    records[copied].class_code = (uint8_t)(class_word >> 24);
                    records[copied].subclass = (uint8_t)(class_word >> 16);
                    records[copied].prog_if = (uint8_t)(class_word >> 8);
                    records[copied].header_type = header;
                    records[copied].reserved0[0] = 0u;
                    records[copied].reserved0[1] = 0u;
                    records[copied].reserved0[2] = 0u;
                    copied += 1u;
                }
                ordinal += 1u;
                if (copied >= capacity) {
                    return copied;
                }
            }
        }
    }
    return copied;
}

static volatile struct luna_uefi_disk_handoff *uefi_disk_handoff(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    return (volatile struct luna_uefi_disk_handoff *)(uintptr_t)(manifest->bootview_base + 0x100u);
}

static int firmware_read_sector(uint32_t lba, uint8_t *out) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC || handoff->read_blocks_entry == 0u || handoff->block_io_protocol == 0u) {
        return 0;
    }
    if (((efi_block_rw_fn_t)(uintptr_t)handoff->read_blocks_entry)(
        (void *)(uintptr_t)handoff->block_io_protocol,
        (uint32_t)handoff->media_id,
        lba,
        512u,
        g_firmware_disk_sector
    ) != 0u) {
        return 0;
    }
    copy_bytes(out, g_firmware_disk_sector, 512u);
    return 1;
}

static int firmware_write_sector(uint32_t lba, const uint8_t *src) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC || handoff->write_blocks_entry == 0u || handoff->block_io_protocol == 0u) {
        return 0;
    }
    copy_bytes(g_firmware_disk_sector, src, 512u);
    if (((efi_block_rw_fn_t)(uintptr_t)handoff->write_blocks_entry)(
        (void *)(uintptr_t)handoff->block_io_protocol,
        (uint32_t)handoff->media_id,
        lba,
        512u,
        g_firmware_disk_sector
    ) == 0u) {
        return 1;
    }
    return 0;
}

static int ata_wait_ready(void) {
    for (uint32_t loops = 0; loops < 100000u; ++loops) {
        uint8_t status = inb(0x1F7);
        if ((status & 0x80u) == 0u && (status & 0x08u) != 0u) {
            return 1;
        }
        if ((status & 0x01u) != 0u) {
            return 0;
        }
    }
    return 0;
}

static int ata_wait_idle(void) {
    for (uint32_t loops = 0; loops < 100000u; ++loops) {
        uint8_t status = inb(0x1F7);
        if ((status & 0x80u) == 0u) {
            return (status & 0x01u) == 0u;
        }
    }
    return 0;
}

static void ata_select(uint32_t lba, uint8_t sector_count, uint8_t command) {
    outb(0x1F6, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    outb(0x1F2, sector_count);
    outb(0x1F3, (uint8_t)(lba & 0xFFu));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFFu));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFFu));
    outb(0x1F7, command);
    io_delay();
}

static int ata_read_sector(uint32_t lba, uint8_t *out) {
    ata_select(lba, 1u, 0x20u);
    if (!ata_wait_ready()) {
        return 0;
    }
    for (size_t i = 0; i < 256u; ++i) {
        uint16_t word = inw(0x1F0);
        out[i * 2u] = (uint8_t)(word & 0xFFu);
        out[i * 2u + 1u] = (uint8_t)(word >> 8);
    }
    return ata_wait_idle();
}

static int ata_write_sector(uint32_t lba, const uint8_t *src) {
    ata_select(lba, 1u, 0x30u);
    if (!ata_wait_ready()) {
        return 0;
    }
    for (size_t i = 0; i < 256u; ++i) {
        uint16_t lo = src[i * 2u];
        uint16_t hi = src[i * 2u + 1u];
        outw(0x1F0, (uint16_t)(lo | (hi << 8)));
    }
    outb(0x1F7, 0xE7u);
    if (!ata_wait_idle()) {
        return 0;
    }
    return 1;
}

static int ahci_wait_ready(struct ahci_port_mmio *port) {
    for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
        uint32_t tfd = port->tfd;
        if ((tfd & (0x80u | 0x08u)) == 0u) {
            return 1;
        }
        if ((tfd & 0x01u) != 0u) {
            return 0;
        }
    }
    return 0;
}

static int ahci_wait_idle(struct ahci_port_mmio *port) {
    for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
        if ((port->ci & 0x1u) == 0u && (port->sact & 0x1u) == 0u) {
            return (port->tfd & 0x01u) == 0u;
        }
    }
    return 0;
}

static int ahci_port_reconnect(struct ahci_port_mmio *port) {
    uint32_t sctl = port->sctl;

    port->sctl = (sctl & ~0x0Fu) | 0x01u;
    for (uint32_t loops = 0u; loops < 100000u; ++loops) {
    }
    port->sctl = (sctl & ~0x0Fu);
    for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
        uint32_t ssts = port->ssts;
        if ((ssts & 0x0Fu) == 0x03u && ((ssts >> 8) & 0x0Fu) == 0x01u) {
            return 1;
        }
    }
    return 0;
}

static void zero_bytes(void *dst, size_t len) {
    uint8_t *out = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        out[i] = 0u;
    }
}

static void net_mmio_write32(uint32_t offset, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(g_net_mmio + offset);
    *reg = value;
}

static uint32_t net_mmio_read32(uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)(g_net_mmio + offset);
    return *reg;
}

static int intel_net_wait_tx_done(void) {
    volatile struct intel_tx_desc *desc;
    uint16_t slot = g_net_tx_index;
    desc = &g_net_tx_ring[slot];
    for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
        if ((desc->status & 0x01u) != 0u) {
            return 1;
        }
    }
    return 0;
}

static int intel_net_write(const uint8_t *src, uint64_t size) {
    uint64_t frame_size;
    uint16_t slot;
    uintptr_t tx_addr;
    if (g_net_ready == 0u || size == 0u || size > sizeof(g_net_tx_buffer)) {
        return 0;
    }
    if (size > sizeof(g_net_tx_buffer) - 14u) {
        return 0;
    }
    slot = g_net_tx_index;
    copy_bytes(g_net_tx_buffer + 0u, g_net_mac, 6u);
    copy_bytes(g_net_tx_buffer + 6u, g_net_mac, 6u);
    g_net_tx_buffer[12] = 0x88u;
    g_net_tx_buffer[13] = 0xB5u;
    copy_bytes(g_net_tx_buffer + 14u, src, (size_t)size);
    frame_size = size + 14u;
    if (frame_size < 60u) {
        zero_bytes(g_net_tx_buffer + frame_size, (size_t)(60u - frame_size));
        frame_size = 60u;
    }
    tx_addr = (uintptr_t)g_net_tx_buffer;
    g_net_tx_ring[slot].addr_low = (uint32_t)(tx_addr & 0xFFFFFFFFu);
    g_net_tx_ring[slot].addr_high = (uint32_t)((uint64_t)tx_addr >> 32);
    g_net_tx_ring[slot].length = (uint16_t)frame_size;
    g_net_tx_ring[slot].cmd = 0x0Bu;
    g_net_tx_ring[slot].status = 0u;
    g_net_tx_tail = (uint16_t)((slot + 1u) & 7u);
    net_mmio_write32(0x3818u, g_net_tx_tail);
    if (!intel_net_wait_tx_done()) {
        return 0;
    }
    g_net_tx_packets += 1u;
    g_net_tx_index = (uint16_t)((slot + 1u) & 7u);
    return 1;
}

static int intel_net_read(uint8_t *out, uint64_t *size) {
    volatile struct intel_rx_desc *desc;
    uint16_t slot;
    uint8_t status;
    if (g_net_ready == 0u) {
        return 0;
    }
    slot = g_net_rx_index;
    desc = &g_net_rx_ring[slot];
    status = 0u;
    for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
        status = desc->status;
        if ((status & 0x01u) != 0u) {
            break;
        }
    }
    if ((status & 0x01u) == 0u) {
        *size = 0u;
        return 1;
    }
    if (desc->length < 14u || desc->length > sizeof(g_net_rx_buffers[slot])) {
        desc->status = 0u;
        desc->length = 0u;
        g_net_rx_tail = slot;
        net_mmio_write32(0x2818u, slot);
        g_net_rx_index = (uint16_t)((slot + 1u) & 7u);
        *size = 0u;
        return 0;
    }
    if (g_net_rx_buffers[slot][12] == 0x88u && g_net_rx_buffers[slot][13] == 0xB5u) {
        *size = (uint64_t)(desc->length - 14u);
        if (*size > LUNA_NETWORK_PACKET_BYTES) {
            desc->status = 0u;
            desc->length = 0u;
            g_net_rx_tail = slot;
            net_mmio_write32(0x2818u, slot);
            g_net_rx_index = (uint16_t)((slot + 1u) & 7u);
            *size = 0u;
            return 0;
        }
        copy_bytes(out, g_net_rx_buffers[slot] + 14u, (size_t)(*size));
    } else {
        if (desc->length > LUNA_NETWORK_PACKET_BYTES) {
            desc->status = 0u;
            desc->length = 0u;
            g_net_rx_tail = slot;
            net_mmio_write32(0x2818u, slot);
            g_net_rx_index = (uint16_t)((slot + 1u) & 7u);
            *size = 0u;
            return 0;
        }
        copy_bytes(out, g_net_rx_buffers[slot], desc->length);
        *size = desc->length;
    }
    desc->status = 0u;
    desc->length = 0u;
    g_net_rx_packets += 1u;
    g_net_rx_tail = slot;
    net_mmio_write32(0x2818u, slot);
    g_net_rx_index = (uint16_t)((slot + 1u) & 7u);
    return 1;
}

static void intel_net_fill_info(struct luna_net_info *info) {
    zero_bytes(info, sizeof(*info));
    info->version = 1u;
    info->driver_family = net_driver_family();
    info->state_flags = net_lane_flags();
    info->ctrl = g_net_mmio != 0u ? net_mmio_read32(0x0000u) : 0u;
    info->vendor_id = g_net_vendor;
    info->device_id = g_net_device;
    if (info->vendor_id == 0u && info->driver_family == LUNA_LANE_DRIVER_E1000) {
        info->vendor_id = 0x8086u;
        info->device_id = 0x100Eu;
    } else if (info->vendor_id == 0u && info->driver_family == LUNA_LANE_DRIVER_E1000E) {
        info->vendor_id = 0x8086u;
        info->device_id = 0x10D3u;
    }
    info->rx_head = g_net_rx_index;
    info->rx_tail = g_net_rx_tail;
    info->tx_head = g_net_tx_index;
    info->tx_tail = g_net_tx_tail;
    info->hw_rx_head = (uint16_t)(g_net_mmio != 0u ? net_mmio_read32(0x2810u) : 0u);
    info->hw_rx_tail = (uint16_t)(g_net_mmio != 0u ? net_mmio_read32(0x2818u) : 0u);
    info->hw_tx_head = (uint16_t)(g_net_mmio != 0u ? net_mmio_read32(0x3810u) : 0u);
    info->hw_tx_tail = (uint16_t)(g_net_mmio != 0u ? net_mmio_read32(0x3818u) : 0u);
    info->mmio_base = (uint64_t)g_net_mmio;
    info->tx_packets = g_net_tx_packets;
    info->rx_packets = g_net_rx_packets;
    info->rctl = g_net_mmio != 0u ? net_mmio_read32(0x0100u) : 0u;
    info->tctl = g_net_mmio != 0u ? net_mmio_read32(0x0400u) : 0u;
    info->status = g_net_mmio != 0u ? net_mmio_read32(0x0008u) : 0u;
    copy_bytes(info->mac, g_net_mac, sizeof(g_net_mac));
}

static int intel_net_init(void) {
    uint8_t bus = 0u;
    uint8_t slot = 0u;
    uint8_t function = 0u;
    uint32_t command;
    uint32_t ctrl;
    uint32_t ral;
    uint32_t rah;
    uint32_t bar0;

    if (g_net_ready != 0u) {
        return 1;
    }
    if (pci_find_class_location(0x02u, 0x00u, 0x00u, 0x8086u, 0x10D3u, &bus, &slot, &function)) {
        g_net_driver_family = LUNA_LANE_DRIVER_E1000E;
        g_net_vendor = 0x8086u;
        g_net_device = 0x10D3u;
    } else if (pci_find_class_location(0x02u, 0x00u, 0x00u, 0x8086u, 0x100Eu, &bus, &slot, &function)) {
        g_net_driver_family = LUNA_LANE_DRIVER_E1000;
        g_net_vendor = 0x8086u;
        g_net_device = 0x100Eu;
    } else {
        return 0;
    }

    command = pci_config_read32(bus, slot, function, 0x04u);
    command |= 0x00000006u;
    pci_config_write16(bus, slot, function, 0x04u, (uint16_t)(command & 0xFFFFu));

    bar0 = pci_config_read32(bus, slot, function, 0x10u);
    if (g_net_device == 0x100Eu && (bar0 & 0xFFFFFFF0u) >= 0x40000000u) {
        uint32_t low_mmio = 0x10000000u;
        outl(0xCF8u, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)function << 8) | 0x10u);
        outl(0xCFCu, low_mmio);
        command = pci_config_read32(bus, slot, function, 0x04u) | 0x00000006u;
        pci_config_write16(bus, slot, function, 0x04u, (uint16_t)(command & 0xFFFFu));
    }

    g_net_mmio = pci_find_mmio_bar(bus, slot, function);
    if (g_net_mmio == 0u) {
        return 0;
    }
    command = pci_config_read32(bus, slot, function, 0x04u);

    ctrl = net_mmio_read32(0x0000u);
    net_mmio_write32(0x0000u, ctrl | (1u << 6) | (1u << 5));
    net_mmio_write32(0x00D8u, 0xFFFFFFFFu);
    net_mmio_write32(0x00D0u, 0u);

    zero_bytes(g_net_rx_ring, sizeof(g_net_rx_ring));
    zero_bytes(g_net_tx_ring, sizeof(g_net_tx_ring));
    zero_bytes(g_net_rx_buffers, sizeof(g_net_rx_buffers));
    zero_bytes(g_net_tx_buffer, sizeof(g_net_tx_buffer));
    g_net_rx_index = 0u;
    g_net_rx_tail = 7u;
    g_net_tx_index = 0u;
    g_net_tx_tail = 0u;
    g_net_tx_packets = 0u;
    g_net_rx_packets = 0u;

    for (uint32_t i = 0u; i < 8u; ++i) {
        uintptr_t rx_addr = (uintptr_t)g_net_rx_buffers[i];
        g_net_rx_ring[i].addr_low = (uint32_t)(rx_addr & 0xFFFFFFFFu);
        g_net_rx_ring[i].addr_high = (uint32_t)((uint64_t)rx_addr >> 32);
        g_net_rx_ring[i].status = 0u;
    }

    {
        uintptr_t tx_addr = (uintptr_t)g_net_tx_buffer;
        for (uint32_t i = 0u; i < 8u; ++i) {
            g_net_tx_ring[i].addr_low = (uint32_t)(tx_addr & 0xFFFFFFFFu);
            g_net_tx_ring[i].addr_high = (uint32_t)((uint64_t)tx_addr >> 32);
            g_net_tx_ring[i].status = 0x01u;
        }
    }

    ral = net_mmio_read32(0x5400u);
    rah = net_mmio_read32(0x5404u);
    net_mmio_write32(0x5400u, ral);
    net_mmio_write32(0x5404u, rah | 0x80000000u);
    rah |= 0x80000000u;
    g_net_mac[0] = (uint8_t)(ral & 0xFFu);
    g_net_mac[1] = (uint8_t)((ral >> 8) & 0xFFu);
    g_net_mac[2] = (uint8_t)((ral >> 16) & 0xFFu);
    g_net_mac[3] = (uint8_t)((ral >> 24) & 0xFFu);
    g_net_mac[4] = (uint8_t)(rah & 0xFFu);
    g_net_mac[5] = (uint8_t)((rah >> 8) & 0xFFu);

    net_mmio_write32(0x2800u, (uint32_t)((uintptr_t)g_net_rx_ring & 0xFFFFFFFFu));
    net_mmio_write32(0x2804u, (uint32_t)((uint64_t)(uintptr_t)g_net_rx_ring >> 32));
    net_mmio_write32(0x2808u, sizeof(g_net_rx_ring));
    net_mmio_write32(0x2810u, 0u);
    net_mmio_write32(0x2818u, g_net_rx_tail);
    net_mmio_write32(0x0100u, 0x0400C00Au);

    net_mmio_write32(0x3800u, (uint32_t)((uintptr_t)g_net_tx_ring & 0xFFFFFFFFu));
    net_mmio_write32(0x3804u, (uint32_t)((uint64_t)(uintptr_t)g_net_tx_ring >> 32));
    net_mmio_write32(0x3808u, sizeof(g_net_tx_ring));
    net_mmio_write32(0x3810u, 0u);
    net_mmio_write32(0x3818u, g_net_tx_tail);
    net_mmio_write32(0x0400u, 0x000401FAu);
    net_mmio_write32(0x0410u, 0x0060200Au);

    g_net_ready = 1u;
    return 1;
}

static int ahci_submit_rw(uint32_t lba, uint8_t *buffer, uint8_t write) {
    struct ahci_hba_mmio *hba = (struct ahci_hba_mmio *)g_ahci_abar;
    struct ahci_port_mmio *port;
    struct ahci_command_header *header;
    struct ahci_command_table *table;
    uintptr_t cmd_list_addr = (uintptr_t)g_ahci_cmd_list;
    uintptr_t fis_addr = (uintptr_t)&g_ahci_fis;
    uintptr_t table_addr = (uintptr_t)&g_ahci_cmd_table;
    uint8_t *cfis;

    if (g_ahci_ready == 0u || g_ahci_abar == 0u) {
        return 0;
    }
    port = &hba->ports[g_ahci_port];
    if (!ahci_wait_ready(port)) {
        serial_write("[AHCI] ready fail op=");
        serial_write(write != 0u ? "write" : "read");
        serial_write(" lba=");
        serial_write_hex32(lba);
        serial_write(" is=");
        serial_write_hex32(port->is);
        serial_write(" tfd=");
        serial_write_hex32(port->tfd);
        serial_write(" serr=");
        serial_write_hex32(port->serr);
        serial_write(" ci=");
        serial_write_hex32(port->ci);
        serial_write(" sact=");
        serial_write_hex32(port->sact);
        serial_write("\r\n");
        return 0;
    }

    zero_bytes(g_ahci_cmd_list, sizeof(g_ahci_cmd_list));
    zero_bytes(&g_ahci_fis, sizeof(g_ahci_fis));
    zero_bytes(&g_ahci_cmd_table, sizeof(g_ahci_cmd_table));

    port->clb = (uint32_t)(cmd_list_addr & 0xFFFFFFFFu);
    port->clbu = (uint32_t)((uint64_t)cmd_list_addr >> 32);
    port->fb = (uint32_t)(fis_addr & 0xFFFFFFFFu);
    port->fbu = (uint32_t)((uint64_t)fis_addr >> 32);

    header = &g_ahci_cmd_list[0];
    header->flags = 5u | (write != 0u ? (1u << 6) : 0u);
    header->prdtl = 1u;
    header->prdbc = 0u;
    header->ctba = (uint32_t)(table_addr & 0xFFFFFFFFu);
    header->ctbau = (uint32_t)((uint64_t)table_addr >> 32);

    table = &g_ahci_cmd_table;
    table->prdt[0].dba = (uint32_t)((uintptr_t)buffer & 0xFFFFFFFFu);
    table->prdt[0].dbau = (uint32_t)((uint64_t)(uintptr_t)buffer >> 32);
    table->prdt[0].dbc = (512u - 1u) | (1u << 31);

    cfis = table->cfis;
    cfis[0] = 0x27u;
    cfis[1] = 0x80u;
    cfis[2] = (uint8_t)(write != 0u ? 0x35u : 0x25u);
    cfis[3] = 0u;
    cfis[4] = (uint8_t)(lba & 0xFFu);
    cfis[5] = (uint8_t)((lba >> 8) & 0xFFu);
    cfis[6] = (uint8_t)((lba >> 16) & 0xFFu);
    cfis[7] = 0x40u;
    cfis[8] = (uint8_t)((lba >> 24) & 0xFFu);
    cfis[9] = 0u;
    cfis[10] = 0u;
    cfis[11] = 0u;
    cfis[12] = 1u;
    cfis[13] = 0u;

    memory_barrier();
    port->is = 0xFFFFFFFFu;
    port->serr = 0xFFFFFFFFu;
    port->ci = 1u;
    if (!ahci_wait_idle(port)) {
        serial_write("[AHCI] idle fail op=");
        serial_write(write != 0u ? "write" : "read");
        serial_write(" lba=");
        serial_write_hex32(lba);
        serial_write(" is=");
        serial_write_hex32(port->is);
        serial_write(" tfd=");
        serial_write_hex32(port->tfd);
        serial_write(" serr=");
        serial_write_hex32(port->serr);
        serial_write(" ci=");
        serial_write_hex32(port->ci);
        serial_write(" sact=");
        serial_write_hex32(port->sact);
        serial_write("\r\n");
        return 0;
    }
    if ((port->is & (1u << 30)) != 0u || (port->tfd & 0x01u) != 0u) {
        serial_write("[AHCI] status fail op=");
        serial_write(write != 0u ? "write" : "read");
        serial_write(" lba=");
        serial_write_hex32(lba);
        serial_write(" is=");
        serial_write_hex32(port->is);
        serial_write(" tfd=");
        serial_write_hex32(port->tfd);
        serial_write(" serr=");
        serial_write_hex32(port->serr);
        serial_write(" ci=");
        serial_write_hex32(port->ci);
        serial_write(" sact=");
        serial_write_hex32(port->sact);
        serial_write("\r\n");
        return 0;
    }
    return 1;
}

static int ahci_read_sector(uint32_t lba, uint8_t *out) {
    if (!ahci_submit_rw(lba, g_ahci_dma_sector, 0u)) {
        return 0;
    }
    copy_bytes(out, g_ahci_dma_sector, 512u);
    return 1;
}

static int ahci_write_sector(uint32_t lba, const uint8_t *src) {
    copy_bytes(g_ahci_dma_sector, src, 512u);
    if (!ahci_submit_rw(lba, g_ahci_dma_sector, 1u)) {
        return 0;
    }
    return 1;
}

static int disk_write_sector(uint32_t lba, const uint8_t *src) {
    if (g_ahci_ready == 0u) {
        ahci_init();
    }
    if (ahci_write_sector(lba, src)) {
        return 1;
    }
    if (firmware_write_sector(lba, src)) {
        return 1;
    }
    if (ata_write_sector(lba, src)) {
        return 1;
    }
    serial_write("[DISK] write fail lba=");
    serial_write_hex32(lba);
    serial_write("\r\n");
    return 0;
}

static int ahci_init(void) {
    uint8_t bus = 0u;
    uint8_t slot = 0u;
    uint8_t function = 0u;
    uint32_t abar_low;
    uint32_t abar_high;
    uint32_t cmd;
    struct ahci_hba_mmio *hba;

    if (g_ahci_ready != 0u) {
        return 1;
    }
    if (!pci_find_ahci_location(&bus, &slot, &function)) {
        return 0;
    }

    cmd = pci_config_read32(bus, slot, function, 0x04u);
    cmd |= 0x00000006u;
    outl(0xCF8u, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)function << 8) | 0x04u);
    outl(0xCFCu, cmd);

    abar_low = pci_config_read32(bus, slot, function, 0x24u) & 0xFFFFFFF0u;
    abar_high = pci_config_read32(bus, slot, function, 0x28u);
    g_ahci_abar = (uintptr_t)(((uint64_t)abar_high << 32) | abar_low);
    if (g_ahci_abar == 0u) {
        return 0;
    }

    hba = (struct ahci_hba_mmio *)g_ahci_abar;
    hba->ghc |= (1u << 31);
    hba->is = 0xFFFFFFFFu;

    for (uint32_t port_index = 0u; port_index < 32u; ++port_index) {
        struct ahci_port_mmio *port;
        uint32_t ssts;
        if ((hba->pi & (1u << port_index)) == 0u) {
            continue;
        }
        port = &hba->ports[port_index];
        ssts = port->ssts;
        if ((ssts & 0x0Fu) != 0x03u || ((ssts >> 8) & 0x0Fu) != 0x01u) {
            continue;
        }
        if (port->sig != 0x00000101u) {
            continue;
        }

        port->cmd &= ~0x01u;
        port->cmd &= ~(1u << 4);
        for (uint32_t loops = 0u; loops < 100000u; ++loops) {
            if ((port->cmd & ((1u << 15) | (1u << 14))) == 0u) {
                break;
            }
        }

        if (!ahci_port_reconnect(port)) {
            continue;
        }
        zero_bytes(g_ahci_cmd_list, sizeof(g_ahci_cmd_list));
        zero_bytes(&g_ahci_fis, sizeof(g_ahci_fis));
        zero_bytes(&g_ahci_cmd_table, sizeof(g_ahci_cmd_table));

        port->clb = (uint32_t)((uintptr_t)g_ahci_cmd_list & 0xFFFFFFFFu);
        port->clbu = (uint32_t)((uint64_t)(uintptr_t)g_ahci_cmd_list >> 32);
        port->fb = (uint32_t)((uintptr_t)&g_ahci_fis & 0xFFFFFFFFu);
        port->fbu = (uint32_t)((uint64_t)(uintptr_t)&g_ahci_fis >> 32);
        port->is = 0xFFFFFFFFu;
        port->serr = 0xFFFFFFFFu;
        port->ie = 0u;
        port->cmd |= (1u << 4);
        port->cmd |= 0x01u;

        if (!ahci_wait_ready(port)) {
            continue;
        }
        g_ahci_port = (uint8_t)port_index;
        g_ahci_ready = 1u;
        return 1;
    }
    return 0;
}

static void govern_driver_class(
    uint32_t driver_class,
    uint32_t device_id,
    uint32_t lane_class,
    uint32_t driver_family,
    uint32_t state_flags,
    const char *lane_name,
    const char *driver_name,
    uint32_t failure_class
) {
    uint32_t volume_state = 0u;
    uint32_t mode = 0u;

    driver_audit("audit driver.probe start");
    if (governance_approve(
            LUNA_CAP_DEVICE_BIND,
            g_driver_bind_cid,
            LUNA_DATA_OBJECT_TYPE_DRIVER_BIND,
            driver_class,
            &volume_state,
            &mode
        ) == LUNA_GATE_OK) {
        driver_audit("audit driver.bind approved=SECURITY");
        stage_driver_binding(
            driver_class,
            LUNA_DRIVER_STAGE_BIND,
            failure_class,
            device_id,
            lane_class,
            driver_family,
            state_flags,
            lane_name,
            driver_name
        );
        return;
    }
    driver_fail_audit("audit driver.bind denied=SECURITY");
    stage_driver_binding(
        driver_class,
        LUNA_DRIVER_STAGE_FAIL,
        failure_class,
        device_id,
        lane_class,
        driver_family,
        state_flags,
        lane_name,
        driver_name
    );
}

void SYSV_ABI device_entry_boot(const struct luna_bootview *bootview) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    int input_ready = 0;
    uint32_t display_failure = LUNA_DRIVER_FAIL_NONE;
    uint32_t input_failure = LUNA_DRIVER_FAIL_NONE;
    uint32_t network_failure = LUNA_DRIVER_FAIL_NONE;
    uint32_t storage_failure = LUNA_DRIVER_FAIL_NONE;
    uint32_t platform_failure = LUNA_DRIVER_FAIL_NONE;
    if (bootview != 0) {
        g_display_framebuffer_base = bootview->framebuffer_base;
        g_display_framebuffer_size = bootview->framebuffer_size;
        g_display_framebuffer_width = bootview->framebuffer_width;
        g_display_framebuffer_height = bootview->framebuffer_height;
        g_display_framebuffer_stride = bootview->framebuffer_pixels_per_scanline;
        g_display_framebuffer_format = bootview->framebuffer_pixel_format;
    }
    if (ahci_init()) {
        g_disk_driver_family = LUNA_LANE_DRIVER_AHCI;
    } else if (handoff->magic == LUNA_UEFI_DISK_MAGIC
        && handoff->block_io_protocol != 0u
        && handoff->read_blocks_entry != 0u) {
        g_disk_driver_family = LUNA_LANE_DRIVER_UEFI_BLOCK_IO;
    } else if (pci_find_class_device(0x01u, 0x01u, 0x80u, 0x8086u, 0x7010u)) {
        g_disk_driver_family = LUNA_LANE_DRIVER_PCI_IDE;
    } else {
        g_disk_driver_family = LUNA_LANE_DRIVER_ATA_PIO;
    }
    if (intel_net_init()) {
    } else if (pci_find_class_device(0x02u, 0x00u, 0x00u, 0x8086u, 0x100Eu)) {
        g_net_driver_family = LUNA_LANE_DRIVER_E1000;
    } else if (pci_find_class_device(0x02u, 0x00u, 0x00u, 0x8086u, 0x10D3u)) {
        g_net_driver_family = LUNA_LANE_DRIVER_E1000E;
    } else {
        g_net_driver_family = LUNA_LANE_DRIVER_SOFT_LOOP;
    }
    g_clock_tsc_hz = calibrate_tsc_hz();
    g_clock_tsc_base = clock_ticks();
    serial_write("[DEVICE] clock ready\r\n");
    if (!virtio_keyboard_init()) {
        input_ready = keyboard_init();
    } else {
        input_ready = 1;
    }
    if (mouse_init()) {
        input_ready = 1;
    }
    if (input_ready != 0) {
        serial_write("[DEVICE] input ready\r\n");
    } else {
        serial_write("[DEVICE] input offline\r\n");
    }
    (void)request_capability(LUNA_CAP_OBSERVE_LOG, &g_observe_log_cid);
    g_observe_logging_enabled = 0u;
    (void)request_capability(LUNA_CAP_DATA_SEED, &g_data_seed_cid);
    (void)request_capability(LUNA_CAP_DATA_POUR, &g_data_pour_cid);
    (void)request_capability(LUNA_CAP_DATA_DRAW, &g_data_draw_cid);
    (void)request_capability(LUNA_CAP_DATA_GATHER, &g_data_gather_cid);
    (void)request_capability(LUNA_CAP_DEVICE_BIND, &g_driver_bind_cid);
    (void)request_capability(LUNA_CAP_LIFE_WAKE, &g_lifecycle_wake_cid);
    (void)request_capability(LUNA_CAP_SYSTEM_QUERY, &g_system_query_cid);

    if ((disk_lane_flags() & LUNA_LANE_FLAG_READY) == 0u) {
        storage_failure = LUNA_DRIVER_FAIL_FATAL;
        g_driver_lifecycle_flags |= LUNA_DRIVER_LIFE_STORAGE_FATAL;
        g_driver_fatal_seen = 1u;
    }
    govern_driver_class(
        LUNA_DRIVER_CLASS_STORAGE_BOOT,
        LUNA_DEVICE_ID_DISK0,
        LUNA_LANE_CLASS_BLOCK,
        disk_driver_family(),
        disk_lane_flags(),
        "disk0",
        disk_driver_name(),
        storage_failure
    );

    if (display_driver_family() == LUNA_LANE_DRIVER_VGA_TEXT) {
        display_failure = LUNA_DRIVER_FAIL_DEGRADED;
        g_driver_lifecycle_flags |= LUNA_DRIVER_LIFE_DISPLAY_DEGRADED;
    }
    govern_driver_class(
        LUNA_DRIVER_CLASS_DISPLAY_MIN,
        LUNA_DEVICE_ID_DISPLAY0,
        LUNA_LANE_CLASS_SCANOUT,
        display_driver_family(),
        display_lane_flags(),
        "display0",
        display_driver_name(),
        display_failure
    );

    if (input_ready == 0) {
        input_failure = LUNA_DRIVER_FAIL_DEGRADED;
        g_driver_lifecycle_flags |= LUNA_DRIVER_LIFE_INPUT_DEGRADED;
    }
    govern_driver_class(
        LUNA_DRIVER_CLASS_INPUT_MIN,
        LUNA_DEVICE_ID_INPUT0,
        LUNA_LANE_CLASS_INPUT,
        keyboard_driver_family(),
        LUNA_LANE_FLAG_PRESENT | (input_ready != 0 ? (LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT) : 0u) | LUNA_LANE_FLAG_POLLING,
        "input0",
        input_driver_name(keyboard_driver_family()),
        input_failure
    );

    if (pci_find_device(0x8086u, 0x1237u) || pci_find_device(0x8086u, 0x7000u)) {
        g_driver_lifecycle_flags |= LUNA_DRIVER_LIFE_PLATFORM_READY;
    } else {
        platform_failure = LUNA_DRIVER_FAIL_OBSERVER_ONLY;
    }
    govern_driver_class(
        LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY,
        LUNA_DEVICE_ID_SERIAL0,
        LUNA_LANE_CLASS_STREAM,
        serial_driver_family(),
        LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT,
        "platform0",
        serial_driver_name(),
        platform_failure
    );

    if (g_net_ready == 0u && net_driver_family() == LUNA_LANE_DRIVER_SOFT_LOOP) {
        network_failure = LUNA_DRIVER_FAIL_DEGRADED;
        g_driver_lifecycle_flags |= LUNA_DRIVER_LIFE_NETWORK_DEGRADED;
    }
    govern_driver_class(
        LUNA_DRIVER_CLASS_NETWORK_BASELINE,
        LUNA_DEVICE_ID_NET0,
        LUNA_LANE_CLASS_LINK,
        net_driver_family(),
        net_lane_flags(),
        "net0",
        net_driver_name(),
        network_failure
    );

    serial_write("[DEVICE] lane ready\r\n");
    if (g_driver_fatal_seen != 0u) {
        serial_write("[DEVICE] driver fatal\r\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
}

void SYSV_ABI device_entry_gate(struct luna_device_gate *gate) {
    static const struct luna_device_record devices[7] = {
        {
        1u, LUNA_DEVICE_KIND_SERIAL, 0u, 0u, "serial0"
        },
        {
        2u, LUNA_DEVICE_KIND_DISK, 0u, 0u, "disk0"
        },
        {
        3u, LUNA_DEVICE_KIND_DISPLAY, 0u, 0u, "display0"
        },
        {
        4u, LUNA_DEVICE_KIND_CLOCK, 0u, 0u, "clock0"
        },
        {
        5u, LUNA_DEVICE_KIND_INPUT, 0u, 0u, "input0"
        },
        {
        6u, LUNA_DEVICE_KIND_NETWORK, 0u, 0u, "net0"
        },
        {
        7u, LUNA_DEVICE_KIND_INPUT, 0u, 0u, "pointer0"
        },
    };
    static const struct luna_lane_record lanes[7] = {
        {
            1u, LUNA_DEVICE_KIND_SERIAL, LUNA_LANE_CLASS_STREAM,
            LUNA_LANE_DRIVER_PIIX_UART,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT,
            0u, "serial0", "piix-uart"
        },
        {
            2u, LUNA_DEVICE_KIND_DISK, LUNA_LANE_CLASS_BLOCK,
            LUNA_LANE_DRIVER_NONE,
            0u, 0u, "disk0", "pending"
        },
        {
            3u, LUNA_DEVICE_KIND_DISPLAY, LUNA_LANE_CLASS_SCANOUT,
            LUNA_LANE_DRIVER_VGA_TEXT,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT,
            0u, "display0", "vga-text"
        },
        {
            4u, LUNA_DEVICE_KIND_CLOCK, LUNA_LANE_CLASS_CLOCK,
            LUNA_LANE_DRIVER_PIT_TSC,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT |
                LUNA_LANE_FLAG_POLLING,
            0u, "clock0", "pit-tsc"
        },
        {
            5u, LUNA_DEVICE_KIND_INPUT, LUNA_LANE_CLASS_INPUT,
            LUNA_LANE_DRIVER_I8042_KEYBOARD,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT |
                LUNA_LANE_FLAG_POLLING,
            0u, "input0", "i8042-kbd"
        },
        {
            6u, LUNA_DEVICE_KIND_NETWORK, LUNA_LANE_CLASS_LINK,
            LUNA_LANE_DRIVER_SOFT_LOOP,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_FALLBACK,
            0u, "net0", "soft-loop"
        },
        {
            7u, LUNA_DEVICE_KIND_INPUT, LUNA_LANE_CLASS_INPUT,
            LUNA_LANE_DRIVER_I8042_MOUSE,
            LUNA_LANE_FLAG_PRESENT | LUNA_LANE_FLAG_READY | LUNA_LANE_FLAG_BOOT |
                LUNA_LANE_FLAG_POLLING,
            0u, "pointer0", "i8042-mouse"
        },
    };
    struct luna_lane_record resolved_lanes[7];
    uint8_t observe_live;
    uint8_t data_live;
    uint8_t user_live;

    observe_live = system_space_active(LUNA_SPACE_OBSERVABILITY);
    data_live = system_space_active(LUNA_SPACE_DATA);
    user_live = system_space_active(LUNA_SPACE_USER);
    if (observe_live != 0u) {
        try_enable_observe_logging();
        flush_pending_driver_logs();
    }
    if (observe_live != 0u && user_live != 0u) {
        lifecycle_publish_driver_flags();
    }
    if (observe_live != 0u && data_live != 0u && user_live != 0u) {
        persist_pending_driver_bindings();
    }

    gate->result_count = 0;
    gate->status = LUNA_DEVICE_ERR_INVALID_CAP;
    if (gate->opcode == LUNA_DEVICE_LIST) {
        if (!allow_device_call(LUNA_CAP_DEVICE_LIST, gate->cid_low, gate->cid_high, LUNA_DEVICE_LIST)) {
            return;
        }
        if (gate->buffer_size < sizeof(devices[0])) {
            gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
            return;
        }
        if (gate->buffer_size >= sizeof(devices)) {
            copy_bytes((void *)(uintptr_t)gate->buffer_addr, devices, sizeof(devices));
            gate->result_count = 7;
        } else {
            copy_bytes((void *)(uintptr_t)gate->buffer_addr, &devices[0], sizeof(devices[0]));
            gate->result_count = 1;
        }
        gate->status = LUNA_DEVICE_OK;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_CENSUS) {
        uint32_t start = gate->flags;
        uint32_t available = 0u;
        uint32_t count = 0u;
        if (!allow_device_call(LUNA_CAP_DEVICE_LIST, gate->cid_low, gate->cid_high, LUNA_DEVICE_CENSUS)) {
            return;
        }
        if (gate->buffer_size < sizeof(resolved_lanes[0])) {
            gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
            return;
        }
        copy_bytes(resolved_lanes, lanes, sizeof(lanes));
        resolved_lanes[0].driver_family = serial_driver_family();
        copy_bytes(resolved_lanes[0].driver_name, serial_driver_name(), 11u);
        resolved_lanes[1].driver_family = disk_driver_family();
        resolved_lanes[1].state_flags = disk_lane_flags();
        copy_bytes(resolved_lanes[1].driver_name, disk_driver_name(), 11u);
        resolved_lanes[2].driver_family = display_driver_family();
        resolved_lanes[2].state_flags = display_lane_flags();
        copy_bytes(resolved_lanes[2].driver_name, display_driver_name(), 13u);
        resolved_lanes[5].driver_family = net_driver_family();
        resolved_lanes[5].state_flags = net_lane_flags();
        copy_bytes(resolved_lanes[5].driver_name, net_driver_name(), 11u);
        resolved_lanes[4].driver_family = keyboard_driver_family();
        copy_bytes(resolved_lanes[4].driver_name, input_driver_name(resolved_lanes[4].driver_family), 10u);
        resolved_lanes[6].driver_family = pointer_driver_family();
        copy_bytes(resolved_lanes[6].driver_name, input_driver_name(resolved_lanes[6].driver_family), 12u);
        if (start >= 7u) {
            gate->result_count = 0u;
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        available = (uint32_t)(gate->buffer_size / sizeof(resolved_lanes[0]));
        count = 7u - start;
        if (count > available) {
            count = available;
        }
        copy_bytes(
            (void *)(uintptr_t)gate->buffer_addr,
            &resolved_lanes[start],
            (size_t)count * sizeof(resolved_lanes[0])
        );
        gate->result_count = count;
        gate->status = LUNA_DEVICE_OK;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_PCI_SCAN) {
        uint32_t capacity = (uint32_t)(gate->buffer_size / sizeof(struct luna_pci_record));
        if (!allow_device_call(LUNA_CAP_DEVICE_LIST, gate->cid_low, gate->cid_high, LUNA_DEVICE_PCI_SCAN)) {
            return;
        }
        if (capacity == 0u) {
            gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
            return;
        }
        gate->result_count = pci_emit_scan(
            (struct luna_pci_record *)(uintptr_t)gate->buffer_addr,
            capacity,
            gate->flags
        );
        gate->status = LUNA_DEVICE_OK;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_READ) {
        if (!allow_device_call(LUNA_CAP_DEVICE_READ, gate->cid_low, gate->cid_high, LUNA_DEVICE_READ)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_SERIAL0) {
            if ((inb(0x3FD) & 0x01) != 0 && gate->buffer_size != 0u) {
                ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = inb(0x3F8);
                gate->size = 1;
            } else {
                gate->size = 0;
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK0
            && gate->buffer_size >= 512u
            && (ahci_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || firmware_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || ata_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr))) {
            gate->size = 512u;
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISPLAY0 && gate->buffer_size >= sizeof(struct luna_display_info)) {
            struct luna_display_info *info = (struct luna_display_info *)(uintptr_t)gate->buffer_addr;
            info->framebuffer_base = g_display_framebuffer_base;
            info->framebuffer_size = g_display_framebuffer_size;
            info->framebuffer_width = g_display_framebuffer_width;
            info->framebuffer_height = g_display_framebuffer_height;
            info->framebuffer_pixels_per_scanline = g_display_framebuffer_stride;
            info->framebuffer_pixel_format = g_display_framebuffer_format;
            info->text_columns = 80u;
            info->text_rows = 25u;
            info->state_flags = display_lane_flags();
            info->driver_family = display_driver_family();
            gate->size = sizeof(struct luna_display_info);
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_CLOCK0 && gate->buffer_size >= sizeof(uint64_t)) {
            *(uint64_t *)(uintptr_t)gate->buffer_addr = clock_microseconds();
            gate->size = sizeof(uint64_t);
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_INPUT0) {
            uint8_t value = 0u;
            if (g_virtio_keyboard_ready == 0u) {
                keyboard_init();
            }
            value = keyboard_poll_byte();
            if (value != 0u && gate->buffer_size != 0u) {
                ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = value;
                gate->size = 1;
            } else if ((inb(0x3FD) & 0x01u) != 0u && gate->buffer_size != 0u) {
                ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = inb(0x3F8);
                gate->size = 1;
            } else {
                gate->size = 0;
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_NET0) {
            if (g_net_size != 0u) {
                if (gate->buffer_size < g_net_size) {
                    gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
                    return;
                }
                copy_bytes((void *)(uintptr_t)gate->buffer_addr, g_net_buffer, (size_t)g_net_size);
                gate->size = g_net_size;
                g_net_size = 0u;
                gate->status = LUNA_DEVICE_OK;
                return;
            }
            if (intel_net_read((uint8_t *)(uintptr_t)gate->buffer_addr, &gate->size)) {
                gate->status = LUNA_DEVICE_OK;
                return;
            }
            gate->size = 0u;
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_POINTER0) {
            struct luna_pointer_event event;
            mouse_init();
            if (gate->buffer_size >= sizeof(event) && mouse_poll_event(&event)) {
                copy_bytes((void *)(uintptr_t)gate->buffer_addr, &event, sizeof(event));
                gate->size = sizeof(event);
            } else if (gate->buffer_size != 0u) {
                uint8_t value = mouse_poll_byte();
                if (value != 0u) {
                    ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = value;
                    gate->size = 1u;
                } else {
                    gate->size = 0u;
                }
            } else {
                gate->size = 0u;
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_SERIAL0 && gate->buffer_size != 0u) {
            ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = inb(0x3F8);
        }
        return;
    }
    if (gate->opcode == LUNA_DEVICE_QUERY) {
        if (!allow_device_call(LUNA_CAP_DEVICE_READ, gate->cid_low, gate->cid_high, LUNA_DEVICE_QUERY)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_NET0 && gate->buffer_size >= sizeof(struct luna_net_info)) {
            struct luna_net_info *info = (struct luna_net_info *)(uintptr_t)gate->buffer_addr;
            intel_net_fill_info(info);
            gate->size = sizeof(struct luna_net_info);
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_WRITE) {
        const uint8_t *src = (const uint8_t *)(uintptr_t)gate->buffer_addr;
        if (!allow_device_call(LUNA_CAP_DEVICE_WRITE, gate->cid_low, gate->cid_high, LUNA_DEVICE_WRITE)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_SERIAL0) {
            for (uint64_t i = 0; i < gate->size && i < gate->buffer_size; ++i) {
                serial_putc((char)src[i]);
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISPLAY0 && gate->buffer_size >= sizeof(struct luna_display_cell)) {
            const struct luna_display_cell *cell = (const struct luna_display_cell *)(uintptr_t)gate->buffer_addr;
            display_putc(cell->x, cell->y, (uint8_t)(cell->glyph & 0xFFu), (uint8_t)(cell->attr & 0xFFu));
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_NET0) {
            uint64_t size = gate->size;
            if (size > gate->buffer_size || size > sizeof(g_net_buffer)) {
                gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
                return;
            }
            if (intel_net_write(src, size)) {
                copy_bytes(g_net_buffer, src, (size_t)size);
                g_net_size = size;
                gate->status = LUNA_DEVICE_OK;
                return;
            }
            copy_bytes(g_net_buffer, src, (size_t)size);
            g_net_size = size;
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK0
            && gate->buffer_size >= 512u
            && gate->size >= 512u
            && disk_write_sector(gate->flags, src)) {
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        observe_log(3u, "device.write error");
        return;
    }
    if (gate->opcode == LUNA_DEVICE_INPUT_READ) {
        if (!allow_device_call(LUNA_CAP_DEVICE_READ, gate->cid_low, gate->cid_high, LUNA_DEVICE_INPUT_READ)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_INPUT0) {
            uint8_t value = 0u;
            if (g_virtio_keyboard_ready == 0u) {
                keyboard_init();
            }
            value = keyboard_poll_byte();
            if (value != 0u && gate->buffer_size != 0u) {
                ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = value;
                gate->size = 1u;
            } else {
                gate->size = 0u;
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_POINTER0) {
            struct luna_pointer_event event;
            mouse_init();
            if (gate->buffer_size >= sizeof(event) && mouse_poll_event(&event)) {
                copy_bytes((void *)(uintptr_t)gate->buffer_addr, &event, sizeof(event));
                gate->size = sizeof(event);
            } else {
                gate->size = 0u;
            }
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        observe_log(3u, "device.input error");
        gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_BLOCK_READ) {
        if (!allow_device_call(LUNA_CAP_DEVICE_READ, gate->cid_low, gate->cid_high, LUNA_DEVICE_BLOCK_READ)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK0
            && gate->buffer_size >= 512u
            && (ahci_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || firmware_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || ata_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr))) {
            gate->size = 512u;
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        observe_log(3u, "device.block.read err");
        gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_BLOCK_WRITE) {
        const uint8_t *src = (const uint8_t *)(uintptr_t)gate->buffer_addr;
        if (!allow_device_call(LUNA_CAP_DEVICE_WRITE, gate->cid_low, gate->cid_high, LUNA_DEVICE_BLOCK_WRITE)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK0
            && gate->buffer_size >= 512u
            && gate->size >= 512u
            && disk_write_sector(gate->flags, src)) {
            gate->status = LUNA_DEVICE_OK;
            return;
        }
        observe_log(3u, "device.block.write err");
        gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_DISPLAY_PRESENT) {
        uint64_t bytes;
        if (!allow_device_call(LUNA_CAP_DEVICE_WRITE, gate->cid_low, gate->cid_high, LUNA_DEVICE_DISPLAY_PRESENT)) {
            return;
        }
        if (gate->device_id != LUNA_DEVICE_ID_DISPLAY0
            || g_display_framebuffer_base == 0u
            || g_display_framebuffer_size == 0u
            || gate->buffer_addr == 0u) {
            observe_log(3u, "device.present error");
            gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
            return;
        }
        bytes = gate->buffer_size;
        if (bytes > g_display_framebuffer_size) {
            bytes = g_display_framebuffer_size;
        }
        copy_bytes((void *)(uintptr_t)g_display_framebuffer_base, (const void *)(uintptr_t)gate->buffer_addr, (size_t)bytes);
        gate->size = bytes;
        gate->status = LUNA_DEVICE_OK;
        return;
    }
}
