#ifndef RULE_BASED_NLU_ENGINE_H
#define RULE_BASED_NLU_ENGINE_H

#include "command/nlu/inlu_engine.h"

#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voice_command {

// Rule-based NLU engine using pattern matching.
// Uses string similarity for intent matching and regex/keyword
// patterns for parameter extraction.
//
// This engine is suitable for structured commands with predictable
// patterns. For complex natural language, use LlmNluEngine.
class RuleBasedNluEngine : public INluEngine {
public:
    RuleBasedNluEngine() = default;
    ~RuleBasedNluEngine() override = default;

    bool Init() override;

    NluResult Process(const std::string& transcript,
                      const std::vector<const CommandDescriptor*>& schemas) override;

    // Configuration
    void SetMinConfidence(float threshold) { min_confidence_ = threshold; }
    float GetMinConfidence() const { return min_confidence_; }

private:
    // Result of intent matching: descriptor, confidence, matched trigger phrase.
    struct IntentMatch {
        const CommandDescriptor* descriptor = nullptr;
        float confidence = 0.0f;
        std::string matched_trigger;
    };

    // Intent matching using trigger phrase similarity.
    // Returns the best matching command descriptor, confidence score, and matched trigger.
    IntentMatch MatchIntent(
        const std::string& transcript,
        const std::vector<const CommandDescriptor*>& schemas) const;

    // Extract the arguments region from transcript by removing the trigger phrase.
    // Returns the portion of transcript after the matched trigger phrase.
    std::string ExtractArgumentsRegion(
        const std::string& transcript,
        const std::string& matched_trigger) const;

    // Extract all parameters from the arguments region based on command schema.
    std::unordered_map<std::string, std::string> ExtractParams(
        const std::string& arguments_region,
        const CommandDescriptor& descriptor) const;

    // Extract value for a specific parameter based on its type.
    std::string ExtractParamValue(
        const std::string& transcript,
        const ParamDescriptor& param) const;

    // String similarity using Levenshtein distance (0.0 - 1.0).
    static float ComputeSimilarity(const std::string& a, const std::string& b);

    // Normalize string: lowercase, trim whitespace.
    static std::string Normalize(const std::string& str);

    // Find all integers in text.
    static std::vector<std::pair<std::string, size_t>> FindIntegers(
        const std::string& text);

    // Find all floating point numbers in text.
    static std::vector<std::pair<std::string, size_t>> FindDoubles(
        const std::string& text);

    // Find position of keyword in text (case-insensitive).
    static size_t FindKeyword(const std::string& text, const std::string& keyword);

    float min_confidence_ = 0.5f;
};

}  // namespace voice_command

#endif // RULE_BASED_NLU_ENGINE_H
