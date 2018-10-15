#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>
#include <string.h>

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

#define RX_TASK_STACK_SIZE		128
#define RX_TASK_NAME			"CDCrx"
#define RX_TASK_PRIORITY		1

#define TX_TASK_STACK_SIZE		128
#define TX_TASK_NAME			"CDCtx"
#define TX_TASK_PRIORITY		1

#define WS2812B_TASK_STACK_SIZE		128
#define WS2812B_TASK_NAME			"WS28"
#define WS2812B_TASK_PRIORITY		1

#define QUEUE_LENGTH			10
#define QUEUE_ITEM_SIZE			sizeof(struct usb_rx_queue_item)

#define LED_TIMEOUT_MS			25
#define STATS_TIMEOUT_MS		1000
#define RENDER_TIMEOUT_MS		20 /* 50 FPS */

/* Fun test string: 
</////////////////////////////////////////////////////////////>[##################]
<2345678901234567890123456789012345678901234567890123456789012>[ABCDEFGHIJKLMNOPQR]
*/

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
} SELF;

#define lm_width  144
#define lm_height 8
#define lm_bpp    3
uint8_t lm_buf[lm_width * lm_height * lm_bpp];

int _write(int fd, const char *msg, int len)
{
	static struct usb_rx_queue_item item;

	if ((size_t)len > sizeof(item.data)) {
		platform_force_hardfault();
	}

	item.len = len;
	memcpy(&item.data[0], msg, len);
	xQueueSendToBack(SELF.tx_queue_handle, &item, pdMS_TO_TICKS(100));

	return len;
}

static void rx_task(void *p_arg)
{
	static struct usb_rx_queue_item item;
	err_t r;

	for (;;) {
		r = usb_cdc_rx(&item, portMAX_DELAY);
		if (r != ERR_OK) {
			while (1)
				;
		}

		led_tx_set(true);
		xTimerReset(SELF.tx_led_timer, 0);

		// Handle received data here
		SELF.received_bytes_total += item.len;
		// write(stdout, rx_queue_item.data, rx_queue_item.len);
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

static void ws2812b_task(void)
{
	const uint8_t *buf1 = &lm_buf[0];
	const uint8_t *buf2 = &lm_buf[(lm_width * lm_height / 2) * 3];

	// Blinky
	led_swd_set(SELF.frames_total & 1);

	// Render a frame
	ledmate_render(SELF.frames_total);

	// Push the pixels
	ws2812b_write(buf1, lm_width * lm_height / 2, CONN_11_GPIO_Port, CONN_11_Pin);
	ws2812b_write(buf2, lm_width * lm_height / 2, CONN_12_GPIO_Port, CONN_12_Pin);

	SELF.frames_total++;
}

static void timer_callback(TimerHandle_t timer_handle)
{
	if (timer_handle == SELF.tx_led_timer) {
		led_tx_set(false);
	} else if (timer_handle == SELF.rx_led_timer) {
		led_rx_set(false);
	} else if (timer_handle == SELF.stats_timer) {
		printf("RX: %ld bytes/s\r\nTX: %ld bytes/s\r\nFPS: %ld frames/s\r\n",
			SELF.received_bytes_total - SELF.received_bytes,
			SELF.transmitted_bytes_total - SELF.transmitted_bytes,
			SELF.frames_total - SELF.frames);
		SELF.received_bytes = SELF.received_bytes_total;
		SELF.transmitted_bytes = SELF.transmitted_bytes_total;
		SELF.frames = SELF.frames_total;
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

	SELF.initialized = true;

	return r;
}
