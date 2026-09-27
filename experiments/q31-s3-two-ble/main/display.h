#pragma once

#include <stdint.h>

// 12 x 16 px cells (5x7 font drawn at 2x) on the 240 x 240 round panel.
#define DISPLAY_COLS 20
#define DISPLAY_ROWS 13

// RGB565, byte-swapped for the panel's big-endian SPI order.
#define DISPLAY_RGB(r, g, b) \
    ((uint16_t)(((((r) & 0xF8) | ((g) >> 5)) & 0xFF) | ((((g) & 0x1C) << 3 | ((b) >> 3)) << 8)))

typedef struct {
    char text[DISPLAY_COLS + 1];
    uint16_t color;
} display_line_t;

void display_init(void);

// Redraws the whole screen. Each line is centred because the panel is round.
void display_show(const display_line_t lines[DISPLAY_ROWS]);
