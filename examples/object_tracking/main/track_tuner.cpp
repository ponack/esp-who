#include "track_tuner.hpp"
#include "nvs.h"
#include <cstdlib>
#include <cstring>

namespace who {
namespace app {
namespace {
constexpr const char *kNvsNamespace = "track";
constexpr const char *kNvsKey = "params";
constexpr uint32_t kNvsMagic = 0x54524b50; // "TRKP"
constexpr uint16_t kNvsVersion = 3;

// active == kNoActiveProfile means the virtual "default" profile (Kconfig).
constexpr uint8_t kNoActiveProfile = 0xFF;

// NVS serialization of the profile store.
struct nvs_profile_t {
    char name[kProfileNameLen];
    track_params_t params;
};

struct nvs_params_blob_t {
    uint32_t magic;
    uint16_t version;
    uint8_t active; // kNoActiveProfile or a slot index
    nvs_profile_t profiles[kMaxProfiles];
};

static_assert(sizeof(nvs_params_blob_t) < 4000, "blob exceeds the NVS entry limit");

track_params_t default_params()
{
    track_params_t p{};
    p.pan_kp = atof(CONFIG_PAN_KP);
    p.pan_ki = atof(CONFIG_PAN_KI);
    p.pan_kd = atof(CONFIG_PAN_KD);
    p.tilt_kp = atof(CONFIG_TILT_KP);
    p.tilt_ki = atof(CONFIG_TILT_KI);
    p.tilt_kd = atof(CONFIG_TILT_KD);
    p.pan_min = CONFIG_PAN_MIN_ANGLE;
    p.pan_max = CONFIG_PAN_MAX_ANGLE;
    p.pan_init = CONFIG_PAN_INIT_ANGLE;
    p.pan_dir = CONFIG_PAN_DIR;
    p.tilt_min = CONFIG_TILT_MIN_ANGLE;
    p.tilt_max = CONFIG_TILT_MAX_ANGLE;
    p.tilt_init = CONFIG_TILT_INIT_ANGLE;
    p.tilt_dir = CONFIG_TILT_DIR;
    p.cam_fx = atof(CONFIG_CAMERA_FX);
    p.cam_fy = atof(CONFIG_CAMERA_FY);
    return p;
}

// Load the profile store from NVS. Returns false if there is no valid blob
// (profiles and active are left untouched in that case).
bool load_blob(std::array<profile_t, kMaxProfiles> &profiles, uint8_t &active)
{
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    nvs_params_blob_t blob;
    size_t len = sizeof(blob);
    bool ok = nvs_get_blob(handle, kNvsKey, &blob, &len) == ESP_OK && len == sizeof(blob) && blob.magic == kNvsMagic &&
        blob.version == kNvsVersion;
    if (ok) {
        for (size_t i = 0; i < kMaxProfiles; i++) {
            blob.profiles[i].name[kProfileNameLen - 1] = '\0';
            profiles[i].name = blob.profiles[i].name;
            profiles[i].params = blob.profiles[i].params;
        }
        active = blob.active;
        if (active != kNoActiveProfile && (active >= kMaxProfiles || profiles[active].name.empty())) {
            active = kNoActiveProfile;
        }
    }
    nvs_close(handle);
    return ok;
}

bool params_equal(const track_params_t &a, const track_params_t &b)
{
    return memcmp(&a, &b, sizeof(track_params_t)) == 0;
}
} // namespace

TrackTuner::TrackTuner() :
    m_params(default_params()),
    m_dirty(false),
    m_home_req(false),
    m_active(kNoActiveProfile),
    m_mutex(xSemaphoreCreateMutex())
{
    // NVS overrides the Kconfig defaults when a valid blob exists.
    if (load_blob(m_profiles, m_active) && m_active != kNoActiveProfile) {
        m_params = m_profiles[m_active].params;
    }
    m_pending = m_params;
}

track_params_t TrackTuner::get() const
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    track_params_t p = m_params;
    xSemaphoreGive(m_mutex);
    return p;
}

void TrackTuner::update(const track_params_t &p)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_pending = p;
    m_dirty = true;
    xSemaphoreGive(m_mutex);
}

