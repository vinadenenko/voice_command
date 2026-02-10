#include "audio_capture/sdl/sdl_audio_capture.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace voice_command {
namespace audio_capture {

SdlAudioCapture::SdlAudioCapture() = default;

SdlAudioCapture::~SdlAudioCapture() { Shutdown(); }

bool SdlAudioCapture::Init(const AudioCaptureConfig& config) {
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SdlAudioCapture: Couldn't initialize SDL: %s",
                     SDL_GetError());
        return false;
    }

    SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium",
                            SDL_HINT_OVERRIDE);

    // Log available capture devices
    {
        const int device_count = SDL_GetNumAudioDevices(SDL_TRUE);
        std::fprintf(stderr, "SdlAudioCapture: found %d capture devices:\n",
                     device_count);
        for (int i = 0; i < device_count; ++i) {
            std::fprintf(stderr, "SdlAudioCapture:   - Device #%d: '%s'\n", i,
                         SDL_GetAudioDeviceName(i, SDL_TRUE));
        }
    }

    SDL_AudioSpec requested_spec;
    SDL_AudioSpec obtained_spec;
    SDL_zero(requested_spec);
    SDL_zero(obtained_spec);

    requested_spec.freq = config.sample_rate;
    requested_spec.format = AUDIO_F32;
    requested_spec.channels = static_cast<Uint8>(config.channels);
    requested_spec.samples = 1024;
    requested_spec.callback = SdlCallback;
    requested_spec.userdata = this;

    // Open audio device
    const char* device_name = nullptr;
    if (config.device_id >= 0) {
        device_name = SDL_GetAudioDeviceName(config.device_id, SDL_TRUE);
        std::fprintf(stderr,
                     "SdlAudioCapture: Opening capture device %d: '%s'\n",
                     config.device_id, device_name ? device_name : "unknown");
    } else if (!config.device_name.empty()) {
        device_name = config.device_name.c_str();
        std::fprintf(stderr,
                     "SdlAudioCapture: Opening capture device by name: '%s'\n",
                     device_name);
    } else {
        std::fprintf(stderr,
                     "SdlAudioCapture: Opening default capture device\n");
    }

    device_id_ = SDL_OpenAudioDevice(device_name, SDL_TRUE, &requested_spec,
                                     &obtained_spec, 0);

    if (device_id_ == 0) {
        std::fprintf(stderr,
                     "SdlAudioCapture: Couldn't open audio device: %s\n",
                     SDL_GetError());
        return false;
    }

    std::fprintf(stderr, "SdlAudioCapture: Obtained spec (SDL ID = %d):\n",
                 device_id_);
    std::fprintf(stderr, "SdlAudioCapture:   - Sample rate: %d\n",
                 obtained_spec.freq);
    std::fprintf(stderr, "SdlAudioCapture:   - Format: %d (requested: %d)\n",
                 obtained_spec.format, requested_spec.format);
    std::fprintf(stderr, "SdlAudioCapture:   - Channels: %d (requested: %d)\n",
                 obtained_spec.channels, requested_spec.channels);
    std::fprintf(stderr, "SdlAudioCapture:   - Samples per frame: %d\n",
                 obtained_spec.samples);

    sample_rate_ = obtained_spec.freq;
    buffer_duration_ms_ = config.buffer_duration_ms;

    // Resize circular buffer
    const size_t buffer_size =
        static_cast<size_t>(sample_rate_) * buffer_duration_ms_ / 1000;
    audio_buffer_.resize(buffer_size);
    buffer_pos_ = 0;
    buffer_len_ = 0;

    return true;
}

void SdlAudioCapture::Shutdown() {
    Stop();

    if (device_id_ != 0) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    audio_buffer_.clear();
    buffer_pos_ = 0;
    buffer_len_ = 0;
}

bool SdlAudioCapture::Start() {
    if (device_id_ == 0) {
        std::fprintf(stderr, "SdlAudioCapture: No audio device to start\n");
        return false;
    }

    if (running_.load()) {
        std::fprintf(stderr, "SdlAudioCapture: Already running\n");
        return false;
    }

    SDL_PauseAudioDevice(device_id_, 0);
    running_.store(true);

    return true;
}

