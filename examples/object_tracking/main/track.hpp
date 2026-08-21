#pragma once
#include "who_detect_app_base.hpp"
#include "who_frame_lcd_disp.hpp"
#include "who_detect_result_handle.hpp"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "pid.hpp"
#include "mcpwm.hpp"
#include "BYTETracker.h"
#include <memory>
#include <vector>

namespace who {
namespace app {
// A tap event: the coordinates where the screen was pressed.
struct touch_tap_t {
    uint16_t x, y;
};

// Polls the touchscreen at ~30 Hz without LVGL and posts tap events
// (press rising edge) to a depth-1 mailbox, keeping only the latest tap.
class WhoTouchPollTask : public task::WhoTask {
public:
    WhoTouchPollTask(QueueHandle_t tap_mailbox);
    ~WhoTouchPollTask();

private:
    void task() override;
    QueueHandle_t m_tap_mailbox;
    esp_lcd_touch_handle_t m_touch;
};

class WhoDetectTrackAppLCD : public WhoDetectAppBase {
public:
    WhoDetectTrackAppLCD(frame_cap::WhoFrameCap *frame_cap);
    ~WhoDetectTrackAppLCD();
    bool run() override;

protected:
    void detect_result_cb(const detect::WhoDetect::result_t &result);
    void lcd_disp_cb(VideoCapture::Frame *fb);
    void on_tap(int x, int y, const std::vector<STrack> &tracks);

private:
    lcd_disp::WhoFrameLCDDisp *m_lcd_disp;
    lcd_disp::WhoDetectResultLCDDisp *m_result_lcd_disp;
    WhoTouchPollTask *m_touch_task;
    QueueHandle_t m_tap_mailbox; // depth-1 mailbox: latest tap only
    PIDController m_pid_pan;
    PIDController m_pid_tilt;
    std::unique_ptr<MCPWM> m_mcpwm_pan;
    std::unique_ptr<MCPWM> m_mcpwm_tilt;
    float m_half_w;
    float m_half_h;
    std::unique_ptr<BYTETracker> m_tracker;
    // Only accessed from the detect task.
    int m_target_id;
    int m_lost_frames;
    static constexpr int kMaxLostFrames = 30;
};
} // namespace app
} // namespace who
