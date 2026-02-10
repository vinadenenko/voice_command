#ifndef VOICE_COMMAND_SIMPLE_VAD_H
#define VOICE_COMMAND_SIMPLE_VAD_H

#include "audio_capture/ivad.h"

namespace voice_command {
namespace audio_capture {

/// Simple energy-based Voice Activity Detector
///
/// This implementation uses energy comparison between the total buffer
/// and a recent time window to detect when speech has ended. It applies
/// an optional high-pass filter to remove low-frequency noise.
///
/// Algorithm:
///   1. Apply high-pass filter (if freq_threshold > 0)
///   2. Calculate average energy of entire buffer
///   3. Calculate average energy of last window_ms
///   4. Speech ended if energy_last <= energy_threshold * energy_all
class SimpleVad : public IVoiceActivityDetector {
public:
    /// Create a SimpleVad with default configuration
    SimpleVad();

    /// Create a SimpleVad with specified configuration
    /// @param config VAD configuration parameters
    explicit SimpleVad(const VadConfig& config);

    ~SimpleVad() override = default;

    // IVoiceActivityDetector interface
    VadResult Detect(const AudioSamples& samples) override;
    const VadConfig& GetConfig() const override;
    void SetConfig(const VadConfig& config) override;

private:
    /// Apply high-pass filter to remove low-frequency noise
    /// @param data Audio samples (modified in place)
    /// @param cutoff Cutoff frequency in Hz
    /// @param sample_rate Sample rate in Hz
    static void ApplyHighPassFilter(AudioSamples& data, float cutoff,
                                    float sample_rate);

    VadConfig config_;
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_SIMPLE_VAD_H
