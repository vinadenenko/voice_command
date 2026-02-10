#define _USE_MATH_DEFINES
#include "audio_capture/vad/simple_vad.h"

#include <cmath>
#include <cstdio>

namespace voice_command {
namespace audio_capture {

SimpleVad::SimpleVad() : config_() {}

SimpleVad::SimpleVad(const VadConfig& config) : config_(config) {}

VadResult SimpleVad::Detect(const AudioSamples& samples) {
    VadResult result;

    const int n_samples = static_cast<int>(samples.size());
    const int n_samples_last =
        (config_.sample_rate * config_.window_ms) / 1000;

    if (n_samples_last >= n_samples) {
        // Not enough samples - assume no speech ended
        return result;
    }

    // Make a copy for filtering (original samples should not be modified)
    AudioSamples filtered_samples = samples;

    // Apply high-pass filter if threshold is set
    if (config_.freq_threshold > 0.0f) {
        ApplyHighPassFilter(filtered_samples, config_.freq_threshold,
                            static_cast<float>(config_.sample_rate));
    }

    // Calculate energy levels
    float energy_all = 0.0f;
    float energy_last = 0.0f;

    for (int i = 0; i < n_samples; ++i) {
        const float abs_sample = std::fabs(filtered_samples[i]);
        energy_all += abs_sample;

        if (i >= n_samples - n_samples_last) {
            energy_last += abs_sample;
        }
    }

    energy_all /= static_cast<float>(n_samples);
    energy_last /= static_cast<float>(n_samples_last);

    result.energy_all = energy_all;
    result.energy_last = energy_last;

    if (config_.verbose) {
        std::fprintf(stderr,
                     "%s: energy_all: %f, energy_last: %f, threshold: %f, "
                     "freq_threshold: %f\n",
                     __func__, energy_all, energy_last, config_.energy_threshold,
                     config_.freq_threshold);
    }

    // Speech has ended if recent energy is below threshold ratio of total
    result.speech_ended =
        (energy_last <= config_.energy_threshold * energy_all);

    return result;
}

const VadConfig& SimpleVad::GetConfig() const { return config_; }

void SimpleVad::SetConfig(const VadConfig& config) { config_ = config; }

void SimpleVad::ApplyHighPassFilter(AudioSamples& data, float cutoff,
                                    float sample_rate) {
    if (data.empty()) {
        return;
    }

    const float rc = 1.0f / (2.0f * static_cast<float>(M_PI) * cutoff);
    const float dt = 1.0f / sample_rate;
    const float alpha = dt / (rc + dt);

    float y = data[0];

    for (size_t i = 1; i < data.size(); ++i) {
        y = alpha * (y + data[i] - data[i - 1]);
        data[i] = y;
    }
}

}  // namespace audio_capture
}  // namespace voice_command
