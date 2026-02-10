#ifndef VOICE_COMMAND_RECOGNITION_STRATEGY_H
#define VOICE_COMMAND_RECOGNITION_STRATEGY_H

#include <memory>
#include <string>
#include <unordered_map>

#include "audio_capture/iaudio_capture.h"
#include "command/context/commandcontext.h"
#include "command/nlu/inlu_engine.h"
#include "command/registry/commandregistry.h"
#include "whisper_engine.h"

namespace voice_command {

/// Result of recognition
struct RecognitionResult {
    /// Whether recognition succeeded
    bool success = false;

    /// Recognized command name
    std::string command_name;

    /// Confidence score (0.0-1.0)
    float confidence = 0.0f;

    /// Extracted parameters (if any)
    std::unordered_map<std::string, std::string> params;

    /// Raw transcript (if available)
    std::string raw_transcript;

    /// Error message if !success
    std::string error;
};

/// Abstract interface for recognition strategies
///
/// Recognition strategies encapsulate different approaches to mapping
/// audio to commands. The strategy pattern allows switching between
/// simple (guided) and complex (NLU-based) recognition.
class IRecognitionStrategy {
public:
    virtual ~IRecognitionStrategy() = default;

    /// Recognize a command from audio samples
    /// @param samples Audio samples (float32 mono PCM at 16kHz)
    /// @return Recognition result
    virtual RecognitionResult Recognize(
        const audio_capture::AudioSamples& samples) = 0;

    /// Get the strategy name for logging/debugging
    /// @return Strategy name
    virtual std::string GetName() const = 0;
};

/// GuidedRecognitionStrategy uses whisper's guided matching for simple commands.
///
/// This strategy is optimal when:
/// - All commands are simple (no parameters)
/// - The set of trigger phrases is small and known
/// - Low latency is critical
///
/// Algorithm:
/// 1. Get all trigger phrases from registry
/// 2. Use WhisperEngine::GuidedMatch to score audio against phrases
/// 3. Map best matching phrase back to command name
class GuidedRecognitionStrategy : public IRecognitionStrategy {
public:
    /// Create a guided recognition strategy
    /// @param whisper_engine Pointer to whisper engine (not owned)
    /// @param registry Pointer to command registry (not owned)
    explicit GuidedRecognitionStrategy(WhisperEngine* whisper_engine,
                                       CommandRegistry* registry);

    ~GuidedRecognitionStrategy() override = default;

    // Non-copyable
    GuidedRecognitionStrategy(const GuidedRecognitionStrategy&) = delete;
    GuidedRecognitionStrategy& operator=(const GuidedRecognitionStrategy&) = delete;

    RecognitionResult Recognize(
        const audio_capture::AudioSamples& samples) override;

    std::string GetName() const override { return "GuidedRecognition"; }

    /// Set minimum confidence threshold for accepting a match
    /// @param threshold Minimum confidence (0.0-1.0)
    void SetMinConfidence(float threshold) { min_confidence_ = threshold; }

    /// Get the minimum confidence threshold
    /// @return Current threshold
    float GetMinConfidence() const { return min_confidence_; }

private:
    /// Build the mapping from trigger phrase to command name
    void BuildPhraseMap();

    WhisperEngine* whisper_engine_;  // Not owned
    CommandRegistry* registry_;      // Not owned

    /// Map from trigger phrase (lowercase) to command name
    std::unordered_map<std::string, std::string> phrase_to_command_;

    /// List of all trigger phrases (in order for whisper)
    std::vector<std::string> all_phrases_;

    float min_confidence_ = 0.3f;
};

/// NluRecognitionStrategy uses full transcription + NLU for parameterized commands.
///
/// This strategy is required when:
/// - Commands have parameters that need extraction
/// - Free-form input needs intent classification
/// - More sophisticated understanding is needed
///
/// Algorithm:
/// 1. Use WhisperEngine::Transcribe to get text
/// 2. Pass transcript to INluEngine::Process with command schemas
/// 3. Return identified command with extracted parameters
class NluRecognitionStrategy : public IRecognitionStrategy {
public:
    /// Create an NLU recognition strategy
    /// @param whisper_engine Pointer to whisper engine (not owned)
    /// @param nlu_engine Pointer to NLU engine (not owned)
    /// @param registry Pointer to command registry (not owned)
    NluRecognitionStrategy(WhisperEngine* whisper_engine,
                           INluEngine* nlu_engine,
                           CommandRegistry* registry);

    ~NluRecognitionStrategy() override = default;

    // Non-copyable
    NluRecognitionStrategy(const NluRecognitionStrategy&) = delete;
    NluRecognitionStrategy& operator=(const NluRecognitionStrategy&) = delete;

    RecognitionResult Recognize(
        const audio_capture::AudioSamples& samples) override;

    std::string GetName() const override { return "NluRecognition"; }

    /// Set minimum confidence threshold for transcription
    /// @param threshold Minimum confidence (0.0-1.0)
    void SetMinTranscriptionConfidence(float threshold) {
        min_transcription_confidence_ = threshold;
    }

    /// Set minimum confidence threshold for NLU
    /// @param threshold Minimum confidence (0.0-1.0)
    void SetMinNluConfidence(float threshold) {
        min_nlu_confidence_ = threshold;
    }

private:
    WhisperEngine* whisper_engine_;  // Not owned
    INluEngine* nlu_engine_;         // Not owned
    CommandRegistry* registry_;      // Not owned

    float min_transcription_confidence_ = 0.0f;  // Use any transcription
    float min_nlu_confidence_ = 0.3f;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_RECOGNITION_STRATEGY_H
