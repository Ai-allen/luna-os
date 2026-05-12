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
    uint64_t diag_status;
    uint64_t diag_handle_count;
    uint64_t diag_full_disk_seen;
    uint64_t target_block_io_protocol;
    uint64_t target_media_id;
    uint64_t target_read_blocks_entry;
    uint64_t target_write_blocks_entry;
    uint64_t flags;
};

#define LUNA_UEFI_DISK_FLAG_SOURCE_REMOVABLE 0x1u
#define LUNA_UEFI_DISK_FLAG_SOURCE_READ_ONLY 0x2u
#define LUNA_UEFI_DISK_FLAG_SOURCE_LOGICAL_PARTITION 0x4u
#define LUNA_UEFI_DISK_FLAG_TARGET_PRESENT 0x8u
#define LUNA_UEFI_DISK_FLAG_TARGET_SEPARATE 0x10u
#define LUNA_NATIVE_LAYOUT_RECORD_SECTORS 24u
#define LUNA_NATIVE_LAYOUT_TXN_SECTORS 2u
#define LUNA_STORE_STATE_CLEAN 0x434C45414E000001ull
#define LUNA_STORE_STATE_DIRTY 0x4449525459000001ull

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

struct xhci_trb {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

struct xhci_erst_entry {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size;
    uint32_t reserved;
} __attribute__((packed));

typedef uint64_t (MS_ABI *efi_block_rw_fn_t)(
    void *self,
    uint32_t media_id,
    uint64_t lba,
    uint64_t buffer_size,
    void *buffer
);

static volatile struct luna_uefi_disk_handoff *uefi_disk_handoff(void);
static volatile const struct luna_bootview *device_bootview(void);
static int installer_target_bound(void);
static int installer_media_mode(void);
static int fwblk_source_ready(void);
static int fwblk_target_ready(void);
static int fwblk_target_separate(void);
static int pci_record_present(const struct luna_pci_record *record);
static void detect_disk_pci_record(void);
static void detect_serial_pci_record(void);
static void detect_display_pci_record(void);
static void detect_input_pci_record(void);
static int detect_usb_input_pci_record(void);
static void detect_net_pci_record(void);
static const char *flag_state_name(int value);
static const char *usb_hid_bind_state_name(void);
static const char *usb_hid_blocker_name(void);
static const char *serial_selection_basis_name(void);
static uint32_t driver_bind_lifecycle_consequence(uint32_t stage, uint32_t failure_class);
static uint32_t driver_bind_blocker_flags(uint32_t driver_class, uint32_t driver_family, uint32_t failure_class);
static uint32_t driver_bind_recovery_hint(uint32_t blocker_flags);
static uint32_t driver_bind_recovery_action_flags(
    uint32_t driver_class,
    uint32_t blocker_flags,
    uint32_t volume_state,
    uint32_t system_mode
);
static uint32_t driver_bind_selection_basis(uint32_t driver_class, uint32_t driver_family);
static uint32_t driver_bind_evidence_flags(uint32_t driver_class);
static uint32_t driver_bind_policy_flags(uint32_t driver_class, uint32_t blocker_flags);
static uint64_t driver_bind_lifecycle_flags(uint32_t blocker_flags);
static uint64_t driver_bind_action_lifecycle_flags(uint32_t recovery_action_flags);
static const struct luna_pci_record *driver_bind_pci_record(uint32_t driver_class);

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
static uint32_t g_input_event_debug_budget = 64u;
static uint8_t g_last_keyboard_event_source = 0u;
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
static struct luna_pci_record g_serial_pci = {0};
static struct luna_pci_record g_disk_pci = {0};
static struct luna_pci_record g_display_pci = {0};
static struct luna_pci_record g_input_pci = {0};
static struct luna_pci_record g_usb_input_pci = {0};
static struct luna_pci_record g_net_pci = {0};
static struct luna_pci_record g_platform_pci = {0};
static uint8_t g_usb_input_pci_known = 0u;
static uint8_t g_usb_hid_keyboard_ready = 0u;
static uint8_t g_xhci_ready = 0u;
static uint8_t g_xhci_port_ready = 0u;
static uint8_t g_xhci_connected_port = 0u;
static uint8_t g_xhci_port_speed = 0u;
static uint8_t g_xhci_max_slots = 0u;
static uint8_t g_xhci_max_ports = 0u;
static uintptr_t g_xhci_mmio = 0u;
static uint32_t g_xhci_op_offset = 0u;
static uint32_t g_xhci_runtime_offset = 0u;
static uint32_t g_xhci_doorbell_offset = 0u;
static const char *g_usb_hid_blocker = "controller-driver-missing";
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
static struct {
    uint32_t gate;
    uint32_t status;
} g_cap_deny_seen[16];
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
#define LUNA_DRIVER_BIND_VERSION 5u

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
static struct xhci_trb g_xhci_command_ring[16] __attribute__((aligned(64)));
static struct xhci_trb g_xhci_event_ring[16] __attribute__((aligned(64)));
static struct xhci_erst_entry g_xhci_erst[1] __attribute__((aligned(64)));
static uint64_t g_xhci_dcbaa[256] __attribute__((aligned(64)));

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
static int xhci_init(void);
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
    if (g_disk_driver_family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        return LUNA_LANE_DRIVER_UEFI_BLOCK_IO;
    }
    if (handoff->magic == LUNA_UEFI_DISK_MAGIC
        && ((handoff->block_io_protocol != 0u && handoff->read_blocks_entry != 0u) ||
            (handoff->target_block_io_protocol != 0u && handoff->target_read_blocks_entry != 0u))) {
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

enum {
    INPUT_EVENT_SOURCE_NONE = 0u,
    INPUT_EVENT_SOURCE_VIRTIO = 1u,
    INPUT_EVENT_SOURCE_I8042 = 2u,
    INPUT_EVENT_SOURCE_OPERATOR = 3u,
};

static const char *input_event_source_name(uint8_t source) {
    switch (source) {
        case INPUT_EVENT_SOURCE_VIRTIO:
            return "virtio-kbd";
        case INPUT_EVENT_SOURCE_I8042:
            return "i8042-kbd";
        case INPUT_EVENT_SOURCE_OPERATOR:
            return "serial-operator";
        default:
            return "unknown";
    }
}

static void serial_write_input_event(uint8_t source, uint8_t value) {
    if (g_input_event_debug_budget == 0u) {
        return;
    }
    serial_write("[DEVICE] input event src=");
    serial_write(input_event_source_name(source));
    serial_write(" key=");
    serial_write_hex8(value);
    serial_write("\r\n");
    g_input_event_debug_budget -= 1u;
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
        case 0x0Cu: return '-';
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
        case 0x34u: return '.';
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
        case 0x4Eu: return '-';
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
        case 0x49u: return '.';
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
        g_last_keyboard_event_source = INPUT_EVENT_SOURCE_VIRTIO;
        return virtio_value;
    }
    uint8_t value = dequeue_key();
    if (value != 0u) {
        g_last_keyboard_event_source = INPUT_EVENT_SOURCE_I8042;
        return value;
    }
    ps2_pump(32u);
    value = dequeue_key();
    if (value != 0u) {
        g_last_keyboard_event_source = INPUT_EVENT_SOURCE_I8042;
    } else {
        g_last_keyboard_event_source = INPUT_EVENT_SOURCE_NONE;
    }
    return value;
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

static uint32_t validate_capability(
    uint64_t cid_low,
    uint64_t cid_high,
    uint32_t target_gate,
    uint32_t device_id,
    uint64_t caller_space,
    uint64_t actor_space
) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    volatile struct luna_gate *gate =
        (volatile struct luna_gate *)(uintptr_t)manifest->security_gate_base;

    for (size_t i = 0; i < sizeof(struct luna_gate); ++i) {
        ((volatile uint8_t *)gate)[i] = 0;
    }
    gate->sequence = 91;
    gate->opcode = LUNA_GATE_VALIDATE_DEVICE;
    gate->caller_space = caller_space;
    gate->domain_key = 0u;
    gate->cid_low = cid_low;
    gate->cid_high = cid_high;
    gate->target_space = LUNA_SPACE_DEVICE;
    gate->target_gate = target_gate;
    gate->ttl = device_id;
    gate->seal_low = actor_space;
    gate->nonce = 0u;

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

static void serial_write_driver_bind_governance(
    const char *result,
    const char *lane_name,
    const char *driver_name,
    uint32_t driver_class,
    uint32_t driver_family,
    uint32_t blocker_flags,
    uint32_t volume_state,
    uint32_t mode,
    uint32_t recovery_action_flags
) {
    serial_write("[DEVICE] bind govern=");
    serial_write(result);
    serial_write(" lane=");
    serial_write(lane_name);
    serial_write(" driver=");
    serial_write(driver_name);
    serial_write(" class=");
    serial_write_hex32(driver_class);
    serial_write(" family=");
    serial_write_hex32(driver_family);
    serial_write(" blocker=");
    serial_write_hex32(blocker_flags);
    serial_write(" state=");
    serial_write_hex32(volume_state);
    serial_write(" mode=");
    serial_write_hex32(mode);
    serial_write(" actions=");
    serial_write_hex32(recovery_action_flags);
    serial_write("\r\n");
}

static void serial_write_driver_flags(void) {
    serial_write("[DEVICE] driver flags=");
    serial_write_hex64(g_driver_lifecycle_flags);
    serial_write(" actions=");
    serial_write_hex32((uint32_t)(g_driver_lifecycle_flags >> LUNA_DRIVER_ACTION_LIFECYCLE_SHIFT));
    serial_write("\r\n");
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
    struct luna_lifecycle_gate gate_local;
    volatile struct luna_lifecycle_gate *gate = &gate_local;

    if (g_driver_flags_published != 0u) {
        return;
    }
    if (g_lifecycle_wake_cid.low == 0u && g_lifecycle_wake_cid.high == 0u) {
        return;
    }
    zero_bytes(&gate_local, sizeof(gate_local));
    gate->sequence = 96;
    gate->opcode = LUNA_LIFE_WAKE_UNIT;
    gate->space_id = LUNA_SPACE_DEVICE;
    gate->state = 2u;
    gate->flags = g_driver_lifecycle_flags;
    gate->cid_low = g_lifecycle_wake_cid.low;
    gate->cid_high = g_lifecycle_wake_cid.high;
    ((void (SYSV_ABI *)(struct luna_lifecycle_gate *))(uintptr_t)manifest->lifecycle_gate_entry)(
        (struct luna_lifecycle_gate *)&gate_local
    );
    if (gate->status == LUNA_LIFE_OK) {
        g_driver_flags_published = 1u;
    }
}

static void stage_driver_binding(
    uint32_t driver_class,
    uint32_t stage,
    uint32_t failure_class,
    uint32_t volume_state,
    uint32_t system_mode,
    uint32_t device_id,
    uint32_t lane_class,
    uint32_t driver_family,
    uint32_t state_flags,
    const char *lane_name,
    const char *driver_name
) {
    volatile const struct luna_bootview *bootview = device_bootview();
    const struct luna_pci_record *pci_record = 0;
    uint32_t blocker_flags = 0u;
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
    blocker_flags = driver_bind_blocker_flags(driver_class, driver_family, failure_class);
    record->approval_space = LUNA_SPACE_SECURITY;
    record->selection_basis = driver_bind_selection_basis(driver_class, driver_family);
    record->evidence_flags = driver_bind_evidence_flags(driver_class);
    record->volume_state = volume_state != 0u ? volume_state : bootview->volume_state;
    record->system_mode = system_mode != 0u ? system_mode : bootview->system_mode;
    record->lifecycle_consequence = driver_bind_lifecycle_consequence(stage, failure_class);
    record->firmware_path = bootview->firmware_path;
    record->firmware_flags = bootview->firmware_flags;
    record->blocker_flags = blocker_flags;
    record->recovery_hint = driver_bind_recovery_hint(blocker_flags);
    record->recovery_action_flags = driver_bind_recovery_action_flags(
        driver_class,
        blocker_flags,
        record->volume_state,
        record->system_mode
    );
    record->lifecycle_flags = g_driver_lifecycle_flags;
    record->firmware_storage_status = bootview->firmware_storage_status;
    record->firmware_display_status = bootview->firmware_display_status;
    record->reserved1[0] = 0u;
    record->bound_at = clock_microseconds();
    record->writer_space = LUNA_SPACE_DEVICE;
    record->authority_space = LUNA_SPACE_DEVICE;
    pci_record = driver_bind_pci_record(driver_class);
    if (pci_record != 0) {
        record->pci_vendor_id = pci_record->vendor_id;
        record->pci_device_id = pci_record->device_id;
        record->pci_bus = pci_record->bus;
        record->pci_slot = pci_record->slot;
        record->pci_function = pci_record->function;
        record->pci_class_code = pci_record->class_code;
        record->pci_subclass = pci_record->subclass;
        record->pci_prog_if = pci_record->prog_if;
        record->pci_header_type = pci_record->header_type;
    }
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
            record.version == 0u ||
            record.version > LUNA_DRIVER_BIND_VERSION ||
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
    uint32_t policy_flags = 0u;
    uint32_t volume_state = 0u;
    uint32_t mode = 0u;

    if ((g_driver_bind_cid.low == 0u && g_driver_bind_cid.high == 0u) ||
        (g_data_seed_cid.low == 0u && g_data_seed_cid.high == 0u) ||
        (g_data_pour_cid.low == 0u && g_data_pour_cid.high == 0u) ||
        (g_data_draw_cid.low == 0u && g_data_draw_cid.high == 0u) ||
        (g_data_gather_cid.low == 0u && g_data_gather_cid.high == 0u)) {
        return;
    }
    policy_flags = driver_bind_policy_flags(record->driver_class, record->blocker_flags);
    if (governance_approve(
            LUNA_CAP_DEVICE_BIND,
            g_driver_bind_cid,
            LUNA_DATA_OBJECT_TYPE_DRIVER_BIND,
            policy_flags,
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
                policy_flags,
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

static int should_log_cap_deny(uint32_t target_gate, uint32_t status) {
    for (size_t i = 0u; i < sizeof(g_cap_deny_seen) / sizeof(g_cap_deny_seen[0]); ++i) {
        if (g_cap_deny_seen[i].gate == target_gate && g_cap_deny_seen[i].status == status) {
            return 0;
        }
    }
    for (size_t i = 0u; i < sizeof(g_cap_deny_seen) / sizeof(g_cap_deny_seen[0]); ++i) {
        if (g_cap_deny_seen[i].gate == 0u && g_cap_deny_seen[i].status == 0u) {
            g_cap_deny_seen[i].gate = target_gate;
            g_cap_deny_seen[i].status = status;
            return 1;
        }
    }
    return 0;
}

static int allow_device_call(
    uint64_t cid_low,
    uint64_t cid_high,
    uint32_t target_gate,
    uint32_t device_id,
    uint64_t caller_space,
    uint64_t actor_space
) {
    uint32_t status = validate_capability(
        cid_low,
        cid_high,
        target_gate,
        device_id,
        caller_space,
        actor_space
    );
    if (status != LUNA_GATE_OK) {
        observe_log(3u, "device.call denied");
        if (should_log_cap_deny(target_gate, status)) {
            serial_write("[DEVICE] cap deny gate=");
            serial_write_hex32(target_gate);
            serial_write(" status=");
            serial_write_hex32(status);
            serial_write("\r\n");
        }
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

static void pci_record_clear(struct luna_pci_record *record) {
    if (record == 0) {
        return;
    }
    *record = (struct luna_pci_record){0};
}

static int pci_record_present(const struct luna_pci_record *record) {
    if (record == 0) {
        return 0;
    }
    return record->vendor_id != 0u && record->vendor_id != 0xFFFFu;
}

static void pci_capture_record(uint8_t bus, uint8_t slot, uint8_t function, struct luna_pci_record *record) {
    uint32_t id_word;
    uint32_t class_word;
    if (record == 0) {
        return;
    }
    id_word = pci_config_read32(bus, slot, function, 0x00u);
    class_word = pci_config_read32(bus, slot, function, 0x08u);
    record->vendor_id = (uint16_t)(id_word & 0xFFFFu);
    record->device_id = (uint16_t)(id_word >> 16);
    record->bus = bus;
    record->slot = slot;
    record->function = function;
    record->class_code = (uint8_t)(class_word >> 24);
    record->subclass = (uint8_t)(class_word >> 16);
    record->prog_if = (uint8_t)(class_word >> 8);
    record->header_type = pci_header_type(bus, slot, function);
    record->reserved0[0] = 0u;
    record->reserved0[1] = 0u;
    record->reserved0[2] = 0u;
}

static int pci_find_vendor_device_record(uint16_t vendor_id, uint16_t device_id, struct luna_pci_record *record) {
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
                if ((uint16_t)(id_word & 0xFFFFu) != vendor_id || (uint16_t)(id_word >> 16) != device_id) {
                    continue;
                }
                pci_capture_record((uint8_t)bus, (uint8_t)slot, (uint8_t)function, record);
                return 1;
            }
        }
    }
    return 0;
}

static int pci_find_class_record(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if,
    int require_subclass,
    int require_prog_if,
    struct luna_pci_record *record
) {
    for (uint32_t bus = 0u; bus < 256u; ++bus) {
        for (uint32_t slot = 0u; slot < 32u; ++slot) {
            uint8_t function_limit = 8u;
            for (uint32_t function = 0u; function < function_limit; ++function) {
                uint32_t class_word;
                uint16_t vendor;
                uint8_t header;
                uint8_t current_subclass;
                uint8_t current_prog_if;
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
                class_word = pci_config_read32((uint8_t)bus, (uint8_t)slot, (uint8_t)function, 0x08u);
                if ((uint8_t)(class_word >> 24) != class_code) {
                    continue;
                }
                current_subclass = (uint8_t)(class_word >> 16);
                current_prog_if = (uint8_t)(class_word >> 8);
                if (require_subclass && current_subclass != subclass) {
                    continue;
                }
                if (require_prog_if && current_prog_if != prog_if) {
                    continue;
                }
                pci_capture_record((uint8_t)bus, (uint8_t)slot, (uint8_t)function, record);
                return 1;
            }
        }
    }
    return 0;
}

static void detect_disk_pci_record(void) {
    uint8_t bus = 0u;
    uint8_t slot = 0u;
    uint8_t function = 0u;
    if (pci_record_present(&g_disk_pci)) {
        return;
    }
    if (pci_find_ahci_location(&bus, &slot, &function)) {
        pci_capture_record(bus, slot, function, &g_disk_pci);
        return;
    }
    if (pci_find_class_location(0x01u, 0x01u, 0x80u, 0x8086u, 0x7010u, &bus, &slot, &function)) {
        pci_capture_record(bus, slot, function, &g_disk_pci);
        return;
    }
    (void)pci_find_class_record(0x01u, 0u, 0u, 0, 0, &g_disk_pci);
}

static void detect_serial_pci_record(void) {
    if (pci_record_present(&g_serial_pci)) {
        return;
    }
    if (pci_find_vendor_device_record(0x8086u, 0x2918u, &g_serial_pci)) {
        return;
    }
    (void)pci_find_vendor_device_record(0x8086u, 0x7000u, &g_serial_pci);
}

static void detect_display_pci_record(void) {
    if (pci_record_present(&g_display_pci)) {
        return;
    }
    (void)pci_find_class_record(0x03u, 0u, 0u, 0, 0, &g_display_pci);
}

static void detect_input_pci_record(void) {
    if (g_virtio_keyboard_ready == 0u) {
        pci_record_clear(&g_input_pci);
        return;
    }
    if (pci_record_present(&g_input_pci)
        && g_input_pci.bus == g_virtio_kbd_bus
        && g_input_pci.slot == g_virtio_kbd_slot
        && g_input_pci.function == g_virtio_kbd_function) {
        return;
    }
    pci_capture_record(g_virtio_kbd_bus, g_virtio_kbd_slot, g_virtio_kbd_function, &g_input_pci);
}

static int detect_usb_input_pci_record(void) {
    if (g_usb_input_pci_known != 0u) {
        return pci_record_present(&g_usb_input_pci);
    }
    g_usb_input_pci_known = 1u;
    if (pci_find_class_record(0x0Cu, 0x03u, 0x30u, 1, 1, &g_usb_input_pci)) {
        return 1;
    }
    if (pci_find_class_record(0x0Cu, 0x03u, 0x20u, 1, 1, &g_usb_input_pci)) {
        return 1;
    }
    if (pci_find_class_record(0x0Cu, 0x03u, 0x10u, 1, 1, &g_usb_input_pci)) {
        return 1;
    }
    if (pci_find_class_record(0x0Cu, 0x03u, 0x00u, 1, 1, &g_usb_input_pci)) {
        return 1;
    }
    return pci_find_class_record(0x0Cu, 0x03u, 0u, 1, 0, &g_usb_input_pci);
}

static void detect_net_pci_record(void) {
    if (pci_record_present(&g_net_pci)) {
        return;
    }
    if (g_net_vendor != 0u && g_net_device != 0u
        && pci_find_vendor_device_record(g_net_vendor, g_net_device, &g_net_pci)) {
        return;
    }
    if (pci_find_class_record(0x02u, 0x00u, 0u, 1, 0, &g_net_pci)) {
        return;
    }
    (void)pci_find_class_record(0x02u, 0u, 0u, 0, 0, &g_net_pci);
}

static void detect_platform_pci_record(void) {
    if (pci_record_present(&g_platform_pci)) {
        return;
    }
    if (pci_find_vendor_device_record(0x8086u, 0x1237u, &g_platform_pci)) {
        return;
    }
    if (pci_find_vendor_device_record(0x8086u, 0x7000u, &g_platform_pci)) {
        return;
    }
    (void)pci_find_class_record(0x06u, 0x00u, 0u, 1, 0, &g_platform_pci);
}

static uint32_t driver_bind_selection_basis(uint32_t driver_class, uint32_t driver_family) {
    switch (driver_class) {
        case LUNA_DRIVER_CLASS_STORAGE_BOOT:
            if (driver_family == LUNA_LANE_DRIVER_AHCI) {
                return LUNA_DRIVER_SELECT_AHCI_RUNTIME;
            }
            if (driver_family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
                if (fwblk_target_ready()) {
                    return LUNA_DRIVER_SELECT_FWBLK_TARGET;
                }
                return LUNA_DRIVER_SELECT_FWBLK_SOURCE;
            }
            if (driver_family == LUNA_LANE_DRIVER_PCI_IDE) {
                return LUNA_DRIVER_SELECT_PIIX_IDE;
            }
            return LUNA_DRIVER_SELECT_ATA_FALLBACK;
        case LUNA_DRIVER_CLASS_DISPLAY_MIN:
            if (driver_family == LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER) {
                return LUNA_DRIVER_SELECT_GOP_FRAMEBUFFER;
            }
            if (driver_family == LUNA_LANE_DRIVER_QEMU_STD_VGA) {
                if (g_display_framebuffer_base != 0u && g_display_framebuffer_size != 0u) {
                    return LUNA_DRIVER_SELECT_STD_VGA_FRAMEBUFFER;
                }
                return LUNA_DRIVER_SELECT_STD_VGA_TEXT;
            }
            return LUNA_DRIVER_SELECT_TEXT_FALLBACK;
        case LUNA_DRIVER_CLASS_INPUT_MIN:
            if (g_virtio_keyboard_ready != 0u) {
                return LUNA_DRIVER_SELECT_VIRTIO_KEYBOARD;
            }
            if (g_ps2_controller_present != 0u) {
                return LUNA_DRIVER_SELECT_I8042;
            }
            return LUNA_DRIVER_SELECT_LEGACY_KEYBOARD;
        case LUNA_DRIVER_CLASS_NETWORK_BASELINE:
            if (driver_family == LUNA_LANE_DRIVER_E1000) {
                return g_net_ready != 0u ? LUNA_DRIVER_SELECT_E1000_READY : LUNA_DRIVER_SELECT_E1000_PRESENT;
            }
            if (driver_family == LUNA_LANE_DRIVER_E1000E) {
                return g_net_ready != 0u ? LUNA_DRIVER_SELECT_E1000E_READY : LUNA_DRIVER_SELECT_E1000E_PRESENT;
            }
            return LUNA_DRIVER_SELECT_SOFT_LOOP;
        case LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY:
            if (driver_family == LUNA_LANE_DRIVER_ICH9_UART) {
                return LUNA_DRIVER_SELECT_ICH9_PCI;
            }
            if (driver_family == LUNA_LANE_DRIVER_PIIX_UART) {
                return LUNA_DRIVER_SELECT_PIIX_PCI;
            }
            return LUNA_DRIVER_SELECT_LEGACY_UART;
        default:
            return LUNA_DRIVER_SELECT_NONE;
    }
}

static uint32_t driver_bind_lifecycle_consequence(uint32_t stage, uint32_t failure_class) {
    if (failure_class == LUNA_DRIVER_FAIL_FATAL) {
        return LUNA_DRIVER_CONSEQUENCE_FATAL_STOP;
    }
    if (failure_class == LUNA_DRIVER_FAIL_DEGRADED) {
        return LUNA_DRIVER_CONSEQUENCE_DEGRADED_CONTINUE;
    }
    if (failure_class == LUNA_DRIVER_FAIL_OBSERVER_ONLY) {
        return LUNA_DRIVER_CONSEQUENCE_OBSERVER_ONLY;
    }
    if (stage == LUNA_DRIVER_STAGE_FAIL) {
        return LUNA_DRIVER_CONSEQUENCE_BIND_DENIED;
    }
    return LUNA_DRIVER_CONSEQUENCE_CONTINUE;
}

static uint32_t driver_bind_blocker_flags(uint32_t driver_class, uint32_t driver_family, uint32_t failure_class) {
    switch (driver_class) {
        case LUNA_DRIVER_CLASS_STORAGE_BOOT:
            if (failure_class == LUNA_DRIVER_FAIL_FATAL) {
                return LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING;
            }
            if (driver_family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
                return LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY;
            }
            if (driver_family == LUNA_LANE_DRIVER_PCI_IDE || driver_family == LUNA_LANE_DRIVER_ATA_PIO) {
                return LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK;
            }
            return LUNA_DRIVER_BLOCKER_NONE;
        case LUNA_DRIVER_CLASS_DISPLAY_MIN:
            if (driver_family == LUNA_LANE_DRIVER_VGA_TEXT
                || (driver_family == LUNA_LANE_DRIVER_QEMU_STD_VGA
                    && (g_display_framebuffer_base == 0u || g_display_framebuffer_size == 0u))) {
                return LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY;
            }
            if (driver_family == LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER
                || (driver_family == LUNA_LANE_DRIVER_QEMU_STD_VGA
                    && g_display_framebuffer_base != 0u
                    && g_display_framebuffer_size != 0u)) {
                return LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY;
            }
            return LUNA_DRIVER_BLOCKER_NONE;
        case LUNA_DRIVER_CLASS_INPUT_MIN:
            if (driver_family != LUNA_LANE_DRIVER_VIRTIO_KEYBOARD) {
                return LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY;
            }
            return LUNA_DRIVER_BLOCKER_NONE;
        case LUNA_DRIVER_CLASS_NETWORK_BASELINE:
            if (g_net_ready == 0u) {
                return LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY;
            }
            return LUNA_DRIVER_BLOCKER_NONE;
        default:
            return LUNA_DRIVER_BLOCKER_NONE;
    }
}

static uint32_t driver_bind_recovery_hint(uint32_t blocker_flags) {
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING) != 0u) {
        return LUNA_DRIVER_RECOVERY_ENTER_RECOVERY;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY) != 0u) {
        return LUNA_DRIVER_RECOVERY_RUNTIME_REBIND;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK) != 0u) {
        return LUNA_DRIVER_RECOVERY_LEGACY_FALLBACK;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY) != 0u) {
        return LUNA_DRIVER_RECOVERY_RUNTIME_REBIND;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY) != 0u) {
        return LUNA_DRIVER_RECOVERY_CONTINUE;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY) != 0u) {
        return LUNA_DRIVER_RECOVERY_LEGACY_FALLBACK;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY) != 0u) {
        return LUNA_DRIVER_RECOVERY_RUNTIME_REBIND;
    }
    return LUNA_DRIVER_RECOVERY_NONE;
}

