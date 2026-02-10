#ifndef VOICE_COMMAND_AUDIO_ENGINE_H
#define VOICE_COMMAND_AUDIO_ENGINE_H

#include <memory>
#include <string>

#include "audio_capture/audio_capture_config.h"
#include "audio_capture/iaudio_capture.h"
#include "audio_capture/ivad.h"

namespace voice_command {

/// Audio backend type selection
enum class AudioBackend {
    kSdl,   ///< SDL2 backend (default)
    kQt     ///< Qt6 Multimedia backend
};

/// Configuration for AudioEngine
struct AudioEngineConfig {
    AudioBackend backend = AudioBackend::kSdl;
    audio_capture::AudioCaptureConfig capture_config;
    audio_capture::VadConfig vad_config;
};

/// AudioEngine wraps IAudioCapture + IVoiceActivityDetector providing
/// a simplified interface for the voice command pipeline.
///
/// Responsibilities:
/// - Creates and manages audio capture backend based on configuration
/// - Creates and manages voice activity detector
/// - Provides unified API for audio capture and speech detection
///
/// Thread Safety:
/// - All methods are thread-safe, delegating to thread-safe IAudioCapture
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Movable
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;

    /// Initialize the audio engine with configuration
    /// @param config Engine configuration (backend, capture settings, VAD settings)
    /// @return true if initialization succeeded
    bool Init(const AudioEngineConfig& config);

    /// Release all resources
    void Shutdown();

    /// Start audio capture
    /// @return true if capture started successfully
    bool Start();

    /// Stop audio capture
    /// @return true if capture stopped successfully
    bool Stop();

    /// Check if audio capture is running
    /// @return true if capturing
    bool IsRunning() const;

    /// Get captured audio samples
    /// @param duration_ms Duration to retrieve (0 = entire buffer)
    /// @param[out] samples Output vector for audio samples
    void GetAudio(int duration_ms, audio_capture::AudioSamples& samples);

    /// Detect if speech has ended using VAD
    /// @param samples Audio samples to analyze
    /// @return VAD detection result
    audio_capture::VadResult DetectSpeech(const audio_capture::AudioSamples& samples);

    /// Clear the audio buffer
    /// @return true if cleared successfully
    bool ClearBuffer();

    /// Get the sample rate being used
    /// @return Sample rate in Hz
    int GetSampleRate() const;

    /// Get the VAD configuration
    /// @return Reference to VAD config
    const audio_capture::VadConfig& GetVadConfig() const;

    /// Update VAD configuration at runtime
    /// @param config New VAD configuration
    void SetVadConfig(const audio_capture::VadConfig& config);

    /// Check if engine is initialized
    /// @return true if initialized
    bool IsInitialized() const;

    /// Create an AudioEngine with SDL backend (convenience factory)
    /// @param capture_config Audio capture configuration
    /// @param vad_config VAD configuration
    /// @return Configured AudioEngine (not yet initialized - call Init())
    static AudioEngineConfig CreateSdlConfig(
        const audio_capture::AudioCaptureConfig& capture_config = {},
        const audio_capture::VadConfig& vad_config = {});

    /// Create an AudioEngine with Qt backend (convenience factory)
    /// @param capture_config Audio capture configuration
    /// @param vad_config VAD configuration
    /// @return Configured AudioEngine (not yet initialized - call Init())
    static AudioEngineConfig CreateQtConfig(
        const audio_capture::AudioCaptureConfig& capture_config = {},
        const audio_capture::VadConfig& vad_config = {});

private:
    /// Create the appropriate audio capture backend
    bool CreateCaptureBackend(AudioBackend backend);

    /// Create the VAD instance
    bool CreateVad();

    std::unique_ptr<audio_capture::IAudioCapture> capture_;
    std::unique_ptr<audio_capture::IVoiceActivityDetector> vad_;
    AudioEngineConfig config_;
    bool initialized_ = false;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_AUDIO_ENGINE_H
