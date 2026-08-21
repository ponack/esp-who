#pragma once
#include "esp_timer.h"
#include <algorithm>

class PIDController {
public:
    PIDController(float kp, float ki, float kd, float output_init, float output_min, float output_max);

    float compute(float error);

    void reset();

    // Reset the controller state and jump the output to the given angle.
    void reset(float output_init);

    void set_gains(float kp, float ki, float kd)
    {
        kp_ = kp;
        ki_ = ki;
        kd_ = kd;
    }

    void set_output_limits(float output_min, float output_max)
    {
        output_min_ = output_min;
        output_max_ = output_max;
        output_ = std::clamp(output_, output_min_, output_max_);
    }

private:
    int64_t nowUs() { return esp_timer_get_time(); }

    float kp_, ki_, kd_;

    float output_;

    float output_min_, output_max_;

    float integral_;
    float last_error_;
    int64_t last_time_us_;
    bool first_update_;
};
