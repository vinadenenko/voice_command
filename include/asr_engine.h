#ifndef VOICE_COMMAND_ASR_ENGINE_H
#define VOICE_COMMAND_ASR_ENGINE_H

#include <string>
#include <vector>

#include "audio_capture/iaudio_capture.h"

namespace voice_command {

/// Result of a transcription operation
struct TranscriptionResult {
    /// Whether transcription succeeded
    bool success = false;

    /// Transcribed text (trimmed)
    std::string text;

    /// Minimum log probability across tokens (for confidence)
    float logprob_min = 0.0f;

    /// Sum of log probabilities
    float logprob_sum = 0.0f;

    /// Number of tokens produced
    int num_tokens = 0;

    /// Processing time in milliseconds
    int64_t processing_time_ms = 0;

    /// Error message if !success
    std::string error;
};

/// Result of a guided matching operation
struct GuidedMatchResult {
    /// Whether matching succeeded
    bool success = false;

    /// Index of the best matching phrase in the input list
    int best_match_index = -1;

    /// The best matching phrase text
    std::string best_match;

    /// Probability of the best match (0.0-1.0)
    float best_score = 0.0f;

    /// Scores for all phrases (same order as input)
    std::vector<float> all_scores;

    /// Processing time in milliseconds
    int64_t processing_time_ms = 0;

    /// Error message if !success
    std::string error;
};

/// Abstract interface for Automatic Speech Recognition (ASR) engines.
///
/// This interface abstracts the ASR functionality, allowing different
/// implementations:
/// - LocalWhisperEngine: Local inference using whisper.cpp
/// - RemoteWhisperEngine: HTTP client to whisper.cpp server
/// - Future: Other ASR backends (Vosk, DeepSpeech, etc.)
///
/// Thread Safety:
/// - Implementations should document their thread safety guarantees
/// - Generally, single engine instances are NOT thread-safe for concurrent inference
class IAsrEngine {
public:
    virtual ~IAsrEngine() = default;

    /// Release all resources
    virtual void Shutdown() = 0;

    /// Check if engine is initialized and ready for inference
    /// @return true if initialized
    virtual bool IsInitialized() const = 0;

    /// Perform general speech-to-text transcription
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return Transcription result
    virtual TranscriptionResult Transcribe(
        const audio_capture::AudioSamples& samples) = 0;

    /// Perform guided matching against known phrases
    ///
    /// This scores how likely the audio matches each of the provided phrases.
    /// Useful for command recognition when the set of possible commands is
    /// known ahead of time.
    ///
    /// For remote backends that don't support guided mode natively,
    /// implementations should use Transcribe() and perform fuzzy matching.
    ///
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @param phrases List of candidate phrases to match against
    /// @return Guided match result with best match and scores
    virtual GuidedMatchResult GuidedMatch(
        const audio_capture::AudioSamples& samples,
        const std::vector<std::string>& phrases) = 0;

    /// Get the sample rate expected by ASR engines
    /// @return Sample rate in Hz (16000 for whisper-based engines)
    static constexpr int GetExpectedSampleRate() { return 16000; }
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_ASR_ENGINE_H
