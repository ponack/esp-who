#include "console_cmd.hpp"
#include "esp_console.h"
#include "track_tuner.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include "argtable3/argtable3.h"

namespace {
using who::app::kMaxProfiles;
using who::app::track_params_t;
using who::app::TrackTuner;

static TrackTuner *s_tuner = nullptr;

static void print_params(const track_params_t &p)
{
    printf("pan : kp=%.4f ki=%.4f kd=%.4f min=%d max=%d init=%d dir=%d\n",
           p.pan_kp,
           p.pan_ki,
           p.pan_kd,
           p.pan_min,
           p.pan_max,
           p.pan_init,
           p.pan_dir);
    printf("tilt: kp=%.4f ki=%.4f kd=%.4f min=%d max=%d init=%d dir=%d\n",
           p.tilt_kp,
           p.tilt_ki,
           p.tilt_kd,
           p.tilt_min,
           p.tilt_max,
           p.tilt_init,
           p.tilt_dir);
    printf("cam : fx=%.1f fy=%.1f\n", p.cam_fx, p.cam_fy);
}

static float *pid_field(track_params_t &p, const char *axis, const char *key)
{
    bool pan = !strcmp(axis, "pan");
    if (!strcmp(key, "kp")) {
        return pan ? &p.pan_kp : &p.tilt_kp;
    }
    if (!strcmp(key, "ki")) {
        return pan ? &p.pan_ki : &p.tilt_ki;
    }
    if (!strcmp(key, "kd")) {
        return pan ? &p.pan_kd : &p.tilt_kd;
    }
    return nullptr;
}

static int *servo_field(track_params_t &p, const char *axis, const char *key)
{
    bool pan = !strcmp(axis, "pan");
    if (!strcmp(key, "min")) {
        return pan ? &p.pan_min : &p.tilt_min;
    }
    if (!strcmp(key, "max")) {
        return pan ? &p.pan_max : &p.tilt_max;
    }
    if (!strcmp(key, "init")) {
        return pan ? &p.pan_init : &p.tilt_init;
    }
    if (!strcmp(key, "dir")) {
        return pan ? &p.pan_dir : &p.tilt_dir;
    }
    return nullptr;
}

static bool valid_axis(const char *axis)
{
    return !strcmp(axis, "pan") || !strcmp(axis, "tilt");
}

// argtable3 hands every token starting with '-' to getopt, so a negative
// positional value ("servo pan dir -1") is rejected as an unknown option.
// Strip the sign from a trailing negative number before arg_parse and
// re-apply it to the parsed value afterwards.
static bool strip_trailing_negative(int argc, char **argv)
{
    if (argc < 2) {
        return false;
    }
    char *last = argv[argc - 1];
    if (last[0] != '-' || (!isdigit((unsigned char)last[1]) && last[1] != '.')) {
        return false;
    }
    argv[argc - 1] = last + 1;
    return true;
}

/* ---- profile [list|save|load|del] [name] ---- */

static struct {
    struct arg_str *sub;
    struct arg_str *name;
    struct arg_end *end;
} profile_args;

static int cmd_profile(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&profile_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, profile_args.end, "profile");
        return 1;
    }
    const char *sub = profile_args.sub->count ? profile_args.sub->sval[0] : "";
    const char *name = profile_args.name->count ? profile_args.name->sval[0] : nullptr;

    if (!*sub || !strcmp(sub, "show")) {
        printf("active: %s%s\n", s_tuner->active_profile().c_str(), s_tuner->dirty() ? " (dirty)" : "");
        print_params(s_tuner->get());
    } else if (!strcmp(sub, "list")) {
        std::string active = s_tuner->active_profile();
        for (const auto &profile : s_tuner->list_profiles()) {
            printf("%s %s\n", profile == active ? "*" : " ", profile.c_str());
        }
    } else if (!strcmp(sub, "save")) {
        if (!s_tuner->save_profile(name)) {
            if (!name) {
                printf("the default profile cannot be saved into, usage: profile save <name>\n");
            } else {
                printf("cannot save '%s' (reserved name, name too long, or all %d slots taken)\n",
                       name,
                       (int)kMaxProfiles);
            }
            return 1;
        }
        printf("saved as profile '%s'\n", s_tuner->active_profile().c_str());
    } else if (!strcmp(sub, "load")) {
        if (!name) {
            printf("usage: profile load <name>\n");
            return 1;
        }
        if (!s_tuner->load_profile(name)) {
            printf("no profile named '%s'\n", name);
            return 1;
        }
        printf("loaded profile '%s'\n", name);
    } else if (!strcmp(sub, "del")) {
        if (!name) {
            printf("usage: profile del <name>\n");
            return 1;
        }
        if (!s_tuner->delete_profile(name)) {
            printf("cannot delete '%s' (no such profile, or it is the active one)\n", name);
            return 1;
        }
        printf("deleted profile '%s'\n", name);
    } else {
        printf("unknown subcommand '%s', expected list|save|load|del\n", sub);
        return 1;
    }
    return 0;
}

