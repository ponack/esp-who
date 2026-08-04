#include "frame_cap_pipeline.hpp"
#include "who_pp_ocr_v6_app_lcd.hpp"
#include "who_pp_ocr_v6_app_term.hpp"
#if CONFIG_PP_OCR_V6_MODEL_IN_SDCARD
#include "bsp/esp-bsp.h"
#endif

using namespace who::frame_cap;
using namespace who::app;

extern "C" void app_main(void)
{
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 5);

#if CONFIG_PP_OCR_V6_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif

#if CONFIG_IDF_TARGET_ESP32P4
    auto frame_cap = get_mipi_csi_frame_cap_pipeline();
    // auto frame_cap = get_uvc_frame_cap_pipeline();
#else
#error "PP-OCRv6 example currently only supports ESP32-P4."
#endif

    auto ocr_app = new WhoPPOCRV6AppLCD(frame_cap);
    // If you do not have a display, use the terminal-only variant instead:
    // auto ocr_app = new WhoPPOCRV6AppTerm(frame_cap);
    ocr_app->run();
}
