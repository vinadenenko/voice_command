#ifndef COMMANDCONTEXT_H
#define COMMANDCONTEXT_H

// Carries extracted parameters and metadata to the command's `Execute` method.
// Provides typed accessors to avoid stringly-typed parameter handling.

#include <string>

namespace voice_command {

// Represents a single parameter value extracted from speech.
// Internally stored as string, with typed accessors.
// class ParamValue {
// public:
//     ParamValue() = default;
//     explicit ParamValue(std::string raw_value);

//     std::string AsString() const;
//     int AsInt() const;            // throws std::invalid_argument
//     double AsDouble() const;      // throws std::invalid_argument
//     bool AsBool() const;          // "true"/"false"/"yes"/"no"/"1"/"0"

//     bool IsEmpty() const;

// private:
//     std::string raw_value_;
// };

// Passed to ICommand::Execute(). Contains extracted parameters,
// the raw transcript, and recognition metadata.
class CommandContext {
public:
    // // Access a parameter by name. Returns empty ParamValue if not found.
    // ParamValue GetParam(const std::string& name) const;

    // // Check if a parameter was extracted.
    // bool HasParam(const std::string& name) const;

    // // All extracted parameters.
    // const std::unordered_map<std::string, ParamValue>& GetAllParams() const;

    // // The raw transcribed text from whisper.
    // const std::string& GetRawTranscript() const;

    // // Recognition confidence score (0.0 - 1.0).
    // float GetConfidence() const;

private:
    // friend class CommandDispatcher;
    // friend class NluResult;

    // std::unordered_map<std::string, ParamValue> params_;
    // std::string raw_transcript_;
    // float confidence_ = 0.0f;
};

}  // namespace voice_command

#endif // COMMANDCONTEXT_H
