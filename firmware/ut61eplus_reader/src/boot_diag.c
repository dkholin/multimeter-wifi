#include "boot_diag.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define DIAG_MAGIC 0x55444941u // "UDIA"
#define DIAG_VERSION 1u
#define DIAG_BOOTS 12
#define DIAG_EVENTS 16

typedef struct {
    uint16_t event;
    uint16_t reserved;
    uint32_t elapsed_ms;
    int32_t value;
} diag_event_t;

typedef struct {
    uint32_t sequence;
    uint32_t reset_reason;
    uint16_t event_count;
    uint16_t reserved;
    diag_event_t events[DIAG_EVENTS];
} diag_boot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t head;
    uint32_t next_sequence;
    diag_boot_t boots[DIAG_BOOTS];
} diag_store_t;

static const char *TAG = "BOOTDIAG";
static diag_store_t store;
static int64_t boot_start_us;
static SemaphoreHandle_t diag_lock;

static const char *event_name(uint16_t event) {
    switch (event) {
    case DIAG_APP_MAIN: return "app_main";
    case DIAG_BATTERY_TASK_START: return "battery_task_start";
    case DIAG_BATTERY_READY: return "battery_ready";
    case DIAG_BATTERY_MV: return "battery_mv";
    case DIAG_WIFI_INIT: return "wifi_init";
    case DIAG_WIFI_START: return "wifi_start";
    case DIAG_WIFI_CONNECTED: return "wifi_connected";
    case DIAG_WIFI_DISCONNECTED: return "wifi_disconnected";
    case DIAG_GOT_IP: return "got_ip";
    case DIAG_MDNS_INIT: return "mdns_init";
    case DIAG_HTTP_START: return "http_start";
    case DIAG_UART_INSTALL: return "uart_install";
    case DIAG_UART_CONFIG: return "uart_config";
    case DIAG_UART_PINS: return "uart_pins";
    case DIAG_FIRST_POLL: return "first_poll";
    case DIAG_FIRST_RX: return "first_rx";
    case DIAG_FIRST_VALID_FRAME: return "first_valid_frame";
    case DIAG_METER_OFFLINE: return "meter_offline";
    default: return "unknown";
    }
}

static void save_locked(void) {
    nvs_handle_t nvs = 0;
    if (nvs_open("bootdiag", NVS_READWRITE, &nvs) != ESP_OK) return;
    esp_err_t r = nvs_set_blob(nvs, "history", &store, sizeof(store));
    if (r == ESP_OK) r = nvs_commit(nvs);
    if (r != ESP_OK) ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(r));
    nvs_close(nvs);
}

static void print_boot(const diag_boot_t *boot) {
    ESP_LOGI(TAG, "boot #%lu reset=%lu events=%u", (unsigned long)boot->sequence,
             (unsigned long)boot->reset_reason, boot->event_count);
    for (int i = 0; i < boot->event_count && i < DIAG_EVENTS; ++i) {
        const diag_event_t *e = &boot->events[i];
        ESP_LOGI(TAG, "  +%lums %s value=%ld", (unsigned long)e->elapsed_ms,
                 event_name(e->event), (long)e->value);
    }
}

void boot_diag_init(void) {
    boot_start_us = esp_timer_get_time();
    diag_lock = xSemaphoreCreateMutex();
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        r = nvs_flash_init();
    }
    if (r != ESP_OK) { ESP_LOGW(TAG, "NVS unavailable: %s", esp_err_to_name(r)); return; }

    nvs_handle_t nvs = 0;
    size_t len = sizeof(store);
    bool loaded = nvs_open("bootdiag", NVS_READWRITE, &nvs) == ESP_OK &&
                  nvs_get_blob(nvs, "history", &store, &len) == ESP_OK &&
                  len == sizeof(store) && store.magic == DIAG_MAGIC && store.version == DIAG_VERSION;
    if (nvs) nvs_close(nvs);
    if (!loaded) {
        memset(&store, 0, sizeof(store));
        store.magic = DIAG_MAGIC;
        store.version = DIAG_VERSION;
        store.head = DIAG_BOOTS - 1;
    } else if (store.next_sequence) {
        ESP_LOGI(TAG, "previous boot record:");
        print_boot(&store.boots[store.head]);
    }

    store.head = (store.head + 1) % DIAG_BOOTS;
    diag_boot_t *current = &store.boots[store.head];
    memset(current, 0, sizeof(*current));
    current->sequence = ++store.next_sequence;
    current->reset_reason = esp_reset_reason();
    save_locked(); // Preserve reset reason even if the next stage never runs.
    ESP_LOGI(TAG, "recording boot #%lu reset=%lu", (unsigned long)current->sequence,
             (unsigned long)current->reset_reason);
}

void boot_diag_event(boot_diag_event_t event, int32_t value) {
    if (!diag_lock || !store.next_sequence) return;
    xSemaphoreTake(diag_lock, portMAX_DELAY);
    diag_boot_t *current = &store.boots[store.head];
    // A milestone is recorded once per boot. This bounds flash writes and keeps
    // reconnect storms/poll traffic out of the persistent history.
    for (int i = 0; i < current->event_count; ++i)
        if (current->events[i].event == event) { xSemaphoreGive(diag_lock); return; }
    if (current->event_count < DIAG_EVENTS) {
        diag_event_t *e = &current->events[current->event_count++];
        e->event = event;
        e->elapsed_ms = (uint32_t)((esp_timer_get_time() - boot_start_us) / 1000);
        e->value = value;
        save_locked();
    }
    xSemaphoreGive(diag_lock);
}

size_t boot_diag_render(char *out, size_t out_size) {
    if (!out_size) return 0;
    int n = 0;
    xSemaphoreTake(diag_lock, portMAX_DELAY);
    // /diag is intentionally a short field-troubleshooting view; the complete
    // circular history remains in NVS for later expansion if ever needed.
    for (int offset = 0; offset < 4 && offset < DIAG_BOOTS && store.next_sequence; ++offset) {
        int idx = (store.head + DIAG_BOOTS - offset) % DIAG_BOOTS;
        const diag_boot_t *boot = &store.boots[idx];
        if (!boot->sequence) continue;
        n += snprintf(out + n, n < (int)out_size ? out_size - n : 0,
                      "boot=%lu reset=%lu\n", (unsigned long)boot->sequence, (unsigned long)boot->reset_reason);
        for (int i = 0; i < boot->event_count && i < DIAG_EVENTS; ++i) {
            const diag_event_t *e = &boot->events[i];
            n += snprintf(out + n, n < (int)out_size ? out_size - n : 0,
                          "  +%lums %-18s value=%ld\n", (unsigned long)e->elapsed_ms,
                          event_name(e->event), (long)e->value);
        }
    }
    xSemaphoreGive(diag_lock);
    if (n >= (int)out_size) out[out_size - 1] = 0;
    return n < 0 ? 0 : (size_t)n;
}
