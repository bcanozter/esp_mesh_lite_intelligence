#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bridge.h"
#include "esp_mesh_lite.h"
#include "app_wifi.h"

#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"

static const char *TAG = "wifi";
static char softap_ssid[33] = "";
static char softap_psw[64] = "";
static TaskHandle_t wifi_task_handle = NULL;
bool sta_got_ip = false;

#if CONFIG_ENABLE_ARP_SCAN
static arp_scan_result_list_t *arp_scan_result_list = NULL;

void arp_scan_result_list_init()
{
    arp_scan_result_list = (arp_scan_result_list_t *)calloc(1, sizeof(arp_scan_result_list_t));
    if (!arp_scan_result_list)
    {
        ESP_LOGE(TAG, "Memory Allocation for arp scan result list failed!");
        return;
    }
    arp_scan_result_list->count = 0;
    arp_scan_result_list->results = NULL;
}

void arp_scan_result_list_add(arp_scan_result_t *result)
{
    if (!arp_scan_result_list)
    {
        ESP_LOGE(TAG, "ARP scan result list not initialized!");
        return;
    }
    arp_scan_result_list->results = (arp_scan_result_t *)realloc(arp_scan_result_list->results, (arp_scan_result_list->count + 1) * sizeof(arp_scan_result_t));
    if (!arp_scan_result_list->results)
    {
        ESP_LOGE(TAG, "Memory Allocation for arp scan result list results failed!");
        return;
    }
    arp_scan_result_list->results[arp_scan_result_list->count] = *result;
    arp_scan_result_list->count++;
    ESP_LOGD(TAG, "Added ARP result to the list: %s -> " MACSTR, result->ip, MAC2STR(result->mac));
}

void arp_scan_result_list_free()
{
    if (arp_scan_result_list)
    {
        free(arp_scan_result_list->results);
        arp_scan_result_list->results = NULL;
        arp_scan_result_list->count = 0;
        free(arp_scan_result_list);
        arp_scan_result_list = NULL;
    }
}

void print_arp_scan_result_list()
{
    if (!arp_scan_result_list)
    {
        ESP_LOGE(TAG, "ARP scan result list not initialized");
        return;
    }
    for (int i = 0; i < arp_scan_result_list->count; i++)
    {
        ESP_LOGI(TAG, "Host %s -> %s ", arp_scan_result_list->results[i].ip, arp_scan_result_list->results[i].mac);
    }
}
#endif

#if CONFIG_ENABLE_WIFI_STA
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "EVENT ID: %d, EVENT_BASE %s", event_id, event_base);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
        sta_got_ip = false;
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        sta_got_ip = true;
    }
}
#endif

void arp_scan()
{
    ESP_LOGI(TAG, "Start ARP scan");

    // esp_mesh_lite.c line 45.
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif)
    {
        ESP_LOGE(TAG, "Failed to get sta_netif");
        return;
    }

    struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(sta_netif);
    if (!lwip_netif)
    {
        ESP_LOGE(TAG, "Failed to get lwIP netif");
        return;
    }

    // Get local IP info
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get ip info");
        return;
    }

    ip4_addr_t ip = {.addr = ip_info.ip.addr};
    ip4_addr_t netmask = {.addr = ip_info.netmask.addr};

    char ip_str[16], nm_str[16];
    inet_ntoa_r(ip, ip_str, sizeof(ip_str));
    inet_ntoa_r(netmask, nm_str, sizeof(nm_str));

    uint32_t network_n = ntohl(ip_info.ip.addr) & ntohl(ip_info.netmask.addr); // e.g 192.168.1.136 & 255.255.255.0
    uint32_t broadcast_n = network_n | (~(ntohl(ip_info.netmask.addr)));

    ESP_LOGI(TAG, "Station IP: %s  Netmask Addr: %s",
             ip_str, nm_str);

    uint32_t first_host = network_n + 1;
    uint32_t last_host = broadcast_n - 1;
    if (first_host > last_host)
    {
        ESP_LOGW(TAG, "Error range of hosts");
        return;
    }

    ip4_addr_t first_ip = {.addr = htonl(first_host)};
    ip4_addr_t last_ip = {.addr = htonl(last_host)};

    // Debug
    char first_ip_str[IP4ADDR_STRLEN_MAX], last_ip_str[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(&first_ip, first_ip_str, sizeof(first_ip_str));
    ip4addr_ntoa_r(&last_ip, last_ip_str, sizeof(last_ip_str));
    ESP_LOGI(TAG, "First IP: %s  Last IP: %s",
             first_ip_str, last_ip_str);

    // Iterate the subnet
    for (uint32_t h = first_host; h <= last_host; ++h)
    {
        ip4_addr_t target = {.addr = htonl(h)};
        char ip_str[IP4ADDR_STRLEN_MAX];
        ip4addr_ntoa_r(&target, ip_str, sizeof(ip_str));

        // Send ARP request
        err_t res = etharp_request(lwip_netif, &target);
        if (res == ERR_OK)
            ESP_LOGD(TAG, "ARP request sent for %s", ip_str);

        vTaskDelay(pdMS_TO_TICKS(200));

        const ip4_addr_t *ip_ret = NULL;
        struct eth_addr *eth_ret = NULL;
        if (etharp_find_addr(lwip_netif, &target, &eth_ret, &ip_ret) != -1 && eth_ret != NULL)
        {
            arp_scan_result_t *result = (arp_scan_result_t *)calloc(1, sizeof(arp_scan_result_t));
            if (!result)
            {
                ESP_LOGE(TAG, "Memory Allocation for arp scan result failed!");
                continue;
            }
            strlcpy(result->ip, ip_str, sizeof(result->ip));
            char mac_str[18] = {0};
            sprintf(mac_str, MACSTR, MAC2STR(eth_ret->addr));
            strlcpy(result->mac, mac_str, sizeof(result->mac));
            arp_scan_result_list_add(result);
            free(result);
        }
        ESP_LOGI(TAG,"ARP Scan in progress %u%%",((((h - first_host) * 100) /(last_host - first_host + 1))));
    }

    ESP_LOGI(TAG, "ARP scan finished");
    print_arp_scan_result_list();
    arp_scan_result_list_free();
    arp_scan_result_list_init();
}