static uint32_t driver_bind_recovery_action_flags(
    uint32_t driver_class,
    uint32_t blocker_flags,
    uint32_t volume_state,
    uint32_t system_mode
) {
    uint32_t actions = 0u;
    volatile const struct luna_bootview *bootview = device_bootview();
    uint32_t baseline_volume_state = bootview->volume_state;
    uint32_t baseline_system_mode = bootview->system_mode;

    switch (driver_class) {
        case LUNA_DRIVER_CLASS_STORAGE_BOOT:
            if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING) != 0u
                || volume_state == LUNA_VOLUME_RECOVERY_REQUIRED
                || system_mode == LUNA_MODE_RECOVERY
                || baseline_volume_state == LUNA_VOLUME_RECOVERY_REQUIRED
                || baseline_system_mode == LUNA_MODE_RECOVERY) {
                actions |= LUNA_DRIVER_ACTION_STORAGE_RECOVERY_BOOT;
                actions |= LUNA_DRIVER_ACTION_STORAGE_RESCAN;
                actions |= LUNA_DRIVER_ACTION_STORAGE_DISABLE_UNSAFE_WRITE;
                return actions;
            }
            if ((blocker_flags & (LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY | LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK)) != 0u) {
                actions |= LUNA_DRIVER_ACTION_STORAGE_FALLBACK_FAMILY;
                actions |= LUNA_DRIVER_ACTION_STORAGE_RESCAN;
                if (baseline_volume_state == LUNA_VOLUME_DEGRADED || baseline_system_mode == LUNA_MODE_READONLY) {
                    actions |= LUNA_DRIVER_ACTION_STORAGE_READONLY;
                    actions |= LUNA_DRIVER_ACTION_STORAGE_DISABLE_UNSAFE_WRITE;
                }
                return actions;
            }
            if (volume_state == LUNA_VOLUME_DEGRADED
                || system_mode == LUNA_MODE_READONLY
                || baseline_volume_state == LUNA_VOLUME_DEGRADED
                || baseline_system_mode == LUNA_MODE_READONLY) {
                actions |= LUNA_DRIVER_ACTION_STORAGE_READONLY;
                actions |= LUNA_DRIVER_ACTION_STORAGE_DISABLE_UNSAFE_WRITE;
            }
            return actions;
        case LUNA_DRIVER_CLASS_INPUT_MIN:
            if ((blocker_flags & LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY) != 0u) {
                actions |= LUNA_DRIVER_ACTION_INPUT_MINIMAL;
                actions |= LUNA_DRIVER_ACTION_INPUT_OPERATOR_RECOVERY;
            }
            return actions;
        case LUNA_DRIVER_CLASS_DISPLAY_MIN:
            if ((blocker_flags & (LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY | LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY)) != 0u) {
                actions |= LUNA_DRIVER_ACTION_DISPLAY_CONSOLE_FALLBACK;
            }
            if ((blocker_flags & LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY) != 0u) {
                actions |= LUNA_DRIVER_ACTION_DISPLAY_FRAMEBUFFER_DISABLED;
            }
            return actions;
        case LUNA_DRIVER_CLASS_NETWORK_BASELINE:
            if ((blocker_flags & LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY) != 0u) {
                actions |= LUNA_DRIVER_ACTION_NETWORK_OFFLINE;
                actions |= LUNA_DRIVER_ACTION_NETWORK_SOFT_LOOP;
            }
            return actions;
        default:
            return actions;
    }
}

