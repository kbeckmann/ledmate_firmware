#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "ledmate_bridge.h"
#include "drivers/led.h"
#include "drivers/max14662.h"
#include "platform/usb/cdc.h"
#include "platform/platform.h"
#include "gpio_pins.h"
#include "ws2812b.h"

#include "ledmate_renderer.h"

#define MODULE_NAME				ledmate_bridge
#include "macros.h"

#define RX_TASK_STACK_SIZE		512 /* TODO: Reduce me */
#define RX_TASK_NAME			"CDCrx"
#define RX_TASK_PRIORITY		1

#define TX_TASK_STACK_SIZE		128
#define TX_TASK_NAME			"CDCtx"
#define TX_TASK_PRIORITY		1

#define WS2812B_TASK_STACK_SIZE		128
#define WS2812B_TASK_NAME			"WS28"
#define WS2812B_TASK_PRIORITY		1

/* Do we actually need more than 1 item here? */
#define QUEUE_LENGTH			1
#define QUEUE_ITEM_SIZE			sizeof(struct usb_rx_queue_item)
#define RX_BUF_LEN				64

#define LED_TIMEOUT_MS			25
#define STATS_TIMEOUT_MS		1000
#define RENDER_TIMEOUT_MS		10 /* Because we turn off interrupts while pushing pixels, time will be off. */

/* This should be big enough for:
 * - A full framebuffer in LUT mode (144 * 8 * 1BytePerPixel)
 * - A full look up table (256 * 4)
 */
#define BIN_BUF_SIZE			(144 * 8 + 64)

/* Fun test string: 
</////////////////////////////////////////////////////////////>[##################]
<2345678901234567890123456789012345678901234567890123456789012>[ABCDEFGHIJKLMNOPQR]
*/

typedef enum {
	PARSER_STATE_STRING,
	PARSER_STATE_BIN,
} parser_state_t;

typedef enum {
	RENDER_STATE_OFF,
	RENDER_STATE_USE_RENDERER,
	RENDER_STATE_LUT_FRAME_READY,
} render_state_t;

static struct {
	bool initialized;
	StackType_t rx_task_stack[RX_TASK_STACK_SIZE];
	TaskHandle_t rx_task_handle;
	StaticTask_t rx_task_tcb;
	StackType_t tx_task_stack[TX_TASK_STACK_SIZE];
	TaskHandle_t tx_task_handle;
	StaticTask_t tx_task_tcb;
	StackType_t ws2812b_task_stack[WS2812B_TASK_STACK_SIZE];
	TaskHandle_t ws2812b_task_handle;
	StaticTask_t ws2812b_task_tcb;
	StaticQueue_t tx_queue;
	QueueHandle_t tx_queue_handle;
	struct usb_rx_queue_item rx_item;
	uint8_t tx_queue_storage[QUEUE_LENGTH * QUEUE_ITEM_SIZE];
	TimerHandle_t rx_led_timer;
	StaticTimer_t rx_led_timer_storage;
	TimerHandle_t tx_led_timer;
	StaticTimer_t tx_led_timer_storage;
	TimerHandle_t stats_timer;
	StaticTimer_t stats_timer_storage;
	TimerHandle_t render_timer;
	StaticTimer_t render_timer_storage;
	uint32_t frames;
	uint32_t frames_total;
	uint32_t received_bytes;
	uint32_t received_bytes_total;
	uint32_t transmitted_bytes;
	uint32_t transmitted_bytes_total;
	uint32_t stats_counter;
	char rx_buf[RX_BUF_LEN + 1];
	int rx_buf_idx;
	render_state_t render_state;
} SELF;

#define lm_width  144
#define lm_height 8
#define lm_bpp    3
uint8_t lm_buf[lm_width * lm_height * lm_bpp];

/* Holds custom data from USB, e.g. LUT-based framebuffer */
uint8_t bin_buf[BIN_BUF_SIZE];

int _write(int fd, const char *msg, int len)
{
	static struct usb_rx_queue_item item;

	item.len = len > 64 ? 64 : len;
	memcpy(&item.data[0], msg, item.len);
	xQueueSendToBack(SELF.tx_queue_handle, &item, pdMS_TO_TICKS(1000));

	return item.len;
}

static void print_help(void)
{
	printf("Usage:\r\n");
	printf("stats;          Prints statistics\r\n");
	printf("cmd:<command>;  Forwards <command> to the renderer\r\n");
	printf("bin:<size>;     Upload <size> binary bytes to the BINBUF. Must send exactly this amount of bytes later. Max " STR(BIN_BUF_SIZE) " bytes. \r\n");
	printf("update_lut;     Copies binbuf into the look-up-table (256 argb values)\r\n");
	printf("copy_and_blit;  Copies binbuf into the framebuffer and pushes the pixels\r\n");
	printf("blit;           Pushes the pixels\r\n");
}

