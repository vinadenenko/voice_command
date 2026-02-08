#ifndef COMMANDDESCRIPTOR_H
#define COMMANDDESCRIPTOR_H

#include <optional>
#include <string>
#include <vector>

// Defines the schema for a command — its trigger phrases, parameters, types, and constraints.
// This is the "tool definition" analogous to LLM function calling schemas.

namespace voice_command {

// Supported parameter types.
enum class ParamType {
    kString,
    kInteger,
    kDouble,
    kBool,
    kEnum,    // String constrained to a set of allowed values
};

// Defines a single parameter in a command's schema.
struct ParamDescriptor {
    std::string name;                          // "geometry_type"
    ParamType type = ParamType::kString;
    std::string description;                   // Human-readable, used by NLU
    bool required = false;
    std::string default_value;                 // Used when param not extracted
    std::vector<std::string> enum_values;      // For kEnum type
    std::optional<double> min_value;           // For kInteger, kDouble
    std::optional<double> max_value;           // For kInteger, kDouble
};

// Full schema for a command. Registered alongside the ICommand instance.
struct CommandDescriptor {
    // Unique identifier for the command. Used as registry key.
    std::string name;                          // "create_placemark"

    // Natural language description. Used by NLU to understand intent.
    std::string description;                   // "Creates a geometry placemark on the map"

    // Phrases that trigger this command. For simple commands, these are
    // the keywords matched by guided mode. For parameterized commands,
    // the NLU uses these plus the description for intent classification.
    std::vector<std::string> trigger_phrases;  // {"create placemark", "add placemark"}

    // Parameter schema. Empty = simple command (no parameters).
    std::vector<ParamDescriptor> parameters;

    // Returns true if this command has parameters (parameterized mode).
    bool IsParameterized() const { return !parameters.empty(); }
};

}  // namespace voice_command

#endif // COMMANDDESCRIPTOR_H
