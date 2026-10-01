#pragma once

#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

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
// The panel and its IO, for LVGL (ui.c) to take over. NULL before init or on failure.
esp_lcd_panel_handle_t display_panel(void);
esp_lcd_panel_io_handle_t display_io(void);

// Redraws the whole screen. Each line is centred because the panel is round.
void display_show(const display_line_t lines[DISPLAY_ROWS]);

#ifdef __cplusplus
}
#endif
