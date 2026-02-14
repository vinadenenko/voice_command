/// Qt-based Voice Assistant (Timer-based Audio Capture)
/// 
/// This is a modified VoiceAssistant that uses QTimer instead of std::thread
/// for audio capture, properly integrating with Qt's event loop.
///
/// Usage: Same as VoiceAssistant, but requires running within a QCoreApplication
/// event loop (which integration_example already does).

#ifndef VOICE_COMMAND_QT_VOICE_ASSISTANT_H
#define VOICE_COMMAND_QT_VOICE_ASSISTANT_H

#include <QObject>
#include <QTimer>
#include <thread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "audio_engine.h"
#include "command/dispatcher/commanddispatcher.h"
#include "command/nlu/inlu_engine.h"
#include "command/registry/commandregistry.h"
#include "recognition_strategy.h"
#include "whisper_engine.h"

namespace voice_command {

/// Listening mode for voice command activation
enum class ListeningMode {
    kContinuous,   ///< VAD-based, always listening (default, current behavior)
    kWakeWord,     ///< Listen for wake phrase, then capture command
    kPushToTalk    ///< Only capture while StartCapture()/StopCapture() active
};

/// Internal state for listening state machine
enum class ListeningState {
    kIdle,           ///< PTT: waiting for trigger
    kListening,      ///< Continuous/WakeWord: listening for speech
    kWakeWordActive, ///< WakeWord: wake detected, listening for command
    kCapturing       ///< PTT: actively capturing
};

/// Configuration for QtVoiceAssistant
struct QtVoiceAssistantConfig {
    /// Audio engine configuration
    AudioEngineConfig audio_config;

    /// Whisper engine configuration
    WhisperEngineConfig whisper_config;

    /// Duration of audio to capture for VAD check (milliseconds)
    int vad_check_duration_ms = 2000;

    /// Duration of audio to capture for command processing (milliseconds)
    int command_capture_duration_ms = 8000;

    /// Poll interval for audio checking (milliseconds)
    int poll_interval_ms = 100;

    /// Maximum audio queue depth (prevents unbounded memory growth)
    size_t max_queue_depth = 10;

    /// Whether to auto-select strategy based on registered commands
    /// If true, uses GuidedRecognition when no parameterized commands exist
    bool auto_select_strategy = true;

    /// Force NLU strategy even for simple commands
    bool force_nlu_strategy = false;

    /// Listening mode (continuous/wake-word/push-to-talk)
    ListeningMode listening_mode = ListeningMode::kContinuous;

    /// Wake word phrase (required if listening_mode == kWakeWord)
    std::string wake_word;

    /// Timeout (ms) to wait for command after wake word detected
    int wake_word_timeout_ms = 5000;

    /// Minimum confidence for wake word detection (0.0-1.0)
    float wake_word_confidence = 0.5f;
};

/// QtVoiceAssistant uses QTimer for audio capture instead of std::thread
///
/// This ensures proper integration with Qt's event loop, which is required
/// for Qt Multimedia (QAudioSource) to deliver audio data.
///
/// Architecture:
/// - QTimer on main thread: Polls audio every poll_interval_ms
/// - Processing thread: Runs whisper, NLU, and dispatches commands
/// - Audio queue: Mutex-protected queue between timer and processing thread
///
/// Thread Safety:
/// - All public methods are thread-safe
/// - Signals are emitted from the processing thread
/// - Must be created and used within a QCoreApplication/QApplication
class QtVoiceAssistant : public QObject {
    Q_OBJECT

public:
    explicit QtVoiceAssistant(QObject* parent = nullptr);
    ~QtVoiceAssistant() override;

    // Non-copyable, non-movable
    QtVoiceAssistant(const QtVoiceAssistant&) = delete;
    QtVoiceAssistant& operator=(const QtVoiceAssistant&) = delete;
    QtVoiceAssistant(QtVoiceAssistant&&) = delete;
    QtVoiceAssistant& operator=(QtVoiceAssistant&&) = delete;

    /// Initialize the voice assistant
    /// @param config Configuration for all components
    /// @param nlu_engine NLU engine to use (takes ownership)
    /// @return true if initialization succeeded
    bool Init(const QtVoiceAssistantConfig& config,
              std::unique_ptr<INluEngine> nlu_engine);

