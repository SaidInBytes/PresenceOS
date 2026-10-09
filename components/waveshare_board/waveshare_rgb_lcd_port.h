#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"

#define PRESENCE_LCD_H_RES 1024
#define PRESENCE_LCD_V_RES 600
#define PRESENCE_LCD_PIXEL_CLOCK_HZ (21 * 1000 * 1000)

esp_err_t presence_board_display_init(esp_lcd_panel_handle_t *panel_handle,
                                      esp_lcd_touch_handle_t *touch_handle);
esp_err_t presence_board_backlight_on(void);