static uint32_t driver_bind_evidence_flags(uint32_t driver_class) {
    uint32_t flags = 0u;
    volatile const struct luna_bootview *bootview = device_bootview();

    if (bootview->system_mode == LUNA_MODE_INSTALL) {
        flags |= LUNA_DRIVER_EVIDENCE_INSTALL_MODE;
    }
    if (bootview->system_mode == LUNA_MODE_RECOVERY) {
        flags |= LUNA_DRIVER_EVIDENCE_RECOVERY_MODE;
    }

    switch (driver_class) {
        case LUNA_DRIVER_CLASS_STORAGE_BOOT:
            if (fwblk_source_ready()) {
                flags |= LUNA_DRIVER_EVIDENCE_FWBLK_SOURCE;
            }
            if (fwblk_target_ready()) {
                flags |= LUNA_DRIVER_EVIDENCE_FWBLK_TARGET;
            }
            if (fwblk_target_separate()) {
                flags |= LUNA_DRIVER_EVIDENCE_FWBLK_TARGET_SEPARATE;
            }
            if (g_ahci_ready != 0u) {
                flags |= LUNA_DRIVER_EVIDENCE_AHCI_READY;
            }
            detect_disk_pci_record();
            if (pci_record_present(&g_disk_pci)) {
                flags |= LUNA_DRIVER_EVIDENCE_PCI_PRESENT;
            }
            return flags;
        case LUNA_DRIVER_CLASS_DISPLAY_MIN:
            if (g_display_framebuffer_base != 0u && g_display_framebuffer_size != 0u) {
                flags |= LUNA_DRIVER_EVIDENCE_FRAMEBUFFER;
            }
            detect_display_pci_record();
            if (pci_record_present(&g_display_pci)) {
                flags |= LUNA_DRIVER_EVIDENCE_PCI_PRESENT;
            }
            return flags;
        case LUNA_DRIVER_CLASS_INPUT_MIN:
            if (g_virtio_keyboard_ready != 0u) {
                flags |= LUNA_DRIVER_EVIDENCE_VIRTIO_READY;
            }
            if (g_ps2_controller_present != 0u) {
                flags |= LUNA_DRIVER_EVIDENCE_PS2_PRESENT;
            }
            if (detect_usb_input_pci_record()) {
                flags |= LUNA_DRIVER_EVIDENCE_USB_INPUT_CONTROLLER;
            }
            detect_input_pci_record();
            if (pci_record_present(&g_input_pci)) {
                flags |= LUNA_DRIVER_EVIDENCE_PCI_PRESENT;
            }
            return flags;
        case LUNA_DRIVER_CLASS_NETWORK_BASELINE:
            if (g_net_ready != 0u) {
                flags |= LUNA_DRIVER_EVIDENCE_NET_READY;
            }
            detect_net_pci_record();
            if (pci_record_present(&g_net_pci)) {
                flags |= LUNA_DRIVER_EVIDENCE_PCI_PRESENT;
            }
            return flags;
        case LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY:
            detect_serial_pci_record();
            if (pci_record_present(&g_serial_pci)) {
                flags |= LUNA_DRIVER_EVIDENCE_PCI_PRESENT;
            }
            return flags;
        default:
            return flags;
    }
}

