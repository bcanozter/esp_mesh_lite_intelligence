#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_netif_types.h"
#include "esp_mesh_lite.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include <espnow.h>
#include <sensor.h>

static const char *TAG = "espnow";

static uint32_t current_seq = 0;
static TaskHandle_t espnow_task_ctrl_handle = NULL;
static QueueHandle_t espnow_recv_queue = NULL;
static SemaphoreHandle_t sent_msgs_mutex = NULL;
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static esp_now_msg_send_t *sent_msgs;
uint8_t espnow_payload[ESPNOW_PAYLOAD_MAX_LEN];

esp_err_t espnow_data_parse(const uint8_t *data, uint16_t data_len)
{
    app_espnow_data_t *buf = (app_espnow_data_t *)data;

    if (data_len < sizeof(app_espnow_data_t))
    {
        ESP_LOGD(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return ESP_FAIL;
    }

    if (buf->mesh_id == CONFIG_MESH_ID)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

/* Prepare ESPNOW data to be sent. */
void espnow_data_prepare(uint8_t *buf, const uint8_t *payload, size_t payload_len, bool seq_init)
{
    app_espnow_data_t *temp = (app_espnow_data_t *)buf;
    if (seq_init)
    {
        temp->seq = 0;
        current_seq = 0;
    }
    else
    {
        temp->seq = ++current_seq;
    }
    temp->mesh_id = CONFIG_MESH_ID;
#ifdef CONFIG_DEBUG
    printf("send seq: %" PRIu32 ", current_seq: %" PRIu32 "\r\n", temp->seq, current_seq);
    ESP_LOGW(TAG, "free heap %" PRIu32 ", minimum %" PRIu32 "", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
#endif
    memcpy(temp->payload, payload, payload_len);
}

esp_err_t app_espnow_create_peer(uint8_t dst_mac[ESP_NOW_ETH_ALEN])
{
    esp_err_t ret = ESP_FAIL;
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return ESP_ERR_NO_MEM;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));

    esp_now_get_peer(dst_mac, peer);
    peer->channel = 0;
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false;
    // memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
    memcpy(peer->peer_addr, dst_mac, ESP_NOW_ETH_ALEN);

    if (esp_now_is_peer_exist(dst_mac) == false)
    {
        ret = esp_now_add_peer(peer);
    }
    else
    {
        ret = esp_now_mod_peer(peer);
    }
    free(peer);

    return ret;
}

static void esp_now_send_timer_cb(TimerHandle_t timer)
{
    xSemaphoreTake(sent_msgs_mutex, portMAX_DELAY);
    if (sent_msgs->max_retry > sent_msgs->retry_times)
    {
        sent_msgs->retry_times++;
        if (sent_msgs->sent_msg)
        {
            app_espnow_create_peer(s_broadcast_mac);
            esp_err_t ret = esp_mesh_lite_espnow_send(ESPNOW_DATA_TYPE_RESERVE, s_broadcast_mac, sent_msgs->sent_msg, sent_msgs->msg_len);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send error: %d [%s %d]", ret, __func__, __LINE__);
            }
        }
    }
    else
    {
        if (sent_msgs->max_retry)
        {
            sent_msgs->retry_times = 0;
            sent_msgs->max_retry = 0;
            sent_msgs->msg_len = 0;
            if (sent_msgs->sent_msg)
            {
                free(sent_msgs->sent_msg);
                sent_msgs->sent_msg = NULL;
            }
        }
    }
    xSemaphoreGive(sent_msgs_mutex);
}

void esp_now_remove_send_msgs(void)
{
    xSemaphoreTake(sent_msgs_mutex, portMAX_DELAY);
    if (sent_msgs->max_retry)
    {
        sent_msgs->retry_times = 0;
        sent_msgs->max_retry = 0;
        sent_msgs->msg_len = 0;
        if (sent_msgs->sent_msg)
        {
            free(sent_msgs->sent_msg);
            sent_msgs->sent_msg = NULL;
        }
    }
    xSemaphoreGive(sent_msgs_mutex);
}

#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 4, 1)
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
#else
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
#endif
{
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 4, 1)
    const uint8_t *mac_addr = tx_info->des_addr;
    if (tx_info == NULL)
    {
#else
    if (mac_addr == NULL)
    {
#endif
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

#ifdef CONFIG_DEBUG
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGW(TAG, "Send OK to " MACSTR " %s %d", MAC2STR(mac_addr), __func__, __LINE__);
    }
    else
    {
        ESP_LOGW(TAG, "Send Fail %s %d ", __func__, __LINE__);
    }
#endif
}

