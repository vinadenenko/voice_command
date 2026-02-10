#ifndef VOICE_COMMAND_IAUDIO_CAPTURE_H
#define VOICE_COMMAND_IAUDIO_CAPTURE_H

#include <vector>

#include "audio_capture/audio_capture_config.h"

namespace voice_command {
namespace audio_capture {

/// Type alias for audio samples (float32 mono PCM, matches whisper.cpp)
using AudioSamples = std::vector<float>;

/// Abstract interface for audio capture backends
///
/// This interface defines the contract for audio capture implementations.
/// Implementations must be thread-safe, using mutex-protected circular buffers
/// for storing captured audio data.
///
/// Lifecycle:
///   1. Create instance with configuration
///   2. Call Init() to initialize the audio device
///   3. Call Start() to begin capturing
///   4. Call GetAudio() to retrieve captured samples
///   5. Call Stop() to pause capturing
///   6. Call Shutdown() to release resources
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    /// Initialize the audio capture device
    /// @param config Configuration parameters for the capture device
    /// @return true if initialization succeeded, false otherwise
    virtual bool Init(const AudioCaptureConfig& config) = 0;

    /// Release all resources and close the audio device
    virtual void Shutdown() = 0;

    /// Start capturing audio
    /// @return true if capture started successfully, false otherwise
    virtual bool Start() = 0;

    /// Stop capturing audio (pause)
    /// @return true if capture stopped successfully, false otherwise
    virtual bool Stop() = 0;

    /// Check if audio capture is currently running
    /// @return true if capturing, false otherwise
    virtual bool IsRunning() const = 0;

    /// Retrieve captured audio samples
    /// @param duration_ms Duration of audio to retrieve in milliseconds
    ///                    (0 or negative = entire buffer)
    /// @param[out] samples Output vector for audio samples
    virtual void GetAudio(int duration_ms, AudioSamples& samples) = 0;

    /// Clear the audio buffer
    /// @return true if buffer was cleared successfully, false otherwise
    virtual bool Clear() = 0;

    /// Get the actual sample rate used by the device
    /// @return Sample rate in Hz
    virtual int GetSampleRate() const = 0;

    /// Get the configured buffer duration
    /// @return Buffer duration in milliseconds
    virtual int GetBufferDurationMs() const = 0;
};

}  // namespace audio_capture
}  // namespace voice_command

#endif  // VOICE_COMMAND_IAUDIO_CAPTURE_H
