#include "command/nlu/rule_based_nlu_engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <regex>
#include <sstream>

namespace voice_command {

namespace {

// Trim whitespace from both ends
std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// Convert to lowercase
std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Check if string contains substring (case-insensitive)
bool ContainsIgnoreCase(const std::string& text, const std::string& substr) {
    std::string text_lower = ToLower(text);
    std::string substr_lower = ToLower(substr);
    return text_lower.find(substr_lower) != std::string::npos;
}

}  // namespace

bool RuleBasedNluEngine::Init() {
    // No initialization needed for rule-based engine
    return true;
}

NluResult RuleBasedNluEngine::Process(
    const std::string& transcript,
    const std::vector<const CommandDescriptor*>& schemas) {
    NluResult result;

    if (transcript.empty()) {
        result.success = false;
        result.error_message = "Empty transcript";
        return result;
    }

    if (schemas.empty()) {
        result.success = false;
        result.error_message = "No command schemas provided";
        return result;
    }

    // Step 1: Match intent (get descriptor, confidence, and matched trigger)
    auto intent_match = MatchIntent(transcript, schemas);

    if (!intent_match.descriptor || intent_match.confidence < min_confidence_) {
        result.success = false;
        result.error_message = "No matching command found (confidence too low)";
        return result;
    }

    // Step 2: Extract arguments region (transcript minus the trigger phrase)
    std::string args_region = ExtractArgumentsRegion(transcript, intent_match.matched_trigger);

    // Step 3: Extract parameters from arguments region
    auto params = ExtractParams(args_region, *intent_match.descriptor);

    // Step 4: Build result
    result.success = true;
    result.command_name = intent_match.descriptor->name;
    result.confidence = intent_match.confidence;
    result.extracted_params = std::move(params);

    return result;
}

RuleBasedNluEngine::IntentMatch RuleBasedNluEngine::MatchIntent(
    const std::string& transcript,
    const std::vector<const CommandDescriptor*>& schemas) const {

    std::string normalized_transcript = Normalize(transcript);

    IntentMatch best_match;

    for (const auto* descriptor : schemas) {
        // Check similarity against each trigger phrase
        for (const auto& trigger : descriptor->trigger_phrases) {
            std::string normalized_trigger = Normalize(trigger);
            float score = ComputeSimilarity(normalized_transcript, normalized_trigger);

            // Also check if transcript contains the trigger phrase
            if (ContainsIgnoreCase(normalized_transcript, normalized_trigger)) {
                // Boost score if trigger is contained in transcript
                score = std::max(score, 0.8f);
            }

            if (score > best_match.confidence) {
                best_match.confidence = score;
                best_match.descriptor = descriptor;
                best_match.matched_trigger = trigger;
            }
        }

        // Also check against command name
        std::string normalized_name = Normalize(descriptor->name);
        // Replace underscores with spaces for comparison
        std::replace(normalized_name.begin(), normalized_name.end(), '_', ' ');
        float name_score = ComputeSimilarity(normalized_transcript, normalized_name);
        if (name_score > best_match.confidence) {
            best_match.confidence = name_score;
            best_match.descriptor = descriptor;
            // Use command name as pseudo-trigger for args extraction
            best_match.matched_trigger = descriptor->name;
            std::replace(best_match.matched_trigger.begin(),
                         best_match.matched_trigger.end(), '_', ' ');
        }
    }

    return best_match;
}

std::string RuleBasedNluEngine::ExtractArgumentsRegion(
    const std::string& transcript,
    const std::string& matched_trigger) const {

    std::string normalized_transcript = Normalize(transcript);
    std::string normalized_trigger = Normalize(matched_trigger);

    // Find where the trigger phrase appears in the transcript
    size_t trigger_pos = normalized_transcript.find(normalized_trigger);

    if (trigger_pos != std::string::npos) {
        // Extract everything after the trigger phrase
        size_t args_start = trigger_pos + normalized_trigger.length();

        // Skip leading whitespace
        while (args_start < normalized_transcript.length() &&
               std::isspace(static_cast<unsigned char>(normalized_transcript[args_start]))) {
            args_start++;
        }

        if (args_start < normalized_transcript.length()) {
            return normalized_transcript.substr(args_start);
        }
        return "";
    }

    // Trigger not found exactly - try word-by-word matching
    // Split transcript and trigger into words
    std::vector<std::string> transcript_words;
    std::vector<std::string> trigger_words;

    std::istringstream transcript_stream(normalized_transcript);
    std::istringstream trigger_stream(normalized_trigger);
    std::string word;

    while (transcript_stream >> word) {
        transcript_words.push_back(word);
    }
    while (trigger_stream >> word) {
        trigger_words.push_back(word);
    }

    if (trigger_words.empty()) {
        return normalized_transcript;
    }

    // Find the best starting position for trigger match in transcript
    size_t best_start = 0;
    float best_match_score = 0.0f;

    for (size_t start = 0; start <= transcript_words.size() - trigger_words.size(); start++) {
        float match_score = 0.0f;
        for (size_t i = 0; i < trigger_words.size() && start + i < transcript_words.size(); i++) {
            if (transcript_words[start + i] == trigger_words[i]) {
                match_score += 1.0f;
            }
        }
        match_score /= static_cast<float>(trigger_words.size());

        if (match_score > best_match_score) {
            best_match_score = match_score;
            best_start = start;
        }
    }

    // If we found a reasonable match, return words after the trigger
    if (best_match_score >= 0.5f) {
        size_t args_start_word = best_start + trigger_words.size();
        if (args_start_word < transcript_words.size()) {
            std::string result;
            for (size_t i = args_start_word; i < transcript_words.size(); i++) {
                if (!result.empty()) result += " ";
                result += transcript_words[i];
            }
            return result;
        }
        return "";
    }

    // Fallback: return original transcript (no trigger found)
    return normalized_transcript;
}

std::unordered_map<std::string, std::string> RuleBasedNluEngine::ExtractParams(
    const std::string& transcript,
    const CommandDescriptor& descriptor) const {

    std::unordered_map<std::string, std::string> params;

    for (const auto& param : descriptor.parameters) {
        std::string value = ExtractParamValue(transcript, param);
        if (!value.empty()) {
            params[param.name] = value;
        }
    }

    return params;
}

std::string RuleBasedNluEngine::ExtractParamValue(
    const std::string& transcript,
    const ParamDescriptor& param) const {

    std::string text = Normalize(transcript);

    switch (param.type) {
        case ParamType::kInteger: {
            // Find all integers in text
            auto integers = FindIntegers(text);
            if (integers.empty()) return "";

            // If only one integer, use it
            if (integers.size() == 1) {
                return integers[0].first;
            }

            // Multiple integers: find one closest to parameter keyword
            std::string param_keyword = ToLower(param.name);
            std::replace(param_keyword.begin(), param_keyword.end(), '_', ' ');

            size_t keyword_pos = FindKeyword(text, param_keyword);
            if (keyword_pos == std::string::npos) {
                // Try finding keyword in description
                // For now, just return first integer
                return integers[0].first;
            }

            // Find integer closest to keyword
            size_t min_distance = std::numeric_limits<size_t>::max();
            std::string closest_value;
            for (const auto& [value, pos] : integers) {
                size_t distance = (pos > keyword_pos) ? (pos - keyword_pos)
                                                      : (keyword_pos - pos);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_value = value;
                }
            }
            return closest_value;
        }

        case ParamType::kDouble: {
            auto numbers = FindDoubles(text);
            if (numbers.empty()) return "";
            // Similar logic to kInteger
            return numbers[0].first;
        }

        case ParamType::kBool: {
            // Look for boolean keywords
            if (ContainsIgnoreCase(text, "yes") ||
                ContainsIgnoreCase(text, "true") ||
                ContainsIgnoreCase(text, "enable") ||
                ContainsIgnoreCase(text, "on")) {
                return "true";
            }
            if (ContainsIgnoreCase(text, "no") ||
                ContainsIgnoreCase(text, "false") ||
                ContainsIgnoreCase(text, "disable") ||
                ContainsIgnoreCase(text, "off")) {
                return "false";
            }
            return "";
        }

        case ParamType::kEnum: {
            // Find which enum value appears in text
            for (const auto& enum_value : param.enum_values) {
                if (ContainsIgnoreCase(text, enum_value)) {
                    return enum_value;
                }
            }
            return "";
        }

        case ParamType::kString: {
            // String extraction from arguments region
            // The text has already had the trigger phrase stripped, so it may
            // contain just the value itself (e.g., "red" from "change color to red")

            std::vector<std::string> prepositions = {"to", "at", "near", "called", "named"};

            std::string param_keyword = ToLower(param.name);
            std::replace(param_keyword.begin(), param_keyword.end(), '_', ' ');

            // First try: look for "param_name <value>" pattern
            size_t keyword_pos = FindKeyword(text, param_keyword);
            if (keyword_pos != std::string::npos) {
                // Extract word(s) after keyword
                size_t start = keyword_pos + param_keyword.length();
                while (start < text.length() && std::isspace(text[start])) start++;

                if (start < text.length()) {
                    std::istringstream iss(text.substr(start));
                    std::string word;
                    std::string result;
                    int word_count = 0;
                    while (iss >> word && word_count < 3) {
                        if (!result.empty()) result += " ";
                        result += word;
                        word_count++;
                    }
                    // Strip trailing punctuation
                    while (!result.empty() &&
                           std::ispunct(static_cast<unsigned char>(result.back()))) {
                        result.pop_back();
                    }
                    if (!result.empty()) {
                        return result;
                    }
                }
            }

            // Second try: look for preposition patterns
            for (const auto& prep : prepositions) {
                size_t prep_pos = FindKeyword(text, prep);
                if (prep_pos != std::string::npos) {
                    size_t start = prep_pos + prep.length();
                    while (start < text.length() && std::isspace(text[start])) start++;

                    if (start < text.length()) {
                        std::istringstream iss(text.substr(start));
                        std::string word;
                        std::string result;
                        int word_count = 0;
                        while (iss >> word && word_count < 4) {
                            if (!result.empty()) result += " ";
                            result += word;
                            word_count++;
                        }
                        // Strip trailing punctuation
                        while (!result.empty() &&
                               std::ispunct(static_cast<unsigned char>(result.back()))) {
                            result.pop_back();
                        }
                        if (!result.empty()) {
                            return result;
                        }
                    }
                }
            }

            // Third try: if no keyword or preposition found, use the entire text
            // This handles the case where the arguments region is just the value
            // (e.g., "red" after stripping trigger "change color to")
            if (!text.empty()) {
                std::string result = text;
                // Strip trailing punctuation
                while (!result.empty() &&
                       std::ispunct(static_cast<unsigned char>(result.back()))) {
                    result.pop_back();
                }
                // Strip leading/trailing whitespace
                size_t start = result.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    size_t end = result.find_last_not_of(" \t");
                    return result.substr(start, end - start + 1);
                }
            }

            return "";
        }

