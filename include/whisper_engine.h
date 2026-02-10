#ifndef VOICE_COMMAND_WHISPER_ENGINE_H
#define VOICE_COMMAND_WHISPER_ENGINE_H

#include <memory>
#include <string>
#include <vector>

#include "audio_capture/iaudio_capture.h"

// Forward declaration to avoid whisper.h in public header
struct whisper_context;

namespace voice_command {

/// Configuration for WhisperEngine
struct WhisperEngineConfig {
    /// Path to the whisper model file (.bin or .gguf)
    std::string model_path;

    /// Number of threads for inference
    int num_threads = 4;

    /// Maximum tokens per transcription segment
    int max_tokens = 32;

    /// Audio context size (0 = use default)
    int audio_ctx = 0;

    /// Language code (e.g., "en", "es", "auto")
    std::string language = "en";

    /// Enable translation to English
    bool translate = false;

    /// Use GPU acceleration if available
    bool use_gpu = true;

    /// Enable flash attention
    bool flash_attn = true;

    /// Print special tokens in output
    bool print_special = false;

    /// Temperature for sampling (0 = greedy)
    float temperature = 0.0f;

    /// Beam search beam size
    int beam_size = 5;
};

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

/// WhisperEngine wraps whisper.cpp providing speech-to-text capabilities.
///
/// Responsibilities:
/// - Manages whisper model loading and context
/// - Provides general transcription (Transcribe)
/// - Provides guided matching against known phrases (GuidedMatch)
///
/// Thread Safety:
/// - Single whisper context is NOT thread-safe for concurrent inference
/// - Use separate WhisperEngine instances for multi-threaded processing
/// - Init/Shutdown must not be called concurrently with Transcribe/GuidedMatch
class WhisperEngine {
public:
    WhisperEngine();
    ~WhisperEngine();

    // Non-copyable
    WhisperEngine(const WhisperEngine&) = delete;
    WhisperEngine& operator=(const WhisperEngine&) = delete;

    // Movable
    WhisperEngine(WhisperEngine&&) noexcept;
    WhisperEngine& operator=(WhisperEngine&&) noexcept;

    /// Initialize the whisper engine with configuration
    /// @param config Engine configuration (model path, settings)
    /// @return true if initialization succeeded
    bool Init(const WhisperEngineConfig& config);

    /// Release all resources
    void Shutdown();

    /// Check if engine is initialized
    /// @return true if initialized
    bool IsInitialized() const;

    /// Get the current configuration
    /// @return Reference to configuration
    const WhisperEngineConfig& GetConfig() const;

    /// Perform general speech-to-text transcription
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return Transcription result
    TranscriptionResult Transcribe(const audio_capture::AudioSamples& samples);

    /// Perform guided matching against known phrases
    ///
    /// This uses whisper's logits to score how likely the audio matches
    /// each of the provided phrases. Useful for command recognition when
    /// the set of possible commands is known ahead of time.
    ///
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @param phrases List of candidate phrases to match against
    /// @return Guided match result with best match and scores
    GuidedMatchResult GuidedMatch(const audio_capture::AudioSamples& samples,
                                  const std::vector<std::string>& phrases);

    /// Get the sample rate expected by the engine
    /// @return Sample rate in Hz (always 16000 for whisper)
    static constexpr int GetExpectedSampleRate() { return 16000; }

private:
    /// Build the prompt for guided matching
    std::string BuildGuidedPrompt(const std::vector<std::string>& phrases) const;

    /// Tokenize a phrase using the whisper model
    std::vector<int> TokenizePhrase(const std::string& phrase) const;

    whisper_context* ctx_ = nullptr;
    WhisperEngineConfig config_;
    bool initialized_ = false;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_WHISPER_ENGINE_H
