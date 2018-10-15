#include <FreeRTOS.h>
#include <task.h>

#include "platform/usb/usb.h"
#include "ledmate_bridge.h"
#include "platform/gpio.h"
#include "target.h"


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
