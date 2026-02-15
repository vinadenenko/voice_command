#ifndef VOICE_COMMAND_VOICE_ASSISTANT_H
#define VOICE_COMMAND_VOICE_ASSISTANT_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "asr_engine.h"
#include "audio_engine.h"
#include "command/dispatcher/commanddispatcher.h"
#include "command/nlu/inlu_engine.h"
#include "command/registry/commandregistry.h"
#include "recognition_strategy.h"

namespace voice_command {

/// Configuration for VoiceAssistant
///
/// Note: ASR engine is now injected via Init() rather than configured here.
/// This allows using different ASR backends (local whisper, remote server, etc.)
struct VoiceAssistantConfig {
    /// Audio engine configuration
    AudioEngineConfig audio_config;

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

/// Callback types for VoiceAssistant events
using CommandCallback = std::function<void(const std::string& command_name,
                                           CommandResult result,
                                           const CommandContext& context)>;
using ErrorCallback = std::function<void(const std::string& error)>;
using UnrecognizedCallback = std::function<void(const std::string& transcript)>;
using SpeechDetectedCallback = std::function<void()>;

/// VoiceAssistant is the main orchestrator for voice command processing.
///
/// It manages the complete pipeline:
/// - Audio capture in a dedicated thread
/// - Voice activity detection
/// - Speech recognition via IAsrEngine (injected)
/// - Intent/parameter extraction via recognition strategy
/// - Command dispatch via CommandDispatcher
///
/// Architecture:
/// - Audio thread: Captures audio, detects speech, queues audio for processing
/// - Processing thread: Runs whisper, NLU, and dispatches commands
/// - Audio queue: Mutex-protected queue between threads
///
/// Thread Safety:
/// - All public methods are thread-safe
/// - Callbacks are invoked from the processing thread
/// - Start/Stop can be called from any thread
class VoiceAssistant {
public:
    VoiceAssistant();
    ~VoiceAssistant();

    // Non-copyable, non-movable (due to threads)
    VoiceAssistant(const VoiceAssistant&) = delete;
    VoiceAssistant& operator=(const VoiceAssistant&) = delete;
    VoiceAssistant(VoiceAssistant&&) = delete;
    VoiceAssistant& operator=(VoiceAssistant&&) = delete;

    /// Initialize the voice assistant
    /// @param asr_engine ASR engine for speech-to-text (not owned, must outlive)
    /// @param nlu_engine NLU engine to use (takes ownership)
    /// @param config Configuration for other components
    /// @return true if initialization succeeded
    bool Init(IAsrEngine* asr_engine,
              std::unique_ptr<INluEngine> nlu_engine,
              const VoiceAssistantConfig& config);

    /// Shutdown and release all resources
    void Shutdown();

    /// Check if initialized
    /// @return true if initialized
    bool IsInitialized() const;

    /// Start voice command processing
    /// Starts both audio capture and processing threads
    /// @return true if started successfully
    bool Start();

    /// Stop voice command processing
    /// Stops threads and clears queues
    void Stop();

    /// Check if processing is running
    /// @return true if running
    bool IsRunning() const;

    /// Get the command registry for registering commands
    /// @return Pointer to registry (owned by VoiceAssistant)
    CommandRegistry* GetRegistry();

    /// Get the command registry (const version)
    /// @return Const pointer to registry
    const CommandRegistry* GetRegistry() const;

    /// Set callback for command execution events
    /// @param callback Function called after command dispatch
    void SetCommandCallback(CommandCallback callback);

    /// Set callback for errors
    /// @param callback Function called on errors
    void SetErrorCallback(ErrorCallback callback);

    /// Set callback for unrecognized speech
    /// @param callback Function called when speech doesn't match commands
    void SetUnrecognizedCallback(UnrecognizedCallback callback);

    /// Set callback for speech detection
    /// @param callback Function called when speech is detected
    void SetSpeechDetectedCallback(SpeechDetectedCallback callback);

    /// Force a specific recognition strategy
    /// @param use_nlu If true, use NLU strategy; if false, use guided
    void SetForceNluStrategy(bool use_nlu);

    /// Get current configuration
    /// @return Reference to configuration
    const VoiceAssistantConfig& GetConfig() const;

private:
    /// Audio capture thread function
    void AudioThreadFunc();

    /// Processing thread function
    void ProcessingThreadFunc();

    /// Select the appropriate recognition strategy
    void SelectStrategy();

    /// Process a single audio buffer
    void ProcessAudio(const audio_capture::AudioSamples& samples);

    // Configuration
    VoiceAssistantConfig config_;

    // Core components
    std::unique_ptr<AudioEngine> audio_engine_;
    IAsrEngine* asr_engine_ = nullptr;  // Not owned, injected via Init()
    std::unique_ptr<INluEngine> nlu_engine_;
    std::unique_ptr<CommandRegistry> registry_;
    std::unique_ptr<CommandDispatcher> dispatcher_;

    // Recognition strategy
    std::unique_ptr<IRecognitionStrategy> strategy_;

    // Thread management
    std::thread audio_thread_;
    std::thread processing_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    // Audio queue (between audio and processing threads)
    std::queue<audio_capture::AudioSamples> audio_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Callbacks
    CommandCallback command_callback_;
    ErrorCallback error_callback_;
    UnrecognizedCallback unrecognized_callback_;
    SpeechDetectedCallback speech_detected_callback_;
    std::mutex callback_mutex_;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_VOICE_ASSISTANT_H
