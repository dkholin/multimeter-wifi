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
#include "net.h"

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
typedef struct { const char *label; const char *unit; } range_t;
#define R(l, u) {l, u}
#define OHM "\xCE\xA9"
#define UA "\xC2\xB5"
static const range_t V4[] = {R("2.2V","V"), R("22V","V"), R("220V","V"), R("1000V","V")};
static const range_t MV1[] = {R("220mV","mV")};
static const range_t RES_R[] = {R("220" OHM,OHM), R("2.2k" OHM,"k" OHM), R("22k" OHM,"k" OHM), R("220k" OHM,"k" OHM),
    R("2.2M" OHM,"M" OHM), R("22M" OHM,"M" OHM), R("220M" OHM,"M" OHM)};
static const range_t CAP_R[] = {R("22nF","nF"), R("220nF","nF"), R("2.2" UA "F", UA "F"), R("22" UA "F", UA "F"),
    R("220" UA "F", UA "F"), R("2.2mF","mF"), R("22mF","mF"), R("220mF","mF")};
static const range_t FREQ_R[] = {R("22Hz","Hz"), R("220Hz","Hz"), R("2.2kHz","kHz"), R("22kHz","kHz"), R("220kHz","kHz"),
    R("2.2MHz","MHz"), R("22MHz","MHz"), R("220MHz","MHz")};
static const range_t DUTY_R[] = {R("Duty","%")};
static const range_t DIODE_R[] = {R("Diode","V")};
static const range_t CONT_R[] = {R("Cont",OHM)};
static const range_t UA2[] = {R("220" UA "A", UA "A"), R("2200" UA "A", UA "A")};
static const range_t MA2[] = {R("22mA","mA"), R("220mA","mA")};
static const range_t A2[] = {R("20A","A"), R("20A","A")};
#define N(a) (int)(sizeof(a) / sizeof((a)[0]))

static const range_t *lookup_range(uint8_t mode, int idx) {
    const range_t *t = NULL; int n = 0;
    switch (mode) { // ACDC_V (0x19) and LPF_V (0x18) share the DC V table in the reference
    case 0x00: case 0x02: case 0x18: case 0x19: t = V4; n = N(V4); break;
    case 0x01: case 0x03: t = MV1; n = N(MV1); break;
    case 0x04: t = FREQ_R; n = N(FREQ_R); break;
    case 0x05: t = DUTY_R; n = N(DUTY_R); break;
    case 0x06: t = RES_R; n = N(RES_R); break;
    case 0x07: t = CONT_R; n = N(CONT_R); break;
    case 0x08: t = DIODE_R; n = N(DIODE_R); break;
    case 0x09: t = CAP_R; n = N(CAP_R); break;
    case 0x0C: case 0x0D: t = UA2; n = N(UA2); break;
    case 0x0E: case 0x0F: t = MA2; n = N(MA2); break;
    case 0x10: case 0x11: t = A2; n = N(A2); break;
    default: return NULL;
    }
    return idx < n ? &t[idx] : NULL;
}

