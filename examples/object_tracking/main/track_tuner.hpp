#pragma once
#include "esp_err.h"
#include <array>
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace who {
namespace app {

// All runtime-tunable parameters. Defaults come from Kconfig, optionally
// overridden by the active NVS profile.
struct track_params_t {
    float pan_kp, pan_ki, pan_kd;
    float tilt_kp, tilt_ki, tilt_kd;
    int pan_min, pan_max, pan_init, pan_dir;
    int tilt_min, tilt_max, tilt_init, tilt_dir;
    float cam_fx, cam_fy;
};

// Named parameter sets ("profiles") stored in NVS.
constexpr size_t kMaxProfiles = 8;
constexpr size_t kProfileNameLen = 16;
// Reserved name of the virtual profile backed by the Kconfig defaults.
constexpr const char *kDefaultProfileName = "default";

// One profile slot; an empty name marks a free slot.
struct profile_t {
    std::string name;
    track_params_t params;
};

// The console-facing tuning interface, decoupled from the app itself: the
// console REPL task talks only to this object, and the detect task consumes
// the queued changes at frame boundaries (never directly from the console
// task).
class TrackTuner {
public:
    // What consume_pending() hands to the detect task.
    struct pending_t {
        bool params_changed = false;
        bool home_requested = false;
        track_params_t params{};
    };

    TrackTuner(); // Kconfig defaults, overridden by the active NVS profile
    TrackTuner(const TrackTuner &) = delete;
    TrackTuner &operator=(const TrackTuner &) = delete;

    // --- console-facing API (called from the REPL task) ---
    track_params_t get() const;
    // Queue a full parameter set, applied by the detect task at the next
    // frame boundary.
    void update(const track_params_t &p);
    // Queue a "move to init angles" request, consumed the same way.
    void request_home();

    // --- profile management (console-facing) ---
    // Name of the active profile; "default" is the virtual Kconfig profile.
    std::string active_profile() const;
    // "default" plus the names of all stored profiles.
    std::vector<std::string> list_profiles() const;
    // True while the current params differ from the active profile's copy.
    bool dirty() const;
    // Load "default" or a stored profile by name; false if there is none.
    bool load_profile(const char *name);
    // Save the current params into a profile and make it active. A null name
    // writes back into the active profile; fails for the "default" profile,
    // for a reserved/invalid name, or when all slots are taken.
    bool save_profile(const char *name);
    // Delete a stored profile; the active one cannot be deleted.
    bool delete_profile(const char *name);

private:
    // --- detect-task-facing API ---
    friend class WhoDetectTrackAppLCD;
    pending_t consume_pending();
    // Write the profile store to NVS.
    esp_err_t persist();

    // All fields below are protected by m_mutex.
    track_params_t m_params;  // currently applied params
    track_params_t m_pending; // queued by update() / load_profile()
    bool m_dirty;
    bool m_home_req;
    uint8_t m_active; // slot index, or kNoActiveProfile (the default profile)
    std::array<profile_t, kMaxProfiles> m_profiles;
    SemaphoreHandle_t m_mutex;
};

} // namespace app
} // namespace who
