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
#include <condition_variable>
#include <functional>
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
};

/// Callback types for QtVoiceAssistant events
using QtCommandCallback = std::function<void(const std::string& command_name,
                                            CommandResult result,
                                            const CommandContext& context)>;
using QtErrorCallback = std::function<void(const std::string& error)>;
using QtUnrecognizedCallback = std::function<void(const std::string& transcript)>;
using QtSpeechDetectedCallback = std::function<void()>;

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
/// - Callbacks are invoked from the processing thread
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

    /// Set callback for command execution events
    /// @param callback Function called after command dispatch
    void SetCommandCallback(QtCommandCallback callback);

    /// Set callback for errors
    /// @param callback Function called on errors
    void SetErrorCallback(QtErrorCallback callback);

    /// Set callback for unrecognized speech
    /// @param callback Function called when speech doesn't match commands
    void SetUnrecognizedCallback(QtUnrecognizedCallback callback);

    /// Set callback for speech detection
    /// @param callback Function called when speech is detected
    void SetSpeechDetectedCallback(QtSpeechDetectedCallback callback);

    /// Force a specific recognition strategy
    /// @param use_nlu If true, use NLU strategy; if false, use guided
    void SetForceNluStrategy(bool use_nlu);

    /// Get current configuration
    /// @return Reference to configuration
    const QtVoiceAssistantConfig& GetConfig() const;

private slots:
    /// Called by QTimer to poll audio (runs on main thread)
    void OnAudioTimer();

private:
    /// Processing thread function (runs whisper, NLU, dispatch)
    void ProcessingThreadFunc();

    /// Select the appropriate recognition strategy
    void SelectStrategy();

    /// Process a single audio buffer
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

    // Callbacks
    QtCommandCallback command_callback_;
    QtErrorCallback error_callback_;
    QtUnrecognizedCallback unrecognized_callback_;
    QtSpeechDetectedCallback speech_detected_callback_;
    std::mutex callback_mutex_;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_QT_VOICE_ASSISTANT_H
