#pragma once

#include "stm32f0xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

void ws2812b_write(uint8_t *p_buf, size_t num_elements, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