static esp_err_t espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    esp_mesh_lite_espnow_event_t evt;
    espnow_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = (uint8_t *)recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return ESP_FAIL;
    }

    if (espnow_data_parse(data, len) != ESP_OK)
    {
        return ESP_FAIL;
    }

    evt.id = ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return ESP_FAIL;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(espnow_recv_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
        recv_cb->data = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void espnow_task(void *pvParameter)
{
    esp_mesh_lite_espnow_event_t evt;

    ESP_LOGI(TAG, "Start espnow task");

    while (xQueueReceive(espnow_recv_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case ESPNOW_RECV_CB:
            espnow_recv_cb_t *recv_cb = &evt.info.recv_cb;
            app_espnow_data_t *buf = (app_espnow_data_t *)recv_cb->data;
            uint32_t recv_seq = buf->seq;
            memset(espnow_payload, 0x0, ESPNOW_PAYLOAD_MAX_LEN);
            memcpy(espnow_payload, buf->payload, recv_cb->data_len - ESPNOW_PAYLOAD_HEAD_LEN);

#ifdef CONFIG_DEBUG

            ESP_LOGI(TAG, "Receive broadcast data from: " MACSTR ", len: %d, recv_seq: %" PRIu32 ", current_seq: %" PRIu32 "",
                     MAC2STR(recv_cb->mac_addr),
                     recv_cb->data_len,
                     recv_seq,
                     current_seq);
#endif
            sensor_packet_t *sensor_data = (sensor_packet_t *)espnow_payload;
            uint64_t timestamp = sensor_data->timestamp;
            uint32_t sensor_id = sensor_data->sensor_id;
            sensor_type_t type = sensor_data->type;
            if (type == SENSOR_TYPE_TEMPERATURE)
            {
                float temperature = sensor_data->data.temperature.value;
#ifdef CONFIG_DEBUG
                ESP_LOGI(TAG, "Timestamp: %llu, Sensor ID: %lu, Type: %u, Temperature: %.2f",
                         timestamp, sensor_id, type, temperature);
#endif
            }
            else if (type == SENSOR_TYPE_HUMIDITY)
            {
                float humidity = sensor_data->data.humidity.value;
#ifdef CONFIG_DEBUG
                ESP_LOGI(TAG, "Timestamp: %llu, Sensor ID: %lu, Type: %u, Humidity: %.2f",
                         timestamp, sensor_id, type, humidity);
#endif
            }
            free(recv_cb->data);
            recv_cb->data = NULL;
            break;
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

esp_err_t esp_now_send_broadcast(const uint8_t *payload, size_t payload_len, bool seq_init)
{
    esp_err_t ret = ESP_OK;
    uint8_t *buf = calloc(1, payload_len + ESPNOW_PAYLOAD_HEAD_LEN);
    espnow_data_prepare(buf, payload, payload_len, seq_init);
    app_espnow_create_peer(s_broadcast_mac);
    ret = esp_mesh_lite_espnow_send(ESPNOW_DATA_TYPE_RESERVE, s_broadcast_mac, buf, payload_len + ESPNOW_PAYLOAD_HEAD_LEN);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Send error: %d [%s %d]", ret, __func__, __LINE__);
    }
    return ret;
}

void espnow_deinit(void)
{
    esp_now_unregister_send_cb();

    if (espnow_task_ctrl_handle)
    {
        vTaskDelete(espnow_task_ctrl_handle);
        espnow_task_ctrl_handle = NULL;
    }

    if (espnow_recv_queue)
    {
        vSemaphoreDelete(espnow_recv_queue);
        espnow_recv_queue = NULL;
    }
}

esp_err_t app_espnow_init(void)
{
    espnow_recv_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(esp_mesh_lite_espnow_event_t));
    if (espnow_recv_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    esp_mesh_lite_espnow_init();
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    esp_mesh_lite_espnow_recv_cb_register(ESPNOW_DATA_TYPE_RESERVE, espnow_recv_cb);

    /* Add broadcast peer information to peer list. */
    if (app_espnow_create_peer(s_broadcast_mac) != ESP_OK)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        esp_now_unregister_send_cb();
        vSemaphoreDelete(espnow_recv_queue);
        espnow_recv_queue = NULL;
        return ESP_FAIL;
    }

    xTaskCreate(espnow_task, "espnow_task", 3 * 1024, NULL, 4, &espnow_task_ctrl_handle);

    sent_msgs = (esp_now_msg_send_t *)malloc(sizeof(esp_now_msg_send_t));
    sent_msgs->max_retry = 0;
    sent_msgs->msg_len = 0;
    sent_msgs->retry_times = 0;
    sent_msgs->sent_msg = NULL;
    sent_msgs_mutex = xSemaphoreCreateMutex();

    TimerHandle_t esp_now_send_timer = xTimerCreate("esp_now_send_timer", 100 / portTICK_PERIOD_MS, pdTRUE,
                                                    NULL, esp_now_send_timer_cb);
    xTimerStart(esp_now_send_timer, portMAX_DELAY);

    return ESP_OK;
}