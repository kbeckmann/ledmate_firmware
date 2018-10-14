PLATFORM := stm32f0xx

TARGET_SRCS += \
	targets/$(TARGET)/target.c \
	targets/$(TARGET)/led.c \
	targets/$(TARGET)/ledmate_bridge.c \
	targets/$(TARGET)/hsv.c \
	targets/$(TARGET)/ws2812b.c \
	targets/$(TARGET)/effect.c \

