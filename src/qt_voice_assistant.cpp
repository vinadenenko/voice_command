/// QtVoiceAssistant Implementation (Timer-based Audio Capture)

#include "qt_voice_assistant.h"

#include <QTimer>

#include <chrono>
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

    // Initialize listening state based on mode
    switch (config_.listening_mode) {
        case ListeningMode::kContinuous:
            SetListeningState(ListeningState::kListening);
            break;
        case ListeningMode::kWakeWord:
            if (config_.wake_word.empty()) {
                qWarning() << "[QtVoiceAssistant] ERROR: Wake word required for kWakeWord mode";
                running_ = false;
                audio_engine_->Stop();
                return false;
            }
            SetListeningState(ListeningState::kListening);
            break;
        case ListeningMode::kPushToTalk:
            SetListeningState(ListeningState::kIdle);
            break;
    }

    // Configure and start the audio timer (replaces audio thread)
    audio_timer_->setInterval(config_.poll_interval_ms);
    audio_timer_->start();

    // Start processing thread
    processing_thread_ = std::thread(&QtVoiceAssistant::ProcessingThreadFunc, this);

    qDebug() << "[QtVoiceAssistant] Started - mode:"
             << static_cast<int>(config_.listening_mode);
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

    // Reset listening state
    listening_state_ = ListeningState::kIdle;
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

void QtVoiceAssistant::SetForceNluStrategy(bool use_nlu) {
    config_.force_nlu_strategy = use_nlu;
    if (running_) {
        SelectStrategy();
    }
}

const QtVoiceAssistantConfig& QtVoiceAssistant::GetConfig() const {
    return config_;
}

bool QtVoiceAssistant::startCapture() {
    if (config_.listening_mode != ListeningMode::kPushToTalk) {
        qWarning() << "[QtVoiceAssistant] startCapture() only valid in PTT mode";
        return false;
    }

    if (!running_) {
        qWarning() << "[QtVoiceAssistant] Cannot start capture - not running";
        return false;
    }

    if (listening_state_ != ListeningState::kIdle) {
        qWarning() << "[QtVoiceAssistant] Cannot start capture - not in idle state";
        return false;
    }

    audio_engine_->ClearBuffer();
    capture_start_time_ = std::chrono::steady_clock::now();
    SetListeningState(ListeningState::kCapturing);
    emit captureStarted();
    qDebug() << "[QtVoiceAssistant] PTT capture started";
    return true;
}

bool QtVoiceAssistant::stopCapture() {
    if (listening_state_ != ListeningState::kCapturing) {
        qWarning() << "[QtVoiceAssistant] Cannot stop capture - not capturing";
        return false;
    }

    auto duration = std::chrono::steady_clock::now() - capture_start_time_;
    int duration_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());

    qDebug() << "[QtVoiceAssistant] PTT capture stopped, duration:" << duration_ms << "ms";

    audio_capture::AudioSamples samples;
    audio_engine_->GetAudio(duration_ms, samples);

    QueueAudio(std::move(samples));

    audio_engine_->ClearBuffer();
    SetListeningState(ListeningState::kIdle);
    emit captureEnded();
    return true;
}

bool QtVoiceAssistant::IsCapturing() const {
    return listening_state_ == ListeningState::kCapturing;
}

ListeningMode QtVoiceAssistant::GetListeningMode() const {
    return config_.listening_mode;
}

ListeningState QtVoiceAssistant::GetListeningState() const {
    return listening_state_.load();
}

void QtVoiceAssistant::SetListeningState(ListeningState new_state) {
    ListeningState old_state = listening_state_.exchange(new_state);
    if (old_state != new_state) {
        emit listeningStateChanged(old_state, new_state);
    }
}

void QtVoiceAssistant::QueueAudio(audio_capture::AudioSamples samples) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (audio_queue_.size() < config_.max_queue_depth) {
        audio_queue_.push(std::move(samples));
        queue_cv_.notify_one();
    } else {
        qWarning() << "[QtVoiceAssistant] WARNING: Queue full, dropping audio";
    }
}

void QtVoiceAssistant::OnAudioTimer() {
    // This runs on the main thread via QTimer
    switch (config_.listening_mode) {
        case ListeningMode::kContinuous:
            OnAudioTimer_Continuous();
            break;
        case ListeningMode::kWakeWord:
            OnAudioTimer_WakeWord();
            break;
        case ListeningMode::kPushToTalk:
            OnAudioTimer_PushToTalk();
            break;
    }
}

