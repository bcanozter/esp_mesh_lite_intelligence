#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_mac.h"
#include "esp_bridge.h"
#include "sdkconfig.h"
#include "sensor.h"

static const char *TAG = "sensor";
static TaskHandle_t sensor_main_task_handle = NULL;

static void broadcast_sensor_readings(void)
{
    // dummy data
    sensor_packet_t *msg_buffer = (sensor_packet_t *)malloc(sizeof(sensor_packet_t));
    msg_buffer->timestamp = 0;
    msg_buffer->sensor_id = 2;
    msg_buffer->type = SENSOR_TYPE_HUMIDITY;
    msg_buffer->data.humidity.value = 85.5f;
    esp_now_send_broadcast((const uint8_t *)&msg_buffer, sizeof(sensor_packet_t), true);
    free(msg_buffer);
}

static void sensor_main_task(void *pvParameter)
{

    ESP_LOGI(TAG, "Start sensor_main_task");

    while (1)
    {
        broadcast_sensor_readings();
        vTaskDelay(10000 / portTICK_PERIOD_MS); // frequent for debugging purposes..
    }
}

esp_err_t init_sensor_read_task(void)
{
    esp_err_t ret = ESP_OK;
    xTaskCreate(sensor_main_task, "sensor_main_task", SENSOR_MAIN_TASK_STACK_SIZE, NULL, SENSOR_MAIN_TASK_PRIORITY, &sensor_main_task_handle);
    return ret;
}