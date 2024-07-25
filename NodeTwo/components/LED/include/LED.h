#ifndef LED_H
#define LED_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BUTTON_NUM_STATE_ID_0 GPIO_NUM_19
#define BUTTON_NUM_MODE_ID_0 GPIO_NUM_21

#define BUTTON_NUM_STATE_ID_1 GPIO_NUM_20
#define BUTTON_NUM_MODE_ID_1 GPIO_NUM_26

#define NUMBER_OF_LED 3
#define NUMBER_OF_BIT 8
#define NUMBER_OF_MODE 3

#define MODE_1 0x07
#define MODE_2 0x05
#define MODE_3 0x01

#define OFF 0x00
#define ON 0xFF

#define MASK_LED_BEDROOM 0b00000111
#define MASK_LED_LIVINGROOM 0b00111000

#define ESP_INTR_FLAG_DEFAULT 0
// set up các chân GPIO điều khiển STATE tương ứng với các id
#define BUTTON_NUM_SEL ((1ULL << BUTTON_NUM_STATE_ID_0) | (1ULL << BUTTON_NUM_MODE_ID_0) | (1ULL << BUTTON_NUM_STATE_ID_1) | (1ULL << BUTTON_NUM_MODE_ID_1))

typedef enum
{
    led_clk = GPIO_NUM_15,
    led_latch = GPIO_NUM_2,
    led_signal = GPIO_NUM_16,
} led_controll_e;

typedef enum
{
    BED_ROOM,
    LIVING_ROOM,

    NUMBER_OF_ID
} ID;

typedef struct
{
    uint8_t state;
    uint8_t mode;
} led_handler_t;

typedef struct
{
    uint8_t mode;
    uint8_t state;
} button_num_t;

extern uint8_t mode[NUMBER_OF_MODE];

void led_init();

void turn_off_led(uint8_t ID);

void turn_on_led(uint8_t ID);

void set_mode(uint8_t ID, uint8_t MODE);

#endif // LED_HS