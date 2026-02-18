#ifndef LED_MANAGER_COLORS_H
#define LED_MANAGER_COLORS_H

#include <stdint.h>

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} rgb_t;

#define RGB8(r8, g8, b8) ((rgb_t){.r = (r8) * 257, .g = (g8) * 257, .b = (b8) * 257})

// Colors matching old ws2812.h
static const rgb_t LED_COLOR_COLD_WHITE  = RGB8(0xFF, 0xFF, 0xFF);
// "black" - not fully off, slight green glow shows board is powered
static const rgb_t LED_COLOR_BLACK       = RGB8(0x00, 0x02, 0x00);
static const rgb_t LED_COLOR_RED         = RGB8(0xFF, 0x00, 0x00);
static const rgb_t LED_COLOR_YELLOW      = RGB8(0xFF, 0x80, 0x00);

#endif // LED_MANAGER_COLORS_H
