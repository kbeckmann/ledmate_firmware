#pragma once

#include "stm32f0xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef union {
	struct {
		uint8_t         a;
		uint8_t         r;
		uint8_t         g;
		uint8_t         b;
	} color;
	uint8_t bytes[4];
	uint32_t argb;
} ARGB_t;

extern ARGB_t argb_lut[256];

void ws2812b_write_single(const uint8_t *p_buf, size_t num_elements,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

void ws2812b_write_single_indexed(const uint8_t *p_buf, size_t num_elements,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

void ws2812b_write_dual(const uint8_t *p_buf, size_t num_pixels,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_A, uint16_t GPIO_Pin_B);

void ws2812b_write_dual_lut(const uint8_t *p_buf, size_t num_pixels,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_A, uint16_t GPIO_Pin_B);