// Normalized state: same contract as the Crenova dashboard (display/value/prefix/unit), plus
// identity, battery and optional diagnostics. Absent key = unsupported, false = supported/inactive.
static const char *function_name(uint8_t m) {
    switch (m) {
    case 0x00: return "AC VOLTAGE"; case 0x01: return "AC mV"; case 0x02: return "DC VOLTAGE";
    case 0x03: return "DC mV"; case 0x04: return "FREQUENCY"; case 0x05: return "DUTY_R CYCLE";
    case 0x06: return "RESISTANCE"; case 0x07: return "CONTINUITY"; case 0x08: return "DIODE";
    case 0x09: return "CAPACITANCE"; case 0x0A: case 0x0B: return "TEMPERATURE";
    case 0x0C: return "DC \xC2\xB5" "A"; case 0x0D: return "AC \xC2\xB5" "A"; case 0x0E: return "DC mA";
    case 0x0F: return "AC mA"; case 0x10: return "DC CURRENT"; case 0x11: return "AC CURRENT";
    case 0x19: return "AC+DC VOLTAGE";
    default: return NULL;
    }
}
static void publish_state(const uint8_t *f, const char *disp, const char *num, bool ol, bool numeric,
                          double v, const range_t *rg) {
    uint8_t f1 = f[14] & 0x0F, f2 = f[15] & 0x0F, f3 = f[16] & 0x0F;
    char prefix[4] = "", unit[8] = "";
    if (rg) { // range unit like "k\xCE\xA9": leading k/M is the SI prefix, remainder the base unit
        const char *u = rg->unit;
        if (!strncmp(u, UA, 2)) { strcpy(prefix, UA); u += 2; }
        else if ((*u == 'k' || *u == 'M' || *u == 'm' || *u == 'n') && u[1]) { prefix[0] = *u; u++; }
        strlcpy(unit, u, sizeof(unit));
    }
    uint8_t m = f[3];
    bool acm = m == 0x00 || m == 0x01 || m == 0x0D || m == 0x0F || m == 0x11 || m == 0x19;
    bool dcm = m == 0x02 || m == 0x03 || m == 0x0C || m == 0x0E || m == 0x10 || m == 0x19;
    bool volt = acm || dcm;
    char j[640]; int n = 0;
    n += snprintf(j + n, sizeof(j) - n, "{\"manufacturer\":\"UNI-T\",\"model\":\"UT61E+\",\"display\":");
    if (ol) n += snprintf(j + n, sizeof(j) - n, "\"OL\"");
    else if (num[0]) n += snprintf(j + n, sizeof(j) - n, "\"%s\"", num);
    else n += snprintf(j + n, sizeof(j) - n, "null");
    if (numeric) n += snprintf(j + n, sizeof(j) - n, ",\"value\":%.6g,\"negative\":%s", v, num[0] == '-' ? "true" : "false");
    else n += snprintf(j + n, sizeof(j) - n, ",\"value\":null,\"negative\":false");
    n += snprintf(j + n, sizeof(j) - n, ",\"unit\":%s%s%s,\"prefix\":%s%s%s", unit[0] ? "\"" : "", unit[0] ? unit : "null", unit[0] ? "\"" : "",
                  prefix[0] ? "\"" : "", prefix[0] ? prefix : "null", prefix[0] ? "\"" : "");
    const char *fn = function_name(f[3]);
    n += snprintf(j + n, sizeof(j) - n, ",\"mode\":\"%s\",\"function\":%s%s%s,\"mode_raw\":%d,\"range_idx\":%d,\"ol\":%s",
                  mode_name(f[3]), fn ? "\"" : "", fn ? fn : "null", fn ? "\"" : "", f[3], f[4] & 0x0F, ol ? "true" : "false");
    if (volt) n += snprintf(j + n, sizeof(j) - n, ",\"ac\":%s,\"dc\":%s", acm ? "true" : "false", dcm ? "true" : "false");
    n += snprintf(j + n, sizeof(j) - n, ",\"auto\":%s,\"hold\":%s,\"max\":%s,\"min\":%s,\"rel\":%s,\"hv\":%s,\"low_bat\":%s",
                  !((f2 >> 2) & 1) ? "true" : "false", (f1 >> 1) & 1 ? "true" : "false", (f1 >> 3) & 1 ? "true" : "false",
                  (f1 >> 2) & 1 ? "true" : "false", f1 & 1 ? "true" : "false", f2 & 1 ? "true" : "false", (f2 >> 1) & 1 ? "true" : "false");
    if (battery_mv() > 0) n += snprintf(j + n, sizeof(j) - n, ",\"battery_mv\":%d,\"battery_level\":%d", battery_mv(), battery_level());
    (void)f3; (void)disp;
    snprintf(j + n, sizeof(j) - n, "}");
    net_publish(j);
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
    else if (numeric) ESP_LOGI(TAG, "  eng=%s %s range=%s", num, rg->unit, rg->label);
    else ESP_LOGI(TAG, "  eng=n/a (unparsed display)");
    ESP_LOGI(TAG, "  bar=%d REL=%d HOLD=%d MIN=%d MAX=%d HV=%d LOWBAT=%d MANUAL=%d APO=%d AC=%d AUTO=%d",
             f[12] * 10 + f[13], f1 & 1, (f1 >> 1) & 1, (f1 >> 2) & 1, (f1 >> 3) & 1,
             f2 & 1, (f2 >> 1) & 1, (f2 >> 2) & 1, (f2 >> 3) & 1, (f3 >> 3) & 1,
             !((f2 >> 2) & 1));
    if (ok) publish_state(f, disp, num, ol, numeric, v, rg);
}

void app_main(void) {
    battery_init();
    net_start();
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
