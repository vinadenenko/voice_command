/// Integration Example for voice_command Library (Qt-based)
///
/// This example demonstrates how to use the voice_command library as a
/// black-box component for voice-controlled applications.
///
/// Uses Qt backend for audio capture (requires QCoreApplication event loop).
///
/// Usage:
///   ./integration_example -m /path/to/whisper/model.bin
///
/// The example registers both simple and parameterized commands, then
/// listens for voice input until Ctrl+C is pressed.

#include <QCoreApplication>
#include <QTimer>
#include <qt_voice_assistant.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "command/command_result.h"
#include "command/context/commandcontext.h"
#include "command/descriptor/commanddescriptor.h"
#include "command/icommand.h"
#include "command/nlu/rule_based_nlu_engine.h"
#include "voice_assistant.h"

// Global flag for signal handling
static std::atomic<bool> g_running{true};
static QCoreApplication* g_app = nullptr;

void SignalHandler(int /*signal*/) {
    g_running = false;
    fprintf(stdout, "\nShutting down...\n");
    if (g_app) {
        QTimer::singleShot(0, g_app, &QCoreApplication::quit);
    }
}

// ============================================================================
// Example Commands
// ============================================================================

/// Simple command: Create Placemark (no parameters)
class CreatePlacemarkCommand : public voice_command::ICommand {
public:
    voice_command::CommandResult Execute(
        const voice_command::CommandContext& context) override {
        fprintf(stdout, "\n");
        fprintf(stdout, "========================================\n");
        fprintf(stdout, " Creating placemark\n");
        fprintf(stdout, " Transcript: %s\n", context.GetRawTranscript().c_str());
        fprintf(stdout, " Confidence: %.1f%%\n", context.GetConfidence() * 100.0f);
        fprintf(stdout, "========================================\n");
        fprintf(stdout, "\n");
        return voice_command::CommandResult::kSuccess;
    }

    std::string GetName() const override { return "create_placemark"; }
};

/// Simple command: Show Help
class ShowHelpCommand : public voice_command::ICommand {
public:
    voice_command::CommandResult Execute(
        const voice_command::CommandContext& /*context*/) override {
        fprintf(stdout, "\n");
        fprintf(stdout, "========================================\n");
        fprintf(stdout, " Available Commands:\n");
        fprintf(stdout, "  - 'create placemark' - Creates a new placemark\n");
        fprintf(stdout, "  - 'show help' - Shows this help message\n");
        fprintf(stdout, "  - 'zoom to <level>' - Zooms to specified level\n");
        fprintf(stdout, "  - 'set brightness <value>' - Sets brightness\n");
        fprintf(stdout, "========================================\n");
        fprintf(stdout, "\n");
        return voice_command::CommandResult::kSuccess;
    }

    std::string GetName() const override { return "show_help"; }
};

/// Parameterized command: Zoom To Level
class ZoomToCommand : public voice_command::ICommand {
public:
    voice_command::CommandResult Execute(
        const voice_command::CommandContext& context) override {
        int level = 10;  // Default

        if (context.HasParam("level")) {
            try {
                level = context.GetParam("level").AsInt();
            } catch (const std::exception& e) {
                fprintf(stderr, "Error parsing level: %s\n", e.what());
                return voice_command::CommandResult::kInvalidParams;
            }
        }

        fprintf(stdout, "\n");
        fprintf(stdout, "========================================\n");
        fprintf(stdout, " Zooming to level %d\n", level);
        fprintf(stdout, " Transcript: %s\n", context.GetRawTranscript().c_str());
        fprintf(stdout, "========================================\n");
        fprintf(stdout, "\n");
        return voice_command::CommandResult::kSuccess;
    }

    std::string GetName() const override { return "zoom_to"; }
};

/// Parameterized command: Set Brightness
class SetBrightnessCommand : public voice_command::ICommand {
public:
    voice_command::CommandResult Execute(
        const voice_command::CommandContext& context) override {
        int brightness = 50;  // Default

        if (context.HasParam("value")) {
            try {
                brightness = context.GetParam("value").AsInt();
            } catch (const std::exception& e) {
                fprintf(stderr, "Error parsing brightness: %s\n", e.what());
                return voice_command::CommandResult::kInvalidParams;
            }
        }

        // Clamp to valid range
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;

        fprintf(stdout, "\n");
        fprintf(stdout, "========================================\n");
        fprintf(stdout, " Setting brightness to %d%%\n", brightness);
        fprintf(stdout, " Transcript: %s\n", context.GetRawTranscript().c_str());
        fprintf(stdout, "========================================\n");
        fprintf(stdout, "\n");
        return voice_command::CommandResult::kSuccess;
    }

