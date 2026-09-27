#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Small persistent boot breadcrumb trail. These are intentionally milestones,
// not a general-purpose log stream.
typedef enum {
    DIAG_APP_MAIN = 1,
    DIAG_BATTERY_TASK_START,
    DIAG_BATTERY_READY,
    DIAG_BATTERY_MV,
    DIAG_WIFI_INIT,
    DIAG_WIFI_START,
    DIAG_WIFI_CONNECTED,
    DIAG_WIFI_DISCONNECTED,
    DIAG_GOT_IP,
    DIAG_MDNS_INIT,
    DIAG_HTTP_START,
    DIAG_UART_INSTALL,
    DIAG_UART_CONFIG,
    DIAG_UART_PINS,
    DIAG_FIRST_POLL,
    DIAG_FIRST_RX,
    DIAG_FIRST_VALID_FRAME,
    DIAG_METER_OFFLINE,
} boot_diag_event_t;

void boot_diag_init(void);
void boot_diag_event(boot_diag_event_t event, int32_t value);
size_t boot_diag_render(char *out, size_t out_size);
