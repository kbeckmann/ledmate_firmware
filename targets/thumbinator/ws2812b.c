#include "ws2812b.h"

#include "gpio_fast.h"

#define ARRAY_SIZE(x) sizeof(x)/sizeof((x)[0])

#pragma GCC optimize ("-O3")
void ws2812b_write(const uint8_t *p_buf, size_t num_pixels, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	uint32_t p;
	uint32_t i = 0;

	__disable_irq();

	num_pixels *= 8 * 3;
	while(i < num_pixels) {
		const uint32_t index   = i >> 3; // aka i/8
		const uint32_t current = p_buf[index];
		const uint32_t bitmask = (1 << (8 - (i & 0x07)));
		const uint32_t bit     = current & bitmask;
		p                      = bit;
		i++;

		GPIO_SET(GPIOx, GPIO_Pin);
		if (p) {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; "
					"nop; nop; nop; nop; nop; ");
		} else {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; "
					"nop;  nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin);
			 __asm("nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop; nop; nop; nop; nop; nop; nop;"
					"nop; nop; nop;");
		}
	}
	__enable_irq();

	/* Need to guarantee that you don't call this function too fast.
	 * The LEDs need some time between "packets". */
//	HAL_Delay(1);
}
