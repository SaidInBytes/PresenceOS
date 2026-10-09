#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "waveshare_rgb_lcd_port.h"

static const char *statuses[] = {"In office", "Remote", "Away", "In a meeting", "Unavailable"};
static const char *names[] = {"Alex Morgan", "Jamie Chen", "Riley Patel", "Sam Rivera"};
static uint8_t selected[4];
static lv_obj_t *count_label;
static lv_obj_t *status_labels[4];
static esp_lcd_panel_handle_t lcd_panel;
static esp_lcd_touch_handle_t touch_panel;

static void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    esp_lcd_panel_draw_bitmap(lcd_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
    lv_display_flush_ready(display);
}

static void touch_read(lv_indev_t *input, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = lv_indev_get_user_data(input);
    esp_lcd_touch_point_data_t point = {0};
    uint8_t point_count = 0;
    esp_lcd_touch_read_data(touch);
    esp_lcd_touch_get_data(touch, &point, &point_count, 1);
    if (point_count > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point.x;
        data->point.y = point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick(void *argument)
{
    (void)argument;
    lv_tick_inc(2);
}

static void update_count(void)
{
    unsigned in_office = 0;
    for (size_t i = 0; i < 4; i++) in_office += selected[i] == 0;
    lv_label_set_text_fmt(count_label, "%u IN OFFICE", in_office);
}

static void status_changed(lv_event_t *event)
{
    const size_t index = (size_t)lv_event_get_user_data(event);
    selected[index] = (uint8_t)lv_dropdown_get_selected(lv_event_get_target(event));
    lv_label_set_text(status_labels[index], statuses[selected[index]]);
    update_count();
}

static void create_presence_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F4F1), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, PRESENCE_LCD_H_RES, 104);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x173C35), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_hor(header, 40, 0);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "OFFICE PRESENCE");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F6F2), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    count_label = lv_label_create(header);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0xB9D9C8), 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_20, 0);
    lv_obj_align(count_label, LV_ALIGN_RIGHT_MID, 0, 0);

    const char *options = "In office\nRemote\nAway\nIn a meeting\nUnavailable";
    for (size_t i = 0; i < 4; i++) {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_set_size(row, 944, 88);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 126 + (int32_t)i * 110);
        lv_obj_set_style_bg_color(row, lv_color_white(), 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xD8DFDA), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_hor(row, 24, 0);
        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, names[i]);
        lv_obj_set_style_text_color(name, lv_color_hex(0x202B28), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_22, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        status_labels[i] = lv_label_create(row);
        lv_label_set_text(status_labels[i], statuses[0]);
        lv_obj_set_style_text_color(status_labels[i], lv_color_hex(0x267453), 0);
        lv_obj_set_style_text_font(status_labels[i], &lv_font_montserrat_18, 0);
        lv_obj_align(status_labels[i], LV_ALIGN_RIGHT_MID, -216, 0);
        lv_obj_t *dropdown = lv_dropdown_create(row);
        lv_dropdown_set_options(dropdown, options);
        lv_obj_set_width(dropdown, 200);
        lv_obj_align(dropdown, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(dropdown, status_changed, LV_EVENT_VALUE_CHANGED, (void *)i);
    }
    update_count();
}

void app_main(void)
{
    ESP_ERROR_CHECK(presence_board_display_init(&lcd_panel, &touch_panel));
    ESP_ERROR_CHECK(presence_board_backlight_on());

    lv_init();
    const esp_timer_create_args_t tick_timer_config = {
        .callback = lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_config, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

    lv_display_t *display = lv_display_create(PRESENCE_LCD_H_RES, PRESENCE_LCD_V_RES);
    assert(display != NULL);
    const size_t draw_buffer_size = PRESENCE_LCD_H_RES * 60 * sizeof(lv_color_t);
    void *draw_buffer = heap_caps_malloc(draw_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(draw_buffer != NULL);
    lv_display_set_buffers(display, draw_buffer, NULL, draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, display_flush);

    if (touch_panel != NULL) {
        lv_indev_t *input = lv_indev_create();
        lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(input, touch_read);
        lv_indev_set_user_data(input, touch_panel);
    } else {
        ESP_LOGE("presence", "Touch unavailable; starting display without input");
    }

    create_presence_ui();
    ESP_LOGI("presence", "Presence display started");
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