static void print_stats(void)
{
	printf(
		"RX: %ld bytes/s\r\n"
		"RX total: %ld bytes\r\n"
		"TX: %ld bytes/s\r\n"
		"TX total:  %ld bytes\r\n"
		"FPS: %ld frames/s\r\n"
		"Total Frames: %ld\r\n"
		"Uptime: %ld \"seconds\"\r\n\r\n",
		SELF.received_bytes_total - SELF.received_bytes,
		SELF.received_bytes_total,
		SELF.transmitted_bytes_total - SELF.transmitted_bytes,
		SELF.transmitted_bytes_total,
		SELF.frames_total - SELF.frames,
		SELF.frames_total,
		SELF.stats_counter);
}

static int parse_usb_data(void)
{
	static parser_state_t parser_state;
	static uint32_t bin_buf_idx;
	static uint32_t bin_buf_incoming_bytes;
	int ret = -1;
	int sscanf_items;

	// printf("parser_state=%d\r\nSELF.rx_buf_idx=%d\r\nbin_buf_idx=%ld\r\n", parser_state, SELF.rx_buf_idx, bin_buf_idx);

	if (parser_state == PARSER_STATE_STRING) {
		/* Only parse if there is a terminator */
		char *semicolon_pos = strchr(SELF.rx_buf, ';');
		if (semicolon_pos == NULL) {
			goto finish;
		}

		*semicolon_pos = '\0';
		ret = 0;

		if (strcmp(SELF.rx_buf, "stats") == 0) {
			print_stats();
		} else if (strncmp(SELF.rx_buf, "cmd:", 4) == 0) {
			char *cmd = SELF.rx_buf + 4;
			// printf("COMMAND: [%s]\r\n", cmd);
			ledmate_push_msg(cmd, strlen(cmd));
			SELF.render_state = RENDER_STATE_USE_RENDERER;
		} else if ((sscanf_items = sscanf(SELF.rx_buf, "bin:%lu", &bin_buf_incoming_bytes)) == 1) {
			if (bin_buf_incoming_bytes > BIN_BUF_SIZE) {
				printf("FAIL:Too long;\r\n");
				goto finish;
			}
			printf("MSG:Waiting for %ld binary bytes...;\r\n", bin_buf_incoming_bytes);
			parser_state = PARSER_STATE_BIN;
			bin_buf_idx = 0;
		} else if (strcmp(SELF.rx_buf, "update_lut") == 0) {
			printf("MSG:Copying from BINBUF to LUT;\r\n");
			memcpy(argb_lut, bin_buf, sizeof(argb_lut));
			printf("OK:Copy Done;\r\n");
		} else if (strcmp(SELF.rx_buf, "copy_and_blit") == 0) {
			memcpy(lm_buf, bin_buf, lm_width * lm_height);
			SELF.render_state = RENDER_STATE_LUT_FRAME_READY;
		} else if (strcmp(SELF.rx_buf, "blit") == 0) {
			SELF.render_state = RENDER_STATE_LUT_FRAME_READY;
		} else {
			printf("FAIL:Unknown command [%s];\r\n", SELF.rx_buf);
			print_help();
			// Uncomment to get periodic stats
			// static int foo;
			// if (foo++ % 200 == 0) print_stats();
		}
	} else if (parser_state == PARSER_STATE_BIN) {
		if (bin_buf_idx + SELF.rx_buf_idx > BIN_BUF_SIZE) {
			printf("FAIL:Binbuf overflow;\r\n");
			parser_state = PARSER_STATE_STRING;
			ret = 0;
			goto finish;
		}

		memcpy(&bin_buf[bin_buf_idx], SELF.rx_buf, SELF.rx_buf_idx);
		bin_buf_idx += SELF.rx_buf_idx;
		SELF.rx_buf_idx = 0; /* Reset index because we have copied the data */

		if (bin_buf_idx == bin_buf_incoming_bytes) {
			printf("OK:Binbuf done;\r\n");
			parser_state = PARSER_STATE_STRING;
			ret = 0;
		}
	}

finish:
	return ret;
}