        default:
            return "";
    }
}

float RuleBasedNluEngine::ComputeSimilarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    // Levenshtein distance
    size_t m = a.length();
    size_t n = b.length();

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; i++) dp[i][0] = i;
    for (size_t j = 0; j <= n; j++) dp[0][j] = j;

    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,      // deletion
                dp[i][j - 1] + 1,      // insertion
                dp[i - 1][j - 1] + cost // substitution
            });
        }
    }

    size_t distance = dp[m][n];
    size_t max_len = std::max(m, n);

    return 1.0f - static_cast<float>(distance) / static_cast<float>(max_len);
}

std::string RuleBasedNluEngine::Normalize(const std::string& str) {
    return ToLower(Trim(str));
}

std::vector<std::pair<std::string, size_t>> RuleBasedNluEngine::FindIntegers(
    const std::string& text) {

    std::vector<std::pair<std::string, size_t>> results;
    std::regex int_regex(R"(\b(\d+)\b)");

    auto begin = std::sregex_iterator(text.begin(), text.end(), int_regex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        results.emplace_back(match.str(), match.position());
    }

    return results;
}

std::vector<std::pair<std::string, size_t>> RuleBasedNluEngine::FindDoubles(
    const std::string& text) {

    std::vector<std::pair<std::string, size_t>> results;
    std::regex double_regex(R"(\b(\d+\.?\d*)\b)");

    auto begin = std::sregex_iterator(text.begin(), text.end(), double_regex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        results.emplace_back(match.str(), match.position());
    }

    return results;
}

size_t RuleBasedNluEngine::FindKeyword(const std::string& text, const std::string& keyword) {
    std::string text_lower = ToLower(text);
    std::string keyword_lower = ToLower(keyword);
    return text_lower.find(keyword_lower);
}

}  // namespace voice_command