/* ---- pid <pan|tilt> [kp|ki|kd <value>] ---- */

static struct {
    struct arg_str *axis;
    struct arg_str *key;
    struct arg_dbl *value;
    struct arg_end *end;
} pid_args;

static int cmd_pid(int argc, char **argv)
{
    bool negative = strip_trailing_negative(argc, argv);
    int nerrors = arg_parse(argc, argv, (void **)&pid_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, pid_args.end, "pid");
        return 1;
    }
    const char *axis = pid_args.axis->sval[0];
    if (!valid_axis(axis)) {
        printf("axis must be pan or tilt\n");
        return 1;
    }
    if (pid_args.key->count == 0) {
        track_params_t p = s_tuner->get();
        printf("%s: kp=%.4f ki=%.4f kd=%.4f\n",
               axis,
               !strcmp(axis, "pan") ? p.pan_kp : p.tilt_kp,
               !strcmp(axis, "pan") ? p.pan_ki : p.tilt_ki,
               !strcmp(axis, "pan") ? p.pan_kd : p.tilt_kd);
        return 0;
    }
    const char *key = pid_args.key->sval[0];
    if (pid_args.value->count == 0) {
        printf("missing value, usage: pid <pan|tilt> <kp|ki|kd> <value>\n");
        return 1;
    }
    track_params_t p = s_tuner->get();
    float *field = pid_field(p, axis, key);
    if (!field) {
        printf("key must be kp, ki or kd\n");
        return 1;
    }
    float value = (float)pid_args.value->dval[0];
    *field = negative ? -value : value;
    s_tuner->update(p);
    printf("%s %s = %.4f\n", axis, key, *field);
    return 0;
}

/* ---- servo <pan|tilt> [min|max|init|dir <value>] ---- */

static struct {
    struct arg_str *axis;
    struct arg_str *key;
    struct arg_int *value;
    struct arg_end *end;
} servo_args;

static int cmd_servo(int argc, char **argv)
{
    bool negative = strip_trailing_negative(argc, argv);
    int nerrors = arg_parse(argc, argv, (void **)&servo_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, servo_args.end, "servo");
        return 1;
    }
    const char *axis = servo_args.axis->sval[0];
    if (!valid_axis(axis)) {
        printf("axis must be pan or tilt\n");
        return 1;
    }
    if (servo_args.key->count == 0) {
        track_params_t p = s_tuner->get();
        printf("%s: min=%d max=%d init=%d dir=%d\n",
               axis,
               !strcmp(axis, "pan") ? p.pan_min : p.tilt_min,
               !strcmp(axis, "pan") ? p.pan_max : p.tilt_max,
               !strcmp(axis, "pan") ? p.pan_init : p.tilt_init,
               !strcmp(axis, "pan") ? p.pan_dir : p.tilt_dir);
        return 0;
    }
    const char *key = servo_args.key->sval[0];
    if (servo_args.value->count == 0) {
        printf("missing value, usage: servo <pan|tilt> <min|max|init|dir> <value>\n");
        return 1;
    }
    track_params_t p = s_tuner->get();
    int *field = servo_field(p, axis, key);
    if (!field) {
        printf("key must be min, max, init or dir\n");
        return 1;
    }
    int value = servo_args.value->ival[0];
    if (negative) {
        value = -value;
    }
    if (!strcmp(key, "dir") && value != 1 && value != -1) {
        printf("dir must be 1 or -1\n");
        return 1;
    }
    if (!strcmp(key, "min") || !strcmp(key, "max")) {
        int *min = servo_field(p, axis, "min");
        int *max = servo_field(p, axis, "max");
        if (!strcmp(key, "min")) {
            *min = value;
        } else {
            *max = value;
        }
        if (*min >= *max) {
            printf("min must be smaller than max (min=%d max=%d)\n", *min, *max);
            return 1;
        }
    } else {
        *field = value;
    }
    s_tuner->update(p);
    printf("%s %s = %d\n", axis, key, *servo_field(p, axis, key));
    return 0;
}

