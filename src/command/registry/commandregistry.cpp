#include "command/registry/commandregistry.h"
#include <mutex>

namespace voice_command {

bool CommandRegistry::Register(const CommandDescriptor& descriptor,
                               std::unique_ptr<ICommand> command) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if command with same name already exists
    if (commands_.find(descriptor.name) != commands_.end()) {
        return false;
    }

    Entry entry;
    entry.descriptor = descriptor;
    entry.command = std::move(command);

    commands_[descriptor.name] = std::move(entry);
    return true;
}

bool CommandRegistry::RegisterSimple(const std::string& name,
                                     const std::vector<std::string>& triggers,
                                     std::unique_ptr<ICommand> command) {
    CommandDescriptor descriptor;
    descriptor.name = name;
    descriptor.description = "Simple command: " + name;
    descriptor.trigger_phrases = triggers;

    return Register(descriptor, std::move(command));
}

bool CommandRegistry::Unregister(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return false;
    }

    commands_.erase(it);
    return true;
}

ICommand* CommandRegistry::FindCommand(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return nullptr;
    }

    return it->second.command.get();
}

const CommandDescriptor* CommandRegistry::FindDescriptor(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return nullptr;
    }

    return &it->second.descriptor;
}

std::vector<std::string> CommandRegistry::GetAllCommandNames() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(commands_.size());

    for (const auto& [name, entry] : commands_) {
        names.push_back(name);
    }

    return names;
}

std::vector<const CommandDescriptor*> CommandRegistry::GetAllDescriptors() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<const CommandDescriptor*> descriptors;
    descriptors.reserve(commands_.size());

    for (const auto& [name, entry] : commands_) {
        descriptors.push_back(&entry.descriptor);
    }

    return descriptors;
}

bool CommandRegistry::HasParameterizedCommands() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& [name, entry] : commands_) {
        if (entry.descriptor.IsParameterized()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CommandRegistry::GetAllTriggerPhrases() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> phrases;
    for (const auto& [name, entry] : commands_) {
        for (const auto& phrase : entry.descriptor.trigger_phrases) {
            phrases.push_back(phrase);
        }
    }
    return phrases;
}

}  // namespace voice_command
