#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "battery.h"

// Seeed Studio XIAO ESP32-C6: D6 / TX is ESP32-C6 GPIO16.
#define D6_TX_GPIO GPIO_NUM_16
#define D7_RX_GPIO GPIO_NUM_17
#define METER_UART UART_NUM_1
static const char *TAG = "UT61E";

// UT61E+ GetMeasurement: AB CD 03 5E, followed by the BE frame checksum 01 D9.
static const uint8_t get_measurement[] = {0xAB, 0xCD, 0x03, 0x5E, 0x01, 0xD9};


static const char *mode_name(uint8_t m) {
    static const char *n[] = {"ACV","ACmV","DCV","DCmV","FREQ","DUTY","RES","CONT",
        "DIODE","CAP","TEMP_C","TEMP_F","DCuA","ACuA","DCmA","ACmA","DCA","ACA",
        "hFE","LIVE","NCV","LOZV","CLAMP_ACA","CLAMP_DCA","LPF_V","ACDC_V"};
    return m < sizeof(n) / sizeof(n[0]) ? n[m] : "UNKNOWN";
}


// Range tables from antoinecellerier/dmm-tools, protocol/ut61eplus/tables/ut61e_plus.rs.
// The display number is already expressed in the range's unit; scale converts to the base unit.
typedef struct { const char *label; const char *unit; double scale; const char *base; } range_t;
static const range_t DCV_RANGES[] = {
    {"2.2V", "V", 1, "V"}, {"22V", "V", 1, "V"}, {"220V", "V", 1, "V"}, {"1000V", "V", 1, "V"}};
static const range_t RES_RANGES[] = {
    {"220\xCE\xA9", "\xCE\xA9", 1, "\xCE\xA9"}, {"2.2k\xCE\xA9", "k\xCE\xA9", 1e3, "\xCE\xA9"},
    {"22k\xCE\xA9", "k\xCE\xA9", 1e3, "\xCE\xA9"}, {"220k\xCE\xA9", "k\xCE\xA9", 1e3, "\xCE\xA9"},
    {"2.2M\xCE\xA9", "M\xCE\xA9", 1e6, "\xCE\xA9"}, {"22M\xCE\xA9", "M\xCE\xA9", 1e6, "\xCE\xA9"},
    {"220M\xCE\xA9", "M\xCE\xA9", 1e6, "\xCE\xA9"}};

static const range_t *lookup_range(uint8_t mode, int idx) {
    // ACDC_V (0x19) shares the DC V table in the reference implementation.
    if (mode == 0x02 || mode == 0x19) return idx < 4 ? &DCV_RANGES[idx] : NULL;
    if (mode == 0x06) return idx < 7 ? &RES_RANGES[idx] : NULL;
    return NULL;
}

static void parse_frame(const uint8_t *f, int64_t t_us) {
    char raw[19 * 3 + 1];
    for (int i = 0; i < 19; ++i) sprintf(raw + i * 3, "%02X ", f[i]);
    uint16_t sum = 0;
    for (int i = 0; i < 17; ++i) sum += f[i];
    uint16_t rx = (f[17] << 8) | f[18];
    bool ok = sum == rx;

    char disp[8];
    memcpy(disp, f + 5, 7);
    disp[7] = 0;
    char num[8];
    int n = 0;
    for (int i = 0; i < 7; ++i) if (disp[i] != ' ') num[n++] = disp[i];
    num[n] = 0;
    bool ol = strchr(num, 'O') && strchr(num, 'L');
    char *end;
    double v = strtod(num, &end);
    bool numeric = !ol && n > 0 && *end == 0;

    uint8_t f1 = f[14] & 0x0F, f2 = f[15] & 0x0F, f3 = f[16] & 0x0F;
    ESP_LOGI(TAG, "%lld us RAW %s", t_us, raw);
    ESP_LOGI(TAG, "  chk=%s(calc %04X rx %04X) mode_raw=0x%02X %s range_raw=0x%02X idx=%d disp=\"%s\"",
             ok ? "OK" : "BAD", sum, rx, f[3], mode_name(f[3]), f[4], f[4] & 0x0F, disp);
    const range_t *rg = lookup_range(f[3], f[4] & 0x0F);
    if (!rg) ESP_LOGI(TAG, "  eng=n/a (no documented range table for mode 0x%02X idx %d)", f[3], f[4] & 0x0F);
    else if (ol) ESP_LOGI(TAG, "  eng=OL range=%s", rg->label);
    else if (numeric) ESP_LOGI(TAG, "  eng=%s %s range=%s base=%.6g %s", num, rg->unit, rg->label, v * rg->scale, rg->base);
    else ESP_LOGI(TAG, "  eng=n/a (unparsed display)");
    ESP_LOGI(TAG, "  bar=%d REL=%d HOLD=%d MIN=%d MAX=%d HV=%d LOWBAT=%d MANUAL=%d APO=%d AC=%d AUTO=%d",
             f[12] * 10 + f[13], f1 & 1, (f1 >> 1) & 1, (f1 >> 2) & 1, (f1 >> 3) & 1,
             f2 & 1, (f2 >> 1) & 1, (f2 >> 2) & 1, (f2 >> 3) & 1, (f3 >> 3) & 1,
             !((f2 >> 2) & 1));
}

void app_main(void) {
    battery_init();
    const uart_config_t config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(METER_UART, 256, 0, 0, NULL, 0);
    uart_param_config(METER_UART, &config);
    uart_set_pin(METER_UART, D6_TX_GPIO, D7_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uint8_t buf[19];
    int have = 0;
    uint32_t polls = 0;
    int64_t next_poll_us = esp_timer_get_time();

    while (true) {
        uint8_t b;
        if (uart_read_bytes(METER_UART, &b, 1, pdMS_TO_TICKS(5)) == 1) {
            if (have == 0 && b != 0xAB) continue;
            if (have == 1 && b != 0xCD) { have = (b == 0xAB); if (have) buf[0] = b; continue; }
            buf[have++] = b;
            if (have == 3 && b != 0x10) { ESP_LOGW(TAG, "unexpected length 0x%02X", b); have = 0; }
            if (have == 19) { parse_frame(buf, esp_timer_get_time()); have = 0; }
        }
        int64_t now_us = esp_timer_get_time();
        if (now_us >= next_poll_us) {
            have = 0;
            uart_write_bytes(METER_UART, get_measurement, sizeof(get_measurement));
            ESP_LOGI(TAG, "TX poll %lu", (unsigned long)++polls);
            next_poll_us += 1000000;
        }
    }
}
