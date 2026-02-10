#ifndef VOICE_COMMAND_QT_AUDIO_CAPTURE_H
#define VOICE_COMMAND_QT_AUDIO_CAPTURE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio_capture/iaudio_capture.h"

// Forward declarations to avoid Qt headers in public interface
class QAudioSource;
class QIODevice;

namespace voice_command {
namespace audio_capture {

// Forward declaration of implementation detail
class AudioBufferDevice;

/// Qt6-based audio capture implementation
///
/// This implementation uses Qt6 Multimedia (QAudioSource) for cross-platform
/// audio capture. Audio is captured as Int16 PCM and converted to Float32
/// for whisper.cpp compatibility.
///
/// Thread Safety:
///   - Qt audio runs on its own thread
///   - All buffer access is protected by mutex
///   - Start/Stop/GetAudio can be called from any thread
///
/// Requirements:
///   - Qt6 with Multimedia module
///   - A running QCoreApplication (or QApplication) event loop
class QtAudioCapture : public IAudioCapture {
public:
    QtAudioCapture();
    ~QtAudioCapture() override;

    // Non-copyable
    QtAudioCapture(const QtAudioCapture&) = delete;
    QtAudioCapture& operator=(const QtAudioCapture&) = delete;

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
    friend class AudioBufferDevice;

    /// Called by AudioBufferDevice when new audio data arrives
    /// @param data Raw Int16 audio data
    /// @param len Length of data in bytes
    void OnAudioData(const char* data, int64_t len);

    std::unique_ptr<QAudioSource> audio_source_;
    std::unique_ptr<AudioBufferDevice> buffer_device_;

    int buffer_duration_ms_ = 0;
    int sample_rate_ = 0;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

    // Circular buffer for audio samples (Float32)
    AudioSamples audio_buffer_;
    size_t buffer_pos_ = 0;  // Write position
    size_t buffer_len_ = 0;  // Number of valid samples
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_QT_AUDIO_CAPTURE_H