// TODO: get connected clients
void wifi_scan(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

#ifdef USE_CHANNEL_BITMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1, sizeof(wifi_scan_config_t));
    if (!scan_config)
    {
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);

#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/

#if CONFIG_APP_DEBUG
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++)
    {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "BSID \t\t" MACSTR, MAC2STR(ap_info[i].bssid));
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
#endif
}

void wifi_task_main(void *pvParameter)
{
#if CONFIG_ENABLE_ARP_SCAN
    arp_scan_result_list_init();
#endif
    ESP_LOGI(TAG, "Start wifi_task");
    wifi_init();
    while (1)
    {
// TODO
//  gather client info
//  wifi_scan() //rename
#if CONFIG_ENABLE_ARP_SCAN
        if (sta_got_ip)
        {
            arp_scan();
        }
#endif
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void wifi_init_ap(void)
{
#if !(CONFIG_ENABLE_WIFI_STA)
    wifi_config_t wifi_config;
    memset(&wifi_config, 0x0, sizeof(wifi_config_t));
    esp_bridge_wifi_set_config(WIFI_IF_STA, &wifi_config);
#endif
    // Softap
    wifi_config_t wifi_softap_config = {
        .ap = {
            .ssid = CONFIG_BRIDGE_SOFTAP_SSID,
            .password = CONFIG_BRIDGE_SOFTAP_PASSWORD,
            .channel = CONFIG_MESH_CHANNEL,
            .ssid_hidden = 1,
        },
    };
    esp_bridge_wifi_set_config(WIFI_IF_AP, &wifi_softap_config);
}

#if CONFIG_ENABLE_WIFI_STA
void wifi_init_sta(void)
{

    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    // Station
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    esp_bridge_wifi_set_config(WIFI_IF_STA, &wifi_config);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 softap_ssid, softap_psw);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return;
    }
}
#endif

void wifi_init(void)
{
#if CONFIG_ENABLE_WIFI_STA
    wifi_init_sta();
#endif
    wifi_init_ap();
}

void wifi_task_init(void)
{
    xTaskCreate(wifi_task_main, "wifi_task", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, &wifi_task_handle);
}

void app_wifi_set_softap_info(void)
{
    // char softap_ssid[33];
    // char softap_psw[64];
    uint8_t softap_mac[6];
    size_t ssid_size = sizeof(softap_ssid);
    size_t psw_size = sizeof(softap_psw);
    esp_wifi_get_mac(WIFI_IF_AP, softap_mac);
    memset(softap_ssid, 0x0, sizeof(softap_ssid));
    memset(softap_psw, 0x0, sizeof(softap_psw));

    if (esp_mesh_lite_get_softap_ssid_from_nvs(softap_ssid, &ssid_size) == ESP_OK)
    {
        ESP_LOGI(TAG, "Get ssid from nvs: %s", softap_ssid);
    }
    else
    {
#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
        snprintf(softap_ssid, sizeof(softap_ssid), "%.25s_%02x%02x%02x", CONFIG_BRIDGE_SOFTAP_SSID, softap_mac[3], softap_mac[4], softap_mac[5]);
#else
        snprintf(softap_ssid, sizeof(softap_ssid), "%.32s", CONFIG_BRIDGE_SOFTAP_SSID);
#endif
        ESP_LOGI(TAG, "Get ssid from nvs failed, set ssid: %s", softap_ssid);
    }

    if (esp_mesh_lite_get_softap_psw_from_nvs(softap_psw, &psw_size) == ESP_OK)
    {
        ESP_LOGI(TAG, "Get psw from nvs: [HIDDEN]");
    }
    else
    {
        strlcpy(softap_psw, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(softap_psw));
        ESP_LOGI(TAG, "Get psw from nvs failed, set psw: [HIDDEN]");
    }

    esp_mesh_lite_set_softap_info(softap_ssid, softap_psw);
}