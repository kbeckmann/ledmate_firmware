#pragma once

#include "stm32l4xx_hal.h"

struct cdc_init_data {
	USBD_HandleTypeDef *p_usbd;
	PCD_HandleTypeDef *p_pcd;
};