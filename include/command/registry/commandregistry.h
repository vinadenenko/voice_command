#ifndef COMMANDREGISTRY_H
#define COMMANDREGISTRY_H

#include "command/icommand.h"
#include "command/descriptor/commanddescriptor.h"

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>


// Thread-safe registry mapping command names to their implementations and descriptors.

namespace voice_command {

class CommandRegistry {
public:
    CommandRegistry() = default;

    // Register a command with its descriptor.
    // Takes ownership of the command via unique_ptr.
    // Returns false if a command with the same name is already registered.
    bool Register(const CommandDescriptor& descriptor,
                  std::unique_ptr<ICommand> command);

    // Convenience: register a simple command with just a name and trigger phrases.
    bool RegisterSimple(const std::string& name,
                        const std::vector<std::string>& triggers,
                        std::unique_ptr<ICommand> command);

    // Unregister a command by name.
    bool Unregister(const std::string& name);

    // Lookup.
    ICommand* FindCommand(const std::string& name) const;
    const CommandDescriptor* FindDescriptor(const std::string& name) const;

    // Iteration.
    std::vector<std::string> GetAllCommandNames() const;
    std::vector<const CommandDescriptor*> GetAllDescriptors() const;

    // Returns true if any registered command has parameters.
    bool HasParameterizedCommands() const;

    // Get all trigger phrases (for guided mode word list).
    std::vector<std::string> GetAllTriggerPhrases() const;

private:
    struct Entry {
        CommandDescriptor descriptor;
        std::unique_ptr<ICommand> command;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry> commands_;
};

}  // namespace voice_command
#endif // COMMANDREGISTRY_H
