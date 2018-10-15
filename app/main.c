#include <FreeRTOS.h>
#include <string.h>
#include <task.h>

#include "cdc_uart_bridge.h"
#include "drivers/led.h"
#include "drivers/max14662.h"
#include "drivers/mcp4018t.h"
#include "platform/uart.h"
#include "platform/usb/usb.h"
#if (FEAT_POWER_PROFILER == 1)
#include "platform/usb/cdc.h"
#include "platform/usb/hid.h"
#include "platform/adc.h"
#include "platform/i2c.h"
#include "power.h"
#endif
#include "platform/gpio.h"
#include "target.h"

#include "ledmate_bridge.h"

int _read(int fd, char *msg, int len)
{
	return 0;
}

int main(void)
{
	err_t r;

	r = target_init();
	ERR_CHECK(r);

	gpio_init();

	r = usb_init();
	ERR_CHECK(r);

	r = ledmate_bridge_init();
	ERR_CHECK(r);

	vTaskStartScheduler();

	while (1) ;

	return 0;
}
