#ifndef __SENSOR_H__
#define __SENSOR_H__

#include <stdint.h>
#include <stdbool.h>
#include "espnow.h"


#define SENSOR_MAIN_TASK_STACK_SIZE 3 * 1024
#define SENSOR_MAIN_TASK_PRIORITY 5


typedef uint8_t sensor_type_t;
enum
{
    SENSOR_TYPE_NONE = 0,
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_GENERIC,
    //?
};

//? units
typedef struct
{
    float value;
} temperature_sensor_data_t;

//?
typedef struct
{
    float value;
} humidity_sensor_data_t;

typedef union
{
    temperature_sensor_data_t temperature;
    humidity_sensor_data_t humidity;
} sensor_data_u;

typedef struct
{
    uint64_t timestamp;
    uint32_t sensor_id;
    sensor_type_t type;
    sensor_data_u data;
} sensor_packet_t;

esp_err_t init_sensor_read_task(void);
#endif