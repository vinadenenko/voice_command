#ifndef VOICE_COMMAND_IVAD_H
#define VOICE_COMMAND_IVAD_H

#include "audio_capture/iaudio_capture.h"

namespace voice_command {
namespace audio_capture {

/// Configuration for Voice Activity Detection
struct VadConfig {
    /// Window duration in milliseconds for energy comparison
    int window_ms = 1000;

    /// Energy threshold ratio for detecting end of speech
    /// Speech is considered ended when recent energy < threshold * total energy
    float energy_threshold = 0.6f;

    /// High-pass filter cutoff frequency in Hz (0 = disabled)
    float freq_threshold = 100.0f;

    /// Sample rate of input audio (must match audio capture)
    int sample_rate = 16000;

    /// Enable verbose logging
    bool verbose = false;
};

/// Result of voice activity detection
struct VadResult {
    /// True if speech activity has ended (silence detected)
    bool speech_ended = false;

    /// Energy level of the entire buffer
    float energy_all = 0.0f;

    /// Energy level of the recent window
    float energy_last = 0.0f;
};

/// Abstract interface for Voice Activity Detection
///
/// VAD implementations analyze audio samples to detect the presence
/// or absence of speech. This is separate from IAudioCapture to follow
/// Single Responsibility Principle.
class IVoiceActivityDetector {
public:
    virtual ~IVoiceActivityDetector() = default;

    /// Analyze audio samples for voice activity
    /// @param samples Audio samples to analyze (float32 mono PCM)
    /// @return Detection result indicating if speech has ended
    virtual VadResult Detect(const AudioSamples& samples) = 0;

    /// Get the current VAD configuration
    /// @return Reference to the configuration
    virtual const VadConfig& GetConfig() const = 0;

    /// Update the VAD configuration
    /// @param config New configuration to use
    virtual void SetConfig(const VadConfig& config) = 0;
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_IVAD_H
