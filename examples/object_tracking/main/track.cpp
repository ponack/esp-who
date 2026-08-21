#include "track.hpp"
#include "who_yield2idle.hpp"
#include <cmath>
#include <numbers>
#include <cstdlib>

namespace who {
namespace app {
namespace {
const std::vector<uint8_t> kColorSelected = {0, 200, 0}; // green
const std::vector<uint8_t> kColorOthers = {255, 0, 0};   // red
} // namespace

WhoTouchPollTask::WhoTouchPollTask(QueueHandle_t tap_mailbox) :
    task::WhoTask("TouchPoll"), m_tap_mailbox(tap_mailbox), m_touch(nullptr)
{
    ESP_ERROR_CHECK(bsp_touch_new(nullptr, &m_touch));
}

WhoTouchPollTask::~WhoTouchPollTask()
{
    bsp_touch_delete();
}

void WhoTouchPollTask::task()
{
    bool touched = false;
    while (true) {
        EventBits_t event_bits =
            xEventGroupWaitBits(m_event_group, TASK_PAUSE | TASK_STOP, pdTRUE, pdFALSE, pdMS_TO_TICKS(33));
        if (event_bits & TASK_STOP) {
            break;
        } else if (event_bits & TASK_PAUSE) {
            xEventGroupSetBits(m_event_group, TASK_PAUSED);
            EventBits_t pause_event_bits =
                xEventGroupWaitBits(m_event_group, TASK_RESUME | TASK_STOP, pdTRUE, pdFALSE, portMAX_DELAY);
            if (pause_event_bits & TASK_STOP) {
                break;
            } else {
                continue;
            }
        }
        if (m_tap_mailbox && esp_lcd_touch_read_data(m_touch) == ESP_OK) {
            uint16_t x = 0, y = 0;
            uint8_t cnt = 0;
            bool pressed = esp_lcd_touch_get_coordinates(m_touch, &x, &y, nullptr, &cnt, 1) && cnt > 0;
            // Only the press rising edge is a tap; the coordinates must be
            // taken now, they are gone once the finger is released.
            if (pressed && !touched) {
                touch_tap_t tap = {x, y};
                xQueueOverwrite(m_tap_mailbox, &tap);
            }
            touched = pressed;
        }
    }
    xEventGroupSetBits(m_event_group, TASK_STOPPED);
    vTaskDelete(NULL);
}

WhoDetectTrackAppLCD::WhoDetectTrackAppLCD(frame_cap::WhoFrameCap *frame_cap) :
    WhoDetectAppBase(frame_cap),
    m_lcd_disp(new lcd_disp::WhoFrameLCDDisp("LCDDisp", frame_cap->get_last_node())),
    m_tap_mailbox(xQueueCreate(1, sizeof(touch_tap_t))),
    m_pid_pan(atof(CONFIG_PAN_KP), atof(CONFIG_PAN_KI), atof(CONFIG_PAN_KD), CONFIG_PAN_INIT_ANGLE, CONFIG_PAN_MIN_ANGLE, CONFIG_PAN_MAX_ANGLE),
    m_pid_tilt(atof(CONFIG_TILT_KP), atof(CONFIG_TILT_KI), atof(CONFIG_TILT_KD), CONFIG_TILT_INIT_ANGLE, CONFIG_TILT_MIN_ANGLE, CONFIG_TILT_MAX_ANGLE),
    m_target_id(-1),
    m_lost_frames(0)
{
    WhoApp::add_task(m_lcd_disp);
    m_lcd_disp->set_lcd_disp_cb(std::bind(&WhoDetectTrackAppLCD::lcd_disp_cb, this, std::placeholders::_1));
#if !BSP_CONFIG_NO_GRAPHIC_LIB
    m_result_lcd_disp = new lcd_disp::WhoDetectResultLCDDisp(m_detect, m_lcd_disp->get_canvas(), {{255, 0, 0}});
#else
    m_result_lcd_disp = new lcd_disp::WhoDetectResultLCDDisp(m_detect, {{255, 0, 0}});
#endif
    m_touch_task = new WhoTouchPollTask(m_tap_mailbox);
    WhoApp::add_task(m_touch_task);
    m_detect->set_detect_result_cb(std::bind(&WhoDetectTrackAppLCD::detect_result_cb, this, std::placeholders::_1));
    m_half_w = frame_cap->get_last_node()->get_fb_width() / 2.f;
    m_half_h = frame_cap->get_last_node()->get_fb_height() / 2.f;
    m_mcpwm_pan = std::make_unique<MCPWM>((gpio_num_t)CONFIG_PAN_GPIO, 0);
    mcpwm_timer_handle_t timer;
    int group_id;
    m_mcpwm_pan->get_timer(&timer, &group_id);
    m_mcpwm_tilt = std::make_unique<MCPWM>((gpio_num_t)CONFIG_TILT_GPIO, timer, group_id);
    m_mcpwm_pan->enable_and_start_timer();
    m_mcpwm_pan->set_servo_angle(CONFIG_PAN_INIT_ANGLE);
    m_mcpwm_tilt->set_servo_angle(CONFIG_TILT_INIT_ANGLE);
    m_tracker = std::make_unique<BYTETracker>();
}

WhoDetectTrackAppLCD::~WhoDetectTrackAppLCD()
{
    delete m_result_lcd_disp;
    delete m_touch_task;
}

bool WhoDetectTrackAppLCD::run()
{
    bool ret = WhoYield2Idle::get_instance()->run();
    for (const auto &frame_cap_node : m_frame_cap->get_all_nodes()) {
        ret &= frame_cap_node->run(4096, 2, 0);
    }
    ret &= m_lcd_disp->run(2560, 2, 0);
    ret &= m_touch_task->run(2048, 2, 0);
    // ret &= m_detect->run(4096, 2, 1);
    ret &= m_detect->run(20000, 2, 1);
    return ret;
}

void WhoDetectTrackAppLCD::detect_result_cb(const detect::WhoDetect::result_t &result)
{
    auto det_res = result.det_res;

    // Feed the tracker with the new detections.
    std::vector<Object> objects;
    objects.reserve(det_res.size());
    for (const auto &res : det_res) {
        Object obj;
        obj.rect.x = res.box[0];
        obj.rect.y = res.box[1];
        obj.rect.width = res.box[2] - res.box[0];
        obj.rect.height = res.box[3] - res.box[1];
        obj.label = res.category;
        obj.prob = res.score;
        objects.push_back(obj);
    }
    std::vector<STrack> tracks = m_tracker->update(objects);

    // Handle the taps posted by the touch task against the current tracks.
    touch_tap_t tap;
    while (xQueueReceive(m_tap_mailbox, &tap, 0) == pdTRUE) {
        on_tap(tap.x, tap.y, tracks);
    }

    // Find the selected target among the current tracks.
    const STrack *target = nullptr;
    if (m_target_id >= 0) {
        for (const auto &track : tracks) {
            if (track.track_id == m_target_id) {
                target = &track;
                break;
            }
        }
        if (target) {
            m_lost_frames = 0;
        } else if (++m_lost_frames > kMaxLostFrames) {
            // Selected target is gone for good, release the selection.
            m_target_id = -1;
        }
    }

    // Build the draw items from the Kalman-filtered track boxes and save
    // them. No drawing happens here.
    std::vector<lcd_disp::draw_item_t> items;
    items.reserve(tracks.size());
    for (const auto &track : tracks) {
        if (track.state != TrackState::New && track.state != TrackState::Tracked) {
            continue;
        }
        bool selected = (track.track_id == m_target_id);
        lcd_disp::draw_item_t item;
        item.box = {(int)track.tlbr[0], (int)track.tlbr[1], (int)track.tlbr[2], (int)track.tlbr[3]};
        item.color = selected ? kColorSelected : kColorOthers;
        item.label = std::to_string(track.track_id);
        item.cross = selected;
        items.push_back(std::move(item));
    }
    m_result_lcd_disp->save_draw_items(items, result.timestamp);

    // Only steer the gimbal while a target is selected.
    if (target) {
        float cx = (target->tlbr[0] + target->tlbr[2]) / 2.0f;
        float cy = (target->tlbr[1] + target->tlbr[3]) / 2.0f;

        float error_x = std::atan2(CONFIG_PAN_DIR * (cx - m_half_w) / 2, atof(CONFIG_CAMERA_FX)) * 180.0f / std::numbers::pi;
        float error_y = std::atan2(CONFIG_TILT_DIR * (cy - m_half_h) / 2, atof(CONFIG_CAMERA_FY)) * 180.0f / std::numbers::pi;

        float pan = m_pid_pan.compute(error_x);
        float tilt = m_pid_tilt.compute(error_y);
        m_mcpwm_pan->set_servo_angle(pan);
        m_mcpwm_tilt->set_servo_angle(tilt);
    }
    // No target selected: keep the last servo angles and wait for a tap.
}

void WhoDetectTrackAppLCD::lcd_disp_cb(VideoCapture::Frame *fb)
{
    m_result_lcd_disp->lcd_disp_cb(fb);
}

void WhoDetectTrackAppLCD::on_tap(int x, int y, const std::vector<STrack> &tracks)
{
    int clicked = -1;
    for (const auto &track : tracks) {
        if (track.state != TrackState::New && track.state != TrackState::Tracked) {
            continue;
        }
        if (x >= track.tlbr[0] && x <= track.tlbr[2] && y >= track.tlbr[1] && y <= track.tlbr[3]) {
            clicked = track.track_id;
            break;
        }
    }
    // Tap on empty area does nothing; tap a box to select it, tap the selected one to deselect.
    if (clicked >= 0) {
        if (m_target_id == clicked) {
            m_target_id = -1;
        } else {
            m_target_id = clicked;
            m_lost_frames = 0;
        }
    }
}

} // namespace app
} // namespace who
