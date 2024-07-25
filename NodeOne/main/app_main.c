
/**************DEFAULT CONFIGURATION*****************/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
/****************************************************/
/****************CUSTOM INCLUDE**********************/
// Fan control includes
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "esp_timer.h"
// Get time settings
#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include <stdbool.h>
// DHT11 sensor includes
#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

//SSD includes
#include "nvs.h"
#include "cJSON.h"
#include "driver/i2c.h"
#include "SSD1306.h"
#include "font.h"


/****************************************************/
/*DEFINE CHO MÀN HÌNH*/
#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define DATA_LENGTH 512                  /*!< Data buffer length of test buffer */
#define RW_TEST_LENGTH 128               /*!< Data length for r/w test, [0,DATA_LENGTH] */
#define DELAY_TIME_BETWEEN_ITEMS_MS 1000 /*!< delay time between different test items */

#define I2C_MASTER_SCL_IO GPIO_NUM_22 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO GPIO_NUM_21 /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUMBER(0)  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */

#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */
/****************CUSTOM DEFINES**********************/
// MQRTT Broker URL
#define MQTT_BROKER_URL "mqtts://c0116143ef0143149b4c43981ffead03.s1.eu.hivemq.cloud"
// Fan configuration
#define ENA_PIN GPIO_NUM_14
#define IN1_PIN GPIO_NUM_27
#define IN2_PIN GPIO_NUM_26
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CH0 LEDC_CHANNEL_0
// DHT11 configuration
#define DHT11_PIN GPIO_NUM_4
#define MQTT_sizeoff 200
/****************************************************/

/*****************MACROS*****************************/
// SNTP Debug
static const char *TAG1 = "get_time";
char Current_Date_Time[100];
// MQTT Credentials
static const char *TAG = "mqtts_example";
static const char username[] = "CE232";
static const char password[] = "Nhi09032003";
esp_mqtt_client_handle_t client;
// FAN Topic
char *fan_topic = "smarthome/fan/control";
char *fan_time_topic = "smarthome/fan/time";
int speed = 0;
static const char *TAG2 = "fan_control";
static bool is_publish_message = false;
bool fan_state = false;
esp_timer_handle_t fan_timer;
// DHT11 Topic
char *dht_topic = "smarthome/sensor";
unsigned long LastSendMQTT = 0;
char MQTT_BUFFER[MQTT_sizeoff];
char Str_ND[MQTT_sizeoff];
char Str_DA[MQTT_sizeoff];
char str[MQTT_sizeoff];
char strtemp[50];
char strhum[50];
bool mqtt_connect_status = false;

//SSD
#define ESP_ERR (__STATUX__) if ()
SemaphoreHandle_t print_mux = NULL;
static const char *TAG3 = "OLED";
/****************************************************/
void ssd1306_init();
void LCD();
void task_ssd1306_display_text(const void *arg_text, uint8_t _page, uint8_t _seg);
void task_ssd1306_display_clear();
esp_err_t task_ssd1306_display_location(uint8_t _page, uint8_t _seg);
esp_err_t task_ssd1306_display_image(uint8_t *images, uint8_t _page, uint8_t _seg, int _size);

/*Khai báo các function của Quạt*/
void time_sync_notification_cb(struct timeval *tv);
static void sntp_setup(void);
void set_systemtime_sntp();
void fan_control(int speed);
void fan_off_callback(void *arg);
void start_fan_timer(int64_t duration_us);
void check_time_and_turn_on_fan(int start_hour, int start_minute, int start_second, int duration_hour, int duration_minute, int duration_second, int fan_speed);
void monitor_and_control_fan(int start_hour, int start_minute, int start_second, int duration_hour, int duration_minute, int duration_second, int fan_speed);
void publish_fan_speed_message(int speed);
static void app_handle_mqtt_data(esp_mqtt_event_handle_t event);

