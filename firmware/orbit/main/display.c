// Status screen on the M5Dial's GC9A01 panel.
// Pins are the ones M5GFX uses for board_M5Dial (m5stack/M5GFX, src/M5GFX.cpp).

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "display.h"
#include "font5x7.h"

#define LCD_HOST SPI2_HOST
#define PIN_MOSI 5
#define PIN_SCLK 6
#define PIN_DC   4
#define PIN_CS   7
#define PIN_RST  8
#define PIN_BL   9

#define LCD_W 240
#define LCD_H 240
#define SCALE 2
#define CELL_W (FONT_W * SCALE + 2)
#define CELL_H (FONT_H * SCALE + 2)
#define TOP_MARGIN ((LCD_H - DISPLAY_ROWS * CELL_H) / 2)

static const char *TAG = "display";
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static uint16_t *band;
static SemaphoreHandle_t band_free;

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(band_free, &woken);
    return woken == pdTRUE;
}

void display_init(void)
{
    band_free = xSemaphoreCreateBinary();
    band = heap_caps_malloc(LCD_W * CELL_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (band_free == NULL || band == NULL) {
        ESP_LOGE(TAG, "out of memory, screen disabled");
        return;
    }

    const spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_W * 40 * sizeof(uint16_t), // LVGL's draw buffer (ui.c) is the larger user
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));
    panel_io = io;

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    // Display Function Control with the source scan reversed, as M5GFX's
    // Panel_GC9A01 sends it. Without it the M5Dial shows text mirrored.
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, 0xB6, (uint8_t[]){0x00, 0x20}, 2));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BL, 1);

    xSemaphoreGive(band_free);
}

esp_lcd_panel_handle_t display_panel(void)
{
    return panel;
}

esp_lcd_panel_io_handle_t display_io(void)
{
    return panel_io;
}

void display_backlight(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

static void draw_band(int y0, int h, const display_line_t *line)
{
    xSemaphoreTake(band_free, portMAX_DELAY);
    memset(band, 0, LCD_W * h * sizeof(uint16_t));
    if (line != NULL) {
        int len = strnlen(line->text, DISPLAY_COLS);
        int x0 = (LCD_W - len * CELL_W) / 2;
        for (int i = 0; i < len; i++) {
            for (int fy = 0; fy < FONT_H; fy++) {
                for (int fx = 0; fx < FONT_W; fx++) {
                    if (!font5x7_pixel(line->text[i], fx, fy)) {
                        continue;
                    }
                    for (int sy = 0; sy < SCALE; sy++) {
                        uint16_t *row = band + (1 + fy * SCALE + sy) * LCD_W;
                        for (int sx = 0; sx < SCALE; sx++) {
                            row[x0 + i * CELL_W + 1 + fx * SCALE + sx] = line->color;
                        }
                    }
                }
            }
        }
    }
    esp_lcd_panel_draw_bitmap(panel, 0, y0, LCD_W, y0 + h, band);
}

void display_show(const display_line_t lines[DISPLAY_ROWS])
{
    if (panel == NULL) {
        return;
    }
    draw_band(0, TOP_MARGIN, NULL);
    for (int r = 0; r < DISPLAY_ROWS; r++) {
        draw_band(TOP_MARGIN + r * CELL_H, CELL_H, &lines[r]);
    }
    int bottom = TOP_MARGIN + DISPLAY_ROWS * CELL_H;
    draw_band(bottom, LCD_H - bottom, NULL);
}
