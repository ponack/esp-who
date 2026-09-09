/*
 * Camera bring-up for the Meshnology W11 ESP32-S3 CAM.
 *
 * Modelled on Espressif's esp32_s3_eye BSP, trimmed to what this board actually
 * has: a DVP camera and nothing else. SCCB is left for esp_video to initialise,
 * since no other peripheral shares the bus here.
 */

#include "esp_cam_sensor_xclk.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"

#include "bsp/esp32_s3_cam.h"

static const char *TAG = "bsp_camera";

esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg)
{
    (void)cfg;

    esp_cam_sensor_xclk_handle_t xclk_handle = NULL;

    esp_cam_sensor_xclk_config_t xclk_config = {
        .ledc_cfg = {
            .timer = LEDC_TIMER_1,
            .clk_cfg = LEDC_AUTO_CLK,
            /* Timer 1 / channel 0; nothing else on this board uses LEDC. */
            .channel = LEDC_CHANNEL_0,
            .xclk_freq_hz = BSP_CAMERA_XCLK_CLOCK_MHZ * 1000000,
            .xclk_pin = BSP_CAMERA_GPIO_XCLK,
        },
    };

    esp_err_t ret = esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_LEDC, &xclk_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to allocate XCLK source: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_cam_sensor_xclk_start(xclk_handle, &xclk_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start XCLK on GPIO%d: %s", BSP_CAMERA_GPIO_XCLK,
                 esp_err_to_name(ret));
        esp_cam_sensor_xclk_free(xclk_handle);
        return ret;
    }

    ESP_LOGI(TAG, "XCLK on GPIO%d at %d MHz", BSP_CAMERA_GPIO_XCLK, BSP_CAMERA_XCLK_CLOCK_MHZ);

    const esp_video_init_dvp_config_t dvp_config = {
        .sccb_config = {
            /* Nothing else sits on this bus, so let esp_video own it. */
            .init_sccb = true,
            .i2c_config = {
                .port = BSP_CAMERA_SCCB_PORT,
                .scl_pin = BSP_CAMERA_SIOC,
                .sda_pin = BSP_CAMERA_SIOD,
            },
            .freq = BSP_CAMERA_SCCB_FREQ,
        },
        .reset_pin = BSP_CAMERA_RST,
        .pwdn_pin = BSP_CAMERA_PWDN,
        .dvp_pin = {
            .data_width = 8,
            .data_io = {
                BSP_CAMERA_D0,
                BSP_CAMERA_D1,
                BSP_CAMERA_D2,
                BSP_CAMERA_D3,
                BSP_CAMERA_D4,
                BSP_CAMERA_D5,
                BSP_CAMERA_D6,
                BSP_CAMERA_D7,
            },
            .vsync_io = BSP_CAMERA_VSYNC,
            .de_io = BSP_CAMERA_HSYNC,
            .pclk_io = BSP_CAMERA_PCLK,
            .xclk_io = BSP_CAMERA_GPIO_XCLK,
        },
        .xclk_freq = BSP_CAMERA_XCLK_CLOCK_MHZ * 1000000,
    };

    esp_video_init_config_t cam_config = {
        .dvp = &dvp_config,
    };

    ret = esp_video_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "if the sensor was detected but capture will not frame, try swapping");
        ESP_LOGE(TAG, "BSP_CAMERA_VSYNC/BSP_CAMERA_HSYNC (GPIO47/GPIO41) in bsp/esp32_s3_cam.h");
    }
    return ret;
}
