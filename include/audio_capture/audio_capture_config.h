#ifndef VOICE_COMMAND_AUDIO_CAPTURE_CONFIG_H
#define VOICE_COMMAND_AUDIO_CAPTURE_CONFIG_H

#include <string>

namespace voice_command {
namespace audio_capture {

/// Configuration for audio capture backends
struct AudioCaptureConfig {
    /// Device ID for capture (-1 = default device)
    int device_id = -1;

    /// Alternative: device selection by name (used by Qt backend)
    std::string device_name;

    /// Sample rate in Hz (default: 16000 Hz for whisper.cpp compatibility)
    int sample_rate = 16000;

    /// Number of audio channels (default: 1 for mono)
    int channels = 1;

    /// Duration of the circular buffer in milliseconds
    int buffer_duration_ms = 30000;
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_AUDIO_CAPTURE_CONFIG_H
