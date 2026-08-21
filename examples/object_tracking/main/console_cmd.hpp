#pragma once

namespace who {
namespace app {
class TrackTuner;
}
} // namespace who

// Register the track tuning commands (tune/pid/servo/cam/home), operating on
// the given tuner. Call after the tuner's owner was created.
void register_track_cmds(who::app::TrackTuner *tuner);
