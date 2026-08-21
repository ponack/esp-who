#include "console_cmd.hpp"
#include "esp_console.h"
#include "frame_cap_pipeline.hpp"
#include "human_face_detect.hpp"
#include "nvs_flash.h"
#include "track.hpp"
#include "bsp/esp-bsp.h"

using namespace who::frame_cap;
using namespace who::app;

dl::detect::Detect *get_detect_model()
{
    auto model =
        new HumanFaceDetect(static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL), false);
    // Lower the score threshold so that ByteTrack's second association
    // can use low-confidence detections to bridge occlusions.
    model->set_score_thr(0.1f, 0);
    if (CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL == HumanFaceDetect::MSRMNP_S8_V1) {
        // Two-stage model: the second stage (MNP) must also pass the low-score boxes through.
        model->set_score_thr(0.1f, 1);
    }
    return model;
}

static void start_console(WhoDetectTrackAppLCD *app)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "track>";

    esp_console_register_help_command();
    register_track_cmds(&app->tuner());

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_USB_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void run_detect_lcd()
{
    auto frame_cap = get_mipi_csi_frame_cap_pipeline();
    auto detect_app = new WhoDetectTrackAppLCD(frame_cap);
    // create model later to avoid memory fragmentation.
    detect_app->set_model(get_detect_model());
    detect_app->run();
    start_console(detect_app);
}

extern "C" void app_main(void)
{
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 5);
#if CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif
    // NVS holds the console-tuned parameters, must be up before the app loads them.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    run_detect_lcd();
}
