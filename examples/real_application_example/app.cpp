#include "app.h"

#include "command/icommand.h"
#include "command/registry/commandregistry.h"
#include "audio_engine.h"
#include "qt_voice_assistant.h"
#include "command/nlu/rule_based_nlu_engine.h"

App::App(QObject *parent)
    : QObject{parent}
{}

struct AppConfig {
    std::string model_path = "models/ggml-tiny.en.bin";
    int num_threads = 4;
    bool use_gpu = true;
    int capture_device_id = -1;
};

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

class ChangeColorCommand : public voice_command::ICommand {
public:
    // explicit ChangeColorCommand(App* app) : app_(app) {}
    voice_command::CommandResult Execute(
        const voice_command::CommandContext& context) override {
        QString color = "red";
        if (context.HasParam("color")) {
            try {
                color = context.GetParam("color").AsString().c_str();
                qDebug() << "Changing color to:" << color;
                // somehow need to emit requestChangeColor(color);
                // emit app_->requestChangeColor(color);
            } catch (const std::exception& e) {
                fprintf(stderr, "Error parsing color: %s\n", e.what());
                return voice_command::CommandResult::kInvalidParams;
            }
        }
        return voice_command::CommandResult::kSuccess;
    }
    App* app_;

    std::string GetName() const override { return "show_help"; }
};

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

        qDebug().noquote()
            << "\n========================================"
            << "\nZooming to level" << level
            << "\nTranscript:" << context.GetRawTranscript().c_str()
            << "\n========================================\n";
        return voice_command::CommandResult::kSuccess;
    }

    std::string GetName() const override { return "zoom_to"; }
};

void RegisterCommands(voice_command::CommandRegistry* registry) {
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
        level_param.required = true;
        // level_param.default_value = "10";
        level_param.min_value = 1;
        level_param.max_value = 20;
        desc.parameters.push_back(level_param);

        registry->Register(desc, std::make_unique<ZoomToCommand>());
        fprintf(stderr, "  Registered: %s (parameterized)\n", desc.name.c_str());
    }

    {
        voice_command::CommandDescriptor desc;
        desc.name = "change_color";
        desc.description = "Changes the color";
        desc.trigger_phrases = {"change color to", "colorize to", "set color to"};

        voice_command::ParamDescriptor color_param;
        color_param.name = "color";
        color_param.type = voice_command::ParamType::kString;
        color_param.description = "Color (red, green, blue)";
        color_param.required = true;
        // level_param.default_value = "10";
        // level_param.min_value = 1;
        // level_param.max_value = 20;
        desc.parameters.push_back(color_param);

        registry->Register(desc, std::make_unique<ChangeColorCommand>());
        fprintf(stderr, "  Registered: %s (parameterized)\n", desc.name.c_str());
    }
}
void App::test()
{
     AppConfig app_config;
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

    // // Create voice assistant
    voice_command::QtVoiceAssistant *assistant = new voice_command::QtVoiceAssistant;
    if (!assistant->Init(config, std::move(nlu_engine))) {
        fprintf(stderr, "Failed to initialize voice assistant!\n");
        fprintf(stderr, "Make sure the model file exists: %s\n",
                app_config.model_path.c_str());
        return;
    }

    // Register commands
    fprintf(stderr, "Registering commands:\n");
    RegisterCommands(assistant->GetRegistry());
    fprintf(stderr, "\n");

    // Set up callbacks
    assistant->SetSpeechDetectedCallback([]() {
        qDebug() << "Speech detected, processing";
    });

    assistant->SetCommandCallback(
        [this](const std::string& command_name, voice_command::CommandResult result,
           const voice_command::CommandContext& context) {
            const char* result_str = "unknown";
            switch (result) {
            case voice_command::CommandResult::kSuccess:
                if (command_name == "change_color") {
                    QString color = QString::fromStdString(context.GetParam("color").AsString());
                    // qDebug() << "ACtion to" << color << ;
                    emit requestChangeColor(QColor(color));
                }
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
            qDebug() << QString("[Command '%1' executed: %2]").arg(command_name.c_str(), result_str);
        });

    assistant->SetUnrecognizedCallback([](const std::string& transcript) {
        fprintf(stdout, "[Unrecognized: '%s']\n", transcript.c_str());
    });

    assistant->SetErrorCallback([](const std::string& error) {
        fprintf(stderr, "[Error: %s]\n", error.c_str());
    });
    assistant->Start();
}