void QtVoiceAssistant::OnAudioTimer_Continuous() {
    // Original continuous VAD-based behavior
    audio_capture::AudioSamples samples;

    // Get audio for VAD check
    audio_engine_->GetAudio(config_.vad_check_duration_ms, samples);

    // Check for speech
    auto vad_result = audio_engine_->DetectSpeech(samples);

    if (vad_result.speech_ended) {
        qDebug() << "[QtVoiceAssistant] Speech detected (continuous mode)";

        emit speechDetected();

        // Get full command audio
        audio_engine_->GetAudio(config_.command_capture_duration_ms, samples);

        QueueAudio(std::move(samples));

        // Clear audio buffer
        audio_engine_->ClearBuffer();
    }
}

void QtVoiceAssistant::OnAudioTimer_WakeWord() {
    if (listening_state_ == ListeningState::kListening) {
        // Listening for wake word
        audio_capture::AudioSamples samples;
        audio_engine_->GetAudio(config_.vad_check_duration_ms, samples);

        auto vad_result = audio_engine_->DetectSpeech(samples);

        if (vad_result.speech_ended) {
            // Check if speech matches wake word
            auto match = whisper_engine_->GuidedMatch(samples, {config_.wake_word});

            if (match.success && match.best_score >= config_.wake_word_confidence) {
                qDebug() << "[QtVoiceAssistant] Wake word detected:"
                         << config_.wake_word.c_str()
                         << "confidence:" << match.best_score;

                emit wakeWordDetected();

                wake_timeout_start_ = std::chrono::steady_clock::now();
                SetListeningState(ListeningState::kWakeWordActive);
            }
            audio_engine_->ClearBuffer();
        }
    } else if (listening_state_ == ListeningState::kWakeWordActive) {
        // Wake word detected, listening for command
        auto elapsed = std::chrono::steady_clock::now() - wake_timeout_start_;
        if (elapsed > std::chrono::milliseconds(config_.wake_word_timeout_ms)) {
            qDebug() << "[QtVoiceAssistant] Wake word command timeout";
            SetListeningState(ListeningState::kListening);
            audio_engine_->ClearBuffer();
            return;
        }

        audio_capture::AudioSamples samples;
        audio_engine_->GetAudio(config_.vad_check_duration_ms, samples);

        auto vad_result = audio_engine_->DetectSpeech(samples);

        if (vad_result.speech_ended) {
            qDebug() << "[QtVoiceAssistant] Command speech detected (wake-word mode)";

            emit speechDetected();

            // Get full command audio
            audio_engine_->GetAudio(config_.command_capture_duration_ms, samples);
            QueueAudio(std::move(samples));
            audio_engine_->ClearBuffer();
            SetListeningState(ListeningState::kListening);  // Return to wake-word mode
        }
    }
}

void QtVoiceAssistant::OnAudioTimer_PushToTalk() {
    // PTT mode: minimal work - just keep audio engine running
    // Actual capture is triggered by startCapture/stopCapture
    // No automatic speech detection
}

void QtVoiceAssistant::ProcessingThreadFunc() {
    while (running_) {
        audio_capture::AudioSamples samples;

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

    auto recognition = strategy_->Recognize(samples);
    qDebug() << "[Timing] ASR: " << recognition.asr_time_ms
             << "ms, NLU: " << recognition.nlu_time_ms
             << "ms, Total: " << recognition.total_time_ms << "ms";

    if (!recognition.success) {
        if (!recognition.raw_transcript.empty()) {
            qWarning() << "[QtVoiceAssistant] Unrecognized:" << recognition.raw_transcript.c_str();
            emit unrecognizedSpeech(recognition.raw_transcript);
        } else if (!recognition.error.empty()) {
            qWarning() << "[QtVoiceAssistant] Error:" << recognition.error.c_str();
            emit errorOccurred(recognition.error);
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

    // Emit signal with command result
    CommandContext signal_context;
    signal_context.SetRawTranscript(recognition.raw_transcript);
    signal_context.SetConfidence(recognition.confidence);
    for (const auto& [key, value] : recognition.params) {
        signal_context.SetParam(key, ParamValue(value));
    }
    emit commandExecuted(recognition.command_name, result, signal_context);
}

}  // namespace voice_command
