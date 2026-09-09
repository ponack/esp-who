/*
 * Display stubs for a board that has no display.
 *
 * esp-who compiles its LCD path unconditionally, so these must link. They are
 * never reached by the terminal app path; if something does call them it gets a
 * clear failure instead of a half-initialised panel.
 */

#include "esp_log.h"

#include "bsp/esp32_s3_cam.h"

static const char *TAG = "bsp_display";

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    (void)config;
    if (ret_panel) {
        *ret_panel = NULL;
    }
    if (ret_io) {
        *ret_io = NULL;
    }
    ESP_LOGE(TAG, "this board has no display; use the terminal app path instead");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_display_backlight_on(void)
{
    ESP_LOGE(TAG, "this board has no display backlight");
    return ESP_ERR_NOT_SUPPORTED;
}