    /// Shutdown and release all resources
    void Shutdown();

    /// Check if initialized
    /// @return true if initialized
    bool IsInitialized() const;

    /// Start voice command processing
    /// Starts audio timer and processing thread
    /// @return true if started successfully
    bool Start();

    /// Stop voice command processing
    /// Stops timer and thread, clears queues
    void Stop();

    /// Check if processing is running
    /// @return true if running
    bool IsRunning() const;

    /// Get the command registry for registering commands
    /// @return Pointer to registry (owned by QtVoiceAssistant)
    CommandRegistry* GetRegistry();

    /// Get the command registry (const version)
    /// @return Const pointer to registry
    const CommandRegistry* GetRegistry() const;

    /// Begin push-to-talk capture (kPushToTalk mode only)
    /// @return true if capture started successfully
    Q_INVOKABLE bool startCapture();

    /// End push-to-talk capture and queue for processing
    /// @return true if capture ended successfully
    Q_INVOKABLE bool stopCapture();

    /// Check if currently capturing (PTT mode)
    /// @return true if in kCapturing state
    bool IsCapturing() const;

    /// Get current listening mode
    /// @return Current listening mode
    ListeningMode GetListeningMode() const;

    /// Get current listening state
    /// @return Current listening state
    ListeningState GetListeningState() const;

    /// Force a specific recognition strategy
    /// @param use_nlu If true, use NLU strategy; if false, use guided
    void SetForceNluStrategy(bool use_nlu);

    /// Get current configuration
    /// @return Reference to configuration
    const QtVoiceAssistantConfig& GetConfig() const;

signals:
    /// Emitted when wake word is detected (kWakeWord mode)
    void wakeWordDetected();

    /// Emitted when PTT capture starts
    void captureStarted();

    /// Emitted when PTT capture ends
    void captureEnded();

    /// Emitted when listening state changes
    void listeningStateChanged(ListeningState oldState, ListeningState newState);

    /// Emitted after a command is executed
    void commandExecuted(const std::string& command_name,
                         CommandResult result,
                         const CommandContext& context);

    /// Emitted when an error occurs
    void errorOccurred(const std::string& error);

    /// Emitted when speech is detected but doesn't match any command
    void unrecognizedSpeech(const std::string& transcript);

    /// Emitted when speech is detected (before recognition)
    void speechDetected();

private slots:
    /// Called by QTimer to poll audio (runs on main thread)
    void OnAudioTimer();

private:
    /// Processing thread function (runs whisper, NLU, dispatch)
    void ProcessingThreadFunc();

    /// Select the appropriate recognition strategy
    void SelectStrategy();

    /// Process a single audio buffer
    /// @param samples Audio samples to process
    void ProcessAudio(const audio_capture::AudioSamples& samples);

    // Configuration
    QtVoiceAssistantConfig config_;

    // Core components
    std::unique_ptr<AudioEngine> audio_engine_;
    std::unique_ptr<WhisperEngine> whisper_engine_;
    std::unique_ptr<INluEngine> nlu_engine_;
    std::unique_ptr<CommandRegistry> registry_;
    std::unique_ptr<CommandDispatcher> dispatcher_;

    // Recognition strategy
    std::unique_ptr<IRecognitionStrategy> strategy_;

    // Qt timer for audio polling (replaces audio thread)
    QTimer* audio_timer_ = nullptr;

    // Processing thread (whisper/NLU still runs in thread)
    std::thread processing_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    // Audio queue (between timer and processing thread)
    std::queue<audio_capture::AudioSamples> audio_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Listening state machine
    std::atomic<ListeningState> listening_state_{ListeningState::kIdle};
    std::chrono::steady_clock::time_point capture_start_time_;
    std::chrono::steady_clock::time_point wake_timeout_start_;

    /// Transition listening state with signal notification
    void SetListeningState(ListeningState new_state);

    /// Mode-specific timer handlers
    void OnAudioTimer_Continuous();
    void OnAudioTimer_WakeWord();
    void OnAudioTimer_PushToTalk();

    /// Queue audio for processing
    void QueueAudio(audio_capture::AudioSamples samples);
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_QT_VOICE_ASSISTANT_H
