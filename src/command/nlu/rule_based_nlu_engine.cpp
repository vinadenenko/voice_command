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

    // Step 1: Match intent
    auto [matched_descriptor, confidence] = MatchIntent(transcript, schemas);

    if (!matched_descriptor || confidence < min_confidence_) {
        result.success = false;
        result.error_message = "No matching command found (confidence too low)";
        return result;
    }

    // Step 2: Extract parameters
    auto params = ExtractParams(transcript, *matched_descriptor);

    // Step 3: Build result
    result.success = true;
    result.command_name = matched_descriptor->name;
    result.confidence = confidence;
    result.extracted_params = std::move(params);

    return result;
}

std::pair<const CommandDescriptor*, float> RuleBasedNluEngine::MatchIntent(
    const std::string& transcript,
    const std::vector<const CommandDescriptor*>& schemas) const {

    std::string normalized_transcript = Normalize(transcript);

    const CommandDescriptor* best_match = nullptr;
    float best_score = 0.0f;

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

            if (score > best_score) {
                best_score = score;
                best_match = descriptor;
            }
        }

        // Also check against command name
        std::string normalized_name = Normalize(descriptor->name);
        // Replace underscores with spaces for comparison
        std::replace(normalized_name.begin(), normalized_name.end(), '_', ' ');
        float name_score = ComputeSimilarity(normalized_transcript, normalized_name);
        if (name_score > best_score) {
            best_score = name_score;
            best_match = descriptor;
        }
    }

    return {best_match, best_score};
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
            // String extraction is more complex
            // Try to find value after prepositions like "to", "at", "near", "called"
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
                    // Take remaining text or until next known keyword
                    size_t end = text.length();
                    // Simple heuristic: take up to 3 words
                    std::istringstream iss(text.substr(start));
                    std::string word;
                    std::string result;
                    int word_count = 0;
                    while (iss >> word && word_count < 3) {
                        if (!result.empty()) result += " ";
                        result += word;
                        word_count++;
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
                        // Take remaining words (simplified)
                        std::istringstream iss(text.substr(start));
                        std::string word;
                        std::string result;
                        int word_count = 0;
                        while (iss >> word && word_count < 4) {
                            if (!result.empty()) result += " ";
                            result += word;
                            word_count++;
                        }
                        if (!result.empty()) {
                            return result;
                        }
                    }
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