/**************************CONFIG CHO MÀN HÌNH***********************************/
static esp_err_t __attribute__((unused)) i2c_master_read_slave(i2c_port_t i2c_num, uint8_t *data_rd, size_t size)
{
    if (size == 0)
    {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | READ_BIT, ACK_CHECK_EN);
    if (size > 1)
    {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
static esp_err_t __attribute__((unused)) i2c_master_write_slave(i2c_port_t i2c_num, uint8_t *data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 /portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK)
    {
        return err;
    }
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/*************GET REAL TIME FUNCTIONS****************/
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG1, "Notification of a time synchronization event");
}
void get_current_time(char *date_time, int *hour, int *minute, int *second)
{
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    setenv("TZ", "UTC-07:00", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG1, "The current date/time is: %s", strftime_buf);
    strcpy(date_time, strftime_buf);
    *hour = timeinfo.tm_hour;
    *minute = timeinfo.tm_min;
    *second = timeinfo.tm_sec;
}
static void sntp_setup(void)
{
    ESP_LOGI(TAG1, "Initalizing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_get_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    sntp_init();
}
static void obtain_time(void)
{
    sntp_setup();
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG1, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}
void set_systemtime_sntp()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG1, "Time not set yet. Connecting to WiFi and getting an NTP time to set time.");
        obtain_time();
        time(&now);
    }
}
/****************************************************/

/*************FAN FUNCTIONS**************************/
void fan_control(int speed)
{
    esp_rom_gpio_pad_select_gpio(ENA_PIN);
    esp_rom_gpio_pad_select_gpio(IN1_PIN);
    esp_rom_gpio_pad_select_gpio(IN2_PIN);
    gpio_set_direction(ENA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2_PIN, GPIO_MODE_OUTPUT);

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 1000,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER};
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CH0,
        .duty = 0,
        .gpio_num = ENA_PIN,
        .speed_mode = LEDC_MODE,
        .timer_sel = LEDC_TIMER};
    ledc_channel_config(&ledc_channel);

    uint32_t duty = (speed * 255) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CH0, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CH0);
    gpio_set_level(IN1_PIN, 1);
    gpio_set_level(IN2_PIN, 0);
}
void fan_off_callback(void *arg)
{
    fan_control(0);
    fan_state = false;
    ESP_LOGI(TAG2, "Fan turned off.");
}
void start_fan_timer(int64_t duration_us)
{
    if (fan_timer != NULL)
    {
        esp_timer_stop(fan_timer);
        esp_timer_delete(fan_timer);
    }
    const esp_timer_create_args_t timer_args = {
        .callback = &fan_off_callback,
        .name = "fan_timer"};
    esp_timer_create(&timer_args, &fan_timer);
    esp_timer_start_once(fan_timer, duration_us);
}
void check_time_and_turn_on_fan(int start_hour, int start_minute, int start_second, int duration_hour, int duration_minute, int duration_second, int fan_speed)
{
    int current_hour, current_minute, current_second;
    get_current_time(Current_Date_Time, &current_hour, &current_minute, &current_second);
    if (start_hour == current_hour && start_minute == current_minute && start_second == current_second && !fan_state)
    {
        fan_control(fan_speed);
        fan_state = true;
        int64_t duration_us = ((int64_t)duration_hour * 3600 + (int64_t)duration_minute * 60 + (int64_t)duration_second) * 1000000;
        start_fan_timer(duration_us);
    }
}

