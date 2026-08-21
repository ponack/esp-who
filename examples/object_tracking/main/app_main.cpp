#include "frame_cap_pipeline.hpp"
#include "track.hpp"
#include "bsp/esp-bsp.h"
#include "human_face_detect.hpp"

using namespace who::frame_cap;
using namespace who::app;

dl::detect::Detect *get_detect_model()
{
    auto model = new HumanFaceDetect(static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
                                     false);
    // Lower the score threshold so that ByteTrack's second association
    // can use low-confidence detections to bridge occlusions.
    model->set_score_thr(0.1f, 0);
    if (CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL == HumanFaceDetect::MSRMNP_S8_V1) {
        // Two-stage model: the second stage (MNP) must also pass the low-score boxes through.
        model->set_score_thr(0.1f, 1);
    }
    return model;
}

void run_detect_lcd()
{
    auto frame_cap = get_mipi_csi_frame_cap_pipeline();
    // auto frame_cap = get_uvc_frame_cap_pipeline();
    auto detect_app = new WhoDetectTrackAppLCD(frame_cap);
    // create model later to avoid memory fragmentation.
    detect_app->set_model(get_detect_model());
    detect_app->run();
}

extern "C" void app_main(void)
{
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 5);
#if CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif

    run_detect_lcd();
}