static void rx_task(void *p_arg)
{
	err_t r;

	for (;;) {
		r = usb_cdc_rx(&SELF.rx_item, portMAX_DELAY);
		if (r != ERR_OK) {
			while (1)
				;
		}

		led_tx_set(true);
		xTimerReset(SELF.tx_led_timer, 0);

		/* Handle received data here */
		SELF.received_bytes_total += SELF.rx_item.len;

		/* local echo */
		// write(STDOUT_FILENO, SELF.rx_item.data, SELF.rx_item.len);

		if (SELF.rx_item.len + SELF.rx_buf_idx > RX_BUF_LEN) {
			/* overflow, reset */
			printf("overflow!\r\n");
			memset(SELF.rx_buf, 0, RX_BUF_LEN);
			SELF.rx_buf_idx = 0;
		} else {
			memcpy(&SELF.rx_buf[SELF.rx_buf_idx], SELF.rx_item.data, SELF.rx_item.len);
			SELF.rx_buf_idx += SELF.rx_item.len;
			if (SELF.rx_buf_idx > RX_BUF_LEN) {
				// This should never happen!
				platform_force_hardfault();
			}
			if (parse_usb_data() == 0) {
				/* cmd is completely parsed */
				// printf("parsed!\r\n");
				memset(SELF.rx_buf, 0, RX_BUF_LEN);
				SELF.rx_buf_idx = 0;
			}
		}
	}
}

static void tx_task(void *p_arg)
{
	err_t r;
	struct usb_rx_queue_item item;

	for (;;) {
		/* Wait for stuff to send back to the host over CDC */
		if (xQueueReceive(SELF.tx_queue_handle, &item, portMAX_DELAY) == pdFALSE) {
			continue;
		}

		led_rx_set(true);
		xTimerReset(SELF.rx_led_timer, 0);

		r = usb_cdc_tx(item.data, item.len);
		if (r != ERR_OK && r != EUSB_CDC_NOT_READY)
			while (1)
				;
		SELF.transmitted_bytes_total += item.len;
	}
}

#include "gpio_fast.h"

static void ws2812b_task(void)
{
#if 1
	switch (SELF.render_state) {
	case RENDER_STATE_OFF:
		/* Don't push any pixels */
		break;
	case RENDER_STATE_USE_RENDERER:
		// Render a frame. LED can be probed to profile rendering performance.

		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);

		led_swd_set(true);
		ledmate_render(SELF.frames_total);
		led_swd_set(false);

		// Push out the raw pixels
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		led_rgb_set(1);
		ws2812b_write_dual(lm_buf, lm_width * lm_height,
			CONN_09_GPIO_Port, CONN_09_Pin, CONN_11_Pin);
		led_rgb_set(0);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);

		break;
	case RENDER_STATE_LUT_FRAME_READY:
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		led_rgb_set(1);
		ws2812b_write_dual_lut(lm_buf, lm_width * lm_height,
			CONN_09_GPIO_Port, CONN_09_Pin, CONN_11_Pin);
		led_rgb_set(0);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);
		HAL_GPIO_TogglePin(CONN_12_GPIO_Port, CONN_12_Pin);

		/* Wait for a new frame! */
		SELF.render_state = RENDER_STATE_OFF;

		/* Signal over USB that we want a new frame */
		printf("OK:Give me a new frame;\r\n");

		break;
	}

#else
	uint8_t *buf1 = &lm_buf[0];
	uint8_t *buf2 = &lm_buf[(lm_width * lm_height / 2) * 3];


	// For testing the ws2812b_write_dual implementation...
	// memset(argb_lut, 0b00000000, 256 * 4);
	// memset(lm_buf, 0b00000000, lm_width * lm_height * lm_bpp);

	// memset(argb_lut, 0b11111111, 256 * 4);
	// memset(lm_buf, 0b11111111, lm_width * lm_height * lm_bpp);

	// memset(buf1, 0, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2, 1, lm_width * lm_height * lm_bpp / 2);

	for (int i = 0; i < 128; i++) {
		argb_lut[i +   0].argb = 0x00101010;
		argb_lut[i + 128].argb = 0x00101010; 
	}

	// Force random access
	for (int i = 0; i < lm_width * lm_height / 2; i++) {
		buf1[i] = (rand() % 128);
		buf2[i] = (rand() % 128) + 128;
	}

	// memset(&argb_lut[  0], 0b00000000, 256 * 2);
	// memset(&argb_lut[128], 0b11111111, 256 * 2);
	// memset(buf1,           0b11111111, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2,           0b00000000, lm_width * lm_height * lm_bpp / 2);

	// memset(lm_buf, 0b00000000, lm_width * lm_height * lm_bpp);
	// memset(lm_buf, 0b11111111, lm_width * lm_height * lm_bpp);
	// memset(buf1, 0b00000000, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2, 0b11111111, lm_width * lm_height * lm_bpp / 2);
	// memset(buf1, 0b11111111, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2, 0b00000000, lm_width * lm_height * lm_bpp / 2);
	// memset(buf1, 0b10101010, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2, 0b01010101, lm_width * lm_height * lm_bpp / 2);
	// memset(buf1, 0b01010101, lm_width * lm_height * lm_bpp / 2);
	// memset(buf2, 0b10101010, lm_width * lm_height * lm_bpp / 2);

	// Push the pixels
	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);

	// ws2812b_write_single(buf1, lm_width * lm_height / 2, CONN_11_GPIO_Port, CONN_11_Pin);

	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);

	// ws2812b_write_single(buf2, lm_width * lm_height / 2, CONN_09_GPIO_Port, CONN_09_Pin);

	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);
	// GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);

	// led_rgb_set(1);
	// ws2812b_write_dual(lm_buf, lm_width * lm_height,
	// 	CONN_09_GPIO_Port, CONN_09_Pin, CONN_11_Pin);
	// led_rgb_set(0);

	GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);

	ws2812b_write_dual_lut(lm_buf, lm_width * lm_height,
		CONN_09_GPIO_Port, CONN_09_Pin, CONN_11_Pin);

	GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_SET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);
	GPIO_RESET(CONN_12_GPIO_Port, CONN_12_Pin);

