#include "command/dispatcher/commanddispatcher.h"

#include <algorithm>
#include <cctype>

namespace voice_command {

namespace {

// Helper for case-insensitive string comparison
std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Case-insensitive string equality
bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    return ToLower(a) == ToLower(b);
}

}  // namespace

CommandDispatcher::CommandDispatcher(CommandRegistry* registry)
    : registry_(registry) {}

CommandResult CommandDispatcher::Dispatch(const std::string& command_name,
                                           CommandContext context) {
    // Look up the command in the registry
    ICommand* command = registry_->FindCommand(command_name);
    if (!command) {
        return CommandResult::kFailure;
    }

    // Look up the descriptor for validation
    const CommandDescriptor* descriptor = registry_->FindDescriptor(command_name);
    if (!descriptor) {
        return CommandResult::kFailure;
    }

    // Validate parameters and fill defaults
    if (!ValidateAndFillDefaults(*descriptor, context)) {
        return CommandResult::kInvalidParams;
    }

    // Execute the command
    CommandResult result = command->Execute(context);

    return result;
}

bool CommandDispatcher::ValidateAndFillDefaults(const CommandDescriptor& descriptor,
                                                 CommandContext& context) {
    for (const auto& param : descriptor.parameters) {
        bool has_param = context.HasParam(param.name);

        // 1. Check required parameters are present
        if (param.required && !has_param) {
            return false;  // Missing required parameter
        }

        // 2. Fill defaults for missing optional params
        if (!has_param && !param.default_value.empty()) {
            context.SetParam(param.name, ParamValue(param.default_value));
            has_param = true;
        }

        // Skip validation if parameter is not present (optional with no default)
        if (!has_param) {
            continue;
        }

        // 3. Validate type and constraints
        const ParamValue& value = context.GetParam(param.name);

        switch (param.type) {
            case ParamType::kInteger: {
                int int_value;
                try {
                    int_value = value.AsInt();
                } catch (...) {
                    return false;  // Invalid integer format
                }
                if (param.min_value.has_value() &&
                    int_value < static_cast<int>(param.min_value.value())) {
                    return false;  // Below minimum
                }
                if (param.max_value.has_value() &&
                    int_value > static_cast<int>(param.max_value.value())) {
                    return false;  // Above maximum
                }
                break;
            }

            case ParamType::kDouble: {
                double double_value;
                try {
                    double_value = value.AsDouble();
                } catch (...) {
                    return false;  // Invalid double format
                }
                if (param.min_value.has_value() && double_value < param.min_value.value()) {
                    return false;  // Below minimum
                }
                if (param.max_value.has_value() && double_value > param.max_value.value()) {
                    return false;  // Above maximum
                }
                break;
            }

            case ParamType::kBool: {
                try {
                    value.AsBool();
                } catch (...) {
                    return false;  // Invalid boolean format
                }
                break;
            }

            case ParamType::kEnum: {
                std::string str_value = value.AsString();
                bool found = false;
                for (const auto& enum_value : param.enum_values) {
                    if (EqualsIgnoreCase(str_value, enum_value)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;  // Value not in enum
                }
                break;
            }

            case ParamType::kString:
            default:
                // No validation needed for strings
                break;
        }
    }

    return true;
}

}  // namespace voice_command