void monitor_and_control_fan_task(void *params)
{
    int *args = (int *)params;
    int start_hour = args[0];
    int start_minute = args[1];
    int start_second = args[2];
    int duration_hour = args[3];
    int duration_minute = args[4];
    int duration_second = args[5];
    int fan_speed = args[6];

    while (1)
    {
        check_time_and_turn_on_fan(start_hour, start_minute, start_second, duration_hour, duration_minute, duration_second, fan_speed);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
void start_fan_monitoring(int start_hour, int start_minute, int start_second, int duration_hour, int duration_minute, int duration_second, int fan_speed)
{
    int *params = malloc(7 * sizeof(int));
    params[0] = start_hour;
    params[1] = start_minute;
    params[2] = start_second;
    params[3] = duration_hour;
    params[4] = duration_minute;
    params[5] = duration_second;
    params[6] = fan_speed;
    xTaskCreate(monitor_and_control_fan_task, "monitor_and_control_fan_task", 4096, params, 5, NULL);
}
void publish_fan_speed_message(int speed)
{
    char fan_speed_msg[10];
    snprintf(fan_speed_msg, sizeof(fan_speed_msg), "%d", speed);
    char publish_msg[50];
    snprintf(publish_msg, sizeof(publish_msg), "{\"Speed\":\"%s\"}", fan_speed_msg);
    int msg_id = esp_mqtt_client_publish(client, fan_topic, publish_msg, 0, 0, 0);
    if (msg_id != -1)
    {
        ESP_LOGI(TAG2, "Published fan speed message to MQTT: %s", fan_speed_msg);
        is_publish_message = true; 
    }
    else
    {
        ESP_LOGE(TAG2, "Failed to publish fan speed message to MQTT");
    }
}
/****************************************************/

/**************MQTT HANDLE DATAS*********************/
static void app_handle_mqtt_data(esp_mqtt_event_handle_t event)
{
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    printf("DATA Length= %d\r\n", event->data_len);

    char *topic = event->topic;
    char *data = event->data;

    if (strncmp(topic, fan_topic, strlen(fan_topic)) == 0)
    {
        data[event->data_len] = 0;
        if (strncmp(data, "{\"Speed\":\"", strlen("{\"Speed\":\"")) == 0)
        {
            int speed = atoi(data + strlen("{\"Speed\":\""));
            fan_control(speed);
        }
        else
        {
            int speed = atoi(data);
            fan_control(speed);
            publish_fan_speed_message(speed);
        }
    }
    else if (strncmp(topic, fan_time_topic, strlen(fan_time_topic)) == 0)
    {
        data[event->data_len] = 0;
        int start_hour, start_minute, start_second, duration_hour, duration_minute, duration_second, fan_speed;
        sscanf(data, "%d:%d:%d %d:%d:%d %d", &start_hour, &start_minute, &start_second, &duration_hour, &duration_minute, &duration_second, &fan_speed);
        start_fan_monitoring(start_hour, start_minute, start_second, duration_hour, duration_minute, duration_second, fan_speed);
    }
}
/****************************************************/

/**************DHT11 FUNCTIONS***********************/
void MQTT_DataJson(void)
{
    for (int i = 0; i < MQTT_sizeoff; i++)
    {
        MQTT_BUFFER[i] = 0;
        Str_ND[i] = 0;
        Str_DA[i] = 0;
    }
    sprintf(Str_ND, "%d", DHT11_read().temperature);
    sprintf(Str_DA, "%d", DHT11_read().humidity);
    strcat(MQTT_BUFFER, "{\"Temperature\":\"");
    strcat(MQTT_BUFFER, Str_ND);
    strcat(MQTT_BUFFER, "\",");
    strcat(MQTT_BUFFER, "\"Humidity\":\"");
    strcat(MQTT_BUFFER, Str_DA);
    strcat(MQTT_BUFFER, "\"}");
}
void delay(uint32_t time)
{
    vTaskDelay(time / portTICK_PERIOD_MS);
}

void read_sensor_task(void *params)
{   
    DHT11_init(DHT11_PIN);
    while (1)
    {
        if(DHT11_read().temperature > 0 && DHT11_read().humidity > 0)
        {
            
            char str[500];
            task_ssd1306_display_clear();
            sprintf (strtemp, "nhiet do: %d", DHT11_read().temperature);
            task_ssd1306_display_text(strtemp, 2, 32);
            sprintf (strhum, "do am: %d", DHT11_read().humidity);
            task_ssd1306_display_text(strhum, 4, 32);
            delay(50);
            MQTT_DataJson();
            esp_mqtt_client_publish(client, dht_topic, MQTT_BUFFER, 0, 1, 0);
            ESP_LOGI(TAG2, "SEND MQTT %s", MQTT_BUFFER);
            vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay for 1 minute
        }
    }
    vTaskDelete(NULL);
}
void ssd1306_init()
{
    esp_err_t espRc;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_ADDR_MODE, true);
    // set lower and upper column register address 0b upper = 0000, lower 0000,
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, 0x10, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
    i2c_master_write_byte(cmd, 0x14, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP_1, true); // reverse left-right mapping
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE_0, true); // reverse up-bottom mapping

    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
    i2c_master_write_byte(cmd, OLED_CMD_DEACTIVE_SCROLL, true); // 2E
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true);  // A6
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);      // AF

    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        ESP_LOGI(TAG3, "OLED configured successfully");
    }
    else
    {
        ESP_LOGE(TAG3, "OLED configuration failed. code: 0x%.2X", espRc);
    }
    i2c_cmd_link_delete(cmd);
    return;
}

