#ifndef VOICE_COMMAND_QT_AUDIO_CAPTURE_H
#define VOICE_COMMAND_QT_AUDIO_CAPTURE_H

#include "audio_capture/iaudio_capture.h"

namespace voice_command {
namespace audio_capture {

/// Qt-based audio capture implementation (placeholder)
///
/// This is a placeholder stub for the Qt audio capture backend.
/// It will be implemented using QAudioSource (Qt6) or QAudioInput (Qt5)
/// for cross-platform audio capture support.
///
/// All methods currently throw std::runtime_error indicating
/// that the implementation is not yet available.
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
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_QT_AUDIO_CAPTURE_H