/* ---- cam [fx|fy <value>] ---- */

static struct {
    struct arg_str *key;
    struct arg_dbl *value;
    struct arg_end *end;
} cam_args;

static int cmd_cam(int argc, char **argv)
{
    bool negative = strip_trailing_negative(argc, argv);
    int nerrors = arg_parse(argc, argv, (void **)&cam_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, cam_args.end, "cam");
        return 1;
    }
    if (cam_args.key->count == 0) {
        track_params_t p = s_tuner->get();
        printf("cam: fx=%.1f fy=%.1f\n", p.cam_fx, p.cam_fy);
        return 0;
    }
    const char *key = cam_args.key->sval[0];
    bool is_fx = !strcmp(key, "fx");
    if (!is_fx && strcmp(key, "fy")) {
        printf("key must be fx or fy\n");
        return 1;
    }
    if (cam_args.value->count == 0) {
        printf("missing value, usage: cam <fx|fy> <value>\n");
        return 1;
    }
    track_params_t p = s_tuner->get();
    float *field = is_fx ? &p.cam_fx : &p.cam_fy;
    float value = (float)cam_args.value->dval[0];
    *field = negative ? -value : value;
    s_tuner->update(p);
    printf("cam %s = %.1f\n", key, *field);
    return 0;
}

/* ---- home: move the gimbal to its init angles and release the selection ---- */

static int cmd_home(int argc, char **argv)
{
    if (argc != 1) {
        printf("home takes no arguments\n");
        return 1;
    }
    s_tuner->request_home();
    printf("homing...\n");
    return 0;
}

static void register_cmd(const char *name, const char *help, esp_console_cmd_func_t func, void *argtable)
{
    esp_console_cmd_t cmd = {};
    cmd.command = name;
    cmd.help = help;
    cmd.func = func;
    cmd.argtable = argtable;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
} // namespace

void register_track_cmds(who::app::TrackTuner *tuner)
{
    s_tuner = tuner;

    profile_args.sub = arg_str0(nullptr, nullptr, "[list|save|load|del]", "profile action, none = show");
    profile_args.name = arg_str0(nullptr, nullptr, "[name]", "profile name, 'default' is reserved");
    profile_args.end = arg_end(3);
    register_cmd("profile", "show parameter profiles, or save/load/delete them", cmd_profile, &profile_args);

    pid_args.axis = arg_str1(nullptr, nullptr, "<pan|tilt>", "servo axis");
    pid_args.key = arg_str0(nullptr, nullptr, "[kp|ki|kd]", "gain to set");
    pid_args.value = arg_dbl0(nullptr, nullptr, "<value>", "gain value");
    pid_args.end = arg_end(4);
    register_cmd("pid", "show or set PID gains", cmd_pid, &pid_args);

    servo_args.axis = arg_str1(nullptr, nullptr, "<pan|tilt>", "servo axis");
    servo_args.key = arg_str0(nullptr, nullptr, "[min|max|init|dir]", "parameter to set");
    servo_args.value = arg_int0(nullptr, nullptr, "<value>", "parameter value, dir is 1 or -1");
    servo_args.end = arg_end(4);
    register_cmd("servo", "show or set servo limits/init/direction", cmd_servo, &servo_args);

    cam_args.key = arg_str0(nullptr, nullptr, "[fx|fy]", "focal length to set");
    cam_args.value = arg_dbl0(nullptr, nullptr, "<value>", "focal length value");
    cam_args.end = arg_end(3);
    register_cmd("cam", "show or set camera focal length", cmd_cam, &cam_args);

    register_cmd("home", "move the gimbal to its init angles and release the selection", cmd_home, nullptr);
}
