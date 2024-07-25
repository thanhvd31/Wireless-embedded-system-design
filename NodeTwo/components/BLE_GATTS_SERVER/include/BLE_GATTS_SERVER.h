#ifndef __BLE_GATTS_SERVER_H__
#define __BLE_GATTS_SERVER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

#include "esp_gatt_common_api.h"
// #include "LED.h"

enum
{
    // phân cấp đầu tiên là service
    IDX_SVC,

    // Service ho tro subcribe topic
    IDX_CHAR_SUBCRIBE,
    IDX_CHAR_SUBCRIBE_VAL,
    IDX_CHAR_SUBCRIBE_CFG,
    // Service ho tro publish topic
    IDX_CHAR_PUBLISH,
    IDX_CHAR_PUBLISH_VAL,
    IDX_CHAR_PUBLISH_CFG,
    // Service ho tro publish data len topic
    IDX_CHAR_MESSAGE,
    IDX_CHAR_MESSAGE_VAL,
    IDX_CHAR_MESSAGE_CFG,
    HRS_IDX_NB,
};

#define DEBUG_ON 0

#if DEBUG_ON
#define EXAMPLE_DEBUG ESP_LOGI
#else
#define EXAMPLE_DEBUG(tag, format, ...)
#endif

#define EXAMPLE_TAG "BLE_COMP"

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x55
#define SAMPLE_DEVICE_NAME "Smart_home"
#define SVC_INST_ID 0

/* The max length of characteristic value. When the gatt client write or prepare write,
 *  the data length must be less than GATTS_EXAMPLE_CHAR_VAL_LEN_MAX.
 */
#define GATTS_EXAMPLE_CHAR_VAL_LEN_MAX 500
#define SUBCRIBE_CHAR_VAL_LEN 100
#define PUBLISH_CHAR_VAL_LEN 100
#define MESSAGE_CHAR_VAL_LEN 200
#define LONG_CHAR_VAL_LEN 500
#define SHORT_CHAR_VAL_LEN 10
#define GATTS_NOTIFY_FIRST_PACKET_LEN_MAX 20

#define OLED_DISPLAY_ROW_LOCATION 4
#define OLED_DISPLAY_COLUMN_LOCATION 32
#define MSSV_INVALID_COMPARE_BYTE 2

#define PREPARE_BUF_MAX_SIZE 1024
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

void ble_gatt_init(void);

#endif