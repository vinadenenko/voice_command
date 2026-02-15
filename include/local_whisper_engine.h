#ifndef VOICE_COMMAND_LOCAL_WHISPER_ENGINE_H
#define VOICE_COMMAND_LOCAL_WHISPER_ENGINE_H

#include <memory>
#include <string>
#include <vector>

#include "asr_engine.h"
#include "audio_capture/iaudio_capture.h"

// Forward declaration to avoid whisper.h in public header
struct whisper_context;

namespace voice_command {

/// Configuration for LocalWhisperEngine
struct LocalWhisperEngineConfig {
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

/// LocalWhisperEngine wraps whisper.cpp providing local speech-to-text.
///
/// This is the local implementation of IAsrEngine using whisper.cpp for
/// on-device inference. For remote inference via whisper.cpp server,
/// see RemoteWhisperEngine.
///
/// Responsibilities:
/// - Manages whisper model loading and context
/// - Provides general transcription (Transcribe)
/// - Provides guided matching against known phrases (GuidedMatch)
///
/// Thread Safety:
/// - Single whisper context is NOT thread-safe for concurrent inference
/// - Use separate LocalWhisperEngine instances for multi-threaded processing
/// - Init/Shutdown must not be called concurrently with Transcribe/GuidedMatch
class LocalWhisperEngine : public IAsrEngine {
public:
    LocalWhisperEngine();
    ~LocalWhisperEngine() override;

    // Non-copyable
    LocalWhisperEngine(const LocalWhisperEngine&) = delete;
    LocalWhisperEngine& operator=(const LocalWhisperEngine&) = delete;

    // Movable
    LocalWhisperEngine(LocalWhisperEngine&&) noexcept;
    LocalWhisperEngine& operator=(LocalWhisperEngine&&) noexcept;

    /// Initialize the whisper engine with configuration
    /// @param config Engine configuration (model path, settings)
    /// @return true if initialization succeeded
    bool Init(const LocalWhisperEngineConfig& config);

    /// Release all resources
    void Shutdown() override;

    /// Check if engine is initialized
    /// @return true if initialized
    bool IsInitialized() const override;

    /// Get the current configuration
    /// @return Reference to configuration
    const LocalWhisperEngineConfig& GetConfig() const;

    /// Perform general speech-to-text transcription
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return Transcription result
    TranscriptionResult Transcribe(
        const audio_capture::AudioSamples& samples) override;

    /// Perform guided matching against known phrases
    ///
    /// This uses whisper's logits to score how likely the audio matches
    /// each of the provided phrases. Useful for command recognition when
    /// the set of possible commands is known ahead of time.
    ///
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @param phrases List of candidate phrases to match against
    /// @return Guided match result with best match and scores
    GuidedMatchResult GuidedMatch(
        const audio_capture::AudioSamples& samples,
        const std::vector<std::string>& phrases) override;

private:
    /// Build the prompt for guided matching
    std::string BuildGuidedPrompt(const std::vector<std::string>& phrases) const;

    /// Tokenize a phrase using the whisper model
    std::vector<int> TokenizePhrase(const std::string& phrase) const;

    whisper_context* ctx_ = nullptr;
    LocalWhisperEngineConfig config_;
    bool initialized_ = false;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_LOCAL_WHISPER_ENGINE_H
