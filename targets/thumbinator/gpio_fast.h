#pragma once

#define GPIO_SET(GPIOx, GPIO_Pin) do {			\
			GPIOx->BSRR = (uint32_t)GPIO_Pin;	\
		} while (0)

#define GPIO_RESET(GPIOx, GPIO_Pin) do {		\
			GPIOx->BRR = (uint32_t)GPIO_Pin;	\
		} while (0)