static uint32_t driver_bind_policy_flags(uint32_t driver_class, uint32_t blocker_flags) {
    return (driver_class << LUNA_DRIVER_POLICY_CLASS_SHIFT)
        | (blocker_flags & LUNA_DRIVER_POLICY_BLOCKER_MASK);
}

static uint64_t driver_bind_lifecycle_flags(uint32_t blocker_flags) {
    uint64_t flags = 0u;

    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_RUNTIME_MISSING) != 0u) {
        flags |= LUNA_DRIVER_LIFE_RECOVERY_REQUIRED;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_FIRMWARE_ONLY) != 0u) {
        flags |= LUNA_DRIVER_LIFE_STORAGE_FIRMWARE_ONLY;
        flags |= LUNA_DRIVER_LIFE_RUNTIME_REBIND;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_STORAGE_LEGACY_FALLBACK) != 0u) {
        flags |= LUNA_DRIVER_LIFE_STORAGE_LEGACY_FALLBACK;
        flags |= LUNA_DRIVER_LIFE_LEGACY_RECOVERY;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_DISPLAY_TEXT_ONLY) != 0u) {
        flags |= LUNA_DRIVER_LIFE_DISPLAY_TEXT_ONLY;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_DISPLAY_FIRMWARE_ONLY) != 0u) {
        flags |= LUNA_DRIVER_LIFE_DISPLAY_FIRMWARE_ONLY;
        flags |= LUNA_DRIVER_LIFE_RUNTIME_REBIND;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_INPUT_LEGACY_ONLY) != 0u) {
        flags |= LUNA_DRIVER_LIFE_INPUT_LEGACY_ONLY;
        flags |= LUNA_DRIVER_LIFE_LEGACY_RECOVERY;
    }
    if ((blocker_flags & LUNA_DRIVER_BLOCKER_NETWORK_LOOP_ONLY) != 0u) {
        flags |= LUNA_DRIVER_LIFE_NETWORK_LOOP_ONLY;
        flags |= LUNA_DRIVER_LIFE_RUNTIME_REBIND;
    }
    return flags;
}

static uint64_t driver_bind_action_lifecycle_flags(uint32_t recovery_action_flags) {
    return ((uint64_t)recovery_action_flags) << LUNA_DRIVER_ACTION_LIFECYCLE_SHIFT;
}

static const struct luna_pci_record *driver_bind_pci_record(uint32_t driver_class) {
    switch (driver_class) {
        case LUNA_DRIVER_CLASS_STORAGE_BOOT:
            detect_disk_pci_record();
            return pci_record_present(&g_disk_pci) ? &g_disk_pci : 0;
        case LUNA_DRIVER_CLASS_DISPLAY_MIN:
            detect_display_pci_record();
            return pci_record_present(&g_display_pci) ? &g_display_pci : 0;
        case LUNA_DRIVER_CLASS_INPUT_MIN:
            detect_input_pci_record();
            return pci_record_present(&g_input_pci) ? &g_input_pci : 0;
        case LUNA_DRIVER_CLASS_NETWORK_BASELINE:
            detect_net_pci_record();
            return pci_record_present(&g_net_pci) ? &g_net_pci : 0;
        case LUNA_DRIVER_CLASS_PLATFORM_DISCOVERY:
            detect_serial_pci_record();
            return pci_record_present(&g_serial_pci) ? &g_serial_pci : 0;
        default:
            return 0;
    }
}

static void serial_write_pci_record(const char *prefix, const struct luna_pci_record *record) {
    serial_write(prefix);
    if (!pci_record_present(record)) {
        serial_write("none\r\n");
        return;
    }
    serial_write("vendor=");
    serial_write_hex16(record->vendor_id);
    serial_write(" device=");
    serial_write_hex16(record->device_id);
    serial_write(" bdf=");
    serial_write_hex8(record->bus);
    serial_putc(':');
    serial_write_hex8(record->slot);
    serial_putc('.');
    serial_write_hex8(record->function);
    serial_write(" class=");
    serial_write_hex8(record->class_code);
    serial_putc('/');
    serial_write_hex8(record->subclass);
    serial_putc('/');
    serial_write_hex8(record->prog_if);
    serial_write(" hdr=");
    serial_write_hex8(record->header_type);
    serial_write("\r\n");
}

static void serial_write_serial_context(void) {
    detect_serial_pci_record();
    serial_write("[DEVICE] serial path driver=");
    serial_write(serial_driver_name());
    serial_write(" family=");
    serial_write_hex32(serial_driver_family());
    serial_write("\r\n");
    serial_write("[DEVICE] serial select basis=");
    serial_write(serial_selection_basis_name());
    serial_write(" pci=");
    serial_write(flag_state_name(pci_record_present(&g_serial_pci)));
    serial_write("\r\n");
    serial_write_pci_record("[DEVICE] serial pci ", &g_serial_pci);
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
        case 0x000Cu: return '-';
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
        case 0x0034u: return '.';
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
    return (volatile struct luna_uefi_disk_handoff *)(uintptr_t)(
        manifest->bootview_base + LUNA_BOOTVIEW_UEFI_DISK_HANDOFF_OFFSET
    );
}

static volatile const struct luna_bootview *device_bootview(void) {
    volatile struct luna_manifest *manifest =
        (volatile struct luna_manifest *)(uintptr_t)LUNA_MANIFEST_ADDR;
    return (volatile const struct luna_bootview *)(uintptr_t)manifest->bootview_base;
}

static int installer_target_bound(void) {
    volatile const struct luna_bootview *bootview = device_bootview();
    return (bootview->installer_target_flags & LUNA_INSTALL_TARGET_BOUND) != 0u;
}

static int installer_media_mode(void) {
    volatile const struct luna_bootview *bootview = device_bootview();
    return bootview->system_mode == LUNA_MODE_INSTALL;
}

static int fwblk_source_ready(void) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    return handoff->magic == LUNA_UEFI_DISK_MAGIC
        && handoff->block_io_protocol != 0u
        && handoff->read_blocks_entry != 0u;
}

static int fwblk_target_ready(void) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    return handoff->magic == LUNA_UEFI_DISK_MAGIC
        && handoff->target_block_io_protocol != 0u
        && handoff->target_read_blocks_entry != 0u;
}

static int fwblk_target_separate(void) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    return handoff->magic == LUNA_UEFI_DISK_MAGIC
        && (handoff->flags & LUNA_UEFI_DISK_FLAG_TARGET_SEPARATE) != 0u;
}

static const char *mode_name(uint32_t mode) {
    if (mode == LUNA_MODE_NORMAL) {
        return "normal";
    }
    if (mode == LUNA_MODE_READONLY) {
        return "readonly";
    }
    if (mode == LUNA_MODE_RECOVERY) {
        return "recovery";
    }
    if (mode == LUNA_MODE_FATAL) {
        return "fatal";
    }
    if (mode == LUNA_MODE_INSTALL) {
        return "install";
    }
    return "unknown";
}

static const char *flag_state_name(int value) {
    return value != 0 ? "ready" : "missing";
}

static const char *ps2_state_name(void) {
    if (g_ps2_controller_known == 0u) {
        return "unknown";
    }
    return g_ps2_controller_present != 0u ? "present" : "missing";
}

static const char *disk_path_chain_name(uint32_t family) {
    if (family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        return "fwblk>ata";
    }
    return "ahci>fwblk>ata";
}

static const char *disk_selection_basis_name(void) {
    if (g_disk_driver_family == LUNA_LANE_DRIVER_AHCI) {
        return "ahci-runtime";
    }
    if (g_disk_driver_family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        if (fwblk_target_ready()) {
            return "fwblk-target";
        }
        if (fwblk_source_ready()) {
            return "fwblk-source";
        }
        return "fwblk-unknown";
    }
    if (g_disk_driver_family == LUNA_LANE_DRIVER_PCI_IDE) {
        return "piix-ide";
    }
    return "ata-fallback";
}

static const char *serial_selection_basis_name(void) {
    uint32_t family = serial_driver_family();
    if (family == LUNA_LANE_DRIVER_ICH9_UART) {
        return "ich9-pci";
    }
    if (family == LUNA_LANE_DRIVER_PIIX_UART) {
        return "piix-pci";
    }
    return "legacy-uart";
}

static const char *display_selection_basis_name(void) {
    uint32_t family = display_driver_family();
    if (family == LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER) {
        return "gop-fb";
    }
    if (family == LUNA_LANE_DRIVER_QEMU_STD_VGA) {
        if (g_display_framebuffer_base != 0u && g_display_framebuffer_size != 0u) {
            return "std-vga-fb";
        }
        return "std-vga-text";
    }
    return "text-fallback";
}

static const char *input_selection_basis_name(void) {
    if (g_virtio_keyboard_ready != 0u) {
        return "virtio-kbd";
    }
    if (g_ps2_controller_present != 0u) {
        return "i8042";
    }
    return "legacy-kbd";
}

static const char *usb_input_controller_name(void) {
    if (!detect_usb_input_pci_record()) {
        return "missing";
    }
    switch (g_usb_input_pci.prog_if) {
        case 0x30u:
            return "xhci";
        case 0x20u:
            return "ehci";
        case 0x10u:
            return "ohci";
        case 0x00u:
            return "uhci";
        default:
            return "usb";
    }
}

static const char *usb_hid_bind_state_name(void) {
    return g_usb_hid_keyboard_ready != 0u ? "bound" : "not-bound";
}

static const char *usb_hid_blocker_name(void) {
    if (g_usb_hid_keyboard_ready != 0u) {
        return "none";
    }
    if (!detect_usb_input_pci_record()) {
        return "controller-missing";
    }
    return g_usb_hid_blocker;
}

static const char *net_selection_basis_name(void) {
    uint32_t family = net_driver_family();
    if (family == LUNA_LANE_DRIVER_E1000) {
        return g_net_ready != 0u ? "e1000-ready" : "e1000-present";
    }
    if (family == LUNA_LANE_DRIVER_E1000E) {
        return g_net_ready != 0u ? "e1000e-ready" : "e1000e-present";
    }
    return "soft-loop";
}

static const char *store_state_name(uint64_t state) {
    if (state == LUNA_STORE_STATE_CLEAN) {
        return "clean";
    }
    if (state == LUNA_STORE_STATE_DIRTY) {
        return "dirty";
    }
    return "other";
}

static const char *native_profile_name(uint64_t profile) {
    if ((uint32_t)profile == LUNA_NATIVE_PROFILE_SYSTEM) {
        return "system";
    }
    if ((uint32_t)profile == LUNA_NATIVE_PROFILE_DATA) {
        return "data";
    }
    return "other";
}

