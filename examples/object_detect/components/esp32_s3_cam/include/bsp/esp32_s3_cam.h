/*
 * Board support for the Meshnology W11 ESP32-S3 CAM (OV3660 on a ZIF socket).
 *
 * The vendor publishes no schematic or pin map, so everything below was
 * established by probing the hardware directly: the sensor was identified by
 * reading chip ID 0x3660 from registers 0x300A/0x300B, and the DVP bus was found
 * by driving a test pattern and watching which pins toggled.
 *
 * The widely-published "ESP32-S3 CAM" pinout (keyestudio MB0184 / Freenove, with
 * XCLK on GPIO15 and SCCB on GPIO4/5) does NOT apply to this board.
 *
 * Board: ESP32-S3 rev v0.2, 16MB quad flash, 8MB embedded octal PSRAM (S3R8),
 * native USB-Serial/JTAG. No LCD, no audio codec, no buttons.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This board has no display, so esp-who's LVGL paths stay compiled out. */
#define BSP_CONFIG_NO_GRAPHIC_LIB 1

#define BSP_BOARD_ESP32_S3_CAM 1

/* ---- Camera, all verified by probing ---- */

#define BSP_CAMERA_GPIO_XCLK (GPIO_NUM_10)
#define BSP_CAMERA_SIOD      (GPIO_NUM_40)
#define BSP_CAMERA_SIOC      (GPIO_NUM_39)
#define BSP_CAMERA_PCLK      (GPIO_NUM_13)

/*
 * Confirmed by capture: with these swapped the driver reported truncated frames
 * (esp_cam_ctlr_dvp "RX:115200-14400"); this way round frames arrive complete.
 */
#define BSP_CAMERA_VSYNC (GPIO_NUM_41)
#define BSP_CAMERA_HSYNC (GPIO_NUM_47)

/*
 * The bit order is NOT sequential and could not be guessed. It was recovered by
 * capturing a frame with an arbitrary order, then searching all 8! bit
 * permutations for the one that renders as a natural image (smoothest result by
 * total variation - 2.87 versus 15.30 for sequential order). The winning
 * permutation produced a clearly coherent scene; sequential produced colour
 * speckle over correct geometry.
 */
#define BSP_CAMERA_D0 (GPIO_NUM_15)
#define BSP_CAMERA_D1 (GPIO_NUM_17)
#define BSP_CAMERA_D2 (GPIO_NUM_18)
#define BSP_CAMERA_D3 (GPIO_NUM_16)
#define BSP_CAMERA_D4 (GPIO_NUM_14)
#define BSP_CAMERA_D5 (GPIO_NUM_12)
#define BSP_CAMERA_D6 (GPIO_NUM_11)
#define BSP_CAMERA_D7 (GPIO_NUM_48)

/* Sensor answers with both lines untouched, so they are tied on-board. */
#define BSP_CAMERA_RST  (GPIO_NUM_NC)
#define BSP_CAMERA_PWDN (GPIO_NUM_NC)

/* The OV3660 register sets in esp_cam_sensor are written for a 20MHz input. */
#define BSP_CAMERA_XCLK_CLOCK_MHZ (20)

#define BSP_CAMERA_SCCB_PORT (0)
#define BSP_CAMERA_SCCB_FREQ (100000)

#define BSP_CAMERA_VFLIP (0)
#define BSP_CAMERA_HFLIP (0)

typedef struct {
    int reserved; /* esp-who passes a zeroed struct; nothing to configure yet */
} bsp_camera_cfg_t;

/**
 * @brief Bring up XCLK and the DVP camera pipeline.
 *
 * @param cfg  May be NULL. Present for API compatibility with the Espressif BSPs
 *             that esp-who's examples expect.
 */
esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg);

/* ---- Display: not present on this board ----
 *
 * esp-who always compiles its LCD path (who_lcd is a hard dependency of
 * who_detect_app, whether or not the app uses it), so these symbols have to
 * exist to link. They are declared here and implemented as failing stubs.
 *
 * The terminal app path (run_detect_term) never touches them. Anything that does
 * gets ESP_ERR_NOT_SUPPORTED and a log line, rather than a board that appears to
 * have a display and then misbehaves.
 *
 * The resolution values below are nominal, sized to the camera output, and exist
 * only so the buffer arithmetic in who_lcd compiles.
 */

#define BSP_LCD_H_RES          (240)
#define BSP_LCD_V_RES          (240)
#define BSP_LCD_BITS_PER_PIXEL (16)

typedef struct {
    int max_transfer_sz;
} bsp_display_config_t;

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

esp_err_t bsp_display_backlight_on(void);

#ifdef __cplusplus
}
#endif
