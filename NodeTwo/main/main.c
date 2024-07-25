/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>

#include "BLE_GATTS_SERVER.h"
#include "LED.h"
#include "WIFI_STATION.h"
#include "servo.h"
#include "driver/gpio.h"

// #define CONFIG_LOG_MAXIMUM_LEVEL 3
#define MQTT_BROKER "mqtts://4a3b0ae555444c35bb88f1b5da956f24.s1.eu.hivemq.cloud"

static const char *MQTT_TAG = "[MQTTS]";
static const char *LED_TAG = "[LED]";
static const char *DOOR_TAG = "[DOOR]";
static const char username[] = "CE232";
static const char password[] = "Binheo12";

static char *publish_topic;
static const char *led_topic_default = "smarthome/led";
static const char *door_topic_default = "smarthome/door";
size_t publish_topic_size = 10;

xQueueHandle queue_ble_mqtt;
xQueueHandle queue_subcribe_topic;
xQueueHandle queue_publish_topic;

esp_mqtt_client_handle_t client;

// Biến để theo dõi trạng thái của cửa
bool is_door_open = false;

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[] asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[] asm("_binary_mqtt_eclipseprojects_io_pem_end");

//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
static void send_binary(esp_mqtt_client_handle_t client)
{
    spi_flash_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
    ESP_LOGI(MQTT_TAG, "binary sent with msg_id=%d", msg_id);
}

//************************************ SERVO ************************************//

