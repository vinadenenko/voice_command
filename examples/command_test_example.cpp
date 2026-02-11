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

// 2-parameter command: move to x, y
class MoveToCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& context) override {
        if (!context.HasParam("x") || !context.HasParam("y")) {
            printf("  [MoveToCommand] ERROR: Missing x or y parameter\n");
            return CommandResult::kInvalidParams;
        }

        int x = context.GetParam("x").AsInt();
        int y = context.GetParam("y").AsInt();
        printf("  [MoveToCommand] Moving to position (%d, %d)\n", x, y);
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "move_to"; }
};

// 3-parameter command: create rectangle with width, height, color
class CreateRectangleCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& context) override {
        if (!context.HasParam("width") || !context.HasParam("height")) {
            printf("  [CreateRectangleCommand] ERROR: Missing width or height\n");
            return CommandResult::kInvalidParams;
        }

        int width = context.GetParam("width").AsInt();
        int height = context.GetParam("height").AsInt();
        std::string color = context.HasParam("color")
            ? context.GetParam("color").AsString()
            : "white";

        printf("  [CreateRectangleCommand] Creating %dx%d rectangle in %s\n",
               width, height, color.c_str());
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "create_rectangle"; }
};

// 2-parameter command: set brightness and contrast
class SetDisplayCommand : public ICommand {
public:
    CommandResult Execute(const CommandContext& context) override {
        int brightness = context.HasParam("brightness")
            ? context.GetParam("brightness").AsInt() : -1;
        int contrast = context.HasParam("contrast")
            ? context.GetParam("contrast").AsInt() : -1;

        if (brightness < 0 && contrast < 0) {
            printf("  [SetDisplayCommand] ERROR: No parameters provided\n");
            return CommandResult::kInvalidParams;
        }

        printf("  [SetDisplayCommand] Setting display:");
        if (brightness >= 0) printf(" brightness=%d", brightness);
        if (contrast >= 0) printf(" contrast=%d", contrast);
        printf("\n");
        return CommandResult::kSuccess;
    }

    std::string GetName() const override { return "set_display"; }
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

    // Parameterized command: change_color (1 param)
    {
        CommandDescriptor desc;
        desc.name = "change_color";
        desc.description = "Changes the color";
        desc.trigger_phrases = {"change color to", "set color to"};

        ParamDescriptor color_param;
        color_param.name = "color";
        color_param.type = ParamType::kString;
        color_param.description = "Target color";
        color_param.required = true;
        desc.parameters.push_back(color_param);

        registry->Register(desc, std::make_unique<ChangeColorCommand>());
    }

    // 2-parameter command: move_to (x, y)
    {
        CommandDescriptor desc;
        desc.name = "move_to";
        desc.description = "Moves to a specific position";
        desc.trigger_phrases = {"move to", "go to position"};

        ParamDescriptor x_param;
        x_param.name = "x";
        x_param.type = ParamType::kInteger;
        x_param.description = "X coordinate";
        x_param.required = true;
        desc.parameters.push_back(x_param);

        ParamDescriptor y_param;
        y_param.name = "y";
        y_param.type = ParamType::kInteger;
        y_param.description = "Y coordinate";
        y_param.required = true;
        desc.parameters.push_back(y_param);

        registry->Register(desc, std::make_unique<MoveToCommand>());
    }

    // 2-parameter command: set_display (brightness, contrast)
    {
        CommandDescriptor desc;
        desc.name = "set_display";
        desc.description = "Adjusts display settings";
        desc.trigger_phrases = {"set display", "adjust display", "display settings"};

        ParamDescriptor brightness_param;
        brightness_param.name = "brightness";
        brightness_param.type = ParamType::kInteger;
        brightness_param.description = "Brightness level (0-100)";
        brightness_param.required = false;
        brightness_param.min_value = 0;
        brightness_param.max_value = 100;
        desc.parameters.push_back(brightness_param);

        ParamDescriptor contrast_param;
        contrast_param.name = "contrast";
        contrast_param.type = ParamType::kInteger;
        contrast_param.description = "Contrast level (0-100)";
        contrast_param.required = false;
        contrast_param.min_value = 0;
        contrast_param.max_value = 100;
        desc.parameters.push_back(contrast_param);

        registry->Register(desc, std::make_unique<SetDisplayCommand>());
    }

    // 3-parameter command: create_rectangle (width, height, color)
    {
        CommandDescriptor desc;
        desc.name = "create_rectangle";
        desc.description = "Creates a rectangle with specified dimensions and color";
        desc.trigger_phrases = {"create rectangle", "draw rectangle", "make rectangle"};

        ParamDescriptor width_param;
        width_param.name = "width";
        width_param.type = ParamType::kInteger;
        width_param.description = "Width in pixels";
        width_param.required = true;
        desc.parameters.push_back(width_param);

        ParamDescriptor height_param;
        height_param.name = "height";
        height_param.type = ParamType::kInteger;
        height_param.description = "Height in pixels";
        height_param.required = true;
        desc.parameters.push_back(height_param);

        ParamDescriptor color_param;
        color_param.name = "color";
        color_param.type = ParamType::kEnum;
        color_param.description = "Rectangle color";
        color_param.required = false;
        color_param.enum_values = {"red", "green", "blue", "yellow", "white", "black"};
        desc.parameters.push_back(color_param);

        registry->Register(desc, std::make_unique<CreateRectangleCommand>());
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
    printf("Registered 6 commands: show_help, zoom_to, change_color, move_to, set_display, create_rectangle\n");

    // Define test cases
    std::vector<std::string> test_inputs = {
        // Simple command tests
        "show help",
        "help",
        "what can I say",

        // 1-parameter command: zoom_to
        "zoom to 5",
        "zoom to 15",
        "zoom in to 10",
        "set zoom 20",

        // 1-parameter command: change_color
        "change color to red",
        "set color to blue",
        "change color to green.",  // With trailing punctuation

        // 2-parameter command: move_to (x, y) - natural phrasing with keywords
        "move to x 100 y 200",
        "go to position x 50 y 75",
        "move to x 0 y 0",

        // 2-parameter command: set_display (brightness, contrast) - natural phrasing
        "set display brightness 80 contrast 60",
        "adjust display brightness 50",
        "display settings contrast 70",

        // 3-parameter command: create_rectangle - natural phrasing with keywords
        "create rectangle width 100 height 200 red",
        "draw rectangle with width 50 and height 50 blue",
        "make rectangle width 300 height 150 green",
        "create rectangle width 80 height 80",  // Without optional color

        // Edge cases
        "zoom to",           // Missing parameter
        "move to x 100",     // Missing second parameter
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
