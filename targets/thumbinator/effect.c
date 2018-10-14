#include "effect.h"

#include <stdbool.h>
#include <stdint.h>

#include "ws2812b.h"
#include "hsv.h"
#include "gpio_pins.h"

#define NUM_LEDS 97
//#define V_MAX 127
#define V_MAX 65

static int current_effect;
static uint8_t g_pixels[2][NUM_LEDS * 3];
typedef void (*effect_render_cb)(uint32_t index, uint32_t t, uint8_t * p_pixels, uint32_t num_bytes);

void render_colors(uint32_t index, uint32_t t, uint8_t * p_pixels, uint32_t num_bytes)
{
    uint8_t h, s, v;

    for (uint32_t i = 0; i < num_bytes; i += 3) {
        uint8_t r, g, b;
        h = i % 255 + t;
        s = 255;
        v = V_MAX;

        hsvtorgb(&r, &g, &b, h, s, v);
        p_pixels[i]     = g;
        p_pixels[i + 1] = r;
        p_pixels[i + 2] = b;
    }
}

void render_constant(uint32_t index, uint32_t t, uint8_t * p_pixels, uint32_t num_bytes)
{
    uint8_t h, s, v;
    uint8_t r, g, b;

    h = t/3;
    s = 255;
    v = V_MAX;
    hsvtorgb(&r, &g, &b, h, s, v);

    for (uint32_t i = 0; i < num_bytes; i += 3) {
        p_pixels[i]     = g;
        p_pixels[i + 1] = r;
        p_pixels[i + 2] = b;
    }
}

void render_car(uint32_t index, uint32_t t, uint8_t * p_pixels, uint32_t num_bytes)
{
    uint32_t middle = (num_bytes / 2)+3;
    for (uint32_t i = 0; i < num_bytes; i += 3) {

        if(i > middle - 6*3 && i < middle + 6*3){
            p_pixels[i]     = 100;
            p_pixels[i + 1] = 100;
            p_pixels[i + 2] = 100;
            continue;
        }


        if(i < 10*3 || i > num_bytes - 10*3 ){
            p_pixels[i]     = 0;
            p_pixels[i + 1] = 100;
            p_pixels[i + 2] = 0;
            continue;
        }


        p_pixels[i]     = 0;
        p_pixels[i + 1] = 0;
        p_pixels[i + 2] = 0;
    }

}

void render_police(uint32_t index, uint32_t t, uint8_t * p_pixels, uint32_t num_bytes)
{
    uint32_t middle = (num_bytes / 2)+3;
    for (uint32_t i = 0; i < num_bytes; i += 3) {
        if((i < middle && index%2==0) || (i > middle && index%2==1)){
            p_pixels[i]     = 0;
            p_pixels[i + 1] = t%20 < 10 ? 100 : 0;
            p_pixels[i + 2] = 0;
        }else{
            p_pixels[i]     = 0;
            p_pixels[i + 1] = 0;
            p_pixels[i + 2] =  t%20 > 10 ? 100 : 0;;
        }

    }
}


static effect_render_cb effects[] = {
  render_colors,
  render_constant,
  //render_police,
};

static int effect_count = sizeof(effects)/sizeof(effects[0]);

void effect_next(void)
{
    current_effect = (current_effect + 1) % effect_count;
}

void effect_show(uint32_t t)
{
    effects[current_effect](0, t, g_pixels[0], sizeof(g_pixels[0]));
    effects[current_effect](1, t, g_pixels[1], sizeof(g_pixels[1]));

    ws2812b_write(g_pixels[0], sizeof(g_pixels[0]), CONN_11_GPIO_Port, CONN_11_Pin);
    ws2812b_write(g_pixels[1], sizeof(g_pixels[1]), CONN_12_GPIO_Port, CONN_12_Pin);
}
