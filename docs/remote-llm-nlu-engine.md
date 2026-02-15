# RemoteLlmNluEngine Implementation Plan

## Goal
Add `RemoteLlmNluEngine` - an NLU engine that sends prompts to a remote LLM server using OpenAI-compatible API.

## Design Decisions

### Config Pattern
- **Constructor injection**: Config passed to constructor
- **Parameterless Init()**: Keeps `INluEngine` interface unchanged
- **Application responsibility**: App creates config, library provides implementation

```cpp
RemoteLlmNluConfig config;
config.server_url = "http://localhost:8000";
config.model = "llama-3.2-3b";

auto nlu = std::make_unique<RemoteLlmNluEngine>(config);
// Later, VoiceAssistant calls nlu->Init()
```

### Future Consideration
Later we may add convenience classes like:
- `PreConfiguredQtVoiceAssistant` - zero-config for common use cases
- `DefaultLocalConfig` / `DefaultRemoteConfig` - sensible defaults

## Implementation

### New Files
| File | Description |
|------|-------------|
| `include/command/nlu/remote_llm_nlu_engine.h` | Header with config and class |
| `src/command/nlu/remote_llm_nlu_engine.cpp` | Implementation |

### RemoteLlmNluConfig
```cpp
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
};
```

### RemoteLlmNluEngine Class
```cpp
class RemoteLlmNluEngine : public INluEngine {
public:
    /// Construct with configuration (stored, used in Init())
    explicit RemoteLlmNluEngine(const RemoteLlmNluConfig& config);
    ~RemoteLlmNluEngine() override;

    // Non-copyable
    RemoteLlmNluEngine(const RemoteLlmNluEngine&) = delete;
    RemoteLlmNluEngine& operator=(const RemoteLlmNluEngine&) = delete;

    /// Initialize (validates config)
    bool Init() override;

    /// Process transcript using remote LLM
    NluResult Process(
        const std::string& transcript,
        const std::vector<const CommandDescriptor*>& schemas) override;

private:
    /// Build system prompt describing the task
    std::string BuildSystemPrompt(
        const std::vector<const CommandDescriptor*>& schemas) const;

    /// Build user message with transcript
    std::string BuildUserMessage(const std::string& transcript) const;

    /// Parse LLM response JSON into NluResult
    NluResult ParseResponse(const std::string& response) const;

    RemoteLlmNluConfig config_;
    bool initialized_ = false;
};
```

### OpenAI-Compatible API

**Request** (POST to `/v1/chat/completions`):
```json
{
  "model": "model-name",
  "messages": [
    {"role": "system", "content": "<system prompt>"},
    {"role": "user", "content": "<transcript>"}
  ],
  "temperature": 0.0,
  "max_tokens": 256
}
```

**Response**:
```json
{
  "choices": [
    {
      "message": {
        "content": "{\"command\": \"...\", \"confidence\": 0.95, \"params\": {...}}"
      }
    }
  ]
}
```

### System Prompt Strategy

The system prompt instructs the LLM to:
1. Classify the user's intent into one of the available commands
2. Extract parameters according to command schemas
3. Return structured JSON

Example system prompt:
```
You are a voice command classifier. Given a transcript, identify the command and extract parameters.

Available commands:
1. "zoom_to" - Zooms to a level
   Parameters:
   - level (integer, required): Zoom level 1-20

2. "set_brightness" - Sets brightness
   Parameters:
   - value (integer, optional, default=50): Brightness 0-100

Respond with JSON only:
{"command": "command_name", "confidence": 0.0-1.0, "params": {"key": "value"}}

If no command matches, respond:
{"command": "", "confidence": 0.0, "params": {}}
```

### Expected LLM Response Format
```json
{
  "command": "zoom_to",
  "confidence": 0.95,
  "params": {
    "level": "15"
  }
}
```

## Files to Modify

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add new source files |
| (No interface changes needed) | |

## Dependencies
Already added:
- `cpp-httplib` - HTTP client
- `nlohmann_json` - JSON parsing

## Testing Strategy
1. Unit test prompt building with mock schemas
2. Integration test with local LLM server (Ollama, llama.cpp server)
3. Test error handling (timeout, invalid response, etc.)

## Open Questions (for later)
1. Should we support streaming responses?
2. Should we add retry logic for transient failures?
3. Should we cache command schemas to avoid rebuilding prompt?