void TrackTuner::request_home()
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_home_req = true;
    xSemaphoreGive(m_mutex);
}

std::string TrackTuner::active_profile() const
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::string name = m_active == kNoActiveProfile ? kDefaultProfileName : m_profiles[m_active].name;
    xSemaphoreGive(m_mutex);
    return name;
}

std::vector<std::string> TrackTuner::list_profiles() const
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    std::vector<std::string> names = {kDefaultProfileName};
    for (const auto &profile : m_profiles) {
        if (!profile.name.empty()) {
            names.push_back(profile.name);
        }
    }
    xSemaphoreGive(m_mutex);
    return names;
}

bool TrackTuner::dirty() const
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    track_params_t active = m_active == kNoActiveProfile ? default_params() : m_profiles[m_active].params;
    bool dirty = !params_equal(m_params, active);
    xSemaphoreGive(m_mutex);
    return dirty;
}

bool TrackTuner::load_profile(const char *name)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    track_params_t params;
    uint8_t active = kNoActiveProfile;
    bool found = false;
    if (!strcmp(name, kDefaultProfileName)) {
        params = default_params();
        found = true;
    } else {
        for (size_t i = 0; i < kMaxProfiles; i++) {
            if (m_profiles[i].name == name) {
                params = m_profiles[i].params;
                active = i;
                found = true;
                break;
            }
        }
    }
    if (found) {
        // Applied by the detect task at the next frame boundary.
        m_pending = params;
        m_dirty = true;
        m_active = active;
    }
    xSemaphoreGive(m_mutex);
    return found;
}

bool TrackTuner::save_profile(const char *name)
{
    if (name && (!*name || !strcmp(name, kDefaultProfileName) || strlen(name) >= kProfileNameLen)) {
        return false;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    int index = -1;
    if (name) {
        // Overwrite the slot with the same name, else take a free one.
        for (size_t i = 0; i < kMaxProfiles && index < 0; i++) {
            if (m_profiles[i].name == name) {
                index = i;
            }
        }
        for (size_t i = 0; i < kMaxProfiles && index < 0; i++) {
            if (m_profiles[i].name.empty()) {
                index = i;
            }
        }
    } else {
        // Save back into the active profile; "default" has no slot.
        index = m_active == kNoActiveProfile ? -1 : m_active;
    }
    if (index < 0) {
        xSemaphoreGive(m_mutex);
        return false;
    }
    if (name) {
        m_profiles[index].name = name;
    }
    m_profiles[index].params = m_params;
    m_active = index;
    xSemaphoreGive(m_mutex);
    return persist() == ESP_OK;
}

bool TrackTuner::delete_profile(const char *name)
{
    if (!name || !*name) {
        return false;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    int index = -1;
    for (size_t i = 0; i < kMaxProfiles; i++) {
        if (m_profiles[i].name == name) {
            index = i;
            break;
        }
    }
    // The active profile cannot be deleted: load another one first.
    if (index < 0 || index == m_active) {
        xSemaphoreGive(m_mutex);
        return false;
    }
    m_profiles[index] = {};
    xSemaphoreGive(m_mutex);
    return persist() == ESP_OK;
}

esp_err_t TrackTuner::persist()
{
    nvs_params_blob_t blob = {};
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    blob.magic = kNvsMagic;
    blob.version = kNvsVersion;
    blob.active = m_active;
    for (size_t i = 0; i < kMaxProfiles; i++) {
        strncpy(blob.profiles[i].name, m_profiles[i].name.c_str(), kProfileNameLen - 1);
        blob.profiles[i].params = m_profiles[i].params;
    }
    xSemaphoreGive(m_mutex);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, kNvsKey, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

TrackTuner::pending_t TrackTuner::consume_pending()
{
    pending_t pending;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (m_dirty) {
        pending.params_changed = true;
        pending.params = m_pending;
        m_params = m_pending;
        m_dirty = false;
    }
    if (m_home_req) {
        pending.home_requested = true;
        m_home_req = false;
    }
    xSemaphoreGive(m_mutex);
    return pending;
}

} // namespace app
} // namespace who
