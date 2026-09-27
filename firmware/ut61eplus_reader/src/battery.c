#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// BAT+ -> 200k -> D2/GPIO2 -> 200k -> GND: battery voltage is 2x the pin voltage.
#define BAT_ADC_CH ADC_CHANNEL_2
#define SAMPLES 64
// 3883 mV (independent meter) / 3790 mV (uncalibrated firmware) = 1.0245
#define BAT_CAL_PPM 1024500LL
static const char *TAG = "BAT";
static volatile int g_mv, g_level;
// Level n is entered above ENTER[n-1] and left below ENTER[n-1] - HYST.
static const int ENTER[5] = {3400, 3650, 3800, 3950, 4100};
#define HYST 40

static void battery_task(void *arg) {
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &adc));
    adc_oneshot_chan_cfg_t ccfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, BAT_ADC_CH, &ccfg));
    adc_cali_handle_t cali;
    adc_cali_curve_fitting_config_t kcfg = {.unit_id = ADC_UNIT_1, .chan = BAT_ADC_CH,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&kcfg, &cali));
    for (;;) {
        int64_t sum = 0; int n = 0;
        for (int i = 0; i < SAMPLES; ++i) {
            int mv;
            if (adc_oneshot_get_calibrated_result(adc, cali, BAT_ADC_CH, &mv) == ESP_OK) { sum += mv; n++; }
            vTaskDelay(1);
        }
        if (n) {
            int bat = (int)(2 * sum / n * BAT_CAL_PPM / 1000000); // 2x divider, then meter-calibrated gain
            g_mv = g_mv ? (g_mv * 3 + bat) / 4 : bat;
            int lvl = g_level;
            while (lvl < 5 && g_mv >= ENTER[lvl]) lvl++;
            while (lvl > 0 && g_mv < ENTER[lvl - 1] - HYST) lvl--;
            g_level = lvl;
            ESP_LOGI(TAG, "pin_avg=%d mV battery_mv=%d level=%d", (int)(sum / n), g_mv, g_level);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
void battery_init(void) { xTaskCreate(battery_task, "battery", 4096, NULL, 2, NULL); }
int battery_mv(void) { return g_mv; }
int battery_level(void) { return g_level; }