static const char *activation_name(uint64_t activation) {
    if ((uint32_t)activation == LUNA_ACTIVATION_EMPTY) {
        return "empty";
    }
    if ((uint32_t)activation == LUNA_ACTIVATION_PROVISIONED) {
        return "provisioned";
    }
    if ((uint32_t)activation == LUNA_ACTIVATION_COMMITTED) {
        return "committed";
    }
    if ((uint32_t)activation == LUNA_ACTIVATION_ACTIVE) {
        return "active";
    }
    if ((uint32_t)activation == LUNA_ACTIVATION_RECOVERY) {
        return "recovery";
    }
    return "other";
}

static const char *disk_region_name(uint32_t lba) {
    volatile const struct luna_bootview *bootview = device_bootview();
    uint32_t system_base = (uint32_t)bootview->system_store_lba;
    uint32_t data_base = (uint32_t)bootview->data_store_lba;
    uint32_t metadata_span = LUNA_NATIVE_LAYOUT_RECORD_SECTORS - LUNA_NATIVE_LAYOUT_TXN_SECTORS;
    uint32_t system_txn_log = system_base + 1u + metadata_span;
    uint32_t system_txn_aux = system_txn_log + 1u;
    uint32_t system_data_start = system_base + 1u + LUNA_NATIVE_LAYOUT_RECORD_SECTORS;
    uint32_t data_txn_log = data_base + 1u + metadata_span;
    uint32_t data_txn_aux = data_txn_log + 1u;
    uint32_t data_start = data_base + 1u + LUNA_NATIVE_LAYOUT_RECORD_SECTORS;

    if (lba == system_base) {
        return "lsys-super";
    }
    if (lba >= system_base + 1u && lba < system_txn_log) {
        return "lsys-object-records";
    }
    if (lba == system_txn_log) {
        return "lsys-txn-log";
    }
    if (lba == system_txn_aux) {
        return "lsys-txn-aux";
    }
    if (lba >= system_data_start && lba < data_base) {
        return "lsys-object-slots";
    }
    if (lba == data_base) {
        return "ldat-super";
    }
    if (lba >= data_base + 1u && lba < data_txn_log) {
        return "ldat-object-records";
    }
    if (lba == data_txn_log) {
        return "ldat-txn-log";
    }
    if (lba == data_txn_aux) {
        return "ldat-txn-aux";
    }
    if (lba >= data_start) {
        return "ldat-object-slots";
    }
    return "unmapped";
}

static void serial_write_store_super_context(uint32_t lba, const uint8_t *src) {
    volatile const struct luna_bootview *bootview = device_bootview();
    const struct luna_store_superblock *super = (const struct luna_store_superblock *)src;

    if (src == 0) {
        return;
    }
    if (lba != (uint32_t)bootview->system_store_lba && lba != (uint32_t)bootview->data_store_lba) {
        return;
    }

    serial_write("[DISK] super checkpoint=");
    serial_write(store_state_name(super->reserved[0]));
    serial_write(" profile=");
    serial_write(native_profile_name(super->reserved[6]));
    serial_write(" activation=");
    serial_write(activation_name(super->reserved[9]));
    serial_write(" mode=");
    serial_write(mode_name((uint32_t)super->reserved[5]));
    serial_write("\r\n");

    serial_write("[DISK] super nonce=");
    serial_write_hex64(super->next_nonce);
    serial_write(" mounts=");
    serial_write_hex64(super->mount_count);
    serial_write(" formats=");
    serial_write_hex64(super->format_count);
    serial_write(" peer=");
    serial_write_hex64(super->reserved[10]);
    serial_write("\r\n");
}

static void ata_log_fail(const char *phase, const char *op, uint32_t lba) {
    uint8_t status = inb(0x1F7);
    uint8_t error = inb(0x1F1);
    uint8_t sector_count = inb(0x1F2);
    uint8_t lba0 = inb(0x1F3);
    uint8_t lba1 = inb(0x1F4);
    uint8_t lba2 = inb(0x1F5);
    uint8_t dev = inb(0x1F6);
    serial_write("[ATA] ");
    serial_write(phase);
    serial_write(" fail op=");
    serial_write(op);
    serial_write(" lba=");
    serial_write_hex32(lba);
    serial_write(" st=");
    serial_write_hex8(status);
    serial_write(" err=");
    serial_write_hex8(error);
    serial_write(" sc=");
    serial_write_hex8(sector_count);
    serial_write(" tf=");
    serial_write_hex8(dev);
    serial_putc(':');
    serial_write_hex8(lba2);
    serial_putc(':');
    serial_write_hex8(lba1);
    serial_putc(':');
    serial_write_hex8(lba0);
    serial_write(" region=");
    serial_write(disk_region_name(lba));
    serial_write("\r\n");
}

struct ata_wait_diag {
    uint32_t loops;
    uint8_t last_status;
    uint8_t saw_bsy_clear;
    uint8_t saw_drq_clear;
    uint8_t saw_error;
    uint8_t saw_df;
};

static void ata_wait_diag_reset(struct ata_wait_diag *diag) {
    if (diag == 0) {
        return;
    }
    zero_bytes(diag, sizeof(*diag));
}

static void ata_wait_diag_record(struct ata_wait_diag *diag, uint8_t status, uint32_t loops) {
    if (diag == 0) {
        return;
    }
    diag->loops = loops;
    diag->last_status = status;
    if ((status & 0x80u) == 0u) {
        diag->saw_bsy_clear = 1u;
    }
    if ((status & 0x08u) == 0u) {
        diag->saw_drq_clear = 1u;
    }
    if ((status & 0x01u) != 0u) {
        diag->saw_error = 1u;
    }
    if ((status & 0x20u) != 0u) {
        diag->saw_df = 1u;
    }
}

static void ata_log_fail_detail(const char *phase, const char *op, uint32_t lba, const struct ata_wait_diag *diag) {
    ata_log_fail(phase, op, lba);
    if (diag == 0) {
        return;
    }
    serial_write("[ATA] poll loops=");
    serial_write_hex32(diag->loops);
    serial_write(" last=");
    serial_write_hex8(diag->last_status);
    serial_write(" bsy-clear=");
    serial_write(flag_state_name(diag->saw_bsy_clear != 0u));
    serial_write(" drq-clear=");
    serial_write(flag_state_name(diag->saw_drq_clear != 0u));
    serial_write(" err-seen=");
    serial_write(flag_state_name(diag->saw_error != 0u));
    serial_write(" df-seen=");
    serial_write(flag_state_name(diag->saw_df != 0u));
    serial_write("\r\n");
}

static void serial_write_fwblk_context(void) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    volatile const struct luna_bootview *bootview = device_bootview();
    int source_ready = fwblk_source_ready();
    int target_ready = fwblk_target_ready();

    serial_write("[DEVICE] fwblk source=");
    serial_write(flag_state_name(source_ready));
    serial_write(" target=");
    serial_write(flag_state_name(target_ready));
    serial_write(" flags=");
    serial_write_hex64(handoff->flags);
    serial_write(" diag=");
    serial_write_hex64(handoff->diag_status);
    serial_write(" mode=");
    serial_write(mode_name(bootview->system_mode));
    serial_write("\r\n");
    serial_write("[DEVICE] fwblk media src=");
    serial_write_hex64(handoff->media_id);
    serial_write(" tgt=");
    serial_write_hex64(handoff->target_media_id);
    serial_write(" install=");
    serial_write(installer_media_mode() ? "media" : "runtime");
    serial_write(" bound=");
    serial_write(installer_target_bound() ? "yes" : "no");
    serial_write("\r\n");
}

static void serial_write_disk_layout_context(void) {
    volatile const struct luna_bootview *bootview = device_bootview();
    serial_write("[DEVICE] disk layout lsys=");
    serial_write_hex32((uint32_t)bootview->system_store_lba);
    serial_write(" ldat=");
    serial_write_hex32((uint32_t)bootview->data_store_lba);
    serial_write(" target=");
    serial_write_hex64(bootview->installer_target_flags);
    serial_write("\r\n");
}

static void serial_write_disk_path_context(void) {
    volatile const struct luna_bootview *bootview = device_bootview();
    uint32_t family = disk_driver_family();
    detect_disk_pci_record();
    serial_write("[DEVICE] disk path driver=");
    serial_write(disk_driver_name());
    serial_write(" family=");
    serial_write_hex32(family);
    serial_write(" chain=");
    serial_write(disk_path_chain_name(family));
    serial_write(" mode=");
    serial_write(mode_name(bootview->system_mode));
    serial_write("\r\n");
    serial_write("[DEVICE] disk select basis=");
    serial_write(disk_selection_basis_name());
    serial_write(" fwblk-src=");
    serial_write(flag_state_name(fwblk_source_ready()));
    serial_write(" fwblk-tgt=");
    serial_write(flag_state_name(fwblk_target_ready()));
    serial_write(" separate=");
    serial_write(flag_state_name(fwblk_target_separate()));
    serial_write(" ahci=");
    serial_write(flag_state_name(g_ahci_ready != 0u));
    serial_write(" pci=");
    serial_write(flag_state_name(pci_record_present(&g_disk_pci)));
    serial_write("\r\n");
    serial_write_pci_record("[DEVICE] disk pci ", &g_disk_pci);
    if (family == LUNA_LANE_DRIVER_AHCI) {
        serial_write("[DEVICE] disk ahci port=");
        serial_write_hex8(g_ahci_port);
        serial_write(" abar=");
        serial_write_hex64((uint64_t)g_ahci_abar);
        serial_write("\r\n");
    }
}

static void serial_write_display_context(void) {
    detect_display_pci_record();
    serial_write("[DEVICE] display path driver=");
    serial_write(display_driver_name());
    serial_write(" family=");
    serial_write_hex32(display_driver_family());
    serial_write(" fb=");
    serial_write(flag_state_name(g_display_framebuffer_base != 0u && g_display_framebuffer_size != 0u));
    serial_write(" size=");
    serial_write_hex64(g_display_framebuffer_size);
    serial_write(" w=");
    serial_write_hex32(g_display_framebuffer_width);
    serial_write(" h=");
    serial_write_hex32(g_display_framebuffer_height);
    serial_write(" stride=");
    serial_write_hex32(g_display_framebuffer_stride);
    serial_write(" fmt=");
    serial_write_hex32(g_display_framebuffer_format);
    serial_write("\r\n");
    serial_write("[DEVICE] display select basis=");
    serial_write(display_selection_basis_name());
    serial_write(" gop=");
    serial_write(flag_state_name(g_display_framebuffer_base != 0u && g_display_framebuffer_size != 0u));
    serial_write(" pci=");
    serial_write(flag_state_name(pci_record_present(&g_display_pci)));
    serial_write("\r\n");
    serial_write_pci_record("[DEVICE] display pci ", &g_display_pci);
}

