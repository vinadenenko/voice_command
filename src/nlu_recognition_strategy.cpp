#include "recognition_strategy.h"

#include <cmath>
#include <chrono>
#include <cstdio>

namespace voice_command {

NluRecognitionStrategy::NluRecognitionStrategy(WhisperEngine* whisper_engine,
                                               INluEngine* nlu_engine,
                                               CommandRegistry* registry)
    : whisper_engine_(whisper_engine),
      nlu_engine_(nlu_engine),
      registry_(registry) {}

RecognitionResult NluRecognitionStrategy::Recognize(
    const audio_capture::AudioSamples& samples) {
    RecognitionResult result;

    auto total_start = std::chrono::steady_clock::now();

    if (whisper_engine_ == nullptr || !whisper_engine_->IsInitialized()) {
        result.error = "Whisper engine not initialized";
        return result;
    }

    if (nlu_engine_ == nullptr) {
        result.error = "NLU engine not available";
        return result;
    }

    if (registry_ == nullptr) {
        result.error = "Command registry not available";
        return result;
    }

    // Step 1: Transcribe audio to text (measure ASR time)
    auto asr_start = std::chrono::steady_clock::now();
    auto transcription = whisper_engine_->Transcribe(samples);
    auto asr_end = std::chrono::steady_clock::now();
    result.asr_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        asr_end - asr_start).count();

    if (!transcription.success) {
        result.error = "Transcription failed: " + transcription.error;
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    if (transcription.text.empty()) {
        result.error = "Empty transcription";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    // Check transcription confidence (using exp of log probability)
    float transcription_confidence = 0.0f;
    if (transcription.num_tokens > 0) {
        transcription_confidence =
            std::exp(transcription.logprob_min);  // 0.0-1.0 range
    }

    if (transcription_confidence < min_transcription_confidence_) {
        result.error = "Transcription confidence below threshold";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    result.raw_transcript = transcription.text;

    // Step 2: Process transcript with NLU (measure NLU time)
    auto descriptors = registry_->GetAllDescriptors();
    if (descriptors.empty()) {
        result.error = "No commands registered";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    auto nlu_start = std::chrono::steady_clock::now();
    auto nlu_result = nlu_engine_->Process(transcription.text, descriptors);
    auto nlu_end = std::chrono::steady_clock::now();
    result.nlu_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        nlu_end - nlu_start).count();

    if (!nlu_result.success) {
        result.error = "NLU processing failed: " + nlu_result.error_message;
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    // Check NLU confidence
    if (nlu_result.confidence < min_nlu_confidence_) {
        result.error = "NLU confidence below threshold";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - total_start).count();
        return result;
    }

    // Step 3: Build result
    result.success = true;
    result.command_name = nlu_result.command_name;
    result.confidence = nlu_result.confidence;
    result.params = nlu_result.extracted_params;

    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - total_start).count();

    return result;
}

}  // namespace voice_command
