#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/max14662.h"
#include "serial_prog.h"
#include "platform/gpio.h"
#include "stm32l4xx_hal.h"

enum programmer_state
{
	STATE_INITIAL,
	STATE_READ_COMMAND,
	STATE_PROGRAM_GET_SIZE,
	STATE_PROGRAM_GET_PROGRAM,
};

#define BUF_SIZE 512

static struct
{
	enum programmer_state state;

	uint8_t buf[BUF_SIZE];
	uint32_t buf_cursor;
	uint32_t buf_end;

	uint32_t program_index;
	uint32_t program_size;
} me;

#if 1
#define SPI_DELAY()                 \
	do {                            \
		volatile x;                 \
		for (x = 0; x < 1; x++);   \
	} while(0)
#elif 0
#define SPI_DELAY() HAL_Delay(1)
#else
#define SPI_DELAY()
#endif

static void print_help(void)
{
	printf("Epic ice40 flasher:\n");
	printf("  P - Programming mode. Enter size in decimal, \\r, then raw binary data.\n");
	printf("  W - Write SPI data. Enter an even number of hex chars and end with \\r.\n");
	printf("  R - Read SPI data (specify length)\n");
	printf("  U - Enters uart passthrough mode\n");
}

void usb_prog_init(void)
{
	err_t r = ERR_OK;
	me.buf_end = 0;
	me.buf_cursor = 0;
}

void usb_prog_handle_rx(uint8_t *p_buf, uint32_t len)
{
	if (me.buf_end + len < sizeof(me.buf))
	{
		memcpy(&me.buf[me.buf_end], p_buf, len);
		me.buf_end += len;
		// printf("{c=%lu, e=%lu, l=%lu}\n", me.buf_cursor, me.buf_end, len);
	}
	else
	{
		printf("BUFFER OVERFLOW: %lu %lu\n", me.buf_end, len);
		usb_prog_init();
	}
}

static void write_raw(char *buf, int len) {
	for (int i = 0; i < len; i++) {
		for (int j = 0; j < 8; j++) {
			char bit = buf[i] & (1 << (7 - j));
			gpio_write(GPIOB, GPIO_PIN_6, false);
			SPI_DELAY();

			gpio_write(GPIOB, GPIO_PIN_7, bit != 0);
			SPI_DELAY();

			gpio_write(GPIOB, GPIO_PIN_6, true);
			SPI_DELAY();
		}
	}
}

void usb_prog_process_events(void)
{
	int need_more = 0;
	uint32_t consumed;

	// printf("!");
	while (!need_more && me.buf_cursor < me.buf_end) {
		// Find the first \r within buf[cursor:end]
		uint8_t *newline = memchr(me.buf + me.buf_cursor, '\r', me.buf_end - me.buf_cursor);

		switch (me.state)
		{
		case STATE_INITIAL:
			// usb_prog_init();
			me.state = STATE_READ_COMMAND;
		break;
		case STATE_READ_COMMAND:
			// printf("Handling: << %02x >> [%lu]\n", me.buf[me.buf_cursor], me.buf_end);

			if (me.buf[me.buf_cursor] == '\r') {
				print_help();
				usb_prog_init();
			}
			else if (me.buf[me.buf_cursor] == 'P') {
				printf("Entering programming mode. Write the size of the payload, then \\r, then the binary data.\n");
				me.state = STATE_PROGRAM_GET_SIZE;
				me.buf_cursor += 1;
			}
			else {
				printf("Unknown command: %s\n", me.buf);
				print_help();
				usb_prog_init();
			}
			break;
		case STATE_PROGRAM_GET_SIZE:
			me.buf[me.buf_end] = 0;
			// printf("Get size.. %lu %lu [%s]\n", me.buf_end, me.buf_cursor, me.buf + me.buf_cursor);
			if (newline) {
				me.program_size = atoi((char*)(me.buf + me.buf_cursor));
				me.program_index = 0;
				printf("Reading %lu bytes.\n", me.program_size);
				me.state = STATE_PROGRAM_GET_PROGRAM;
				me.buf_cursor += newline - me.buf;

				char tmp[] = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xBD\xB3";
				write_raw(tmp, sizeof(tmp));
			} else {
				need_more = 1;
			}
			break;
		case STATE_PROGRAM_GET_PROGRAM:
			consumed = me.buf_end - me.buf_cursor;
			// printf("Getting bytes.. %lu[%02x] (%lu/%lu)\n", consumed, me.buf[me.buf_cursor], me.program_index, me.program_size);

			write_raw(&me.buf[me.buf_cursor], consumed);

			me.program_index += consumed;
			me.buf_cursor += consumed;
			if (me.program_index >= me.program_size) {
				printf("done.\n");
				me.state = STATE_INITIAL;
			}
			break;
		}
	}

	if (!need_more) {
		me.buf_cursor = 0;
		me.buf_end = 0;
	}
}
