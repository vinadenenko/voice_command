#ifndef VOICE_COMMAND_SDL_AUDIO_CAPTURE_H
#define VOICE_COMMAND_SDL_AUDIO_CAPTURE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "audio_capture/iaudio_capture.h"

namespace voice_command {
namespace audio_capture {

/// SDL2-based audio capture implementation
///
/// This implementation wraps SDL2 audio capture functionality, storing
/// captured audio in a thread-safe circular buffer. Audio is captured
/// as float32 mono PCM at the configured sample rate.
///
/// Thread Safety:
///   - The audio callback runs on a separate SDL thread
///   - All buffer access is protected by mutex
///   - Start/Stop/GetAudio can be called from any thread
class SdlAudioCapture : public IAudioCapture {
public:
    SdlAudioCapture();
    ~SdlAudioCapture() override;

    // Non-copyable
    SdlAudioCapture(const SdlAudioCapture&) = delete;
    SdlAudioCapture& operator=(const SdlAudioCapture&) = delete;

    // IAudioCapture interface
    bool Init(const AudioCaptureConfig& config) override;
    void Shutdown() override;
    bool Start() override;
    bool Stop() override;
    bool IsRunning() const override;
    void GetAudio(int duration_ms, AudioSamples& samples) override;
    bool Clear() override;
    int GetSampleRate() const override;
    int GetBufferDurationMs() const override;

    /// Get the list of available capture devices
    /// @return Vector of device names
    static std::vector<std::string> GetDeviceList();

private:
    /// SDL audio callback - called from SDL audio thread
    /// @param stream Pointer to audio data
    /// @param len Length of audio data in bytes
    void AudioCallback(uint8_t* stream, int len);

    /// Static callback wrapper for SDL
    static void SdlCallback(void* userdata, uint8_t* stream, int len);

    SDL_AudioDeviceID device_id_ = 0;
    int buffer_duration_ms_ = 0;
    int sample_rate_ = 0;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

    // Circular buffer for audio samples
    AudioSamples audio_buffer_;
    size_t buffer_pos_ = 0;  // Write position
    size_t buffer_len_ = 0;  // Number of valid samples
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_SDL_AUDIO_CAPTURE_H
