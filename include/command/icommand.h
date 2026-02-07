#ifndef ICOMMAND_H
#define ICOMMAND_H

#include "command/command_result.h"
#include "command/context/commandcontext.h"

namespace voice_command {

class ICommand {
public:
    virtual ~ICommand() = default;

    // Execute the command with the given context.
    // The context contains extracted parameters (if any),
    // the raw transcript, and confidence metadata.
    virtual CommandResult Execute(const CommandContext& context) = 0;

    // Optional: Return a human-readable name for logging.
    virtual std::string GetName() const { return "unnamed_command"; }
};

}  // namespace voice_command
#endif // ICOMMAND_H
