#ifndef __WIFI_H__
#define __WIFI_H__

#include "sdkconfig.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE
#define WIFI_SCAN_TASK_STACK_SIZE 3 * 1024
#define WIFI_SCAN_TASK_PRIORITY 5
#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif

#ifdef USE_CHANNEL_BITMAP
void array_2_channel_bitmap(const uint8_t[], const uint8_t, wifi_scan_config_t *);
#endif
void wifi_scan(void);
void wifi_scan_task(void *);
void wifi_init(void);
void app_wifi_set_softap_info(void);

#endif