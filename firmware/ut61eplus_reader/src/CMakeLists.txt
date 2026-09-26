FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)

# The dashboard page is the same docs/index.html that GitHub Pages serves.
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "."
    REQUIRES esp_adc driver esp_timer esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_http_client esp-tls mdns)