static void serial_write_input_context(int input_ready) {
    int usb_input_candidate = detect_usb_input_pci_record();
    detect_input_pci_record();
    serial_write("[DEVICE] input path kbd=");
    serial_write(input_driver_name(keyboard_driver_family()));
    serial_write(" ptr=");
    serial_write(input_driver_name(pointer_driver_family()));
    serial_write(" virtio=");
    serial_write(flag_state_name(g_virtio_keyboard_ready != 0u));
    serial_write(" ps2=");
    serial_write(ps2_state_name());
    serial_write(" lane=");
    serial_write(input_ready != 0 ? "ready" : "offline");
    serial_write("\r\n");
    serial_write("[DEVICE] input select basis=");
    serial_write(input_selection_basis_name());
    serial_write(" virtio-dev=");
    serial_write(flag_state_name(pci_find_class_device(0x09u, 0x00u, 0x00u, 0x1AF4u, 0x1052u)));
    serial_write(" virtio-ready=");
    serial_write(flag_state_name(g_virtio_keyboard_ready != 0u));
    serial_write(" legacy=");
    serial_write(flag_state_name(g_ps2_controller_present != 0u));
    serial_write(" usb-ctrl=");
    serial_write(flag_state_name(usb_input_candidate));
    serial_write(" usb-hid=");
    serial_write(usb_hid_bind_state_name());
    serial_write(" usb-hid-blocker=");
    serial_write(usb_hid_blocker_name());
    serial_write("\r\n");
    if (pci_record_present(&g_input_pci)) {
        serial_write_pci_record("[DEVICE] input pci ", &g_input_pci);
    } else if (g_ps2_controller_present != 0u) {
        serial_write("[DEVICE] input ctrl legacy=i8042\r\n");
    } else {
        serial_write("[DEVICE] input ctrl legacy=missing\r\n");
    }
    if (usb_input_candidate) {
        serial_write("[DEVICE] input usb candidate ctrl=");
        serial_write(usb_input_controller_name());
        serial_write(" hid=");
        serial_write(usb_hid_bind_state_name());
        serial_write(" blocker=");
        serial_write(usb_hid_blocker_name());
        serial_write(" owner=DEVICE consequence=degraded-continue\r\n");
        serial_write_pci_record("[DEVICE] input usb pci ", &g_usb_input_pci);
        serial_write("[DEVICE] input usb host ctrl=");
        serial_write(usb_input_controller_name());
        serial_write(" init=");
        serial_write(flag_state_name(g_xhci_ready != 0u));
        serial_write(" port=");
        serial_write_hex8(g_xhci_connected_port);
        serial_write(" port-ready=");
        serial_write(flag_state_name(g_xhci_port_ready != 0u));
        serial_write(" blocker=");
        serial_write(usb_hid_blocker_name());
        serial_write("\r\n");
    }
}

static void serial_write_network_context(void) {
    detect_net_pci_record();
    serial_write("[DEVICE] net path driver=");
    serial_write(net_driver_name());
    serial_write(" family=");
    serial_write_hex32(net_driver_family());
    serial_write(" lane=");
    serial_write(g_net_ready != 0u ? "ready" : "fallback");
    serial_write(" mmio=");
    serial_write_hex64((uint64_t)g_net_mmio);
    serial_write("\r\n");
    serial_write("[DEVICE] net select basis=");
    serial_write(net_selection_basis_name());
    serial_write(" pci=");
    serial_write(flag_state_name(pci_record_present(&g_net_pci)));
    serial_write(" live=");
    serial_write(flag_state_name(g_net_ready != 0u));
    serial_write("\r\n");
    serial_write_pci_record("[DEVICE] net pci ", &g_net_pci);
}

static void serial_write_platform_context(void) {
    detect_platform_pci_record();
    serial_write_pci_record("[DEVICE] platform pci ", &g_platform_pci);
}

static int firmware_read_sector(uint32_t lba, uint8_t *out) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    static uint8_t g_firmware_read_diag_once = 0u;
    uint64_t status = 0u;
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC || handoff->read_blocks_entry == 0u || handoff->block_io_protocol == 0u) {
        if (g_firmware_read_diag_once == 0u) {
            g_firmware_read_diag_once = 1u;
            serial_write("[FWBLK] handoff missing\r\n");
            if (handoff->magic == LUNA_UEFI_DISK_MAGIC && handoff->version >= 2u) {
                serial_write("[FWBLK] diag status=");
                serial_write_hex64(handoff->diag_status);
                serial_write(" handles=");
                serial_write_hex64(handoff->diag_handle_count);
                serial_write(" full=");
                serial_write_hex64(handoff->diag_full_disk_seen);
                serial_write("\r\n");
            }
        }
        return 0;
    }
    status = ((efi_block_rw_fn_t)(uintptr_t)handoff->read_blocks_entry)(
        (void *)(uintptr_t)handoff->block_io_protocol,
        (uint32_t)handoff->media_id,
        lba,
        512u,
        g_firmware_disk_sector
    );
    if (status != 0u) {
        if (g_firmware_read_diag_once == 0u) {
            g_firmware_read_diag_once = 1u;
            serial_write("[FWBLK] read fail lba=");
            serial_write_hex32(lba);
            serial_write(" status=");
            serial_write_hex64(status);
            serial_write("\r\n");
        }
        return 0;
    }
    copy_bytes(out, g_firmware_disk_sector, 512u);
    return 1;
}

static int firmware_write_sector(uint32_t lba, const uint8_t *src) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    uint64_t block_io_protocol;
    uint64_t media_id;
    uint64_t write_blocks_entry;
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC) {
        return 0;
    }
    if (handoff->target_block_io_protocol != 0u && handoff->target_write_blocks_entry != 0u) {
        block_io_protocol = handoff->target_block_io_protocol;
        media_id = handoff->target_media_id;
        write_blocks_entry = handoff->target_write_blocks_entry;
    } else {
        block_io_protocol = handoff->block_io_protocol;
        media_id = handoff->media_id;
        write_blocks_entry = handoff->write_blocks_entry;
    }
    if (write_blocks_entry == 0u || block_io_protocol == 0u) {
        return 0;
    }
    copy_bytes(g_firmware_disk_sector, src, 512u);
    if (((efi_block_rw_fn_t)(uintptr_t)write_blocks_entry)(
        (void *)(uintptr_t)block_io_protocol,
        (uint32_t)media_id,
        lba,
        512u,
        g_firmware_disk_sector
    ) == 0u) {
        return 1;
    }
    return 0;
}

static int firmware_read_target_sector(uint32_t lba, uint8_t *out) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC
        || handoff->target_read_blocks_entry == 0u
        || handoff->target_block_io_protocol == 0u) {
        return 0;
    }
    if (((efi_block_rw_fn_t)(uintptr_t)handoff->target_read_blocks_entry)(
        (void *)(uintptr_t)handoff->target_block_io_protocol,
        (uint32_t)handoff->target_media_id,
        lba,
        512u,
        g_firmware_disk_sector
    ) != 0u) {
        return 0;
    }
    copy_bytes(out, g_firmware_disk_sector, 512u);
    return 1;
}