void task_ssd1306_display_text(const void *arg_text, uint8_t _page, uint8_t _seg)
{
    char *text = (char *)arg_text;
    uint8_t text_len = strlen(text);

    uint8_t image[8];

    if (task_ssd1306_display_location(_page, _seg) == ESP_OK)
    {
        for (uint8_t i = 0; i < text_len; i++)
        {
            memcpy(image,font8x8_basic_tr[(uint8_t)text[i]],8);
            task_ssd1306_display_image(image, _page, _seg,sizeof(image));
            _seg = _seg + 8;
        }
    }
    return;
}
esp_err_t task_ssd1306_display_location(uint8_t _page, uint8_t _seg)
{
    i2c_cmd_handle_t cmd;

    esp_err_t status = 0;

    uint8_t lowColumnSeg = _seg & 0x0F;
    uint8_t highColumnSeg = (_seg >> 4) & 0x0F;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, 0x00 | lowColumnSeg, true);  // reset column - choose column --> 0
    i2c_master_write_byte(cmd, 0x10 | highColumnSeg, true); // reset line - choose line --> 0
    i2c_master_write_byte(cmd, 0xB0 | _page, true);         // reset page

    i2c_master_stop(cmd);
    
    status = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (status == ESP_OK)
    {
        
        return status;
    }
    else
    {
    }
    i2c_cmd_link_delete(cmd);
    return status;
}
esp_err_t task_ssd1306_display_image(uint8_t *images, uint8_t _page, uint8_t _seg, int _size)
{
    // ESP_LOGI(TAG, "Size : %d" , _size);
    esp_err_t status = 0;

    i2c_cmd_handle_t cmd;
    
    if (task_ssd1306_display_location(_page, _seg) == ESP_OK)
    {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

        i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
        i2c_master_write(cmd, images , _size, true);

        i2c_master_stop(cmd);
        status = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

    }
    return status;
}
void task_ssd1306_display_clear()
{
    esp_err_t status ;

    uint8_t clear[128];
    for (uint8_t i = 0; i < 128; i++)
    {
        clear[i] = 0x00;
    }

    for (uint8_t _page = 0; _page < 8; _page++)
    {
        status = task_ssd1306_display_image(clear, _page , 0x00 , sizeof(clear));  
    }
    return;
}
/****************************************************/
#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipseprojects_io_pem_start[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[] asm("_binary_mqtt_eclipseprojects_io_pem_start");
#endif
extern const uint8_t mqtt_eclipseprojects_io_pem_end[] asm("_binary_mqtt_eclipseprojects_io_pem_end");

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, fan_topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, fan_time_topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        app_handle_mqtt_data(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URL,
            .verification.certificate = (const char *)mqtt_eclipseprojects_io_pem_start
            },
        .credentials = {
            .username = username,
            .authentication.password = password
        },
    };
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}
void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    print_mux = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(i2c_master_init());
    ssd1306_init();
    DHT11_init(DHT11_PIN);
    ESP_ERROR_CHECK(example_connect());
    set_systemtime_sntp();
    mqtt_app_start();
    xTaskCreate(read_sensor_task, "task", 4096, NULL, 5, NULL);
}
