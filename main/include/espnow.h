#ifndef __ESPNOW_H__
#define __ESPNOW_H__

#include "esp_now.h"

#define ESPNOW_TASK_STACK_SIZE 3 * 1024
#define ESPNOW_TASK_PRIORITY 5

typedef struct esp_now_msg_send
{
    uint32_t retry_times;
    uint32_t max_retry;
    uint32_t msg_len;
    void *sent_msg;
} esp_now_msg_send_t;

#define ESPNOW_PAYLOAD_HEAD_LEN (5)
#define ESPNOW_QUEUE_SIZE (50)

typedef struct
{
    uint32_t seq;       // Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t mesh_id;    // Mesh ID of ESPNOW data
    uint8_t payload[0]; // Real payload of ESPNOW data.
} __attribute__((packed)) app_espnow_data_t;

esp_err_t app_espnow_init(void);
esp_err_t esp_now_send_broadcast(const uint8_t *, size_t, bool);

#endif