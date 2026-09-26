#include <string.h>
#include <stdlib.h>
#include "net.h"
#include "secrets.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define HOSTNAME "unit-meter"
#define RELAY_TOPIC "multimeter-wifi-dkholin-ut61eplus-4e8a1c"
#define RELAY_URL "https://ntfy.sh/" RELAY_TOPIC
#define MAX_JSON 640
#define MAX_WS 6
static const char *TAG = "NET";
extern const unsigned char page_html[];
extern const unsigned int page_html_len;

static httpd_handle_t server;
static int ws_fds[MAX_WS] = {-1, -1, -1, -1, -1, -1};
static char last_json[MAX_JSON] = "{\"status\":\"starting\"}";
static char relay_pending[MAX_JSON];
static uint32_t relay_version;
static SemaphoreHandle_t lock;
static volatile bool got_ip;

static esp_err_t root_get(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, max-age=0");
    return httpd_resp_send(r, (const char *)page_html, page_html_len);
}
static esp_err_t ws_handler(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(r);
        for (int i = 0; i < MAX_WS; i++) if (ws_fds[i] == fd) goto send;
        for (int i = 0; i < MAX_WS; i++) if (ws_fds[i] < 0) { ws_fds[i] = fd; break; }
    send:;
        httpd_ws_frame_t fr = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)last_json, .len = strlen(last_json)};
        return httpd_ws_send_frame(r, &fr);
    }
    // Ignore client frames, but drain them so close frames are handled.
    httpd_ws_frame_t fr = {0};
    httpd_ws_recv_frame(r, &fr, 0);
    if (fr.len && fr.len < 128) { uint8_t b[128]; fr.payload = b; httpd_ws_recv_frame(r, &fr, fr.len); }
    return ESP_OK;
}
static void on_close(httpd_handle_t s, int fd) {
    for (int i = 0; i < MAX_WS; i++) if (ws_fds[i] == fd) ws_fds[i] = -1;
    close(fd);
}
static void start_web(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 13; // browsers preconnect idle sockets; leave room for WS clients
    cfg.lru_purge_enable = true;
    cfg.close_fn = on_close;
    if (httpd_start(&server, &cfg) != ESP_OK) return;
    httpd_uri_t a = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    httpd_uri_t b = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true};
    httpd_register_uri_handler(server, &a);
    httpd_register_uri_handler(server, &b);
}

static void ws_send_work(void *arg) {
    char *s = arg;
    for (int i = 0; i < MAX_WS; i++) if (ws_fds[i] >= 0) {
        httpd_ws_frame_t fr = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)s, .len = strlen(s)};
        { esp_err_t e = httpd_ws_send_frame_async(server, ws_fds[i], &fr); if (e != ESP_OK) { ESP_LOGW(TAG, "ws send fd=%d slot=%d err=%s", ws_fds[i], i, esp_err_to_name(e)); ws_fds[i] = -1; } }
    }
    free(s);
}
void net_publish(const char *json) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    strlcpy(last_json, json, sizeof(last_json));
    strlcpy(relay_pending, json, sizeof(relay_pending));
    relay_version++;
    xSemaphoreGive(lock);
    if (server) {
        char *copy = strdup(json);
        if (copy && httpd_queue_work(server, ws_send_work, copy) != ESP_OK) free(copy);
    }
}

// Latest-value handoff: the relay task always posts the newest state, never blocking acquisition.
static void relay_task(void *arg) {
    uint32_t last_ok = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (!got_ip || (xTaskGetTickCount() - last_ok) < pdMS_TO_TICKS(12000)) continue;
        char body[MAX_JSON]; uint32_t ver;
        xSemaphoreTake(lock, portMAX_DELAY);
        strlcpy(body, relay_pending, sizeof(body)); ver = relay_version;
        xSemaphoreGive(lock);
        if (!body[0]) continue;
        esp_http_client_config_t cfg = {.url = RELAY_URL, .method = HTTP_METHOD_POST, .timeout_ms = 5000,
                                        .crt_bundle_attach = esp_crt_bundle_attach};
        esp_http_client_handle_t c = esp_http_client_init(&cfg);
        esp_http_client_set_header(c, "Content-Type", "text/plain");
        esp_http_client_set_post_field(c, body, strlen(body));
        esp_err_t e = esp_http_client_perform(c);
        int code = esp_http_client_get_status_code(c);
        esp_http_client_cleanup(c);
        last_ok = xTaskGetTickCount();
        if (e == ESP_OK && code >= 200 && code < 300) {
            xSemaphoreTake(lock, portMAX_DELAY);
            if (relay_version == ver) relay_pending[0] = 0;
            xSemaphoreGive(lock);
        } else ESP_LOGW(TAG, "relay post failed err=%d http=%d", e, code);
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { got_ip = false; esp_wifi_connect(); }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        got_ip = true;
        ESP_LOGI(TAG, "dashboard http://" HOSTNAME ".local/  http://" IPSTR "/", IP2STR(&e->ip_info.ip));
    }
}
void net_start(void) {
    lock = xSemaphoreCreateMutex();
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta, HOSTNAME);
    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wc);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, METER_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, METER_WIFI_PASSWORD, sizeof(cfg.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    mdns_init();
    mdns_hostname_set(HOSTNAME);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    start_web();
    xTaskCreate(relay_task, "relay", 6144, NULL, 1, NULL);
}
