#include "frame_cap_pipeline.hpp"
#include "who_qrcode_app_lcd.hpp"
#include "who_qrcode_app_term.hpp"
#include "bsp/esp-bsp.h"

using namespace who::frame_cap;
using namespace who::app;

extern "C" void app_main(void)
{
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 5);

// close led
#ifdef BSP_BOARD_ESP32_S3_EYE
    led_indicator_handle_t leds[BSP_LED_NUM];
    ESP_ERROR_CHECK(bsp_led_indicator_create(leds, NULL, BSP_LED_NUM));
    ESP_ERROR_CHECK(bsp_led_set(leds[0], false));
#endif

#if CONFIG_IDF_TARGET_ESP32S3
    auto frame_cap = get_dvp_frame_cap_pipeline();
#elif CONFIG_IDF_TARGET_ESP32P4
    auto frame_cap = get_mipi_csi_frame_cap_pipeline();
    // auto frame_cap = get_uvc_frame_cap_pipeline();
#endif

#if CONFIG_IDF_TARGET_ESP32S3
    int w = BSP_LCD_H_RES, h = BSP_LCD_V_RES;
#elif CONFIG_IDF_TARGET_ESP32P4
    int w = BSP_LCD_H_RES / 2, h = BSP_LCD_V_RES / 2;
#endif
    auto qrcode_app = new WhoQRCodeAppLCD(frame_cap, w, h);
    // try this if you don't have a lcd.
    // auto qrcode_app = new WhoQRCodeAppTerm(frame_cap, w, h);
    qrcode_app->run();
}
