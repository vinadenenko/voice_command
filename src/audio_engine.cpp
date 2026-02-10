#include "audio_engine.h"

#include "audio_capture/vad/simple_vad.h"

// Conditionally include backends based on compile-time flags
#ifdef VOICE_COMMAND_AUDIO_SDL_ENABLED
#include "audio_capture/sdl/sdl_audio_capture.h"
#endif

#ifdef VOICE_COMMAND_AUDIO_QT_ENABLED
#include "audio_capture/qt/qt_audio_capture.h"
#endif

#include <stdexcept>

namespace voice_command {

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    if (initialized_) {
        Shutdown();
    }
}

AudioEngine::AudioEngine(AudioEngine&&) noexcept = default;
AudioEngine& AudioEngine::operator=(AudioEngine&&) noexcept = default;

bool AudioEngine::Init(const AudioEngineConfig& config) {
    if (initialized_) {
        return false;  // Already initialized
    }

    config_ = config;

    // Create the audio capture backend
    if (!CreateCaptureBackend(config.backend)) {
        return false;
    }

    // Initialize the capture device
    if (!capture_->Init(config.capture_config)) {
        capture_.reset();
        return false;
    }

    // Create VAD
    if (!CreateVad()) {
        capture_->Shutdown();
        capture_.reset();
        return false;
    }

    initialized_ = true;
    return true;
}

void AudioEngine::Shutdown() {
    if (!initialized_) {
        return;
    }

    if (capture_) {
        if (capture_->IsRunning()) {
            capture_->Stop();
        }
        capture_->Shutdown();
        capture_.reset();
    }

    vad_.reset();
    initialized_ = false;
}

bool AudioEngine::Start() {
    if (!initialized_ || !capture_) {
        return false;
    }
    return capture_->Start();
}

bool AudioEngine::Stop() {
    if (!initialized_ || !capture_) {
        return false;
    }
    return capture_->Stop();
}

bool AudioEngine::IsRunning() const {
    if (!initialized_ || !capture_) {
        return false;
    }
    return capture_->IsRunning();
}

void AudioEngine::GetAudio(int duration_ms, audio_capture::AudioSamples& samples) {
    if (!initialized_ || !capture_) {
        samples.clear();
        return;
    }
    capture_->GetAudio(duration_ms, samples);
}

audio_capture::VadResult AudioEngine::DetectSpeech(
    const audio_capture::AudioSamples& samples) {
    if (!initialized_ || !vad_) {
        return {};
    }
    return vad_->Detect(samples);
}

bool AudioEngine::ClearBuffer() {
    if (!initialized_ || !capture_) {
        return false;
    }
    return capture_->Clear();
}

int AudioEngine::GetSampleRate() const {
    if (!initialized_ || !capture_) {
        return 0;
    }
    return capture_->GetSampleRate();
}

const audio_capture::VadConfig& AudioEngine::GetVadConfig() const {
    if (!vad_) {
        static const audio_capture::VadConfig kDefaultConfig;
        return kDefaultConfig;
    }
    return vad_->GetConfig();
}

void AudioEngine::SetVadConfig(const audio_capture::VadConfig& config) {
    if (vad_) {
        vad_->SetConfig(config);
        config_.vad_config = config;
    }
}

bool AudioEngine::IsInitialized() const {
    return initialized_;
}

AudioEngineConfig AudioEngine::CreateSdlConfig(
    const audio_capture::AudioCaptureConfig& capture_config,
    const audio_capture::VadConfig& vad_config) {
    AudioEngineConfig config;
    config.backend = AudioBackend::kSdl;
    config.capture_config = capture_config;
    config.vad_config = vad_config;
    return config;
}

AudioEngineConfig AudioEngine::CreateQtConfig(
    const audio_capture::AudioCaptureConfig& capture_config,
    const audio_capture::VadConfig& vad_config) {
    AudioEngineConfig config;
    config.backend = AudioBackend::kQt;
    config.capture_config = capture_config;
    config.vad_config = vad_config;
    return config;
}

bool AudioEngine::CreateCaptureBackend(AudioBackend backend) {
    switch (backend) {
        case AudioBackend::kSdl:
#ifdef VOICE_COMMAND_AUDIO_SDL_ENABLED
            capture_ = std::make_unique<audio_capture::SdlAudioCapture>();
            return true;
#else
            return false;  // SDL backend not compiled
#endif

        case AudioBackend::kQt:
#ifdef VOICE_COMMAND_AUDIO_QT_ENABLED
            capture_ = std::make_unique<audio_capture::QtAudioCapture>();
            return true;
#else
            return false;  // Qt backend not compiled
#endif
    }
    return false;
}

bool AudioEngine::CreateVad() {
    vad_ = std::make_unique<audio_capture::SimpleVad>(config_.vad_config);
    return vad_ != nullptr;
}

}  // namespace voice_command