bool SdlAudioCapture::Stop() {
    if (device_id_ == 0) {
        std::fprintf(stderr, "SdlAudioCapture: No audio device to stop\n");
        return false;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "SdlAudioCapture: Already stopped\n");
        return false;
    }

    SDL_PauseAudioDevice(device_id_, 1);
    running_.store(false);

    return true;
}

bool SdlAudioCapture::IsRunning() const { return running_.load(); }

void SdlAudioCapture::GetAudio(int duration_ms, AudioSamples& samples) {
    samples.clear();

    if (device_id_ == 0) {
        std::fprintf(stderr,
                     "SdlAudioCapture: No audio device to get audio from\n");
        return;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "SdlAudioCapture: Not running\n");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (duration_ms <= 0) {
        duration_ms = buffer_duration_ms_;
    }

    size_t n_samples =
        static_cast<size_t>(sample_rate_) * duration_ms / 1000;
    if (n_samples > buffer_len_) {
        n_samples = buffer_len_;
    }

    samples.resize(n_samples);

    // Calculate start position in circular buffer
    int start_pos = static_cast<int>(buffer_pos_) - static_cast<int>(n_samples);
    if (start_pos < 0) {
        start_pos += static_cast<int>(audio_buffer_.size());
    }

    // Copy from circular buffer
    const size_t s0 = static_cast<size_t>(start_pos);
    if (s0 + n_samples > audio_buffer_.size()) {
        // Wrap around
        const size_t first_part = audio_buffer_.size() - s0;
        std::memcpy(samples.data(), &audio_buffer_[s0],
                    first_part * sizeof(float));
        std::memcpy(&samples[first_part], audio_buffer_.data(),
                    (n_samples - first_part) * sizeof(float));
    } else {
        std::memcpy(samples.data(), &audio_buffer_[s0],
                    n_samples * sizeof(float));
    }
}

bool SdlAudioCapture::Clear() {
    if (device_id_ == 0) {
        std::fprintf(stderr, "SdlAudioCapture: No audio device to clear\n");
        return false;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "SdlAudioCapture: Not running\n");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    buffer_pos_ = 0;
    buffer_len_ = 0;

    return true;
}

int SdlAudioCapture::GetSampleRate() const { return sample_rate_; }

int SdlAudioCapture::GetBufferDurationMs() const { return buffer_duration_ms_; }

std::vector<std::string> SdlAudioCapture::GetDeviceList() {
    std::vector<std::string> devices;

    // Ensure SDL audio is initialized
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            return devices;
        }
    }

    const int device_count = SDL_GetNumAudioDevices(SDL_TRUE);
    devices.reserve(static_cast<size_t>(device_count));

    for (int i = 0; i < device_count; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, SDL_TRUE);
        if (name != nullptr) {
            devices.emplace_back(name);
        }
    }

    return devices;
}

void SdlAudioCapture::AudioCallback(uint8_t* stream, int len) {
    if (!running_.load()) {
        return;
    }

    const size_t n_samples = static_cast<size_t>(len) / sizeof(float);
    size_t samples_to_copy = n_samples;

    // Handle case where incoming data is larger than buffer
    const float* src = reinterpret_cast<const float*>(stream);
    if (samples_to_copy > audio_buffer_.size()) {
        samples_to_copy = audio_buffer_.size();
        // Skip to the end of the incoming data
        src += (n_samples - samples_to_copy);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Copy to circular buffer
    if (buffer_pos_ + samples_to_copy > audio_buffer_.size()) {
        // Wrap around
        const size_t first_part = audio_buffer_.size() - buffer_pos_;
        std::memcpy(&audio_buffer_[buffer_pos_], src,
                    first_part * sizeof(float));
        std::memcpy(audio_buffer_.data(), src + first_part,
                    (samples_to_copy - first_part) * sizeof(float));
    } else {
        std::memcpy(&audio_buffer_[buffer_pos_], src,
                    samples_to_copy * sizeof(float));
    }

    buffer_pos_ = (buffer_pos_ + samples_to_copy) % audio_buffer_.size();
    buffer_len_ = std::min(buffer_len_ + samples_to_copy, audio_buffer_.size());
}

void SdlAudioCapture::SdlCallback(void* userdata, uint8_t* stream, int len) {
    auto* capture = static_cast<SdlAudioCapture*>(userdata);
    capture->AudioCallback(stream, len);
}

}  // namespace audio_capture
}  // namespace voice_command
