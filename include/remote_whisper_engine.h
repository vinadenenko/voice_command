#ifndef VOICE_COMMAND_REMOTE_WHISPER_ENGINE_H
#define VOICE_COMMAND_REMOTE_WHISPER_ENGINE_H

#include <string>
#include <vector>

#include "asr_engine.h"
#include "audio_capture/iaudio_capture.h"

namespace voice_command {

/// Configuration for remote ASR server (whisper.cpp server)
struct RemoteAsrConfig {
    /// Server URL (e.g., "http://localhost:8080")
    std::string server_url;

    /// Inference endpoint path (default: "/inference")
    std::string inference_path = "/inference";

    /// HTTP request timeout in milliseconds
    int timeout_ms = 30000;

    /// Language hint for server (e.g., "en", "auto")
    std::string language = "en";

    /// Request translation to English
    bool translate = false;

    /// Temperature for sampling (0 = greedy)
    float temperature = 0.0f;
};

/// RemoteWhisperEngine sends audio to a whisper.cpp server for transcription.
///
/// This is the remote implementation of IAsrEngine using HTTP to communicate
/// with a whisper.cpp server. For local inference, see LocalWhisperEngine.
///
/// Server API (whisper.cpp server):
/// - POST /inference with multipart/form-data
/// - "file" field: WAV audio data
/// - Optional fields: language, temperature, response_format, etc.
/// - Response: {"text": "transcription"}
///
/// Thread Safety:
/// - Each HTTP request is independent
/// - Safe for concurrent use from multiple threads
class RemoteWhisperEngine : public IAsrEngine {
public:
    RemoteWhisperEngine();
    ~RemoteWhisperEngine() override;

    // Non-copyable
    RemoteWhisperEngine(const RemoteWhisperEngine&) = delete;
    RemoteWhisperEngine& operator=(const RemoteWhisperEngine&) = delete;

    // Movable
    RemoteWhisperEngine(RemoteWhisperEngine&&) noexcept;
    RemoteWhisperEngine& operator=(RemoteWhisperEngine&&) noexcept;

    /// Initialize with remote server configuration
    /// @param config Server configuration (URL, timeout, etc.)
    /// @return true if configuration is valid
    bool Init(const RemoteAsrConfig& config);

    /// Release resources (no-op for remote engine)
    void Shutdown() override;

    /// Check if engine is initialized
    /// @return true if initialized
    bool IsInitialized() const override;

    /// Get the current configuration
    /// @return Reference to configuration
    const RemoteAsrConfig& GetConfig() const;

    /// Perform speech-to-text transcription via remote server
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return Transcription result
    TranscriptionResult Transcribe(
        const audio_capture::AudioSamples& samples) override;

    /// Perform guided matching against known phrases
    ///
    /// Note: The whisper.cpp server doesn't support guided mode natively.
    /// This implementation uses Transcribe() and performs fuzzy matching
    /// locally against the provided phrases.
    ///
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @param phrases List of candidate phrases to match against
    /// @return Guided match result with best match and scores
    GuidedMatchResult GuidedMatch(
        const audio_capture::AudioSamples& samples,
        const std::vector<std::string>& phrases) override;

private:
    /// Encode audio samples as WAV data
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return WAV file data
    std::vector<char> EncodeAsWav(const audio_capture::AudioSamples& samples);

    /// Calculate normalized Levenshtein similarity between two strings
    /// @param s1 First string
    /// @param s2 Second string
    /// @return Similarity score (0.0 to 1.0)
    float CalculateSimilarity(const std::string& s1, const std::string& s2);

    RemoteAsrConfig config_;
    bool initialized_ = false;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_REMOTE_WHISPER_ENGINE_H
