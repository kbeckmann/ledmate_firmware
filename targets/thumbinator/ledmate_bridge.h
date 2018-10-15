#pragma once

#include <stdbool.h>

#include "errors.h"

#define ELEDMATE_BRIDGE_NO_INIT			(ELEDMATE_BRIDGE_BASE + 0)
#define ELEDMATE_BRIDGE_INVALID_ARG		(ELEDMATE_BRIDGE_BASE + 1)
#define ELEDMATE_BRIDGE_TASK_CREATE		(ELEDMATE_BRIDGE_BASE + 2)

err_t ledmate_bridge_init(void);
