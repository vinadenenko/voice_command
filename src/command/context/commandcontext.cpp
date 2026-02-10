#include "command/context/commandcontext.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace voice_command {

// Helper function for case-insensitive string comparison
namespace {

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

}  // namespace

// ParamValue implementation

ParamValue::ParamValue(std::string raw_value)
    : raw_value_(std::move(raw_value)) {}

std::string ParamValue::AsString() const {
    return raw_value_;
}

int ParamValue::AsInt() const {
    try {
        size_t pos = 0;
        int result = std::stoi(raw_value_, &pos);
        // Ensure entire string was consumed (no trailing garbage)
        if (pos != raw_value_.size()) {
            throw std::invalid_argument("Invalid integer value: " + raw_value_);
        }
        return result;
    } catch (const std::out_of_range&) {
        throw std::invalid_argument("Integer value out of range: " + raw_value_);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Invalid integer value: " + raw_value_);
    }
}

double ParamValue::AsDouble() const {
    try {
        size_t pos = 0;
        double result = std::stod(raw_value_, &pos);
        // Ensure entire string was consumed (no trailing garbage)
        if (pos != raw_value_.size()) {
            throw std::invalid_argument("Invalid double value: " + raw_value_);
        }
        return result;
    } catch (const std::out_of_range&) {
        throw std::invalid_argument("Double value out of range: " + raw_value_);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Invalid double value: " + raw_value_);
    }
}

bool ParamValue::AsBool() const {
    std::string lower = ToLower(raw_value_);

    if (lower == "true" || lower == "yes" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "0") {
        return false;
    }

    throw std::invalid_argument(
        "Invalid boolean value: " + raw_value_ +
        ". Expected: true/false/yes/no/1/0");
}

bool ParamValue::IsEmpty() const {
    return raw_value_.empty();
}

// CommandContext implementation

ParamValue CommandContext::GetParam(const std::string& name) const {
    auto it = params_.find(name);
    if (it == params_.end()) {
        return ParamValue();  // Return empty ParamValue
    }
    return it->second;
}

bool CommandContext::HasParam(const std::string& name) const {
    return params_.find(name) != params_.end();
}

const std::unordered_map<std::string, ParamValue>& CommandContext::GetAllParams() const {
    return params_;
}

const std::string& CommandContext::GetRawTranscript() const {
    return raw_transcript_;
}

float CommandContext::GetConfidence() const {
    return confidence_;
}

void CommandContext::SetParam(const std::string& name, ParamValue value) {
    params_[name] = std::move(value);
}

void CommandContext::SetRawTranscript(std::string transcript) {
    raw_transcript_ = std::move(transcript);
}

void CommandContext::SetConfidence(float confidence) {
    confidence_ = confidence;
}

}  // namespace voice_command
