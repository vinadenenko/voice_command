#include "voice_assistant.h"

#include <chrono>
#include <iostream>

namespace voice_command {

VoiceAssistant::VoiceAssistant()
    : audio_engine_(std::make_unique<AudioEngine>()),
      whisper_engine_(std::make_unique<WhisperEngine>()),
      registry_(std::make_unique<CommandRegistry>()) {}

VoiceAssistant::~VoiceAssistant() {
    if (running_) {
        Stop();
    }
    if (initialized_) {
        Shutdown();
    }
}

bool VoiceAssistant::Init(const VoiceAssistantConfig& config,
                          std::unique_ptr<INluEngine> nlu_engine) {
    if (initialized_) {
        return false;  // Already initialized
    }

    config_ = config;
    nlu_engine_ = std::move(nlu_engine);

    // Initialize audio engine
    if (!audio_engine_->Init(config.audio_config)) {
        return false;
    }

    // Initialize whisper engine
    if (!whisper_engine_->Init(config.whisper_config)) {
        audio_engine_->Shutdown();
        return false;
    }

    // Initialize NLU engine if provided
    if (nlu_engine_ && !nlu_engine_->Init()) {
        whisper_engine_->Shutdown();
        audio_engine_->Shutdown();
        return false;
    }

    // Create dispatcher
    dispatcher_ = std::make_unique<CommandDispatcher>(registry_.get());

    initialized_ = true;
    return true;
}

void VoiceAssistant::Shutdown() {
    if (!initialized_) {
        return;
    }

    if (running_) {
        Stop();
    }

    strategy_.reset();
    dispatcher_.reset();
    nlu_engine_.reset();
    whisper_engine_->Shutdown();
    audio_engine_->Shutdown();

    initialized_ = false;
}

bool VoiceAssistant::IsInitialized() const {
    return initialized_;
}

bool VoiceAssistant::Start() {
    if (!initialized_ || running_) {
        return false;
    }

    // Select recognition strategy
    SelectStrategy();

    if (!strategy_) {
        return false;
    }

    // Start audio capture
    if (!audio_engine_->Start()) {
        return false;
    }

    running_ = true;

    // Start threads
    audio_thread_ = std::thread(&VoiceAssistant::AudioThreadFunc, this);
    processing_thread_ = std::thread(&VoiceAssistant::ProcessingThreadFunc, this);

    return true;
}

void VoiceAssistant::Stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Wake up processing thread
    queue_cv_.notify_all();

    // Wait for threads to finish
    if (audio_thread_.joinable()) {
        audio_thread_.join();
    }
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }

    // Stop audio capture
    audio_engine_->Stop();

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!audio_queue_.empty()) {
            audio_queue_.pop();
        }
    }
}

bool VoiceAssistant::IsRunning() const {
    return running_;
}

CommandRegistry* VoiceAssistant::GetRegistry() {
    return registry_.get();
}

const CommandRegistry* VoiceAssistant::GetRegistry() const {
    return registry_.get();
}

void VoiceAssistant::SetCommandCallback(CommandCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    command_callback_ = std::move(callback);
}

void VoiceAssistant::SetErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void VoiceAssistant::SetUnrecognizedCallback(UnrecognizedCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    unrecognized_callback_ = std::move(callback);
}

void VoiceAssistant::SetSpeechDetectedCallback(SpeechDetectedCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    speech_detected_callback_ = std::move(callback);
}

void VoiceAssistant::SetForceNluStrategy(bool use_nlu) {
    config_.force_nlu_strategy = use_nlu;
    if (running_) {
        SelectStrategy();
    }
}

const VoiceAssistantConfig& VoiceAssistant::GetConfig() const {
    return config_;
}

void VoiceAssistant::AudioThreadFunc() {
    audio_capture::AudioSamples samples;

    // Wait a bit for audio to stabilize
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.poll_interval_ms));
    audio_engine_->ClearBuffer();

    while (running_) {
        // Sleep between checks
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.poll_interval_ms));

        if (!running_) {
            break;
        }

        // Get audio for VAD check
        audio_engine_->GetAudio(config_.vad_check_duration_ms, samples);

        // Check for speech
        auto vad_result = audio_engine_->DetectSpeech(samples);

        if (vad_result.speech_ended) {
            // Notify speech detected
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (speech_detected_callback_) {
                    speech_detected_callback_();
                }
            }

            // Get full command audio
            audio_engine_->GetAudio(config_.command_capture_duration_ms, samples);

            // Queue for processing
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);

                // Check queue depth limit
                if (audio_queue_.size() < config_.max_queue_depth) {
                    audio_queue_.push(std::move(samples));
                    queue_cv_.notify_one();
                }
                // If queue is full, drop audio (prevents memory issues)
            }

            // Clear audio buffer
            audio_engine_->ClearBuffer();
        }
    }
}

void VoiceAssistant::ProcessingThreadFunc() {
    audio_capture::AudioSamples samples;

    while (running_) {
        // Wait for audio in queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !audio_queue_.empty() || !running_;
            });

            if (!running_ && audio_queue_.empty()) {
                break;
            }

            if (!audio_queue_.empty()) {
                samples = std::move(audio_queue_.front());
                audio_queue_.pop();
            } else {
                continue;
            }
        }

        // Process the audio
        ProcessAudio(samples);
    }
}

void VoiceAssistant::SelectStrategy() {
    if (!initialized_) {
        return;
    }

    bool use_nlu = config_.force_nlu_strategy;

    if (!use_nlu && config_.auto_select_strategy) {
        // Auto-select: use NLU if any parameterized commands exist
        use_nlu = registry_->HasParameterizedCommands();
    }

    if (use_nlu) {
        if (nlu_engine_) {
            strategy_ = std::make_unique<NluRecognitionStrategy>(
                whisper_engine_.get(), nlu_engine_.get(), registry_.get());
        } else {
            // Fall back to guided if no NLU engine
            strategy_ = std::make_unique<GuidedRecognitionStrategy>(
                whisper_engine_.get(), registry_.get());
        }
    } else {
        strategy_ = std::make_unique<GuidedRecognitionStrategy>(
            whisper_engine_.get(), registry_.get());
    }
}

void VoiceAssistant::ProcessAudio(const audio_capture::AudioSamples& samples) {
    if (!strategy_) {
        return;
    }

    // Recognize command
    auto recognition = strategy_->Recognize(samples);

    if (!recognition.success) {
        // Notify unrecognized or error
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (!recognition.raw_transcript.empty() && unrecognized_callback_) {
            unrecognized_callback_(recognition.raw_transcript);
        } else if (error_callback_ && !recognition.error.empty()) {
            error_callback_(recognition.error);
        }
        return;
    }

    // Build command context
    CommandContext context;
    context.SetRawTranscript(recognition.raw_transcript);
    context.SetConfidence(recognition.confidence);

    for (const auto& [key, value] : recognition.params) {
        context.SetParam(key, ParamValue(value));
    }

    // Dispatch command
    CommandResult result =
        dispatcher_->Dispatch(recognition.command_name, std::move(context));

    // Notify callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (command_callback_) {
            // Rebuild context for callback (original was moved)
            CommandContext cb_context;
            cb_context.SetRawTranscript(recognition.raw_transcript);
            cb_context.SetConfidence(recognition.confidence);
            for (const auto& [key, value] : recognition.params) {
                cb_context.SetParam(key, ParamValue(value));
            }
            command_callback_(recognition.command_name, result, cb_context);
        }
    }
}

}  // namespace voice_command
