#include "recognition_strategy.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>

namespace voice_command {

namespace {

/// Convert string to lowercase
std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

}  // namespace

GuidedRecognitionStrategy::GuidedRecognitionStrategy(
    WhisperEngine* whisper_engine, CommandRegistry* registry)
    : whisper_engine_(whisper_engine), registry_(registry) {
    BuildPhraseMap();
}

void GuidedRecognitionStrategy::BuildPhraseMap() {
    phrase_to_command_.clear();
    all_phrases_.clear();

    if (registry_ == nullptr) {
        return;
    }

    // Get all descriptors and build mapping
    auto descriptors = registry_->GetAllDescriptors();
    for (const auto* desc : descriptors) {
        if (desc == nullptr) {
            continue;
        }

        for (const auto& phrase : desc->trigger_phrases) {
            std::string lower_phrase = ToLower(phrase);
            phrase_to_command_[lower_phrase] = desc->name;
            all_phrases_.push_back(lower_phrase);
        }
    }
}

RecognitionResult GuidedRecognitionStrategy::Recognize(
    const audio_capture::AudioSamples& samples) {
    RecognitionResult result;

    if (whisper_engine_ == nullptr || !whisper_engine_->IsInitialized()) {
        result.error = "Whisper engine not initialized";
        return result;
    }

    if (registry_ == nullptr) {
        result.error = "Command registry not available";
        return result;
    }

    // Rebuild phrase map in case registry changed
    BuildPhraseMap();

    if (all_phrases_.empty()) {
        result.error = "No trigger phrases registered";
        return result;
    }

    // Perform guided matching with timing
    auto start = std::chrono::high_resolution_clock::now();
    auto match_result = whisper_engine_->GuidedMatch(samples, all_phrases_);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    fprintf(stdout, "[Whisper] Guided matching took %ld ms\n", duration_ms);

    if (!match_result.success) {
        result.error = match_result.error;
        return result;
    }

    // Check confidence threshold
    if (match_result.best_score < min_confidence_) {
        result.error = "Confidence below threshold";
        return result;
    }

    // Map phrase back to command
    auto it = phrase_to_command_.find(ToLower(match_result.best_match));
    if (it == phrase_to_command_.end()) {
        result.error = "Matched phrase not found in mapping";
        return result;
    }

    result.success = true;
    result.command_name = it->second;
    result.confidence = match_result.best_score;
    result.raw_transcript = match_result.best_match;
    // No parameters for guided recognition

    return result;
}

}  // namespace voice_command
