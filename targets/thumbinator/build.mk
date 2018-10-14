PLATFORM := stm32f0xx

TARGET_SRCS += \
	targets/$(TARGET)/target.c \
	targets/$(TARGET)/led.c \
	targets/$(TARGET)/ledmate_bridge.c \
