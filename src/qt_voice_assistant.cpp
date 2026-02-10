/// QtVoiceAssistant Implementation (Timer-based Audio Capture)

#include "qt_voice_assistant.h"

#include <QTimer>

#include <QElapsedTimer>
#include <chrono>
#include <iostream>
#include <qdebug.h>

namespace voice_command {

QtVoiceAssistant::QtVoiceAssistant(QObject* parent)
    : QObject(parent),
      audio_engine_(std::make_unique<AudioEngine>()),
      whisper_engine_(std::make_unique<WhisperEngine>()),
      registry_(std::make_unique<CommandRegistry>()) {
    // Create the audio timer
    audio_timer_ = new QTimer(this);
    connect(audio_timer_, &QTimer::timeout, this, &QtVoiceAssistant::OnAudioTimer);
}

QtVoiceAssistant::~QtVoiceAssistant() {
    if (running_) {
        Stop();
    }
    if (initialized_) {
        Shutdown();
    }
}

bool QtVoiceAssistant::Init(const QtVoiceAssistantConfig& config,
                            std::unique_ptr<INluEngine> nlu_engine) {
    if (initialized_) {
        qWarning() << "[QtVoiceAssistant] ERROR: Already initialized";
        return false;
    }

    config_ = config;
    nlu_engine_ = std::move(nlu_engine);

    // Initialize audio engine
    if (!audio_engine_->Init(config.audio_config)) {
        qWarning() << "[QtVoiceAssistant] ERROR: Audio engine init failed";
        return false;
    }

    // Initialize whisper engine
    if (!whisper_engine_->Init(config.whisper_config)) {
        qWarning() << "[QtVoiceAssistant] ERROR: Whisper engine init failed";
        audio_engine_->Shutdown();
        return false;
    }

    // Initialize NLU engine if provided
    if (nlu_engine_ && !nlu_engine_->Init()) {
        qWarning() << "[QtVoiceAssistant] ERROR: NLU engine init failed";
        whisper_engine_->Shutdown();
        audio_engine_->Shutdown();
        return false;
    }

    // Create dispatcher
    dispatcher_ = std::make_unique<CommandDispatcher>(registry_.get());

    initialized_ = true;
    return true;
}

void QtVoiceAssistant::Shutdown() {
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

bool QtVoiceAssistant::IsInitialized() const {
    return initialized_;
}

bool QtVoiceAssistant::Start() {
    if (!initialized_ || running_) {
        return false;
    }

    // Select recognition strategy
    SelectStrategy();

    if (!strategy_) {
        qWarning() << "[QtVoiceAssistant] ERROR: No strategy selected";
        return false;
    }

    // Start audio capture
    if (!audio_engine_->Start()) {
        qWarning() << "[QtVoiceAssistant] ERROR: Audio engine start failed";
        return false;
    }

    running_ = true;

    // Configure and start the audio timer (replaces audio thread)
    audio_timer_->setInterval(config_.poll_interval_ms);
    audio_timer_->start();

    // Start processing thread
    processing_thread_ = std::thread(&QtVoiceAssistant::ProcessingThreadFunc, this);

    qDebug() << "[QtVoiceAssistant] Started - listening for commands";
    return true;
}

void QtVoiceAssistant::Stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop the audio timer
    audio_timer_->stop();

    // Wake up processing thread
    queue_cv_.notify_all();

    // Wait for processing thread to finish
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

bool QtVoiceAssistant::IsRunning() const {
    return running_;
}

CommandRegistry* QtVoiceAssistant::GetRegistry() {
    return registry_.get();
}

const CommandRegistry* QtVoiceAssistant::GetRegistry() const {
    return registry_.get();
}

void QtVoiceAssistant::SetCommandCallback(QtCommandCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    command_callback_ = std::move(callback);
}

void QtVoiceAssistant::SetErrorCallback(QtErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void QtVoiceAssistant::SetUnrecognizedCallback(QtUnrecognizedCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    unrecognized_callback_ = std::move(callback);
}

void QtVoiceAssistant::SetSpeechDetectedCallback(QtSpeechDetectedCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    speech_detected_callback_ = std::move(callback);
}

void QtVoiceAssistant::SetForceNluStrategy(bool use_nlu) {
    config_.force_nlu_strategy = use_nlu;
    if (running_) {
        SelectStrategy();
    }
}

const QtVoiceAssistantConfig& QtVoiceAssistant::GetConfig() const {
    return config_;
}

void QtVoiceAssistant::OnAudioTimer() {
    // This runs on the main thread via QTimer
    audio_capture::AudioSamples samples;

    // Get audio for VAD check
    audio_engine_->GetAudio(config_.vad_check_duration_ms, samples);

    // Check for speech
    auto vad_result = audio_engine_->DetectSpeech(samples);

    if (vad_result.speech_ended) {
        qDebug() << "[QtVoiceAssistant] Speech detected";
        
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
            } else {
                qWarning() << "[QtVoiceAssistant] WARNING: Queue full, dropping audio";
            }
        }

        // Clear audio buffer
        audio_engine_->ClearBuffer();
    }
}

void QtVoiceAssistant::ProcessingThreadFunc() {
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

void QtVoiceAssistant::SelectStrategy() {
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

void QtVoiceAssistant::ProcessAudio(const audio_capture::AudioSamples& samples) {
    if (!strategy_) {
        qWarning() << "[QtVoiceAssistant] ERROR: No recognition strategy";
        return;
    }

    // Recognize command
    QElapsedTimer recognitionTimer;
    recognitionTimer.start();
    auto recognition = strategy_->Recognize(samples);
    qDebug() << "Recognition took" << recognitionTimer.elapsed() << "ms";

    if (!recognition.success) {
        // Notify unrecognized or error
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (!recognition.raw_transcript.empty() && unrecognized_callback_) {
            qWarning() << "[QtVoiceAssistant] Unrecognized:" << recognition.raw_transcript.c_str();
            unrecognized_callback_(recognition.raw_transcript);
        } else if (error_callback_ && !recognition.error.empty()) {
            qWarning() << "[QtVoiceAssistant] Error:" << recognition.error.c_str();
            error_callback_(recognition.error);
        }
        return;
    }

    qDebug().noquote()
        << "[QtVoiceAssistant] Recognized:"
        << "'" << recognition.raw_transcript.c_str() << "'"
        << "->"
        << "'" << recognition.command_name.c_str() << "'"
        << "(" << qRound(recognition.confidence * 100.0f) << "%)";

    // Build command context
    CommandContext context;
    context.SetRawTranscript(recognition.raw_transcript);
    context.SetConfidence(recognition.confidence);

    for (const auto& [key, value] : recognition.params) {
        context.SetParam(key, ParamValue(value));
    }

    // Dispatch command
    CommandResult result = dispatcher_->Dispatch(recognition.command_name, std::move(context));

    // Notify callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (command_callback_) {
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
