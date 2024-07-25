/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "LED.h"
#include "74hc595.h"

// const static char* GPIO_BUTTON_TAG = "[Button]";

uint8_t mode[NUMBER_OF_MODE] = {MODE_1, MODE_2, MODE_3};
uint8_t mode_index = 0;
uint8_t mask[NUMBER_OF_ID] = {MASK_LED_BEDROOM, MASK_LED_LIVINGROOM};
led_handler_t led[NUMBER_OF_ID];

static const char *TAG = "SETMODE";

static shift_reg_config_t sft_config = {
    .num_reg = 1,
    .reg_value = 0,
    .pin = {
        .clk = led_clk,
        .latch = led_latch,
        .signal = led_signal}};

// Khai báo mảng danh sách các chân GPIO
static button_num_t button[NUMBER_OF_ID] =
    {
        [BED_ROOM] = {
            .mode = BUTTON_NUM_MODE_ID_0,
            .state = BUTTON_NUM_STATE_ID_0,
        },

        [LIVING_ROOM] = {
            .mode = BUTTON_NUM_MODE_ID_1,
            .state = BUTTON_NUM_STATE_ID_1,
        }};

static uint8_t get_id(uint8_t *button_num, button_num_t *button)
{
    uint8_t ID = BED_ROOM; // Giá trị mặc định cho trường hợp không tìm thấy ID
    // Kiểm tra từng gpio_handle để tìm gpio_num tương ứng
    for (uint8_t id = 0; id < NUMBER_OF_ID; id++)
    {
        // Kiểm tra gpio_num của button_num bởi vì chỉ có button mới tạo ra ngắt
        if (button[id].mode == *button_num || button[id].state == *button_num)
        {
            return id;
        }
    }
    return ID;
};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint8_t *button_pin = (uint8_t *)arg;    // Ep kieu dung cach
    uint8_t ID = get_id(button_pin, button); // Truyen tham so dung cach

    if (*button_pin == BUTTON_NUM_STATE_ID_0 || *button_pin == BUTTON_NUM_STATE_ID_1)
    {
        // bật / tắt đèn
        if (led[ID].mode == OFF)
        {
            set_mode(ID, ON);
        }
        else
            set_mode(ID, OFF);
    }
    // chuyển chế độ đèn
    else if (*button_pin == BUTTON_NUM_MODE_ID_1 || *button_pin == BUTTON_NUM_MODE_ID_0)
    {
        mode_index = (mode_index + 1) % 3;
        set_mode(ID, mode[mode_index]);
    }
}

static void gpio_num_config()
{
    // esp_err_t err;
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};

    //------- Config Button ------
    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = BUTTON_NUM_SEL;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable pull-up mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(GPIO_BUTTON_TAG, "Button pin config error!!");
    // }
    // else
    //     ESP_LOGI(GPIO_BUTTON_TAG, "Button pin config success");

    // quản lí các GPIO của các LED tương ứng với ID bedroom và ID kitchen.
    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_NUM_STATE_ID_0, gpio_isr_handler, (void *)BUTTON_NUM_STATE_ID_0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_NUM_MODE_ID_0, gpio_isr_handler, (void *)BUTTON_NUM_MODE_ID_0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_NUM_STATE_ID_1, gpio_isr_handler, (void *)BUTTON_NUM_STATE_ID_1);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_NUM_MODE_ID_1, gpio_isr_handler, (void *)BUTTON_NUM_MODE_ID_1);
}

static uint8_t get_data_controll(uint8_t ID, uint8_t MODE)
{
    uint8_t bit_controll = 0x00;
    if (MODE == ON)
    {
        if (led[ID].mode == OFF)
        {
            led[ID].mode = mask[ID] & (MODE << (ID * NUMBER_OF_LED));
        }
    }
    else
        led[ID].mode = mask[ID] & (MODE << (ID * NUMBER_OF_LED));
    for (int ID = 0; ID < NUMBER_OF_ID; ID++)
    {
        bit_controll |= led[ID].mode;
    }
    return bit_controll;
}

void turn_on_led(uint8_t ID)
{
    set_mode(ID, ON);
}

void turn_off_led(uint8_t ID)
{
    set_mode(ID, OFF);
}

void set_mode(uint8_t ID, uint8_t MODE)
{
    uint8_t bit_controll = get_data_controll(ID, MODE);
    ESP_LOGI(TAG, "%d", bit_controll);
    ic74hc595_set8bit(bit_controll, &sft_config);
    // _DELAY_US(1);
}

void led_init(void)
{
    gpio_num_config();
    ic74hc595_init(&sft_config);
}
