#include "frame_cap_pipeline.hpp"
#include "video_capture.hpp"
#include <cstdlib>
#include "bsp/esp-bsp.h"

using namespace who::frame_cap;

// num of frames the model take to get result
#define MODEL_TIME 4

WhoFrameCap *get_mipi_csi_frame_cap_pipeline()
{
    esp_log_level_set("ISP_AWB", ESP_LOG_ERROR);
    bsp_camera_cfg_t cam_cfg{};
    ESP_ERROR_CHECK(bsp_camera_start(&cam_cfg));

    auto cap = new VideoCapture();
    auto cfg = VideoCapture::Config(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, V4L2_PIX_FMT_RGB565, MODEL_TIME + 3);
    cap->init(cfg);
    cap->start();

    auto frame_cap = new WhoFrameCap();
    frame_cap->add_node<WhoFetchNode>("FrameCapFetch", cap);
    return frame_cap;
}
