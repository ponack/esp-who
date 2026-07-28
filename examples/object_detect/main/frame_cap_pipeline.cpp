#include "frame_cap_pipeline.hpp"
#include "esp_video_init.h"
#include "video_capture.hpp"
#include "bsp/esp-bsp.h"

using namespace who::frame_cap;

#if defined(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION)
// num of frames the model take to get result
#define MODEL_TIME 2
#define MODEL_INPUT_W 160
#define MODEL_INPUT_H 120
#elif defined(CONFIG_PEDESTRIAN_DETECT_MODEL_LOCATION)
// num of frames the model take to get result
#define MODEL_TIME 3
#define MODEL_INPUT_W 224
#define MODEL_INPUT_H 224
#elif defined(CONFIG_CAT_DETECT_MODEL_LOCATION)
// num of frames the model take to get result
#if defined(CONFIG_ESPDET_PICO_224_224_CAT)
#define MODEL_TIME 3
#define MODEL_INPUT_W 224
#define MODEL_INPUT_H 224
#endif
#if defined(CONFIG_ESPDET_PICO_416_416_CAT)
#define MODEL_TIME 8
#define MODEL_INPUT_W 416
#define MODEL_INPUT_H 416
#endif
#elif defined(CONFIG_DOG_DETECT_MODEL_LOCATION)
// num of frames the model take to get result
#if defined(CONFIG_ESPDET_PICO_224_224_DOG)
#define MODEL_TIME 3
#define MODEL_INPUT_W 224
#define MODEL_INPUT_H 224
#endif
#if defined(CONFIG_ESPDET_PICO_416_416_DOG)
#define MODEL_TIME 8
#define MODEL_INPUT_W 416
#define MODEL_INPUT_H 416
#endif
#endif

// The size of the fb_count and ringbuf_len must be big enough. If you have no idea how to set them, try with 5 and
// larger.
#if CONFIG_IDF_TARGET_ESP32S3
WhoFrameCap *get_dvp_frame_cap_pipeline(bool lcd)
{
    bsp_camera_cfg_t cam_cfg{};
    ESP_ERROR_CHECK(bsp_camera_start(&cam_cfg));
    // The ringbuf_len of FetchNode equals cam_fb_count - 2. The WhoFetchNode fb will display on lcd, if you want to
    // make sure the displayed detection result is synced with the frame, the ringbuf size must be big enough to
    // cover the process time from now to the the detection result is ready. If the ring_buf_len is 3, the frame
    // which the disp task will display is 2 frames before than the frame which feeds into detection task. So the
    // detection task must finish within 2 frames, or the detect result will have a delay compared to the displayed
    // frame.

    auto cap = new VideoCapture();
    auto buffer_cnt = lcd ? (MODEL_TIME + 3) : (MODEL_TIME + 2);
    auto cfg = VideoCapture::Config(ESP_VIDEO_DVP_DEVICE_NAME, V4L2_PIX_FMT_RGB565X, buffer_cnt);

#ifdef BSP_BOARD_ESP32_S3_EYE
    cfg.set_vflip(true);
#elifdef BSP_BOARD_ESP32_S3_KORVO_2
    cfg.set_hflip(true).set_vflip(true);
#endif
    cap->init(cfg);
    cap->start();

    auto frame_cap = new WhoFrameCap();
    frame_cap->add_node<WhoFetchNode>("FrameCapFetch", cap);
    return frame_cap;
}

#elif CONFIG_IDF_TARGET_ESP32P4
WhoFrameCap *get_mipi_csi_frame_cap_pipeline(bool lcd)
{
    esp_log_level_set("ISP_AWB", ESP_LOG_ERROR);
    bsp_camera_cfg_t cam_cfg{};
    ESP_ERROR_CHECK(bsp_camera_start(&cam_cfg));

    auto cap = new VideoCapture();
    auto buffer_cnt = lcd ? (MODEL_TIME + 3) : (MODEL_TIME + 2);
    auto cfg = VideoCapture::Config(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, V4L2_PIX_FMT_RGB565, buffer_cnt).set_hflip(true);
    cap->init(cfg);
    cap->start();

    auto frame_cap = new WhoFrameCap();
    frame_cap->add_node<WhoFetchNode>("FrameCapFetch", cap);
    return frame_cap;
}

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
WhoFrameCap *get_uvc_frame_cap_pipeline(bool lcd)
{
    esp_video_init_usb_uvc_config_t usb_uvc_cfg = {
        .uvc =
            {
                .uvc_dev_num = 1,
                .task_stack = 4096,
                .task_priority = 10,
                .task_affinity = -1,
            },
        .usb =
            {
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
    auto cfg = VideoCapture::Config(ESP_VIDEO_USB_UVC_NAME(0), V4L2_PIX_FMT_JPEG, 4).set_uvc_config({640, 480, 30});
    cap->init(cfg);
    cap->start();

    auto frame_cap = new WhoFrameCap();
    // The ringbuf_len of FetchNode equals cam_fb_count - 2, the ringbuf_len of FetchNode should take care of the
    // process time of the following Node. For example, if the DecodeNode takes 2 frame to decode, then the
    // FetchNode ringbuf_len is at least 2, and the fb_count of the cam is at least 4.
    frame_cap->add_node<WhoFetchNode>("FrameCapFetch", cap, false);

    if (lcd) {
        // The DecodeNode ringbuf_len relies on the following PPAResizeNode process time, the time of data transfer.
        frame_cap->add_node<WhoDecodeNode>("FrameCapDecode", dl::image::DL_IMAGE_PIX_TYPE_RGB565LE, 2, false);
        // The ppa resized fb will display on lcd, if you want to make sure the displayed detection result is synced
        // with the frame, the ringbuf size must be big enough to cover the process time from now to the the detection
        // result is ready.
        frame_cap->add_node<WhoPPAResizeNode>(
            "FrameCapPPAResize", 800, 600, dl::image::DL_IMAGE_PIX_TYPE_RGB565LE, MODEL_TIME + 1);
        return frame_cap;
    } else {
        frame_cap->add_node<WhoDecodeNode>("FrameCapDecode", dl::image::DL_IMAGE_PIX_TYPE_RGB565LE, MODEL_TIME);
        return frame_cap;
    }
}
#endif
#endif
