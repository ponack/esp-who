/*
 * Camera bring-up for the Meshnology W11 ESP32-S3 CAM.
 *
 * Modelled on Espressif's esp32_s3_eye BSP, trimmed to what this board actually
 * has: a DVP camera and nothing else.
 *
 * The BSP owns the SCCB bus (init_sccb = false) rather than letting esp_video
 * create it, because the sensor's sync polarity has to be corrected after the
 * driver has applied its format table - see bsp_camera_fix_sync_polarity().
 */

#include "esp_cam_sensor_xclk.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"

#include "bsp/esp32_s3_cam.h"

static const char *TAG = "bsp_camera";

#define OV3660_SCCB_ADDR 0x3C

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_sensor_dev;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_CAMERA_SCCB_PORT,
        .sda_io_num = BSP_CAMERA_SIOD,
        .scl_io_num = BSP_CAMERA_SIOC,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OV3660_SCCB_ADDR,
        .scl_speed_hz = BSP_CAMERA_SCCB_FREQ,
    };
    return i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_sensor_dev);
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return s_i2c_bus;
}

esp_err_t bsp_camera_write_reg(uint16_t reg, uint8_t val)
{
    if (!s_sensor_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t tx[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val};
    return i2c_master_transmit(s_sensor_dev, tx, sizeof(tx), 300);
}

esp_err_t bsp_camera_read_reg(uint16_t reg, uint8_t *val)
{
    if (!s_sensor_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t tx[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    return i2c_master_transmit_receive(s_sensor_dev, tx, sizeof(tx), val, 1, 300);
}

esp_err_t bsp_camera_fix_sync_polarity(void)
{
    /* 0x4740: bit5 PCLK polarity, bit1 HREF polarity, bit0 VSYNC polarity.
     * 0x20 flips only VSYNC, leaving PCLK active high as the sensor default has
     * it. Chosen by inspecting the decoded image, NOT by the driver's frame
     * error counter - 0x02 scores slightly better on that counter but samples
     * PCLK on the wrong edge and produces banded garbage. See the header. */
    const uint8_t kPolarity = 0x20;

    esp_err_t ret = bsp_camera_write_reg(0x4740, kPolarity);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set sync polarity: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t back = 0xFF;
    if (bsp_camera_read_reg(0x4740, &back) == ESP_OK && back != kPolarity) {
        ESP_LOGW(TAG, "sync polarity read back as 0x%02X, expected 0x%02X", back, kPolarity);
    } else {
        ESP_LOGI(TAG, "sync polarity set to 0x%02X", kPolarity);
    }
    return ESP_OK;
}

esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg)
{
    (void)cfg;

    esp_err_t bus_ret = bsp_i2c_init();
    if (bus_ret != ESP_OK) {
        return bus_ret;
    }

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
            /* The BSP owns the bus so sensor registers remain writable after
             * the driver has applied its format table. */
            .init_sccb = false,
            .i2c_handle = s_i2c_bus,
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
        ESP_LOGE(TAG, "check the DVP pin map in bsp/esp32_s3_cam.h; note the sync pins are "
                      "VSYNC=GPIO%d HREF=GPIO%d and GPIO41 carries nothing",
                 BSP_CAMERA_VSYNC, BSP_CAMERA_HSYNC);
    }
    return ret;
}
