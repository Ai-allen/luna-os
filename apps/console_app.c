#include <stdint.h>

#include "../include/luna_proto.h"

#define SYSV_ABI __attribute__((sysv_abi))

typedef void (SYSV_ABI *app_write_fn_t)(const struct luna_bootview *bootview, const char *text);

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

void SYSV_ABI console_app_entry(const struct luna_bootview *bootview) {
    app_write(bootview, "apps 1-5 or F/N/G/C/H\r\n");
}
