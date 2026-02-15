#ifndef VOICE_COMMAND_REMOTE_LLM_NLU_ENGINE_H
#define VOICE_COMMAND_REMOTE_LLM_NLU_ENGINE_H

#include <string>
#include <vector>

#include "command/nlu/inlu_engine.h"

namespace voice_command {

/// Configuration for remote LLM NLU server (OpenAI-compatible API)
struct RemoteLlmNluConfig {
    /// Server URL (e.g., "http://localhost:8000")
    std::string server_url;

    /// API endpoint path (default: "/v1/chat/completions")
    std::string endpoint = "/v1/chat/completions";

    /// API key (optional, for authenticated APIs)
    std::string api_key;

    /// Model name for the API (e.g., "llama-3.2-3b", "gpt-4")
    std::string model;

    /// HTTP request timeout in milliseconds
    int timeout_ms = 30000;

    /// Sampling temperature (0 = deterministic)
    float temperature = 0.0f;

    /// Maximum tokens in response
    int max_tokens = 256;

    /// Enable debug logging of LLM requests/responses
    bool enable_debug_logging = false;
};

/// RemoteLlmNluEngine sends transcripts to a remote LLM for intent classification.
///
/// Uses OpenAI-compatible chat completions API, which works with:
/// - OpenAI API
/// - Ollama
/// - vLLM
/// - llama.cpp server
/// - Any OpenAI-compatible endpoint
///
/// The LLM is prompted to:
/// 1. Classify the transcript into one of the available commands
/// 2. Extract parameters according to command schemas
/// 3. Return structured JSON response
///
/// Thread Safety:
/// - Each HTTP request is independent
/// - Safe for concurrent use from multiple threads
class RemoteLlmNluEngine : public INluEngine {
public:
    /// Construct with configuration (stored, used in Init())
    /// @param config Server configuration
    explicit RemoteLlmNluEngine(const RemoteLlmNluConfig& config);

    ~RemoteLlmNluEngine() override;

    // Non-copyable
    RemoteLlmNluEngine(const RemoteLlmNluEngine&) = delete;
    RemoteLlmNluEngine& operator=(const RemoteLlmNluEngine&) = delete;

    /// Initialize the engine (validates configuration)
    /// @return true if configuration is valid
    bool Init() override;

    /// Process transcript using remote LLM
    /// @param transcript The text to classify
    /// @param schemas Available command schemas
    /// @return NLU result with command name, confidence, and extracted params
    NluResult Process(
        const std::string& transcript,
        const std::vector<const CommandDescriptor*>& schemas) override;

    /// Check if engine is initialized
    /// @return true if initialized
    bool IsInitialized() const { return initialized_; }

    /// Get the current configuration
    /// @return Reference to configuration
    const RemoteLlmNluConfig& GetConfig() const { return config_; }

private:
    /// Build system prompt describing the task and available commands
    /// @param schemas Command schemas to include in prompt
    /// @return System prompt string
    std::string BuildSystemPrompt(
        const std::vector<const CommandDescriptor*>& schemas) const;

    /// Format a parameter type as string
    /// @param type Parameter type enum
    /// @return Type name string
    std::string ParamTypeToString(ParamType type) const;

    /// Parse LLM response content into NluResult
    /// @param content The content field from LLM response
    /// @return Parsed NLU result
    NluResult ParseLlmContent(const std::string& content) const;

    RemoteLlmNluConfig config_;
    bool initialized_ = false;
};

}  // namespace voice_command

#endif  // VOICE_COMMAND_REMOTE_LLM_NLU_ENGINE_H
