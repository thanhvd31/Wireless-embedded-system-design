/**
 * @file ic74hc595.h
 * @author Jaime Albuquerque (jaime.albq@gmail.com)
 * @brief Output shifter register library
 * @version 0.1
 * @date 2022-09-27
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _74HC595_H_
#define _74HC595_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
// #define IDF_VER

#if defined(IDF_VER) // Settings for ESP-IDF

#include "driver/gpio.h"
#include "esp32/rom/ets_sys.h"

#define SETPIN(PIN)     gpio_set_level(PIN, 1)
#define CLRPIN(PIN)     gpio_set_level(PIN, 0)
#define _DELAY_US(x)    ets_delay_us(x)
#define _DELAY_MS(x)    ets_delay_us(x*1000*1000)

#endif // IDF_VER

typedef struct {
	uint8_t num_reg;	// Number of shift registers
	uint8_t *reg_value;	// Last value of all registers

	// GPIO
	struct pin {
		
#if defined(IDF_VER)
		gpio_num_t clk;
		gpio_num_t signal;
		gpio_num_t latch;
#else // IDF_VER
		uint8_t clk;
		uint8_t signal;
		uint8_t latch;
#endif // Generic

	} pin;
} shift_reg_config_t;

/**
 * @brief Initialize the microcontroller to do the output
 * 
 * @param shft 
 * @return < 0 means error
 */
int8_t ic74hc595_init(shift_reg_config_t *shft);

/**
 * @brief
 * 
 * @param shft 
 * @return int8_t 
 */
int8_t ic74hc595_deinit(shift_reg_config_t *shft);

/**
 * @brief Send the whole data
 * 
 * @param data 
 * @param len 
 * @param shft 
 * @return -1 = data longer than number of registers; 1 = successfully sent
 */
int8_t ic74hc595_send(uint8_t *data, uint8_t len, shift_reg_config_t *shft);

/**
 * @brief Send 1 byte at a time
 * 
 * @param data 
 * @param shft 
 * @return int8_t 
 */
int8_t ic74hc595_send8bits(uint8_t data, shift_reg_config_t *shft);

/**
 * @brief Latch the registers
 * 
 * @param shft 
 * @return int8_t 
 */
int8_t ic74hc595_latch(shift_reg_config_t *shft);

/**
 * @brief Latch the registers
 * 
 * @param data,
 * @param shft 
 * @return int8_t 
 */
int8_t ic74hc595_set8bit(uint8_t data , shift_reg_config_t *shft);

#endif // _74HC595_H_