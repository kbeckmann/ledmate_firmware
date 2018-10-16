#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#include "platform/usb/usb.h"
#include "ledmate_bridge.h"
#include "platform/gpio.h"
#include "target.h"

extern uint32_t _Min_Stack_Size;
#define Min_Stack_Size ((size_t)(&_Min_Stack_Size))

int _read(int fd, char *msg, int len)
{
	return 0;
}

int main(void)
{
	err_t r;

	// use this to taint the whole memory before testing
	// memset(0x20000000, 0x11, 0x4000);

	// set 0xfe watermark on the whole non-freertos stack
	memset((void *)(0x20004000 - Min_Stack_Size), 0xfe, Min_Stack_Size);

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