    std::string GetName() const override { return "set_brightness"; }
};

// ============================================================================
// Command Registration
// ============================================================================

void RegisterCommands(voice_command::CommandRegistry* registry) {
    // Simple command: Create Placemark
    {
        voice_command::CommandDescriptor desc;
        desc.name = "create_placemark";
        desc.description = "Creates a placemark on the map";
        desc.trigger_phrases = {"create placemark", "add placemark",
                                "new placemark", "make placemark"};
        registry->Register(desc, std::make_unique<CreatePlacemarkCommand>());
        fprintf(stderr, "  Registered: %s\n", desc.name.c_str());
    }

    // Simple command: Show Help
    {
        voice_command::CommandDescriptor desc;
        desc.name = "show_help";
        desc.description = "Shows available voice commands";
        desc.trigger_phrases = {"show help", "help", "what can I say",
                                "list commands"};
        registry->Register(desc, std::make_unique<ShowHelpCommand>());
        fprintf(stderr, "  Registered: %s\n", desc.name.c_str());
    }

    // Parameterized command: Zoom To
    {
        voice_command::CommandDescriptor desc;
        desc.name = "zoom_to";
        desc.description = "Zooms the view to a specific level";
        desc.trigger_phrases = {"zoom to", "zoom in to", "set zoom",
                                "zoom level"};

        voice_command::ParamDescriptor level_param;
        level_param.name = "level";
        level_param.type = voice_command::ParamType::kInteger;
        level_param.description = "Zoom level (1-20)";
        level_param.required = false;
        level_param.default_value = "10";
        level_param.min_value = 1;
        level_param.max_value = 20;
        desc.parameters.push_back(level_param);

        registry->Register(desc, std::make_unique<ZoomToCommand>());
        fprintf(stderr, "  Registered: %s (parameterized)\n", desc.name.c_str());
    }

    // Parameterized command: Set Brightness
    {
        voice_command::CommandDescriptor desc;
        desc.name = "set_brightness";
        desc.description = "Sets the display brightness";
        desc.trigger_phrases = {"set brightness", "brightness to",
                                "change brightness"};

        voice_command::ParamDescriptor value_param;
        value_param.name = "value";
        value_param.type = voice_command::ParamType::kInteger;
        value_param.description = "Brightness percentage (0-100)";
        value_param.required = false;
        value_param.default_value = "50";
        value_param.min_value = 0;
        value_param.max_value = 100;
        desc.parameters.push_back(value_param);

        registry->Register(desc, std::make_unique<SetBrightnessCommand>());
        fprintf(stderr, "  Registered: %s (parameterized)\n", desc.name.c_str());
    }
}

// ============================================================================
// Configuration
// ============================================================================

struct AppConfig {
    std::string model_path = "models/ggml-base.en.bin";
    int num_threads = 4;
    bool use_gpu = true;
    int capture_device_id = -1;
};

