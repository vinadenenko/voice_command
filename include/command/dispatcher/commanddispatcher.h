#ifndef COMMANDDISPATCHER_H
#define COMMANDDISPATCHER_H

#include "command/command_result.h"
#include "command/context/commandcontext.h"
#include "command/registry/commandregistry.h"

// Routes recognized intents to commands.
// Handles parameter validation and default value injection.

namespace voice_command {

// Callback for command execution events (logging, telemetry).
// using DispatchCallback = std::function<void(const std::string& command_name,
//                                             CommandResult result,
//                                             const CommandContext& context)>;

class CommandDispatcher {
public:
    explicit CommandDispatcher(CommandRegistry* registry);

    // Dispatch a recognized command.
    // - Looks up the command in the registry.
    // - Validates parameters against the descriptor's schema.
    // - Injects default values for missing optional parameters.
    // - Calls ICommand::Execute().
    // Returns the result from Execute(), or kInvalidParams if validation fails.
    CommandResult Dispatch(const std::string& command_name,
                           CommandContext context);

    // Set a callback invoked after every dispatch (for logging/telemetry).
    // void SetCallback(DispatchCallback callback);

private:
    // Validate extracted parameters against the command's schema.
    // Injects defaults for missing optional params.
    // Returns false if a required param is missing or type validation fails.
    bool ValidateAndFillDefaults(const CommandDescriptor& descriptor,
                                 CommandContext& context);

    CommandRegistry* registry_;  // Not owned
    // DispatchCallback callback_;
};

}  // namespace voice_command

#endif // COMMANDDISPATCHER_H
