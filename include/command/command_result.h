#ifndef COMMAND_RESULT_H
#define COMMAND_RESULT_H

namespace voice_command {

// Result of command execution.
enum class CommandResult {
    kSuccess,
    kFailure,
    kInvalidParams,
    kNotHandled,  // Command recognized but chose not to handle
};

}  // namespace voice_command

#endif // COMMAND_RESULT_H