static int firmware_write_target_sector(uint32_t lba, const uint8_t *src) {
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    static uint8_t g_firmware_target_write_diag_once = 0u;
    uint64_t status = 0u;
    if (handoff->magic != LUNA_UEFI_DISK_MAGIC
        || handoff->target_write_blocks_entry == 0u
        || handoff->target_block_io_protocol == 0u) {
        if (g_firmware_target_write_diag_once == 0u) {
            g_firmware_target_write_diag_once = 1u;
            serial_write("[FWBLK] target write handoff miss lba=");
            serial_write_hex32(lba);
            serial_write(" magic=");
            serial_write_hex32(handoff->magic);
            serial_write(" entry=");
            serial_write_hex64(handoff->target_write_blocks_entry);
            serial_write(" proto=");
            serial_write_hex64(handoff->target_block_io_protocol);
            serial_write("\r\n");
        }
        return 0;
    }
    copy_bytes(g_firmware_disk_sector, src, 512u);
    status = ((efi_block_rw_fn_t)(uintptr_t)handoff->target_write_blocks_entry)(
        (void *)(uintptr_t)handoff->target_block_io_protocol,
        (uint32_t)handoff->target_media_id,
        lba,
        512u,
        g_firmware_disk_sector
    );
    if (status == 0u) {
        return 1;
    }
    if (g_firmware_target_write_diag_once == 0u) {
        g_firmware_target_write_diag_once = 1u;
        serial_write("[FWBLK] target write fail lba=");
        serial_write_hex32(lba);
        serial_write(" status=");
        serial_write_hex64(status);
        serial_write(" media=");
        serial_write_hex64(handoff->target_media_id);
        serial_write(" flags=");
        serial_write_hex64(handoff->flags);
        serial_write("\r\n");
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

static int ata_wait_not_busy_detail(struct ata_wait_diag *diag) {
    ata_wait_diag_reset(diag);
    for (uint32_t loops = 0; loops < 1000000u; ++loops) {
        uint8_t status = inb(0x1F7);
        ata_wait_diag_record(diag, status, loops);
        if ((status & (0x01u | 0x20u)) != 0u) {
            return -1;
        }
        if ((status & 0x80u) == 0u) {
            return 1;
        }
        io_delay();
    }
    return 0;
}

static int ata_wait_flush_status_clear(struct ata_wait_diag *diag) {
    ata_wait_diag_reset(diag);
    for (uint32_t loops = 0; loops < 1000000u; ++loops) {
        uint8_t status = inb(0x1F7);
        ata_wait_diag_record(diag, status, loops);
        if ((status & (0x01u | 0x20u)) != 0u) {
            return -1;
        }
        if ((status & (0x80u | 0x08u)) == 0u) {
            return 1;
        }
        io_delay();
    }
    return 0;
}

static int ata_wait_write_data_complete(void) {
    for (uint32_t loops = 0; loops < 100000u; ++loops) {
        uint8_t status = inb(0x1F7);
        if ((status & 0x01u) != 0u || (status & 0x20u) != 0u) {
            return 0;
        }
        if ((status & 0x80u) == 0u && (status & 0x08u) == 0u) {
            return 1;
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
        ata_log_fail("ready", "read", lba);
        return 0;
    }
    for (size_t i = 0; i < 256u; ++i) {
        uint16_t word = inw(0x1F0);
        out[i * 2u] = (uint8_t)(word & 0xFFu);
        out[i * 2u + 1u] = (uint8_t)(word >> 8);
    }
    if (!ata_wait_idle()) {
        ata_log_fail("idle", "read", lba);
        return 0;
    }
    return 1;
}

static int ata_write_sector(uint32_t lba, const uint8_t *src) {
    struct ata_wait_diag flush_diag;
    ata_select(lba, 1u, 0x30u);
    if (!ata_wait_ready()) {
        ata_log_fail("ready", "write", lba);
        return 0;
    }
    for (size_t i = 0; i < 256u; ++i) {
        uint16_t lo = src[i * 2u];
        uint16_t hi = src[i * 2u + 1u];
        outw(0x1F0, (uint16_t)(lo | (hi << 8)));
    }
    if (!ata_wait_write_data_complete()) {
        ata_log_fail("data-complete", "write", lba);
        return 0;
    }
    outb(0x1F7, 0xE7u);
    io_delay();
    switch (ata_wait_not_busy_detail(&flush_diag)) {
        case 1:
            break;
        case -1:
            ata_log_fail_detail("cache-flush", "write", lba, &flush_diag);
            return 0;
        default:
            ata_log_fail_detail("completion-poll", "write", lba, &flush_diag);
            return 0;
    }
    switch (ata_wait_flush_status_clear(&flush_diag)) {
        case 1:
            break;
        case -1:
            ata_log_fail_detail("status-clear", "write", lba, &flush_diag);
            return 0;
        default:
            ata_log_fail_detail("status-clear", "write", lba, &flush_diag);
            return 0;
    }
    if (!ata_wait_idle()) {
        ata_log_fail("post-write-idle", "write", lba);
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

static uint32_t xhci_read32(uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)(g_xhci_mmio + offset);
    return *reg;
}

static void xhci_write32(uint32_t offset, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(g_xhci_mmio + offset);
    *reg = value;
}

static uint32_t xhci_op_read32(uint32_t offset) {
    return xhci_read32(g_xhci_op_offset + offset);
}

static void xhci_op_write32(uint32_t offset, uint32_t value) {
    xhci_write32(g_xhci_op_offset + offset, value);
}

static void xhci_op_write64(uint32_t offset, uint64_t value) {
    xhci_op_write32(offset + 4u, (uint32_t)(value >> 32));
    xhci_op_write32(offset, (uint32_t)(value & 0xFFFFFFFFu));
}

static void xhci_runtime_write32(uint32_t offset, uint32_t value) {
    xhci_write32(g_xhci_runtime_offset + offset, value);
}

static void xhci_runtime_write64(uint32_t offset, uint64_t value) {
    xhci_runtime_write32(offset + 4u, (uint32_t)(value >> 32));
    xhci_runtime_write32(offset, (uint32_t)(value & 0xFFFFFFFFu));
}

static uint32_t xhci_port_offset(uint8_t port) {
    return g_xhci_op_offset + 0x400u + ((uint32_t)port - 1u) * 0x10u;
}

static int xhci_wait_expired(uint64_t start_us, uint32_t loops) {
    uint64_t now_us;
    if ((loops & 0x3FFu) != 0u) {
        return 0;
    }
    now_us = clock_microseconds();
    return now_us > start_us && (now_us - start_us) > 100000u;
}

static int xhci_wait_op_clear(uint32_t offset, uint32_t mask) {
    uint64_t start_us = clock_microseconds();
    for (uint32_t loops = 0u; loops < 100000000u; ++loops) {
        if ((xhci_op_read32(offset) & mask) == 0u) {
            return 1;
        }
        if (xhci_wait_expired(start_us, loops)) {
            break;
        }
        __asm__ volatile ("pause");
    }
    return (xhci_op_read32(offset) & mask) == 0u;
}

static int xhci_wait_status(uint32_t mask, int want_set) {
    uint64_t start_us = clock_microseconds();
    uint32_t status;
    for (uint32_t loops = 0u; loops < 100000000u; ++loops) {
        status = xhci_op_read32(0x04u);
        if (want_set) {
            if ((status & mask) == mask) {
                return 1;
            }
        } else if ((status & mask) == 0u) {
            return 1;
        }
        if (xhci_wait_expired(start_us, loops)) {
            break;
        }
        __asm__ volatile ("pause");
    }
    status = xhci_op_read32(0x04u);
    if (want_set) {
        return (status & mask) == mask;
    }
    return (status & mask) == 0u;
}

static void xhci_legacy_handoff(uint32_t hccparams1) {
    uint32_t ext_offset = ((hccparams1 >> 16) & 0xFFFFu) << 2;

    for (uint32_t index = 0u; index < 32u && ext_offset != 0u; ++index) {
        uint32_t cap = xhci_read32(ext_offset);
        uint8_t cap_id = (uint8_t)(cap & 0xFFu);
        uint8_t next = (uint8_t)((cap >> 8) & 0xFFu);
        if (cap_id == 0x01u) {
            if ((cap & (1u << 16)) != 0u) {
                xhci_write32(ext_offset, cap | (1u << 24));
                for (uint32_t loops = 0u; loops < 1000000u; ++loops) {
                    cap = xhci_read32(ext_offset);
                    if ((cap & (1u << 16)) == 0u) {
                        break;
                    }
                }
            }
            serial_write("[XHCI] legacy handoff cap=");
            serial_write_hex32(cap);
            serial_write(" off=");
            serial_write_hex32(ext_offset);
            serial_write("\r\n");
            return;
        }
        if (next == 0u) {
            return;
        }
        ext_offset += ((uint32_t)next << 2);
    }
}

static int xhci_reset_connected_port(void) {
    const uint32_t port_rwc_mask =
        (1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) |
        (1u << 21) | (1u << 22) | (1u << 23);

    g_xhci_connected_port = 0u;
    g_xhci_port_ready = 0u;
    g_xhci_port_speed = 0u;

    for (uint8_t port = 1u; port <= g_xhci_max_ports; ++port) {
        uint32_t offset = xhci_port_offset(port);
        uint32_t portsc = xhci_read32(offset);
        if ((portsc & 0x01u) == 0u) {
            continue;
        }

        g_xhci_connected_port = port;
        g_xhci_port_speed = (uint8_t)((portsc >> 10) & 0x0Fu);
        if ((portsc & 0x02u) == 0u) {
            xhci_write32(offset, (portsc & ~port_rwc_mask) | (1u << 4));
            for (uint32_t loops = 0u; loops < 10000000u; ++loops) {
                portsc = xhci_read32(offset);
                if ((portsc & (1u << 4)) == 0u) {
                    break;
                }
            }
        }

        portsc = xhci_read32(offset);
        g_xhci_port_speed = (uint8_t)((portsc >> 10) & 0x0Fu);
        if ((portsc & 0x02u) != 0u) {
            g_xhci_port_ready = 1u;
            g_usb_hid_blocker = "usb-enumeration-missing";
            serial_write("[XHCI] port ready port=");
            serial_write_hex8(port);
            serial_write(" speed=");
            serial_write_hex8(g_xhci_port_speed);
            serial_write(" portsc=");
            serial_write_hex32(portsc);
            serial_write("\r\n");
            return 1;
        }

        g_usb_hid_blocker = "port-reset";
        serial_write("[XHCI] port reset blocked port=");
        serial_write_hex8(port);
        serial_write(" portsc=");
        serial_write_hex32(portsc);
        serial_write("\r\n");
        return 0;
    }

    g_usb_hid_blocker = "usb-device-missing";
    return 0;
}

static int xhci_init(void) {
    uint8_t bus = 0u;
    uint8_t slot = 0u;
    uint8_t function = 0u;
    uint32_t command;
    uint32_t hcs1;
    uint32_t hcs2;
    uint32_t hcc1;
    uint32_t scratchpads;
    uint32_t usbcmd;
    uintptr_t bar;

    if (g_xhci_ready != 0u) {
        return 1;
    }
    if (!detect_usb_input_pci_record()) {
        g_usb_hid_blocker = "controller-missing";
        return 0;
    }
    if (g_usb_input_pci.prog_if != 0x30u) {
        g_usb_hid_blocker = "controller-family-not-xhci";
        return 0;
    }
    bus = g_usb_input_pci.bus;
    slot = g_usb_input_pci.slot;
    function = g_usb_input_pci.function;

    command = pci_config_read32(bus, slot, function, 0x04u);
    command |= 0x00000006u;
    pci_config_write16(bus, slot, function, 0x04u, (uint16_t)(command & 0xFFFFu));

    bar = pci_read_bar_address(bus, slot, function, 0u);
    if (bar == 0u) {
        g_usb_hid_blocker = "pci-bar-mmio-missing";
        return 0;
    }
    g_xhci_mmio = bar;
    g_xhci_op_offset = (uint8_t)(xhci_read32(0x00u) & 0xFFu);
    g_xhci_doorbell_offset = xhci_read32(0x14u) & ~0x03u;
    g_xhci_runtime_offset = xhci_read32(0x18u) & ~0x1Fu;
    hcs1 = xhci_read32(0x04u);
    hcs2 = xhci_read32(0x08u);
    hcc1 = xhci_read32(0x10u);
    g_xhci_max_slots = (uint8_t)(hcs1 & 0xFFu);
    g_xhci_max_ports = (uint8_t)((hcs1 >> 24) & 0xFFu);
    scratchpads = ((hcs2 >> 27) & 0x1Fu) << 5;
    scratchpads |= (hcs2 >> 21) & 0x1Fu;
    if (scratchpads != 0u) {
        g_usb_hid_blocker = "scratchpad-unsupported";
        return 0;
    }
    if (g_xhci_max_slots == 0u || g_xhci_max_ports == 0u) {
        g_usb_hid_blocker = "controller-capability-invalid";
        return 0;
    }
    xhci_legacy_handoff(hcc1);
    serial_write("[XHCI] probe mmio=");
    serial_write_hex64((uint64_t)g_xhci_mmio);
    serial_write(" op=");
    serial_write_hex32(g_xhci_op_offset);
    serial_write(" hcs1=");
    serial_write_hex32(hcs1);
    serial_write(" hcs2=");
    serial_write_hex32(hcs2);
    serial_write(" hcc1=");
    serial_write_hex32(hcc1);
    serial_write(" cmd=");
    serial_write_hex32(xhci_op_read32(0x00u));
    serial_write(" sts=");
    serial_write_hex32(xhci_op_read32(0x04u));
    serial_write("\r\n");

    usbcmd = xhci_op_read32(0x00u);
    xhci_op_write32(0x00u, usbcmd & ~0x01u);
    if (!xhci_wait_status(0x01u, 1)) {
        g_usb_hid_blocker = "controller-halt";
        serial_write("[XHCI] controller halt blocked cmd=");
        serial_write_hex32(xhci_op_read32(0x00u));
        serial_write(" sts=");
        serial_write_hex32(xhci_op_read32(0x04u));
        serial_write("\r\n");
        return 0;
    }

    xhci_op_write32(0x00u, xhci_op_read32(0x00u) | (1u << 1));
    if (!xhci_wait_op_clear(0x00u, (1u << 1))) {
        g_usb_hid_blocker = "controller-reset";
        serial_write("[XHCI] controller reset blocked cmd=");
        serial_write_hex32(xhci_op_read32(0x00u));
        serial_write(" sts=");
        serial_write_hex32(xhci_op_read32(0x04u));
        serial_write("\r\n");
        return 0;
    }
    if (!xhci_wait_status((1u << 11), 0)) {
        g_usb_hid_blocker = "controller-not-ready";
        serial_write("[XHCI] controller not ready cmd=");
        serial_write_hex32(xhci_op_read32(0x00u));
        serial_write(" sts=");
        serial_write_hex32(xhci_op_read32(0x04u));
        serial_write("\r\n");
        return 0;
    }

    zero_bytes(g_xhci_command_ring, sizeof(g_xhci_command_ring));
    zero_bytes(g_xhci_event_ring, sizeof(g_xhci_event_ring));
    zero_bytes(g_xhci_erst, sizeof(g_xhci_erst));
    zero_bytes(g_xhci_dcbaa, sizeof(g_xhci_dcbaa));

    g_xhci_command_ring[15].parameter = (uint64_t)(uintptr_t)g_xhci_command_ring;
    g_xhci_command_ring[15].status = 0u;
    g_xhci_command_ring[15].control = (6u << 10) | (1u << 1);

    g_xhci_erst[0].ring_segment_base = (uint64_t)(uintptr_t)g_xhci_event_ring;
    g_xhci_erst[0].ring_segment_size = 16u;
    g_xhci_erst[0].reserved = 0u;

    xhci_op_write64(0x30u, (uint64_t)(uintptr_t)g_xhci_dcbaa);
    xhci_op_write64(0x18u, ((uint64_t)(uintptr_t)g_xhci_command_ring) | 1u);
    xhci_runtime_write32(0x28u, 1u);
    xhci_runtime_write64(0x30u, (uint64_t)(uintptr_t)g_xhci_erst);
    xhci_runtime_write64(0x38u, (uint64_t)(uintptr_t)g_xhci_event_ring);
    xhci_op_write32(0x38u, g_xhci_max_slots < 8u ? g_xhci_max_slots : 8u);

    memory_barrier();
    xhci_op_write32(0x00u, xhci_op_read32(0x00u) | 0x01u);
    if (!xhci_wait_status(0x01u, 0)) {
        g_usb_hid_blocker = "controller-run";
        serial_write("[XHCI] controller run blocked cmd=");
        serial_write_hex32(xhci_op_read32(0x00u));
        serial_write(" sts=");
        serial_write_hex32(xhci_op_read32(0x04u));
        serial_write("\r\n");
        return 0;
    }

    g_xhci_ready = 1u;
    g_usb_hid_blocker = "usb-device-missing";
    serial_write("[XHCI] controller ready slots=");
    serial_write_hex8(g_xhci_max_slots);
    serial_write(" ports=");
    serial_write_hex8(g_xhci_max_ports);
    serial_write(" mmio=");
    serial_write_hex64((uint64_t)g_xhci_mmio);
    serial_write(" db=");
    serial_write_hex32(g_xhci_doorbell_offset);
    serial_write(" rt=");
    serial_write_hex32(g_xhci_runtime_offset);
    serial_write("\r\n");

    (void)xhci_reset_connected_port();
    return 1;
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
    volatile struct luna_uefi_disk_handoff *handoff = uefi_disk_handoff();
    uint32_t family = g_disk_driver_family;
    if (handoff->magic == LUNA_UEFI_DISK_MAGIC
        && (handoff->flags & LUNA_UEFI_DISK_FLAG_TARGET_PRESENT) != 0u
        && (handoff->flags & LUNA_UEFI_DISK_FLAG_TARGET_SEPARATE) != 0u
        && (((handoff->flags & (
                LUNA_UEFI_DISK_FLAG_SOURCE_REMOVABLE |
                LUNA_UEFI_DISK_FLAG_SOURCE_READ_ONLY |
                LUNA_UEFI_DISK_FLAG_SOURCE_LOGICAL_PARTITION
            )) != 0u)
            || handoff->block_io_protocol == 0u
            || handoff->read_blocks_entry == 0u
            || handoff->diag_status != 0u)) {
        return 0;
    }
    /* Keep the runtime disk path consistent with the boot-selected driver.
       VMware currently boots through firmware block I/O; lazy AHCI promotion
       during the first persistent write causes later reads to diverge. */
    if (family != LUNA_LANE_DRIVER_UEFI_BLOCK_IO && g_ahci_ready == 0u) {
        ahci_init();
    }
    if (family == LUNA_LANE_DRIVER_UEFI_BLOCK_IO) {
        if (firmware_write_sector(lba, src)) {
            return 1;
        }
        if (ata_write_sector(lba, src)) {
            return 1;
        }
    } else {
        if (ahci_write_sector(lba, src)) {
            return 1;
        }
        if (firmware_write_sector(lba, src)) {
            return 1;
        }
        if (ata_write_sector(lba, src)) {
            return 1;
        }
    }
    serial_write("[DISK] write fail lba=");
    serial_write_hex32(lba);
    serial_write(" driver=");
    serial_write(disk_driver_name());
    serial_write(" family=");
    serial_write_hex32(disk_driver_family());
    serial_write(" chain=");
    serial_write(disk_path_chain_name(family));
    serial_write(" region=");
    serial_write(disk_region_name(lba));
    serial_write(" mode=");
    serial_write(mode_name(device_bootview()->system_mode));
    serial_write("\r\n");
    serial_write_store_super_context(lba, src);
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
    uint32_t blocker_flags = 0u;
    uint32_t policy_flags = 0u;
    uint32_t recovery_action_flags = 0u;
    uint32_t volume_state = 0u;
    uint32_t mode = 0u;

    driver_audit("audit driver.probe start");
    blocker_flags = driver_bind_blocker_flags(driver_class, driver_family, failure_class);
    policy_flags = driver_bind_policy_flags(driver_class, blocker_flags);
    if (governance_approve(
            LUNA_CAP_DEVICE_BIND,
            g_driver_bind_cid,
            LUNA_DATA_OBJECT_TYPE_DRIVER_BIND,
            policy_flags,
            &volume_state,
            &mode
        ) == LUNA_GATE_OK) {
        recovery_action_flags = driver_bind_recovery_action_flags(driver_class, blocker_flags, volume_state, mode);
        g_driver_lifecycle_flags |= driver_bind_action_lifecycle_flags(recovery_action_flags);
        driver_audit("audit driver.bind approved=SECURITY");
        if (blocker_flags != 0u || recovery_action_flags != 0u) {
            serial_write_driver_bind_governance(
                "approved",
                lane_name,
                driver_name,
                driver_class,
                driver_family,
                blocker_flags,
                volume_state,
                mode,
                recovery_action_flags
            );
        }
        stage_driver_binding(
            driver_class,
            LUNA_DRIVER_STAGE_BIND,
            failure_class,
            volume_state,
            mode,
            device_id,
            lane_class,
            driver_family,
            state_flags,
            lane_name,
            driver_name
        );
        return;
    }
    recovery_action_flags = driver_bind_recovery_action_flags(
        driver_class,
        blocker_flags,
        volume_state != 0u ? volume_state : device_bootview()->volume_state,
        mode != 0u ? mode : device_bootview()->system_mode
    );
    g_driver_lifecycle_flags |= driver_bind_action_lifecycle_flags(recovery_action_flags);
    driver_fail_audit("audit driver.bind denied=SECURITY");
    serial_write_driver_bind_governance(
        "denied",
        lane_name,
        driver_name,
        driver_class,
        driver_family,
        blocker_flags,
        volume_state != 0u ? volume_state : device_bootview()->volume_state,
        mode != 0u ? mode : device_bootview()->system_mode,
        recovery_action_flags
    );
    stage_driver_binding(
        driver_class,
        LUNA_DRIVER_STAGE_FAIL,
        failure_class,
        volume_state,
        mode,
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
        && ((handoff->block_io_protocol != 0u && handoff->read_blocks_entry != 0u) ||
            (handoff->target_block_io_protocol != 0u && handoff->target_read_blocks_entry != 0u))) {
        g_disk_driver_family = LUNA_LANE_DRIVER_UEFI_BLOCK_IO;
    } else if (pci_find_class_device(0x01u, 0x01u, 0x80u, 0x8086u, 0x7010u)) {
        g_disk_driver_family = LUNA_LANE_DRIVER_PCI_IDE;
    } else {
        g_disk_driver_family = LUNA_LANE_DRIVER_ATA_PIO;
    }
    serial_write_fwblk_context();
    serial_write_disk_layout_context();
    serial_write_disk_path_context();
    serial_write_serial_context();
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
    (void)xhci_init();
    if (!virtio_keyboard_init()) {
        input_ready = keyboard_init();
    } else {
        input_ready = 1;
    }
    if (mouse_init()) {
        input_ready = 1;
    }
    serial_write_display_context();
    serial_write_input_context(input_ready);
    serial_write_network_context();
    serial_write_platform_context();
    (void)request_capability(LUNA_CAP_OBSERVE_LOG, &g_observe_log_cid);
    g_observe_logging_enabled = 0u;
    g_input_event_debug_budget = 64u;
    g_last_keyboard_event_source = INPUT_EVENT_SOURCE_NONE;
    g_driver_flags_published = 0u;
    g_lifecycle_wake_cid.low = 0u;
    g_lifecycle_wake_cid.high = 0u;
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
    g_driver_lifecycle_flags |= driver_bind_lifecycle_flags(
        driver_bind_blocker_flags(LUNA_DRIVER_CLASS_STORAGE_BOOT, disk_driver_family(), storage_failure)
    );
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
    g_driver_lifecycle_flags |= driver_bind_lifecycle_flags(
        driver_bind_blocker_flags(LUNA_DRIVER_CLASS_DISPLAY_MIN, display_driver_family(), display_failure)
    );
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
    g_driver_lifecycle_flags |= driver_bind_lifecycle_flags(
        driver_bind_blocker_flags(LUNA_DRIVER_CLASS_INPUT_MIN, keyboard_driver_family(), input_failure)
    );
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
    g_driver_lifecycle_flags |= driver_bind_lifecycle_flags(
        driver_bind_blocker_flags(LUNA_DRIVER_CLASS_NETWORK_BASELINE, net_driver_family(), network_failure)
    );
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

    serial_write_driver_flags();
    lifecycle_publish_driver_flags();
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_LIST, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_CENSUS, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_PCI_SCAN, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_READ, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (gate->device_id == LUNA_DEVICE_ID_DISK1
            && installer_target_bound()
            && gate->buffer_size >= 512u
            && (ahci_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || firmware_read_target_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr))) {
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
            if ((inb(0x3FD) & 0x01u) != 0u && gate->buffer_size != 0u) {
                ((uint8_t *)(uintptr_t)gate->buffer_addr)[0] = inb(0x3F8);
                gate->size = 1;
                serial_write_input_event(INPUT_EVENT_SOURCE_OPERATOR, ((uint8_t *)(uintptr_t)gate->buffer_addr)[0]);
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_QUERY, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_WRITE, gate->device_id, gate->caller_space, gate->actor_space)) {
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_INPUT_READ, gate->device_id, gate->caller_space, gate->actor_space)) {
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
                serial_write_input_event(g_last_keyboard_event_source, value);
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
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_BLOCK_READ, gate->device_id, gate->caller_space, gate->actor_space)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK0
            && installer_media_mode()
            && gate->buffer_size >= 512u
            && firmware_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)) {
            gate->size = 512u;
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
        if (gate->device_id == LUNA_DEVICE_ID_DISK1
            && installer_target_bound()
            && gate->buffer_size >= 512u
            && (ahci_read_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr)
                || firmware_read_target_sector(gate->flags, (uint8_t *)(uintptr_t)gate->buffer_addr))) {
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
        if (gate->device_id == LUNA_DEVICE_ID_DISK1) {
            if (!installer_target_bound()
                || !allow_device_call(
                    gate->cid_low,
                    gate->cid_high,
                    LUNA_DEVICE_BLOCK_WRITE_INSTALL_TARGET,
                    gate->device_id,
                    gate->caller_space,
                    gate->actor_space
                )) {
                return;
            }
        } else if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_BLOCK_WRITE, gate->device_id, gate->caller_space, gate->actor_space)) {
            return;
        }
        if (gate->device_id == LUNA_DEVICE_ID_DISK1
            && gate->buffer_size >= 512u
            && gate->size >= 512u
            && (ahci_write_sector(gate->flags, src)
                || firmware_write_target_sector(gate->flags, src))) {
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
        if (gate->device_id == LUNA_DEVICE_ID_DISK1) {
            serial_write("[INSTALLER] target write path fail lba=");
            serial_write_hex32(gate->flags);
            serial_write(" first=");
            serial_write_hex32(*(const uint32_t *)src);
            serial_write("\r\n");
        }
        observe_log(3u, "device.block.write err");
        gate->status = LUNA_DEVICE_ERR_NOT_FOUND;
        return;
    }
    if (gate->opcode == LUNA_DEVICE_DISPLAY_PRESENT) {
        uint64_t bytes;
        if (!allow_device_call(gate->cid_low, gate->cid_high, LUNA_DEVICE_DISPLAY_PRESENT, gate->device_id, gate->caller_space, gate->actor_space)) {
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
