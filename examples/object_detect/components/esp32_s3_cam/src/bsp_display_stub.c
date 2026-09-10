/*
 * Display stubs for a camera-only board.
 *
 * esp-who compiles its LCD path unconditionally, so these have to link even
 * though there is no panel. Both always fail, and who_lcd wraps the first of
 * them in ESP_ERROR_CHECK, so any attempt to use the display aborts at once
 * with the reason on the console instead of limping along half-initialised.
 *
 * Nothing on the run_detect_term() path reaches either function.
 */

#include "esp_log.h"

#include "bsp/esp32_s3_cam.h"

static const char *TAG = "bsp_display";

static esp_err_t no_display(const char *what)
{
    ESP_LOGE(TAG, "%s: this board (Meshnology W11 ESP32-S3 CAM) is camera-only "
                  "and has no display; use run_detect_term() instead of run_detect_lcd()",
             what);
    return ESP_ERR_NOT_SUPPORTED;
}

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
    return no_display("bsp_display_new");
}

esp_err_t bsp_display_backlight_on(void)
{
    return no_display("bsp_display_backlight_on");
}
