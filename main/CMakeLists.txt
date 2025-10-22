idf_component_register(SRCS "main.c" "http_server.c" "espnow.c" "nimble.c" "sensor.c" "app_wifi.c"
                       PRIV_REQUIRES esp_wifi nvs_flash iot_bridge esp_eth esp_http_server esp-tls driver
                       INCLUDE_DIRS "." "include"
                       )