#endif

	SELF.frames_total++;
}

static void timer_callback(TimerHandle_t timer_handle)
{
	if (timer_handle == SELF.tx_led_timer) {
		led_tx_set(false);
	} else if (timer_handle == SELF.rx_led_timer) {
		led_rx_set(false);
	} else if (timer_handle == SELF.stats_timer) {
		/* Just store the stats */
		SELF.received_bytes = SELF.received_bytes_total;
		SELF.transmitted_bytes = SELF.transmitted_bytes_total;
		SELF.frames = SELF.frames_total;
		SELF.stats_counter++;
	} else if (timer_handle == SELF.render_timer) {
		ws2812b_task();
	}
}

err_t ledmate_bridge_init(void)
{
	err_t r = ERR_OK;

	if (SELF.initialized)
		return ERR_OK;

	SELF.rx_task_handle = xTaskCreateStatic(
		rx_task,
		RX_TASK_NAME,
		RX_TASK_STACK_SIZE,
		NULL,
		RX_TASK_PRIORITY,
		&SELF.rx_task_stack[0],
		&SELF.rx_task_tcb);
	if (SELF.rx_task_handle == NULL)
		return ELEDMATE_BRIDGE_TASK_CREATE;

	SELF.tx_task_handle = xTaskCreateStatic(
		tx_task,
		TX_TASK_NAME,
		TX_TASK_STACK_SIZE,
		NULL,
		TX_TASK_PRIORITY,
		&SELF.tx_task_stack[0],
		&SELF.tx_task_tcb);
	if (SELF.tx_task_handle == NULL)
		return ELEDMATE_BRIDGE_TASK_CREATE;

	SELF.tx_queue_handle = xQueueCreateStatic(QUEUE_LENGTH,
		QUEUE_ITEM_SIZE,
		SELF.tx_queue_storage,
		&SELF.tx_queue);

	SELF.tx_led_timer = xTimerCreateStatic(
		"tx_led",
		pdMS_TO_TICKS(LED_TIMEOUT_MS),
		pdFALSE,
		( void * ) 0,
		timer_callback,
		&SELF.tx_led_timer_storage);

	SELF.rx_led_timer = xTimerCreateStatic(
		"rx_led",
		pdMS_TO_TICKS(LED_TIMEOUT_MS),
		pdFALSE,
		( void * ) 0,
		timer_callback,
		&SELF.rx_led_timer_storage);

	SELF.stats_timer = xTimerCreateStatic(
		"stats",
		pdMS_TO_TICKS(STATS_TIMEOUT_MS),
		pdTRUE,
		( void * ) 0,
		timer_callback,
		&SELF.stats_timer_storage);
	xTimerReset(SELF.stats_timer, 0);

	SELF.render_timer = xTimerCreateStatic(
		"render",
		pdMS_TO_TICKS(RENDER_TIMEOUT_MS),
		pdTRUE,
		( void * ) 0,
		timer_callback,
		&SELF.render_timer_storage);
	xTimerReset(SELF.render_timer, 0);

	ledmate_init(lm_buf, lm_width, lm_height);
	const char foo[] = "\x04\x00\x00\x00" "welcome to 35c3";
	ledmate_push_msg(foo, sizeof(foo));

	SELF.render_state = RENDER_STATE_USE_RENDERER;

	SELF.initialized = true;

	return r;
}
