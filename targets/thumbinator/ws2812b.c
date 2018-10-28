#include "ws2812b.h"

#include "gpio_fast.h"

#define ARRAY_SIZE(x) sizeof(x)/sizeof((x)[0])

ARGB_t argb_lut[256] = {0};

#pragma GCC optimize ("-O3")
void ws2812b_write_single(const uint8_t *p_buf, size_t num_pixels, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
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

#pragma GCC optimize ("-O3")
void ws2812b_write_single_lut(const uint8_t *p_buf, size_t num_pixels, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	uint32_t current_bit = 23;

	__disable_irq();

	const uint32_t num_bits = num_pixels * 8 * 3;
	uint32_t index = 0;
	uint32_t i = 0;

	while(i < num_bits) {
		const uint32_t bitmask     = (1 << current_bit);
		const uint32_t p           = argb_lut[p_buf[index]].argb & bitmask;

		i++;
		if (current_bit == 0) {
			current_bit = 23;
			index++;
		} else {
			// constant time...? seems this is good enough
			current_bit--;
		}

		// Then turn on the gpio.. This is so nasty but works
		GPIO_SET(GPIOx, GPIO_Pin);

		if (p) {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop;");
		} else {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop;");
		}
	}
	__enable_irq();
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
			// 750 ns
			// 1250 ns total
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
			__asm("nop; nop;");
		} else if (b1 && !b2) {
			// 375 ns
			// 750 ns
			// 1250ns total
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("nop; nop;");
		} else if (!b1 && b2) {
			// 375 ns
			// 750 ns
			// 1250ns total
			__asm("nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop;");
		} else {
			// 375 ns
			// 1250 ns total
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
}

/* Pins must be on the same PORT */
#pragma GCC optimize ("-O3")
void ws2812b_write_dual_lut(const uint8_t *p_buf, size_t num_pixels,
	GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin_A, uint16_t GPIO_Pin_B)
{
	__disable_irq();

	const uint32_t offset = (num_pixels * 3) / 2;
	const uint32_t num_bits = num_pixels * (8 * 3) / 2;
	uint32_t index1 = 0;
	uint32_t index2 = offset;
	uint32_t i = 0;
	uint32_t bitmask = (1 << 23);

	while(i < num_bits) {
		const uint32_t bit1 = argb_lut[p_buf[index1]].argb & bitmask;
		// Then turn on the gpio.. This is so nasty but works
		GPIO_SET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);

		const uint32_t bit2 = argb_lut[p_buf[index2]].argb & bitmask;

		if (bit1 && bit2) {
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
			__asm("");
		} else if (bit1 && !bit2) {
			// This one is weird! Why does this take longer than all others?
			__asm(" ");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("");
		} else if (!bit1 && bit2) {
			__asm("nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_A);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop; ");
			GPIO_RESET(GPIOx, GPIO_Pin_B);
			__asm("");
		} else {
			__asm("");
			GPIO_RESET(GPIOx, GPIO_Pin_A | GPIO_Pin_B);
			__asm("nop; nop; nop; nop; nop; nop; nop; nop;");
			__asm("nop; nop; nop; nop; nop; nop;");
		}

		// This code is moved down here for timing reasons...
		i++;
		if (bitmask == 0) {
			bitmask = (1 << 23);
			index1++;
			index2++;
		} else {
			// constant time...? seems this is good enough
			bitmask >>= 1;
		}

	}
	__enable_irq();
}