bool ParseArgs(int argc, char** argv, AppConfig& config) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            fprintf(stderr, "Usage: %s [options]\n\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  -m, --model PATH    Path to whisper model\n");
            fprintf(stderr, "  -t, --threads N     Number of threads\n");
            fprintf(stderr, "  -c, --capture ID    Capture device ID (-1=default)\n");
            fprintf(stderr, "  --no-gpu            Disable GPU acceleration\n");
            fprintf(stderr, "  -h, --help          Show this help\n");
            return false;
        } else if ((arg == "-m" || arg == "--model") && i + 1 < argc) {
            config.model_path = argv[++i];
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            config.num_threads = std::stoi(argv[++i]);
        } else if ((arg == "-c" || arg == "--capture") && i + 1 < argc) {
            config.capture_device_id = std::stoi(argv[++i]);
        } else if (arg == "--no-gpu") {
            config.use_gpu = false;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            return false;
        }
    }
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    // Create Qt application (required for Qt Multimedia)
    QCoreApplication app(argc, argv);
    g_app = &app;

    // Parse command line arguments
    AppConfig app_config;
    if (!ParseArgs(argc, argv, app_config)) {
        return 1;
    }

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    fprintf(stderr, "\n");
    fprintf(stderr, "==============================================\n");
    fprintf(stderr, " Voice Command Integration Example (Qt)\n");
    fprintf(stderr, "==============================================\n");
    fprintf(stderr, "\n");

    // Configure audio engine (Qt backend)
    voice_command::audio_capture::AudioCaptureConfig audio_capture_config;
    audio_capture_config.device_id = app_config.capture_device_id;
    audio_capture_config.sample_rate = 16000;
    audio_capture_config.channels = 1;
    audio_capture_config.buffer_duration_ms = 30000;

    voice_command::audio_capture::VadConfig vad_config;
    vad_config.window_ms = 1000;
    vad_config.energy_threshold = 0.1f;
    vad_config.freq_threshold = 100.0f;
    vad_config.sample_rate = 16000;

    // Configure voice assistant with Qt audio backend
    // voice_command::VoiceAssistantConfig config;
    voice_command::QtVoiceAssistantConfig config;
    config.audio_config =
        voice_command::AudioEngine::CreateQtConfig(audio_capture_config, vad_config);

    config.whisper_config.model_path = app_config.model_path;
    config.whisper_config.num_threads = app_config.num_threads;
    config.whisper_config.use_gpu = app_config.use_gpu;
    config.whisper_config.language = "en";

    config.vad_check_duration_ms = 2000;
    config.command_capture_duration_ms = 8000;
    config.poll_interval_ms = 100;
    config.auto_select_strategy = true;

    // Create NLU engine
    auto nlu_engine = std::make_unique<voice_command::RuleBasedNluEngine>();

    // Create voice assistant
    voice_command::QtVoiceAssistant assistant;

    fprintf(stderr, "Initializing voice assistant...\n");
    fprintf(stderr, "  Model: %s\n", app_config.model_path.c_str());
    fprintf(stderr, "  Threads: %d\n", app_config.num_threads);
    fprintf(stderr, "  GPU: %s\n", app_config.use_gpu ? "enabled" : "disabled");
    fprintf(stderr, "  Audio backend: Qt\n");
    fprintf(stderr, "\n");

    if (!assistant.Init(config, std::move(nlu_engine))) {
        fprintf(stderr, "Failed to initialize voice assistant!\n");
        fprintf(stderr, "Make sure the model file exists: %s\n",
                app_config.model_path.c_str());
        return 1;
    }

    // Register commands
    fprintf(stderr, "Registering commands:\n");
    RegisterCommands(assistant.GetRegistry());
    fprintf(stderr, "\n");

    // Set up callbacks
    assistant.SetSpeechDetectedCallback([]() {
        qDebug() << "Speech detected, processing";
    });

    assistant.SetCommandCallback(
        [](const std::string& command_name, voice_command::CommandResult result,
           const voice_command::CommandContext& /*context*/) {
            const char* result_str = "unknown";
            switch (result) {
                case voice_command::CommandResult::kSuccess:
                    result_str = "success";
                    break;
                case voice_command::CommandResult::kFailure:
                    result_str = "failure";
                    break;
                case voice_command::CommandResult::kInvalidParams:
                    result_str = "invalid params";
                    break;
                case voice_command::CommandResult::kNotHandled:
                    result_str = "not handled";
                    break;
            }
            fprintf(stdout, "[Command '%s' executed: %s]\n", command_name.c_str(),
                    result_str);
        });

    assistant.SetUnrecognizedCallback([](const std::string& transcript) {
        fprintf(stdout, "[Unrecognized: '%s']\n", transcript.c_str());
    });

    assistant.SetErrorCallback([](const std::string& error) {
        fprintf(stderr, "[Error: %s]\n", error.c_str());
    });

    // Start processing
    fprintf(stderr, "Starting voice command processing...\n");
    fprintf(stderr, "Say 'show help' to see available commands.\n");
    fprintf(stderr, "Press Ctrl+C to exit.\n");
    fprintf(stderr, "\n");

    if (!assistant.Start()) {
        fprintf(stderr, "Failed to start voice assistant!\n");
        return 1;
    }

    // Run Qt event loop (required for Qt audio backend)
    int result = app.exec();

    // Cleanup
    fprintf(stderr, "\nStopping voice assistant...\n");
    assistant.Stop();
    assistant.Shutdown();

    fprintf(stderr, "Done.\n");
    return result;
}
