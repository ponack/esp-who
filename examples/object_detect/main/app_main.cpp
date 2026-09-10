#include "frame_cap_pipeline.hpp"
#include "who_detect_app_lcd.hpp"
#include "who_detect_app_term.hpp"
#include "bsp/esp-bsp.h"
#if defined(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION)
#include "human_face_detect.hpp"
#elif defined(CONFIG_PEDESTRIAN_DETECT_MODEL_LOCATION)
#include "pedestrian_detect.hpp"
#elif defined(CONFIG_CAT_DETECT_MODEL_LOCATION)
#include "cat_detect.hpp"
#elif defined(CONFIG_DOG_DETECT_MODEL_LOCATION)
#include "dog_detect.hpp"
#endif

using namespace who::frame_cap;
using namespace who::app;

dl::detect::Detect *get_detect_model()
{
#if defined(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION)
    auto *model =
        new HumanFaceDetect(static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
                            false);
    // MSRMNP is two stages: MSR proposes regions, MNP refines and scores them.
    // On this camera MSR's proposals score below 0.15, so at the shared default
    // of 0.5 nothing ever reaches MNP and no face is ever reported - even for a
    // large, sharp, well-lit face. Measured over 10 frames with a face present:
    //   MSR 0.50 ->  0/10 frames    MSR 0.10 ->  0/10 frames
    //   MSR 0.05 -> 10/10 frames, scores 0.731..1.000  (~1.1 boxes/frame)
    //   MSR 0.02 -> 10/10 frames, 78 boxes             (false positives)
    // Loosen stage 1 only; MNP stays at its default so output stays confident.
    model->set_score_thr(0.05f, 0);
    return model;
#elif defined(CONFIG_PEDESTRIAN_DETECT_MODEL_LOCATION)
    return new PedestrianDetect(static_cast<PedestrianDetect::model_type_t>(CONFIG_DEFAULT_PEDESTRIAN_DETECT_MODEL),
                                false);
#elif defined(CONFIG_CAT_DETECT_MODEL_LOCATION)
    return new CatDetect(static_cast<CatDetect::model_type_t>(CONFIG_DEFAULT_CAT_DETECT_MODEL), false);
#elif defined(CONFIG_DOG_DETECT_MODEL_LOCATION)
    return new DogDetect(static_cast<DogDetect::model_type_t>(CONFIG_DEFAULT_DOG_DETECT_MODEL), false);
#else
    ESP_LOGE("MAIN", "No detect model component in idf_component.yml");
    return nullptr;
#endif
}

void run_detect_lcd()
{
    WhoFrameCapNode *lcd_disp_frame_cap_node = nullptr;
#if CONFIG_IDF_TARGET_ESP32S3
    auto frame_cap = get_dvp_frame_cap_pipeline(true);
#elif CONFIG_IDF_TARGET_ESP32P4
    auto frame_cap = get_mipi_csi_frame_cap_pipeline(true);
    // auto frame_cap = get_uvc_frame_cap_pipeline(true);
#endif
    auto detect_app = new WhoDetectAppLCD({{255, 0, 0}}, frame_cap, lcd_disp_frame_cap_node);
    // create model later to avoid memory fragmentation.
    detect_app->set_model(get_detect_model());
    detect_app->run();
}

void run_detect_term()
{
#if CONFIG_IDF_TARGET_ESP32S3
    auto frame_cap = get_dvp_frame_cap_pipeline(false);
#elif CONFIG_IDF_TARGET_ESP32P4
    auto frame_cap = get_mipi_csi_frame_cap_pipeline(false);
    // auto frame_cap = get_uvc_frame_cap_pipeline(false);
#endif
    auto detect_app = new WhoDetectAppTerm(frame_cap);
    // create model later to avoid memory fragmentation.
    detect_app->set_model(get_detect_model());
    detect_app->run();
}

extern "C" void app_main(void)
{
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 5);
#if CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD || CONFIG_PEDESTRIAN_DETECT_MODEL_IN_SDCARD || \
    CONFIG_CAT_DETECT_MODEL_IN_SDCARD || CONFIG_DOG_DETECT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif

// close led
#ifdef BSP_BOARD_ESP32_S3_EYE
    led_indicator_handle_t leds[BSP_LED_NUM];
    ESP_ERROR_CHECK(bsp_led_indicator_create(leds, NULL, BSP_LED_NUM));
    ESP_ERROR_CHECK(bsp_led_set(leds[0], false));
#endif

    // This board has no display; detections are printed to the serial monitor.
    run_detect_term();
}
