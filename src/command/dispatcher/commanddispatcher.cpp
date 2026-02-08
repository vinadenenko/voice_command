#include "command/dispatcher/commanddispatcher.h"

namespace voice_command {

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
    // Currently simplified since ParamDescriptor is commented out
    if (!ValidateAndFillDefaults(*descriptor, context)) {
        return CommandResult::kInvalidParams;
    }

    // Execute the command
    CommandResult result = command->Execute(context);

    return result;
}

bool CommandDispatcher::ValidateAndFillDefaults(const CommandDescriptor& descriptor,
                                                 CommandContext& context) {
    // Currently simplified implementation since:
    // - ParamDescriptor is commented out
    // - parameters field in CommandDescriptor is commented out
    // - CommandContext params_ is commented out
    
    // When these are uncommented, full validation logic will be:
    // 1. Check all required parameters are present
    // 2. Validate parameter types
    // 3. Check min/max constraints for numeric types
    // 4. Check enum values
    // 5. Fill default values for missing optional parameters
    
    // For now, always return true (all commands are valid)
    return true;
}

}  // namespace voice_command