#include "waveshare_rgb_lcd_port.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_rom_sys.h"

#define I2C_SDA_GPIO 8
#define I2C_SCL_GPIO 9
#define TOUCH_RESET_GPIO GPIO_NUM_4

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t ch422g_mode_device;
static i2c_master_dev_handle_t ch422g_output_device;

static esp_err_t i2c_init_once(void)
{
    if (i2c_bus != NULL) return ESP_OK;
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&config, &i2c_bus);
}

static esp_err_t ch422g_write(uint8_t address, uint8_t value)
{
    i2c_master_dev_handle_t *device = address == 0x24 ? &ch422g_mode_device : &ch422g_output_device;
    if (*device == NULL) {
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 400000,
        };
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &config, device), "board", "Add CH422G device");
    }
    return i2c_master_transmit(*device, &value, 1, 1000);
}

static esp_err_t reset_touch(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << TOUCH_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), "board", "Configure touch reset");
    ESP_RETURN_ON_ERROR(ch422g_write(0x24, 0x01), "board", "Configure CH422G");
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x2C), "board", "Set touch reset");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 0), "board", "Pulse touch reset");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x2E), "board", "Release touch reset");
    esp_rom_delay_us(200000);
    return ESP_OK;
}

esp_err_t presence_board_backlight_on(void)
{
    ESP_RETURN_ON_ERROR(i2c_init_once(), "board", "Initialize I2C");
    ESP_RETURN_ON_ERROR(ch422g_write(0x24, 0x01), "board", "Enable CH422G outputs");
    return ch422g_write(0x38, 0x1E);
}

esp_err_t presence_board_display_init(esp_lcd_panel_handle_t *panel_handle,
                                      esp_lcd_touch_handle_t *touch_handle)
{
    if (panel_handle == NULL || touch_handle == NULL) return ESP_ERR_INVALID_ARG;
    *panel_handle = NULL;
    *touch_handle = NULL;
    const esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = PRESENCE_LCD_PIXEL_CLOCK_HZ,
            .h_res = PRESENCE_LCD_H_RES,
            .v_res = PRESENCE_LCD_V_RES,
            .hsync_back_porch = 145,
            .hsync_front_porch = 170,
            .hsync_pulse_width = 30,
            .vsync_back_porch = 23,
            .vsync_front_porch = 12,
            .vsync_pulse_width = 2,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .bounce_buffer_size_px = PRESENCE_LCD_H_RES * 10,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17,
            GPIO_NUM_10, GPIO_NUM_39, GPIO_NUM_0, GPIO_NUM_45,
            GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_1,
            GPIO_NUM_2, GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,
        },
        .flags = {
            .fb_in_psram = true,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, panel_handle), "board", "Create RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel_handle), "board", "Initialize RGB panel");
    ESP_RETURN_ON_ERROR(i2c_init_once(), "board", "Initialize I2C");
    ESP_RETURN_ON_ERROR(presence_board_backlight_on(), "board", "Enable backlight");

    esp_err_t touch_result = reset_touch();
    if (touch_result != ESP_OK) {
        ESP_LOGE("board", "GT911 reset failed: %s", esp_err_to_name(touch_result));
        return ESP_OK;
    }

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz = 0;
    touch_result = esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &touch_io);
    if (touch_result != ESP_OK) {
        ESP_LOGE("board", "GT911 I2C setup failed: %s", esp_err_to_name(touch_result));
        return ESP_OK;
    }
    const esp_lcd_touch_config_t touch_config = {
        .x_max = PRESENCE_LCD_H_RES,
        .y_max = PRESENCE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
    };
    touch_result = esp_lcd_touch_new_i2c_gt911(touch_io, &touch_config, touch_handle);
    if (touch_result != ESP_OK) {
        ESP_LOGE("board", "GT911 initialization failed: %s", esp_err_to_name(touch_result));
        *touch_handle = NULL;
    }
    return ESP_OK;
}
