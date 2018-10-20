#pragma once

#include "stm32f0xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

void ws2812b_write(const uint8_t *p_buf, size_t num_elements,
    GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

void ws2812b_write_dual(const uint8_t *p_buf, size_t num_pixels,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_A, uint16_t GPIO_Pin_B);