// Khai báo hàm handle_user_input để tránh lỗi "implicit declaration"
// Hàm này từ trong hàm process_webclient_data để tránh lỗi và đồng thời thực hiện chức năng điều khiển servo
// Nếu nhận được msg từ mqtt = "open" thì cập nhật góc quay 0 -> 90 độ và "close" 90 -> 0 độ
static void handle_user_input(const char *command)
{
    int angle = 0;
    if (strcmp(command, "open") == 0)
    {
        angle = 90;
        is_door_open = true;
        for (int i = 0; i <= SERVO_MAX_DEGREE; i++)
        {
            servo_per_degree_init(i);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    else if (strcmp(command, "close") == 0)
    {
        angle = 0;
        is_door_open = false;
        for (int i = SERVO_MAX_DEGREE; i >= 0; i--)
        {
            servo_per_degree_init(i);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    else
    {
        ESP_LOGW(DOOR_TAG, "Invalid command: %s", command);
        return;
    }
    // Set the target angle for the servo
    targetAngle = angle;
    ESP_LOGI(DOOR_TAG, "Servo angle updated: %d\n", angle); // In ra góc đã được cập nhật
}

// Hàm xử lý dữ liệu từ webclient
static void process_webclient_data(char *data)
{
    // Kiểm tra xem dữ liệu có tồn tại không
    if (data == NULL)
    {
        printf("Error: Empty data received from webclient.\n");
        return;
    }

    // Kiểm tra độ dài của dữ liệu
    int data_length = strlen(data);
    if (data_length == 0)
    {
        printf("Error: Empty data received from webclient.\n");
        return;
    }

    // Gọi hàm xử lý lệnh từ webclient
    handle_user_input(data);
}

static void check_and_close_door(void *arg)
{
    while (true)
    {
        vTaskDelay(15000 / portTICK_PERIOD_MS); // Chờ 10 giây
        if (is_door_open)
        {
            ESP_LOGI(DOOR_TAG, "Door has been open for 10 seconds, closing...");
            handle_user_input("close");
            is_door_open = false;
        }
    }
}

// Hàm xử lí message từ mqtt
static void door_mqtt_handle(esp_mqtt_event_handle_t event)
{
    // Copy dữ liệu nhận từ mqtt được để xử lý
    char *data = (char *)malloc(event->data_len + 1);
    if (data == NULL)
    {
        printf("Error: Could not allocate memory for data\n");
        return;
    }
    strncpy(data, event->data, event->data_len);
    data[event->data_len] = '\0';
    printf("Received data: %s\r\n", data);
    // Gọi hàm xử lý dữ liệu
    process_webclient_data(data);
    free(data);
}
//*********************************************************************************************************

//************************************ LED ************************************//

// Hàm xử lí message từ mqtt
static void led_mqtt_handle(esp_mqtt_event_handle_t event)
{
    // Luu data va topic
    char *
        data = (char *)malloc(event->data_len + 1);
    if (data == NULL)
    {
        printf("Error: Could not allocate memory for data\n");
        return;
    }
    strncpy(data, event->data, event->data_len);
    uint8_t mode_index = 0;

    ID id = NUMBER_OF_ID;

    char *token = strtok(data, " ");
    // lấy ID từ data
    if (token != NULL)
    {
        if (strcmp(token, "bedroom") == 0)
        {
            id = BED_ROOM;
            ESP_LOGI(LED_TAG, "ID : bedroom");
        }
        else if (strcmp(token, "livingroom") == 0)
        {
            id = LIVING_ROOM;
            ESP_LOGI(LED_TAG, "ID : livingroom");
        }
        else
        {
            ESP_LOGE(MQTT_TAG, "Invalid ID!!");
        }
    }
    else
    {
        ESP_LOGE(MQTT_TAG, "invalid led topic!!");
        return;
    }
    // tiếp theo lấy command từ data
    token = strtok(NULL, " ");
    if (token != NULL)
    {
        if (strcmp(token, "mode") == 0)
        {
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                mode_index = atoi(token) - 1;
                set_mode(id, mode[mode_index]);
                ESP_LOGI(LED_TAG, "set mode led");
            }
            else
            {
                ESP_LOGE(LED_TAG, "error mode");
                free(data);
                return;
            }
        }
        else if (strcmp(token, "state") == 0)
        {
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                // printf("%s",token);
                if (strcmp(token, "off") == 0)
                {
                    turn_off_led(id);
                    ESP_LOGI(LED_TAG, "Turn off led");
                }
                else if (strcmp(token, "on") == 0)
                {
                    turn_on_led(id);
                    ESP_LOGI(LED_TAG, "Turn on led");
                }
                else
                {
                    ESP_LOGE(MQTT_TAG, "error state!!");
                    free(data);
                    return;
                }
            }
            else
            {
                ESP_LOGE(MQTT_TAG, "invalid state!!");
                free(data);
                return;
            }
        }
        else
        {
            ESP_LOGE(MQTT_TAG, "invalid command");
            free(data);
            return;
        }
    }
    free(data);
}

//*********************************************************************************************************

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, led_topic_default, 0);
        ESP_LOGI(MQTT_TAG, "sent subscribe LED topic successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, door_topic_default, 1);
        ESP_LOGI(MQTT_TAG, "sent subscribe DOOR topic successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        char *topic = event->topic;
        if (strncmp(led_topic_default, topic, strlen(led_topic_default)) == 0)
        {
            led_mqtt_handle(event);
        }
        else if (strncmp(door_topic_default, topic, strlen(door_topic_default)) == 0)
        {
            door_mqtt_handle(event);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(MQTT_TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(MQTT_TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(MQTT_TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(MQTT_TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(MQTT_TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER,
        .cert_pem = (const char *)mqtt_eclipseprojects_io_pem_start,
        .username = username,
        .password = password};

    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void mqtt_subcribe_topic()
{
    BaseType_t ret;
    while (1)
    {
        char *subcribe_topic = (char *)malloc(SUBCRIBE_CHAR_VAL_LEN);
        if (xQueueReceive(queue_subcribe_topic, subcribe_topic, (TickType_t)10))
        {
            ret = esp_mqtt_client_subscribe(client, subcribe_topic, 0);
            if (ret != -1)
            {
                ESP_LOGI(MQTT_TAG, "subcribe topic successfully ");
            }
            else
                ESP_LOGE(MQTT_TAG, "\tsubcribe topic error");
        }
        free(subcribe_topic);
    }
}

static void mqtt_publish_topic()
{
    while (1)
    {
        char *rxBuffer = (char *)malloc(PUBLISH_CHAR_VAL_LEN);
        if (xQueueReceive(queue_publish_topic, rxBuffer, (TickType_t)10))
        {
            publish_topic_size = strlen(rxBuffer) + 1;
            publish_topic = (char *)malloc(publish_topic_size);
            memcpy(publish_topic, rxBuffer, publish_topic_size);
        }
        free(rxBuffer);
    }
}

static void send_mqtt_mesage()
{

    while (1)
    {
        char *message = (char *)malloc(MESSAGE_CHAR_VAL_LEN);
        if (xQueueReceive(queue_ble_mqtt, message, (TickType_t)10))
        {
            esp_mqtt_client_publish(client, publish_topic, message, 0, 0, 0);
        }
        free(message);
    }
}

void app_main(void)
{
    ESP_LOGI(MQTT_TAG, "[APP] Startup..");
    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(MQTT_TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ****************** CONNECTION IINT ******************
    wifi_init();
    mqtt_app_start();
    ble_gatt_init();

    // ******************   DRIVER INIT   ******************
    led_init();
    servo_init();

    // ******************   THREAD INIT   ******************
    BaseType_t ret;
    TaskHandle_t sending_message_ble_to_mqtt = NULL;
    TaskHandle_t subcribe_topic_from_ble_to_mqtt = NULL;
    TaskHandle_t publish_topic_from_ble_to_mqtt = NULL;
    TaskHandle_t check_door_status = NULL;

    queue_subcribe_topic = xQueueCreate(3, SUBCRIBE_CHAR_VAL_LEN);
    if (queue_subcribe_topic == 0)
    {
        ESP_LOGE(MQTT_TAG, "Cannot create subcribe queue!!");
    }
    ret = xTaskCreate(mqtt_subcribe_topic, "subcribe topic from ble to mqtt", 4096, NULL, 3, &subcribe_topic_from_ble_to_mqtt);
    if (ret != pdPASS)
    {
        ESP_LOGE(MQTT_TAG, "Error create task!!");
    }

    queue_publish_topic = xQueueCreate(3, PUBLISH_CHAR_VAL_LEN);
    if (queue_publish_topic == 0)
    {
        ESP_LOGE(MQTT_TAG, "Cannot create publish queue!!");
    }
    ret = xTaskCreate(mqtt_publish_topic, "Choose topic to Publish from ble to mqtt", 4096, NULL, 2, &publish_topic_from_ble_to_mqtt);
    if (ret != pdPASS)
    {
        ESP_LOGE(MQTT_TAG, "Error create task!!");
    }

    queue_ble_mqtt = xQueueCreate(6, MESSAGE_CHAR_VAL_LEN);
    if (queue_ble_mqtt == 0)
    {
        ESP_LOGE(MQTT_TAG, "Cannot create message queue!!");
    }
    ret = xTaskCreate(send_mqtt_mesage, "sending message from ble to mqtt", 4096, NULL, 1, &sending_message_ble_to_mqtt);
    if (ret != pdPASS)
    {
        ESP_LOGE(MQTT_TAG, "Error create task!!");
    }
    ret = xTaskCreate(check_and_close_door, "check_and_close_door", 2048, NULL, 5, &check_door_status);
    if (ret != pdPASS)
    {
        ESP_LOGE(MQTT_TAG, "Error create task!!");
    }
}
