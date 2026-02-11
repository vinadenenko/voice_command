/**
 * @file command_test_example.cpp
 * @brief Example demonstrating how to test voice commands without audio input.
 *
 * This example shows how to use CommandTester to verify command recognition
 * and parameter extraction by providing text strings directly, bypassing
 * the Whisper speech recognition pipeline.
 */

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "command/command_result.h"
#include "command/context/commandcontext.h"
#include "command/descriptor/commanddescriptor.h"
#include "command/icommand.h"
#include "testing/command_tester.h"

using namespace voice_command;
using namespace voice_command::testing;

// Example command implementations

class ShowHelpCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& /*context*/) override {
        printf("  [ShowHelpCommand] Displaying help...\n");
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "show_help"; }
};

class ZoomToCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& context) override {
        if (!context.HasParam("level")) {
            printf("  [ZoomToCommand] ERROR: Missing 'level' parameter\n");
            return CommandResult::kInvalidParams;
        }

        int level = context.GetParam("level").AsInt();
        printf("  [ZoomToCommand] Zooming to level %d\n", level);
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "zoom_to"; }
};

class ChangeColorCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& context) override {
        if (!context.HasParam("color")) {
            printf("  [ChangeColorCommand] ERROR: Missing 'color' parameter\n");
            return CommandResult::kInvalidParams;
        }

        std::string color = context.GetParam("color").AsString();
        printf("  [ChangeColorCommand] Changing color to '%s'\n", color.c_str());
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "change_color"; }
};

// Register commands (same pattern as with VoiceAssistant)
void RegisterCommands(CommandRegistry* registry) {
    // Simple command: show_help
    {
        CommandDescriptor desc;
        desc.name = "show_help";
        desc.description = "Shows available voice commands";
        desc.trigger_phrases = {"show help", "help", "what can I say"};

        registry->Register(desc, std::make_unique<ShowHelpCommand>());
    }

    // Parameterized command: zoom_to
    {
        CommandDescriptor desc;
        desc.name = "zoom_to";
        desc.description = "Zooms the view to a specific level";
        desc.trigger_phrases = {"zoom to", "zoom in to", "set zoom"};

        ParamDescriptor level_param;
        level_param.name = "level";
        level_param.type = ParamType::kInteger;
        level_param.description = "Zoom level (1-20)";
        level_param.required = true;
        level_param.min_value = 1;
        level_param.max_value = 20;
        desc.parameters.push_back(level_param);

        registry->Register(desc, std::make_unique<ZoomToCommand>());
    }

    // Parameterized command: change_color
    {
        CommandDescriptor desc;
        desc.name = "change_color";
        desc.description = "Changes the color";
        desc.trigger_phrases = {"change color to", "set color to", "color"};

        ParamDescriptor color_param;
        color_param.name = "color";
        color_param.type = ParamType::kString;
        color_param.description = "Target color";
        color_param.required = true;
        desc.parameters.push_back(color_param);

        registry->Register(desc, std::make_unique<ChangeColorCommand>());
    }
}

// Helper to convert CommandResult to string
const char* ResultToString(CommandResult result) {
    switch (result) {
        case CommandResult::kSuccess:
            return "SUCCESS";
        case CommandResult::kFailure:
            return "FAILURE";
        case CommandResult::kInvalidParams:
            return "INVALID_PARAMS";
        case CommandResult::kNotHandled:
            return "NOT_HANDLED";
        default:
            return "UNKNOWN";
    }
}

// Print test result in a formatted way
void PrintResult(const TestResult& result) {
    printf("\n----------------------------------------\n");
    printf("Input: \"%s\"\n", result.raw_transcript.c_str());

    if (result.recognized) {
        printf("Recognized: YES\n");
        printf("Command: %s\n", result.command_name.c_str());
        printf("Confidence: %.2f\n", result.confidence);

        if (!result.params.empty()) {
            printf("Parameters:\n");
            for (const auto& [name, value] : result.params) {
                printf("  %s = \"%s\"\n", name.c_str(), value.c_str());
            }
        }

        printf("Execution: %s\n", ResultToString(result.execution_result));
    } else {
        printf("Recognized: NO\n");
        if (!result.error.empty()) {
            printf("Error: %s\n", result.error.c_str());
        }
    }
}

int main() {
    printf("=== Voice Command Test Example ===\n");
    printf("Testing command recognition without audio input.\n\n");

    // Create and initialize tester
    CommandTester tester;
    if (!tester.Init()) {
        fprintf(stderr, "Failed to initialize CommandTester\n");
        return 1;
    }

    // Register commands
    RegisterCommands(tester.GetRegistry());
    printf("Registered 3 commands: show_help, zoom_to, change_color\n");

    // Define test cases
    std::vector<std::string> test_inputs = {
        // Simple command tests
        "show help",
        "help",
        "what can I say",

        // Parameterized command: zoom_to
        "zoom to 5",
        "zoom to 15",
        "zoom in to 10",
        "set zoom 20",

        // Parameterized command: change_color
        "change color to red",
        "set color to blue",
        "change color to green.",  // With trailing punctuation

        // Edge cases
        "zoom to",           // Missing parameter
        "random gibberish",  // Unrecognized
        "",                  // Empty input
    };

    // Run all tests
    printf("\n=== Running %zu test cases ===\n", test_inputs.size());

    auto results = tester.ProcessBatch(test_inputs);

    for (const auto& result : results) {
        PrintResult(result);
    }

    // Summary
    printf("\n=== Summary ===\n");
    int recognized_count = 0;
    int success_count = 0;

    for (const auto& result : results) {
        if (result.recognized) {
            recognized_count++;
            if (result.execution_result == CommandResult::kSuccess) {
                success_count++;
            }
        }
    }

    printf("Total tests: %zu\n", results.size());
    printf("Recognized: %d\n", recognized_count);
    printf("Executed successfully: %d\n", success_count);

    return 0;
}
