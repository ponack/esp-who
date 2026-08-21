#include "frame_cap_pipeline.hpp"
#include "video_capture.hpp"
#include "esp_video_init.h"
#include "bsp/esp-bsp.h"
#include <cstdlib>

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

WhoFrameCap *get_uvc_frame_cap_pipeline()
{
    esp_video_init_usb_uvc_config_t usb_uvc_cfg = {
        .uvc = {
            .uvc_dev_num = 1,
            .task_stack = 4096,
            .task_priority = 10,
            .task_affinity = -1,
        },
        .usb = {
            .init_usb_host_lib = true,
            .peripheral_map = 0x00,
            .task_stack = 4096,
            .task_priority = 11,
            .task_affinity = -1,
        },
    };
    esp_video_init_config_t video_cfg = {};
    video_cfg.usb_uvc = &usb_uvc_cfg;
    esp_video_init(&video_cfg);

    auto cap = new VideoCapture();
    auto cfg = VideoCapture::Config(ESP_VIDEO_USB_UVC_NAME(0), V4L2_PIX_FMT_JPEG, 4).set_uvc_config({CONFIG_CAMERA_WIDTH, CONFIG_CAMERA_HEIGHT, (float)atof(CONFIG_CAMERA_FPS)});
    cap->init(cfg);
    cap->start();
    auto frame_cap = new WhoFrameCap();
    frame_cap->add_node<WhoFetchNode>("FrameCapFetch", cap, false);
    frame_cap->add_node<WhoDecodeNode>("FrameCapDecode", dl::image::DL_IMAGE_PIX_TYPE_RGB565LE, 2, false);
    frame_cap->add_node<WhoPPAResizeNode>(
        "FrameCapPPAResize", CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT, dl::image::DL_IMAGE_PIX_TYPE_RGB565LE, MODEL_TIME + 1);
    return frame_cap;
}
