#ifndef LED_MANAGER_COLORS_H
#define LED_MANAGER_COLORS_H

#include <stdint.h>

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} rgb_t;

#define RGB8(r8, g8, b8) ((rgb_t){.r = (r8) * 257, .g = (g8) * 257, .b = (b8) * 257})

static const rgb_t LED_COLOR_OFF           = RGB8(  0,   0,   0);
static const rgb_t LED_COLOR_RESET_RED     = RGB8(255,   0,   0);
static const rgb_t LED_COLOR_RESET_ORANGE  = RGB8(255,  40,   0);
static const rgb_t LED_COLOR_SEARCH_BLUE   = RGB8(  0,   0,  75);
static const rgb_t LED_COLOR_JOIN_GREEN    = RGB8(  0, 255,   0);
static const rgb_t LED_COLOR_WHITE_DIM     = RGB8( 75,  75,  75);

#endif // LED_MANAGER_COLORS_H
