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
		const uint32_t bitmask = (1 << (7 - (i & 0x07)));
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

/* Pins must be on the same PORT */
#pragma GCC optimize ("-O3")
void ws2812b_write_dual(const uint8_t *p_buf, size_t num_pixels,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_A, uint16_t GPIO_Pin_B)
{
	uint8_t b1, b2;
	uint32_t i = 0;
	const uint32_t offset = (num_pixels * 3) / 2;
	const uint32_t num_bits = num_pixels * (8 * 3) / 2;

	__disable_irq();

	while(i < num_bits) {
		const uint32_t index   = i / 8; // bits to bytes
		const uint8_t current1 = p_buf[index];
		const uint8_t current2 = p_buf[index + offset];
		const uint8_t bitmask = (1 << (7 - (i & 0x07)));

		const uint8_t bit1    = current1 & bitmask;
		b1                    = bit1;

		const uint8_t bit2     = current2 & bitmask;
		b2                     = bit2;

		i++;

		GPIO_SET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
		if (b1 && b2) {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop;");
		} else if (b1 && !b2) {
			__asm("nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; nop; nop;");
		} else if (!b1 && b2) {
			__asm("nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("nop; nop; nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; nop; nop;");
		} else {
			__asm("nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; nop; nop; nop; nop; nop; nop;"
				  "nop; nop; ");
		}
	}
	__enable_irq();

	/* Need to guarantee that you don't call this function too fast.
	 * The LEDs need some time between "packets". */
//	HAL_Delay(1);
}
