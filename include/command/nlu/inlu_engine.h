#ifndef INLU_ENGINE_H
#define INLU_ENGINE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "command/descriptor/commanddescriptor.h"

namespace voice_command {

// Result from NLU processing.
struct NluResult {
    bool success = false;
    std::string command_name;           // Identified intent
    float confidence = 0.0f;            // 0.0 - 1.0
    std::unordered_map<std::string, std::string> extracted_params;
    std::string error_message;          // If !success
};

// Interface for NLU engines.
// NLU engines take a transcript and command schemas, and return
// the identified intent with extracted parameters.
class INluEngine {
public:
    virtual ~INluEngine() = default;

    // Initialize the engine (load models, etc.).
    virtual bool Init() = 0;

    // Process a transcript against registered command schemas.
    // Returns the best matching command and extracted parameters.
    virtual NluResult Process(
        const std::string& transcript,
        const std::vector<const CommandDescriptor*>& schemas) = 0;
};

}  // namespace voice_command

#endif // INLU_ENGINE_H
