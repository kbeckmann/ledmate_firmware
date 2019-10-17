#pragma once

#include <stdint.h>
#include <stdbool.h>

void usb_prog_handle_rx(uint8_t *p_buf, uint32_t len);
void usb_prog_process_events(void);
void usb_prog_init(void);
