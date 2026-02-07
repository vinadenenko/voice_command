# VoiceCommand Library — Technical Design Document

## 1. Overview

VoiceCommand is a C++17 library that enables application developers to add voice-controlled command execution to their software. It combines real-time speech recognition (via whisper.cpp) with a structured command framework (command design pattern) and natural language understanding for parameter extraction.

### Problem Statement

Integrating voice commands into an application today requires stitching together speech recognition, intent parsing, parameter extraction, and command dispatch — each with its own complexity. Developers who want to add a simple "zoom in" voice command face the same integration burden as those building complex parameterized commands like "generate a linestring placemark with 10 vertices around New York city with dashed style and yellow color."

### Goals

- **Simple integration**: Developers subclass `ICommand`, override `Execute()`, register it, and it works.
- **Two-tier command model**: Support both simple keyword commands (fast, no LLM) and parameterized commands (full NLU with schema-based parameter extraction).
- **Offline-first**: All core functionality runs locally using whisper.cpp and llama.cpp. No cloud dependency required.
- **Pluggable NLU**: Ship a default LLM-based NLU engine, but let developers swap in their own (cloud APIs, custom NLP, rule-based).
- **Schema-driven parameterized commands**: Inspired by LLM function calling (OpenAI, MCP), commands declare typed parameter schemas. The NLU engine extracts and validates parameters from natural language against these schemas.
- **Performance**: Sub-second recognition for simple commands. Reasonable latency for parameterized commands (dependent on LLM model size).
- **Thread-safe**: Designed for real-time audio processing with concurrent command dispatch.

### Non-Goals

- Not a general-purpose speech-to-text library (whisper.cpp handles that).
- Not a conversational AI / chatbot framework.
- Not a GUI framework — this is a headless library.

---

## 2. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│                        VoiceAssistant                                │
│                     (Top-level Orchestrator)                         │
│                                                                      │
│  ┌──────────┐   ┌───────────┐   ┌──────────────┐   ┌─────────────┐ │
│  │ Audio    │──>│ Whisper   │──>│ Recognition  │──>│ Command     │ │
│  │ Engine   │   │ ASR       │   │ Strategy     │   │ Dispatcher  │ │
│  │ (SDL2)   │   │           │   │              │   │             │ │
│  └──────────┘   └───────────┘   │ ┌──────────┐ │   └──────┬──────┘ │
│                                 │ │ Simple   │ │          │        │
│                                 │ │ Matcher  │ │          ▼        │
│  ┌──────────────────────┐       │ ├──────────┤ │   ┌─────────────┐ │
│  │ CommandRegistry      │<──────│ │ NLU      │ │   │ ICommand    │ │
│  │                      │       │ │ Engine   │ │   │ ::Execute() │ │
│  │ - commands map       │       │ └──────────┘ │   └─────────────┘ │
│  │ - descriptors        │       └──────────────┘                    │
│  └──────────────────────┘                                           │
└──────────────────────────────────────────────────────────────────────┘
```

### Data Flow

**Simple command** (e.g., "list files"):
```
Microphone → SDL2 audio capture → VAD detects speech → Whisper guided-mode inference
→ Probability scoring against known command list → Best match: "list files"
→ CommandDispatcher finds "list_files" in registry → ListFilesCommand::Execute()
```

**Parameterized command** (e.g., "create a red dashed linestring with 10 vertices near New York"):
```
Microphone → SDL2 audio capture → VAD detects speech → Whisper general transcription
→ Full transcript text → NLU Engine receives text + all command schemas
→ NLU returns: intent="create_placemark", params={geometry_type: "linestring", vertex_count: 10, ...}
→ Parameter validation against schema → CommandDispatcher → CreatePlacemarkCommand::Execute(context)
```

### Mode Selection

The system operates in one of two modes, determined at startup based on registered commands:

| Condition | Mode | Pipeline |
|-----------|------|----------|
| All commands are simple (no parameter schemas) | **Guided** | Whisper guided probability matching → direct dispatch |
| Any command has parameter schemas | **General** | Whisper full transcription → NLU → dispatch |

In General mode, the NLU engine handles both simple and parameterized commands uniformly — simple commands are just parameterized commands with zero parameters.

---

## 3. Core Abstractions

### 3.1 CommandContext

Carries extracted parameters and metadata to the command's `Execute` method. Provides typed accessors to avoid stringly-typed parameter handling.

```cpp
namespace voice_command {

// Represents a single parameter value extracted from speech.
// Internally stored as string, with typed accessors.
class ParamValue {
 public:
  ParamValue() = default;
  explicit ParamValue(std::string raw_value);

  std::string AsString() const;
  int AsInt() const;            // throws std::invalid_argument
  double AsDouble() const;      // throws std::invalid_argument
  bool AsBool() const;          // "true"/"false"/"yes"/"no"/"1"/"0"

  bool IsEmpty() const;

 private:
  std::string raw_value_;
};

// Passed to ICommand::Execute(). Contains extracted parameters,
// the raw transcript, and recognition metadata.
class CommandContext {
 public:
  // Access a parameter by name. Returns empty ParamValue if not found.
  ParamValue GetParam(const std::string& name) const;

  // Check if a parameter was extracted.
  bool HasParam(const std::string& name) const;

  // All extracted parameters.
  const std::unordered_map<std::string, ParamValue>& GetAllParams() const;

  // The raw transcribed text from whisper.
  const std::string& GetRawTranscript() const;

  // Recognition confidence score (0.0 - 1.0).
  float GetConfidence() const;

 private:
  friend class CommandDispatcher;
  friend class NluResult;

  std::unordered_map<std::string, ParamValue> params_;
  std::string raw_transcript_;
  float confidence_ = 0.0f;
};

}  // namespace voice_command
```

### 3.2 ICommand

Abstract base class. Developers subclass this and override `Execute()`.

```cpp
namespace voice_command {

// Result of command execution.
enum class CommandResult {
  kSuccess,
  kFailure,
  kInvalidParams,
  kNotHandled,  // Command recognized but chose not to handle
};

class ICommand {
 public:
  virtual ~ICommand() = default;

  // Execute the command with the given context.
  // The context contains extracted parameters (if any),
  // the raw transcript, and confidence metadata.
  virtual CommandResult Execute(const CommandContext& context) = 0;

  // Optional: Return a human-readable name for logging.
  virtual std::string GetName() const { return "unnamed_command"; }
};

}  // namespace voice_command
```

### 3.3 CommandDescriptor

Defines the schema for a command — its trigger phrases, parameters, types, and constraints. This is the "tool definition" analogous to LLM function calling schemas.

```cpp
namespace voice_command {

// Supported parameter types.
enum class ParamType {
  kString,
  kInteger,
  kDouble,
  kBool,
  kEnum,    // String constrained to a set of allowed values
};

// Defines a single parameter in a command's schema.
struct ParamDescriptor {
  std::string name;                          // "geometry_type"
  ParamType type = ParamType::kString;
  std::string description;                   // Human-readable, used by NLU
  bool required = false;
  std::string default_value;                 // Used when param not extracted
  std::vector<std::string> enum_values;      // For kEnum type
  std::optional<double> min_value;           // For kInteger, kDouble
  std::optional<double> max_value;           // For kInteger, kDouble
};

// Full schema for a command. Registered alongside the ICommand instance.
struct CommandDescriptor {
  // Unique identifier for the command. Used as registry key.
  std::string name;                          // "create_placemark"

  // Natural language description. Used by NLU to understand intent.
  std::string description;                   // "Creates a geometry placemark on the map"

  // Phrases that trigger this command. For simple commands, these are
  // the keywords matched by guided mode. For parameterized commands,
  // the NLU uses these plus the description for intent classification.
  std::vector<std::string> trigger_phrases;  // {"create placemark", "add placemark"}

  // Parameter schema. Empty = simple command (no parameters).
  std::vector<ParamDescriptor> parameters;

  // Returns true if this command has parameters (parameterized mode).
  bool IsParameterized() const { return !parameters.empty(); }
};

}  // namespace voice_command
```

### 3.4 CommandRegistry

Thread-safe registry mapping command names to their implementations and descriptors.

```cpp
namespace voice_command {

class CommandRegistry {
 public:
  CommandRegistry() = default;

  // Register a command with its descriptor.
  // Takes ownership of the command via unique_ptr.
  // Returns false if a command with the same name is already registered.
  bool Register(const CommandDescriptor& descriptor,
                std::unique_ptr<ICommand> command);

  // Convenience: register a simple command with just a name and trigger phrases.
  bool RegisterSimple(const std::string& name,
                      const std::vector<std::string>& triggers,
                      std::unique_ptr<ICommand> command);

  // Unregister a command by name.
  bool Unregister(const std::string& name);

  // Lookup.
  ICommand* FindCommand(const std::string& name) const;
  const CommandDescriptor* FindDescriptor(const std::string& name) const;

  // Iteration.
  std::vector<std::string> GetAllCommandNames() const;
  std::vector<const CommandDescriptor*> GetAllDescriptors() const;

  // Returns true if any registered command has parameters.
  bool HasParameterizedCommands() const;

  // Get all trigger phrases (for guided mode word list).
  std::vector<std::string> GetAllTriggerPhrases() const;

 private:
  struct Entry {
    CommandDescriptor descriptor;
    std::unique_ptr<ICommand> command;
  };

  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, Entry> commands_;
};

}  // namespace voice_command
```

### 3.5 CommandDispatcher

Routes recognized intents to commands. Handles parameter validation and default value injection.

```cpp
namespace voice_command {

// Callback for command execution events (logging, telemetry).
using DispatchCallback = std::function<void(const std::string& command_name,
                                            CommandResult result,
                                            const CommandContext& context)>;

class CommandDispatcher {
 public:
  explicit CommandDispatcher(CommandRegistry* registry);

  // Dispatch a recognized command.
  // - Looks up the command in the registry.
  // - Validates parameters against the descriptor's schema.
  // - Injects default values for missing optional parameters.
  // - Calls ICommand::Execute().
  // Returns the result from Execute(), or kInvalidParams if validation fails.
  CommandResult Dispatch(const std::string& command_name,
                         CommandContext context);

  // Set a callback invoked after every dispatch (for logging/telemetry).
  void SetCallback(DispatchCallback callback);

 private:
  // Validate extracted parameters against the command's schema.
  // Injects defaults for missing optional params.
  // Returns false if a required param is missing or type validation fails.
  bool ValidateAndFillDefaults(const CommandDescriptor& descriptor,
                               CommandContext& context);

  CommandRegistry* registry_;  // Not owned
  DispatchCallback callback_;
};

}  // namespace voice_command
```

---

## 4. Command Schema System

### 4.1 Design Rationale: Why Schemas?

The fundamental challenge with complex voice commands is bridging the gap between **unstructured natural language** and **structured function calls**. Consider:

> "generate a linestring placemark with 10 vertices around New York city with dashed style and yellow color"

This single utterance must be decomposed into:
- **Intent**: `create_placemark`
- **Parameters**: `geometry_type=linestring`, `vertex_count=10`, `location=New York city`, `line_style=dashed`, `color=yellow`

This is exactly the same problem that LLM function calling solves. When you define a "tool" for an LLM (OpenAI function calling, Anthropic tool use, MCP), you provide:
1. A function name and description (so the LLM knows when to call it)
2. A parameter schema with types, constraints, and descriptions (so the LLM knows how to extract arguments)

Our command schema system follows the same pattern. Each `CommandDescriptor` is effectively a "tool definition" that the NLU engine uses to:
1. **Classify intent** — Which command does the utterance match?
2. **Extract parameters** — What are the typed argument values?
3. **Validate** — Do extracted values satisfy type/range/enum constraints?

### 4.2 Schema Format (JSON Configuration)

Commands can be defined in a JSON configuration file, loaded at startup. The C++ `CommandDescriptor` struct can be populated from this JSON.

```json
{
  "commands": [
    {
      "name": "list_files",
      "description": "List all files in the current directory",
      "trigger_phrases": ["list files", "show files", "directory listing"],
      "parameters": []
    },
    {
      "name": "create_placemark",
      "description": "Creates a geometry placemark on the map with specified properties",
      "trigger_phrases": [
        "create placemark",
        "generate placemark",
        "add placemark",
        "make placemark"
      ],
      "parameters": [
        {
          "name": "geometry_type",
          "type": "enum",
          "description": "The type of geometry to create",
          "enum_values": ["point", "linestring", "polygon"],
          "required": true
        },
        {
          "name": "vertex_count",
          "type": "integer",
          "description": "Number of vertices for the geometry",
          "required": false,
          "default": "4",
          "min": 2,
          "max": 10000
        },
        {
          "name": "location",
          "type": "string",
          "description": "Geographic location name or area for the placemark",
          "required": true
        },
        {
          "name": "line_style",
          "type": "enum",
          "description": "Visual line rendering style",
          "enum_values": ["solid", "dashed", "dotted"],
          "required": false,
          "default": "solid"
        },
        {
          "name": "color",
          "type": "string",
          "description": "Color name or hex code for the geometry",
          "required": false,
          "default": "red"
        }
      ]
    },
    {
      "name": "zoom_to",
      "description": "Zoom the map view to a specific location",
      "trigger_phrases": ["zoom to", "go to", "navigate to"],
      "parameters": [
        {
          "name": "location",
          "type": "string",
          "description": "Place name to zoom to",
          "required": true
        },
        {
          "name": "zoom_level",
          "type": "integer",
          "description": "Zoom level (1-20, higher is closer)",
          "required": false,
          "default": "12",
          "min": 1,
          "max": 20
        }
      ]
    }
  ]
}
```

### 4.3 Comparison to LLM Function Calling

| Aspect | OpenAI Function Calling | MCP Tools | VoiceCommand Schema |
|--------|------------------------|-----------|---------------------|
| Schema format | JSON Schema | JSON Schema | Simplified JSON (flat params) |
| Nested objects | Yes | Yes | No (flat parameter list) |
| Type system | JSON Schema types | JSON Schema types | string, integer, double, bool, enum |
| Constraints | JSON Schema (min, max, pattern, etc.) | JSON Schema | min, max, enum_values |
| Intent detection | Implicit (LLM decides) | Implicit (LLM decides) | trigger_phrases + NLU |
| Input source | Text | Text | Speech → text |

We intentionally keep the schema simpler than full JSON Schema. Nested objects add complexity to both NLU extraction and parameter access without proportional benefit for voice commands. If a command needs complex structured input, it can accept a flattened set of parameters or parse a single string parameter internally.

### 4.4 Parameter Type Validation Rules

| Type | Validation | Example |
|------|-----------|---------|
| `string` | Non-empty if required | `"New York city"` |
| `integer` | Parseable as int, within min/max | `10` |
| `double` | Parseable as double, within min/max | `3.14` |
| `bool` | One of: true/false/yes/no/1/0 | `true` |
| `enum` | Value is in enum_values list (case-insensitive) | `"dashed"` |

---

## 5. Recognition Pipeline

### 5.1 Audio Engine

Wraps SDL2 for real-time audio capture. Based on whisper.cpp's `audio_async` class.

```cpp
namespace voice_command {

struct AudioEngineConfig {
  int capture_device_id = 0;      // SDL audio device
  int sample_rate = 16000;        // Whisper expects 16kHz
  int vad_window_ms = 1000;       // VAD smoothing window
  float vad_threshold = 0.6f;     // Voice activity threshold
  float freq_threshold = 100.0f;  // High-pass filter cutoff
};

class AudioEngine {
 public:
  explicit AudioEngine(const AudioEngineConfig& config);
  ~AudioEngine();

  bool Init();
  void Start();  // Begin capturing
  void Stop();   // Pause capturing

  // Get the latest audio samples (last n_ms milliseconds).
  // Returns PCM float32 mono at configured sample rate.
  void GetAudio(int duration_ms, std::vector<float>& samples);

  // Clear the audio buffer.
  void ClearBuffer();

  // Voice Activity Detection: returns true if speech detected.
  bool DetectSpeech(const std::vector<float>& samples) const;

 private:
  AudioEngineConfig config_;
  // SDL audio internals...
};

}  // namespace voice_command
```

### 5.2 Whisper ASR Engine

Wraps whisper.cpp for speech-to-text inference. Supports both guided and general transcription modes.

```cpp
namespace voice_command {

struct WhisperConfig {
  std::string model_path;            // Path to ggml model file
  std::string language = "en";       // Language code
  int num_threads = 4;               // CPU threads for inference
  bool use_gpu = true;               // GPU acceleration
  bool translate = false;            // Translate to English
  int max_tokens = 64;               // Max tokens per segment
  int audio_context_size = 0;        // 0 = auto
};

// Result from whisper inference.
struct TranscriptionResult {
  std::string text;                  // Transcribed text
  float confidence;                  // Average token probability
  int64_t duration_ms;               // Processing time
};

// Result from guided-mode scoring.
struct GuidedMatchResult {
  std::string best_match;            // Highest-probability command
  float score;                       // Normalized probability (0-1)
  std::vector<std::pair<float, std::string>> all_scores;  // All candidates
};

class WhisperEngine {
 public:
  explicit WhisperEngine(const WhisperConfig& config);
  ~WhisperEngine();

  bool Init();

  // General transcription: convert audio to text.
  // Used for parameterized commands.
  TranscriptionResult Transcribe(const std::vector<float>& audio_samples);

  // Guided matching: score audio against a list of known phrases.
  // Used for simple keyword commands. Returns probability-ranked matches.
  GuidedMatchResult GuidedMatch(const std::vector<float>& audio_samples,
                                const std::vector<std::string>& allowed_phrases);

 private:
  WhisperConfig config_;
  whisper_context* ctx_ = nullptr;
};

}  // namespace voice_command
```

### 5.3 Recognition Strategy

Abstraction over the two recognition modes.

```cpp
namespace voice_command {

// Output of the recognition pipeline.
struct RecognitionResult {
  std::string command_name;            // Matched command
  CommandContext context;              // Extracted parameters + metadata
  float confidence;                   // Overall confidence
  bool success;                       // Whether recognition succeeded
  std::string error_message;          // If !success
};

// Interface for recognition strategies.
class IRecognitionStrategy {
 public:
  virtual ~IRecognitionStrategy() = default;
  virtual RecognitionResult Recognize(const std::vector<float>& audio_samples) = 0;
};

// Strategy for simple keyword commands. Uses whisper guided mode.
class GuidedRecognitionStrategy : public IRecognitionStrategy {
 public:
  GuidedRecognitionStrategy(WhisperEngine* whisper, CommandRegistry* registry);
  RecognitionResult Recognize(const std::vector<float>& audio_samples) override;

 private:
  WhisperEngine* whisper_;
  CommandRegistry* registry_;
  float min_confidence_ = 0.3f;  // Below this, recognition fails
};

// Strategy for parameterized commands. Uses whisper general mode + NLU.
class NluRecognitionStrategy : public IRecognitionStrategy {
 public:
  NluRecognitionStrategy(WhisperEngine* whisper, INluEngine* nlu,
                         CommandRegistry* registry);
  RecognitionResult Recognize(const std::vector<float>& audio_samples) override;

 private:
  WhisperEngine* whisper_;
  INluEngine* nlu_;
  CommandRegistry* registry_;
};

}  // namespace voice_command
```

### 5.4 Full Pipeline Sequence

```
┌─ Audio Thread ──────────────────────────────────┐
│                                                  │
│  loop (every 100ms):                             │
│    audio_engine.GetAudio(1000ms, samples)        │
│    if audio_engine.DetectSpeech(samples):        │
│      // Speech detected — grab full utterance    │
│      wait_for_silence(samples)                   │
│      push samples to processing queue ──────────>│──┐
│      audio_engine.ClearBuffer()                  │  │
│                                                  │  │
└──────────────────────────────────────────────────┘  │
                                                      │
┌─ Processing Thread ─────────────────────────────────┘
│                                                  │
│  pop samples from queue                          │
│  result = recognition_strategy.Recognize(samples)│
│  if result.success:                              │
│    dispatcher.Dispatch(result.command_name,       │
│                        result.context)            │
│  else:                                           │
│    notify_unrecognized(result.error_message)      │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## 6. NLU Engine

This is the critical component for handling parameterized commands. The NLU engine receives a text transcript and the set of registered command schemas, and returns the identified intent with extracted parameters.

### 6.1 INluEngine Interface

```cpp
namespace voice_command {

// Result from NLU processing.
struct NluResult {
  bool success;
  std::string command_name;           // Identified intent
  float confidence;                   // 0.0 - 1.0
  std::unordered_map<std::string, std::string> extracted_params;
  std::string error_message;          // If !success
};

// Interface for NLU engines.
class INluEngine {
 public:
  virtual ~INluEngine() = default;

  // Initialize the engine (load models, etc.).
  virtual bool Init() = 0;

  // Process a transcript against registered command schemas.
  // Returns the best matching command and extracted parameters.
  virtual NluResult Process(
      const std::string& transcript,
      const std::vector<const CommandDescriptor*>& schemas) = 0;
};

}  // namespace voice_command
```

### 6.2 LlmNluEngine (Default — llama.cpp)

Uses a local language model via llama.cpp to perform intent classification and parameter extraction. This is the same ggml ecosystem as whisper.cpp, keeping the dependency tree coherent.

```cpp
namespace voice_command {

struct LlmNluConfig {
  std::string model_path;          // Path to GGUF model file
  int num_threads = 4;
  int max_output_tokens = 256;
  float temperature = 0.0f;        // Deterministic output
  int context_size = 2048;
  bool use_gpu = true;
};

class LlmNluEngine : public INluEngine {
 public:
  explicit LlmNluEngine(const LlmNluConfig& config);
  ~LlmNluEngine() override;

  bool Init() override;
  NluResult Process(const std::string& transcript,
                    const std::vector<const CommandDescriptor*>& schemas) override;

 private:
  // Construct the prompt with transcript + schemas.
  std::string BuildPrompt(const std::string& transcript,
                          const std::vector<const CommandDescriptor*>& schemas) const;

  // Parse the LLM's JSON response into NluResult.
  NluResult ParseResponse(const std::string& llm_output) const;

  LlmNluConfig config_;
  llama_model* model_ = nullptr;
  llama_context* ctx_ = nullptr;
};

}  // namespace voice_command
```

#### Prompt Construction

The prompt sent to the local LLM is the key to accurate parameter extraction. It follows the established pattern from LLM function calling:

```
You are a command parser. Given a spoken command transcript and a list of
available commands with their parameter schemas, identify which command
was intended and extract the parameter values.

Available commands:

Command: create_placemark
Description: Creates a geometry placemark on the map with specified properties
Parameters:
  - geometry_type (enum: point, linestring, polygon) [REQUIRED]: The type of geometry to create
  - vertex_count (integer, min=2, max=10000) [optional, default=4]: Number of vertices for the geometry
  - location (string) [REQUIRED]: Geographic location name or area for the placemark
  - line_style (enum: solid, dashed, dotted) [optional, default=solid]: Visual line rendering style
  - color (string) [optional, default=red]: Color name or hex code for the geometry

Command: zoom_to
Description: Zoom the map view to a specific location
Parameters:
  - location (string) [REQUIRED]: Place name to zoom to
  - zoom_level (integer, min=1, max=20) [optional, default=12]: Zoom level (1-20, higher is closer)

Command: list_files
Description: List all files in the current directory
Parameters: none

---

Transcript: "generate a linestring placemark with 10 vertices around New York city with dashed style and yellow color"

Respond ONLY with a JSON object in this exact format:
{"command": "<command_name>", "params": {"<param_name>": "<value>", ...}}
```

Expected LLM output:
```json
{"command": "create_placemark", "params": {"geometry_type": "linestring", "vertex_count": "10", "location": "New York city", "line_style": "dashed", "color": "yellow"}}
```

#### Why This Works

The LLM-based approach solves several hard problems simultaneously:

1. **Intent classification from synonyms**: User says "generate" but the command is "create_placemark". The LLM understands the semantic equivalence using the command description and trigger phrases.

2. **Parameter extraction from varied phrasing**: "with 10 vertices", "10 points", "having ten vertices" — the LLM handles all variations because it understands the parameter description "Number of vertices."

3. **Parameter disambiguation**: "dashed style" maps to `line_style`, not some other parameter, because the LLM reads the parameter descriptions.

4. **Graceful handling of missing parameters**: If the user says "create a point placemark at London", the LLM correctly extracts only `geometry_type=point` and `location=London`, leaving optional params to their defaults.

#### Recommended Models

For the local LLM, small instruction-tuned models work well for this structured extraction task:

| Model | Size | Quality | Latency (CPU) |
|-------|------|---------|---------------|
| Phi-3 Mini 4K Instruct | ~2.4 GB (Q4) | Good | ~1-2s |
| Qwen2.5 1.5B Instruct | ~1.0 GB (Q4) | Adequate | ~0.5-1s |
| Llama 3.2 3B Instruct | ~2.0 GB (Q4) | Good | ~1-2s |
| Gemma 2 2B Instruct | ~1.5 GB (Q4) | Good | ~0.8-1.5s |

The task is relatively simple (structured extraction, not creative generation), so even very small models perform well. Use `temperature=0.0` for deterministic output.

#### GBNF Grammar Constraint

To guarantee valid JSON output from the local LLM, use llama.cpp's GBNF grammar support to constrain generation:

```bnf
root   ::= "{" ws "\"command\"" ws ":" ws string "," ws "\"params\"" ws ":" ws object "}"
object ::= "{" ws (pair ("," ws pair)*)? "}"
pair   ::= string ws ":" ws value
value  ::= string | number | "true" | "false" | "null"
string ::= "\"" ([^"\\] | "\\" .)* "\""
number ::= "-"? [0-9]+ ("." [0-9]+)?
ws     ::= [ \t\n]*
```

This forces the LLM to always produce parseable JSON in the exact format expected, eliminating the need for fuzzy parsing or error recovery.

### 6.3 RuleBasedNluEngine (Lightweight Alternative)

For applications that cannot afford LLM overhead or have simple parameterized commands, a rule-based engine uses pattern matching.

```cpp
namespace voice_command {

class RuleBasedNluEngine : public INluEngine {
 public:
  bool Init() override;
  NluResult Process(const std::string& transcript,
                    const std::vector<const CommandDescriptor*>& schemas) override;

 private:
  // Match trigger phrases using edit distance / fuzzy matching.
  std::pair<std::string, float> MatchIntent(
      const std::string& transcript,
      const std::vector<const CommandDescriptor*>& schemas) const;

  // Extract parameter values using keyword proximity heuristics.
  // For each parameter, look for its enum values or type-specific
  // patterns (numbers for int/double, etc.) near relevant keywords.
  std::unordered_map<std::string, std::string> ExtractParams(
      const std::string& transcript,
      const CommandDescriptor& descriptor) const;
};

}  // namespace voice_command
```

The rule-based engine works by:
1. **Intent matching**: Compute string similarity (Levenshtein distance or similar) between the transcript and all trigger phrases. Pick the best match above a threshold.
2. **Parameter extraction**:
   - For `enum` params: scan the transcript for any of the enum values (case-insensitive).
   - For `integer`/`double` params: find numeric tokens and map them to the nearest parameter by keyword proximity (e.g., "10 vertices" — "10" is near "vertices" which is close to the `vertex_count` description).
   - For `string` params: use preposition-based extraction (e.g., "around [LOCATION]", "near [LOCATION]", "at [LOCATION]").

This engine handles moderately complex commands but breaks down with ambiguous or unusual phrasing. Use the LLM engine for production-quality extraction.

### 6.4 Comparison of NLU Engines

| Aspect | LlmNluEngine | RuleBasedNluEngine |
|--------|-------------|-------------------|
| Accuracy | High — handles paraphrasing, synonyms, word order variation | Moderate — requires close-to-template phrasing |
| Latency | 0.5-2s (depends on model and hardware) | <10ms |
| Memory | 1-3 GB (model dependent) | Negligible |
| Dependencies | llama.cpp + GGUF model file | None beyond standard lib |
| Offline | Yes | Yes |
| Best for | Complex parameterized commands | Simple parameterized commands or resource-constrained |

---

## 7. VoiceAssistant — Top-Level Orchestrator

Ties all components together. This is the primary entry point for library users.

```cpp
namespace voice_command {

struct VoiceAssistantConfig {
  AudioEngineConfig audio;
  WhisperConfig whisper;

  // NLU engine type. If "llm", uses LlmNluEngine.
  // If "rule_based", uses RuleBasedNluEngine.
  // If "custom", the user must call SetNluEngine() before Start().
  std::string nlu_engine_type = "llm";

  // LLM NLU config (only used if nlu_engine_type == "llm").
  LlmNluConfig llm_nlu;

  // Path to commands JSON config file (optional).
  // If provided, command descriptors are loaded from this file.
  // The ICommand implementations must still be registered in C++.
  std::string commands_config_path;

  // Minimum confidence for a recognition to be dispatched.
  float min_confidence = 0.3f;

  // Callback for unrecognized speech.
  std::function<void(const std::string& transcript)> on_unrecognized;

  // Callback for errors.
  std::function<void(const std::string& error)> on_error;
};

class VoiceAssistant {
 public:
  explicit VoiceAssistant(const VoiceAssistantConfig& config);
  ~VoiceAssistant();

  // Initialize all subsystems. Must be called before Start().
  bool Init();

  // Access the registry to register commands.
  CommandRegistry& GetRegistry();

  // Set a custom NLU engine (for nlu_engine_type == "custom").
  void SetNluEngine(std::unique_ptr<INluEngine> engine);

  // Start listening and processing voice commands.
  // This spawns audio and processing threads.
  // Non-blocking — returns immediately.
  void Start();

  // Stop listening. Blocks until threads are joined.
  void Stop();

  // Check if the assistant is currently running.
  bool IsRunning() const;

 private:
  void AudioLoop();       // Runs on audio thread
  void ProcessingLoop();  // Runs on processing thread

  VoiceAssistantConfig config_;
  std::unique_ptr<AudioEngine> audio_engine_;
  std::unique_ptr<WhisperEngine> whisper_engine_;
  std::unique_ptr<INluEngine> nlu_engine_;
  CommandRegistry registry_;
  CommandDispatcher dispatcher_;

  std::thread audio_thread_;
  std::thread processing_thread_;
  std::atomic<bool> running_{false};

  // Thread-safe queue for audio segments ready to process.
  // Audio thread pushes, processing thread pops.
  struct AudioSegment {
    std::vector<float> samples;
    int64_t timestamp_ms;
  };
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<AudioSegment> audio_queue_;
};

}  // namespace voice_command
```

---

## 8. Configuration Format

The full JSON configuration file that users provide:

```json
{
  "audio": {
    "capture_device_id": 0,
    "sample_rate": 16000,
    "vad_threshold": 0.6,
    "freq_threshold": 100.0
  },
  "whisper": {
    "model_path": "./models/ggml-base.en.bin",
    "language": "en",
    "num_threads": 4,
    "use_gpu": true,
    "max_tokens": 64
  },
  "nlu": {
    "engine": "llm",
    "llm": {
      "model_path": "./models/phi-3-mini-4k-instruct-q4.gguf",
      "num_threads": 4,
      "max_output_tokens": 256,
      "temperature": 0.0,
      "context_size": 2048,
      "use_gpu": true
    }
  },
  "recognition": {
    "min_confidence": 0.3
  },
  "commands": [
    {
      "name": "create_placemark",
      "description": "Creates a geometry placemark on the map",
      "trigger_phrases": ["create placemark", "generate placemark", "add placemark"],
      "parameters": [
        {
          "name": "geometry_type",
          "type": "enum",
          "description": "The type of geometry to create",
          "enum_values": ["point", "linestring", "polygon"],
          "required": true
        },
        {
          "name": "vertex_count",
          "type": "integer",
          "description": "Number of vertices",
          "min": 2,
          "max": 10000,
          "required": false,
          "default": "4"
        },
        {
          "name": "location",
          "type": "string",
          "description": "Geographic location name",
          "required": true
        },
        {
          "name": "line_style",
          "type": "enum",
          "description": "Line rendering style",
          "enum_values": ["solid", "dashed", "dotted"],
          "required": false,
          "default": "solid"
        },
        {
          "name": "color",
          "type": "string",
          "description": "Color name or hex code",
          "required": false,
          "default": "red"
        }
      ]
    },
    {
      "name": "zoom_to",
      "description": "Zoom the map to a location",
      "trigger_phrases": ["zoom to", "go to", "navigate to"],
      "parameters": [
        {
          "name": "location",
          "type": "string",
          "description": "Place name to zoom to",
          "required": true
        }
      ]
    },
    {
      "name": "list_files",
      "description": "List files in the current directory",
      "trigger_phrases": ["list files", "show files"],
      "parameters": []
    }
  ]
}
```

The configuration is parsed at startup by `VoiceAssistant::Init()` using nlohmann/json. Command descriptors are loaded from the JSON. The `ICommand` implementations are registered separately in C++ code via `CommandRegistry::Register()`.

---

## 9. Thread Model and Concurrency

```
┌────────────────────────────┐
│       Main Thread          │
│                            │
│  VoiceAssistant::Init()    │
│  registry.Register(...)    │
│  VoiceAssistant::Start()   │
│  ...                       │
│  VoiceAssistant::Stop()    │
└────────────────────────────┘
         │
         │ spawns
         ▼
┌────────────────────────────┐     ┌────────────────────────────────┐
│     Audio Thread           │     │     Processing Thread          │
│                            │     │                                │
│ loop:                      │     │ loop:                          │
│   poll audio (100ms)       │     │   wait on condition_variable   │
│   if speech detected:      │     │   pop AudioSegment from queue  │
│     capture full utterance │     │   recognition_strategy         │
│     push to queue ─────────│────>│     .Recognize(samples)        │
│     signal cv              │     │   if success:                  │
│     clear buffer           │     │     dispatcher.Dispatch(...)   │
│                            │     │                                │
└────────────────────────────┘     └────────────────────────────────┘
```

### Synchronization Points

1. **Audio queue**: `std::deque<AudioSegment>` protected by `std::mutex` + `std::condition_variable`. Audio thread pushes, processing thread pops. Single-producer, single-consumer.

2. **CommandRegistry**: Protected by `std::shared_mutex`. Reads (during recognition) take shared locks. Writes (register/unregister) take exclusive locks. Commands are typically registered at startup before `Start()`, so contention is minimal.

3. **ICommand::Execute()**: Called on the processing thread. If the command implementation needs to interact with the application's main thread (e.g., UI updates), it is the command author's responsibility to post work to the appropriate thread. The library does not provide a UI thread dispatcher — this is application-specific.

4. **Shutdown**: `VoiceAssistant::Stop()` sets `running_ = false` (atomic), signals the condition variable, and joins both threads.

---

## 10. Error Handling Strategy

### Recognition Failures

| Scenario | Behavior |
|----------|----------|
| No speech detected (silence) | Continue polling. No action. |
| Speech detected but whisper returns empty/garbage | Discard. Log at debug level. |
| Guided mode: no command scores above `min_confidence` | Call `on_unrecognized` callback with transcript. |
| NLU mode: LLM returns unparseable JSON | Retry once with stricter prompt. If still fails, call `on_unrecognized`. |
| NLU mode: LLM returns unknown command name | Call `on_unrecognized`. |
| NLU mode: LLM returns valid command but missing required params | Call `on_error` with details. Do not dispatch. |

### Parameter Validation Failures

The `CommandDispatcher::ValidateAndFillDefaults()` method handles:
- **Missing required parameter**: Return `kInvalidParams`. Do not call `Execute()`.
- **Type mismatch** (e.g., "abc" for integer param): Return `kInvalidParams`.
- **Out of range** (e.g., `vertex_count=0` when `min=2`): Return `kInvalidParams`.
- **Invalid enum value**: Return `kInvalidParams`.
- **Missing optional parameter**: Fill with `default_value` from schema. If no default, leave absent.

### Engine Initialization Failures

- Whisper model file not found → `Init()` returns false with logged error.
- LLM model file not found → `Init()` returns false with logged error.
- SDL audio device unavailable → `Init()` returns false with logged error.

All errors are reported through return values (no exceptions). Logging uses a configurable logger interface (default: stderr).

---

## 11. Integration Guide

### Step 1: Implement Your Command

```cpp
#include "voice_command/icommand.h"

class CreatePlacemarkCommand : public voice_command::ICommand {
 public:
  explicit CreatePlacemarkCommand(MapApplication* app) : app_(app) {}

  voice_command::CommandResult Execute(
      const voice_command::CommandContext& ctx) override {
    // Extract parameters (with defaults already filled by dispatcher).
    std::string geom_type = ctx.GetParam("geometry_type").AsString();
    int vertex_count = ctx.GetParam("vertex_count").AsInt();
    std::string location = ctx.GetParam("location").AsString();
    std::string line_style = ctx.GetParam("line_style").AsString();
    std::string color = ctx.GetParam("color").AsString();

    // Call your application logic.
    app_->CreatePlacemark(geom_type, vertex_count, location,
                          line_style, color);

    return voice_command::CommandResult::kSuccess;
  }

  std::string GetName() const override { return "create_placemark"; }

 private:
  MapApplication* app_;
};
```

### Step 2: Configure and Register

```cpp
#include "voice_command/voice_assistant.h"

int main() {
  // Load config from JSON file.
  voice_command::VoiceAssistantConfig config;
  config.commands_config_path = "commands.json";
  config.whisper.model_path = "./models/ggml-base.en.bin";
  config.llm_nlu.model_path = "./models/phi-3-mini-q4.gguf";

  config.on_unrecognized = [](const std::string& transcript) {
    std::cerr << "Unrecognized: " << transcript << std::endl;
  };

  voice_command::VoiceAssistant assistant(config);
  if (!assistant.Init()) {
    std::cerr << "Failed to initialize voice assistant" << std::endl;
    return 1;
  }

  // Register command implementations.
  // The descriptor is already loaded from commands.json — just bind the impl.
  MapApplication app;
  assistant.GetRegistry().Register(
      *assistant.GetRegistry().FindDescriptor("create_placemark"),
      std::make_unique<CreatePlacemarkCommand>(&app));

  // Or register fully in code (no JSON needed):
  voice_command::CommandDescriptor desc;
  desc.name = "list_files";
  desc.description = "List files in current directory";
  desc.trigger_phrases = {"list files", "show files"};
  // No parameters — simple command.
  assistant.GetRegistry().Register(desc,
                                   std::make_unique<ListFilesCommand>());

  // Start listening.
  assistant.Start();

  std::cout << "Voice assistant running. Press Ctrl+C to stop." << std::endl;
  // Wait for signal...
  signal_wait();

  assistant.Stop();
  return 0;
}
```

### Step 3: Test

1. Run the application.
2. Say "list files" → should call `ListFilesCommand::Execute()`.
3. Say "create a dashed yellow linestring placemark with 10 vertices around New York" → should call `CreatePlacemarkCommand::Execute()` with all parameters extracted.

---

## 12. Build System

### CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.16)
project(voice_command VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ──────────────────────────────────────────────

# whisper.cpp (speech-to-text)
add_subdirectory(third_party/whisper.cpp)

# llama.cpp (local LLM for NLU) — optional
option(VOICECMD_ENABLE_LLM_NLU "Enable LLM-based NLU engine (requires llama.cpp)" ON)
if(VOICECMD_ENABLE_LLM_NLU)
  add_subdirectory(third_party/llama.cpp)
endif()

# nlohmann/json (JSON parsing)
find_package(nlohmann_json 3.11 REQUIRED)

# SDL2 (audio capture)
find_package(SDL2 REQUIRED)

# ── Library ───────────────────────────────────────────────────

set(VOICECMD_SOURCES
  src/audio_engine.cpp
  src/whisper_engine.cpp
  src/command_context.cpp
  src/command_registry.cpp
  src/command_dispatcher.cpp
  src/guided_recognition_strategy.cpp
  src/nlu_recognition_strategy.cpp
  src/rule_based_nlu_engine.cpp
  src/voice_assistant.cpp
  src/config_loader.cpp
)

if(VOICECMD_ENABLE_LLM_NLU)
  list(APPEND VOICECMD_SOURCES src/llm_nlu_engine.cpp)
endif()

add_library(voice_command ${VOICECMD_SOURCES})

target_include_directories(voice_command
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(voice_command
  PUBLIC
    whisper
    nlohmann_json::nlohmann_json
    SDL2::SDL2
    Threads::Threads
)

if(VOICECMD_ENABLE_LLM_NLU)
  target_link_libraries(voice_command PUBLIC llama)
  target_compile_definitions(voice_command PUBLIC VOICECMD_HAS_LLM_NLU)
endif()

# ── Example ───────────────────────────────────────────────────

option(VOICECMD_BUILD_EXAMPLES "Build example applications" ON)
if(VOICECMD_BUILD_EXAMPLES)
  add_executable(voice_command_example examples/main.cpp)
  target_link_libraries(voice_command_example PRIVATE voice_command)
endif()
```

### Dependencies Summary

| Dependency | Purpose | Required |
|-----------|---------|----------|
| whisper.cpp | Speech-to-text (ASR) | Yes |
| SDL2 | Real-time audio capture | Yes |
| nlohmann/json | JSON config parsing | Yes |
| llama.cpp | Local LLM for NLU parameter extraction | Optional (for LlmNluEngine) |
| pthreads | Threading | Yes (system) |

---

## 13. Project Structure

```
commands/
├── CMakeLists.txt
├── include/
│   └── voice_command/
│       ├── audio_engine.h
│       ├── command_context.h
│       ├── command_descriptor.h
│       ├── command_dispatcher.h
│       ├── command_registry.h
│       ├── icommand.h
│       ├── inlu_engine.h
│       ├── llm_nlu_engine.h          # ifdef VOICECMD_HAS_LLM_NLU
│       ├── recognition_strategy.h
│       ├── rule_based_nlu_engine.h
│       ├── voice_assistant.h
│       └── whisper_engine.h
├── src/
│   ├── audio_engine.cpp
│   ├── command_context.cpp
│   ├── command_dispatcher.cpp
│   ├── command_registry.cpp
│   ├── config_loader.h              # Internal
│   ├── config_loader.cpp
│   ├── guided_recognition_strategy.cpp
│   ├── llm_nlu_engine.cpp
│   ├── nlu_recognition_strategy.cpp
│   ├── rule_based_nlu_engine.cpp
│   ├── voice_assistant.cpp
│   └── whisper_engine.cpp
├── examples/
│   ├── main.cpp                     # Basic usage example
│   └── map_app_example.cpp          # Raster map placemark example
├── config/
│   └── commands.json                # Example config
├── models/                          # .gitignored, user provides models
│   ├── ggml-base.en.bin
│   └── phi-3-mini-q4.gguf
└── third_party/
    ├── whisper.cpp/                  # Git submodule
    └── llama.cpp/                   # Git submodule
```

---

## 14. Example: Raster Map Application

Complete worked example showing how a map application developer would integrate VoiceCommand.

### commands.json

```json
{
  "audio": {
    "capture_device_id": 0,
    "vad_threshold": 0.6
  },
  "whisper": {
    "model_path": "./models/ggml-base.en.bin",
    "language": "en",
    "num_threads": 4
  },
  "nlu": {
    "engine": "llm",
    "llm": {
      "model_path": "./models/phi-3-mini-q4.gguf",
      "num_threads": 4,
      "temperature": 0.0
    }
  },
  "commands": [
    {
      "name": "create_placemark",
      "description": "Creates a geometry placemark on the map with specified visual properties",
      "trigger_phrases": ["create placemark", "generate placemark", "add placemark", "make placemark", "draw placemark"],
      "parameters": [
        {
          "name": "geometry_type",
          "type": "enum",
          "description": "The type of geometry to create",
          "enum_values": ["point", "linestring", "polygon"],
          "required": true
        },
        {
          "name": "vertex_count",
          "type": "integer",
          "description": "Number of vertices for the geometry",
          "min": 2,
          "max": 10000,
          "required": false,
          "default": "4"
        },
        {
          "name": "location",
          "type": "string",
          "description": "Geographic location name or area (e.g., 'New York city', 'downtown London')",
          "required": true
        },
        {
          "name": "line_style",
          "type": "enum",
          "description": "Visual line rendering style",
          "enum_values": ["solid", "dashed", "dotted"],
          "required": false,
          "default": "solid"
        },
        {
          "name": "color",
          "type": "string",
          "description": "Color name (e.g., 'red', 'yellow', 'blue') or hex code",
          "required": false,
          "default": "red"
        }
      ]
    },
    {
      "name": "delete_placemark",
      "description": "Delete the currently selected placemark from the map",
      "trigger_phrases": ["delete placemark", "remove placemark", "erase placemark"],
      "parameters": []
    },
    {
      "name": "zoom_to",
      "description": "Zoom and pan the map to center on a location",
      "trigger_phrases": ["zoom to", "go to", "navigate to", "show me"],
      "parameters": [
        {
          "name": "location",
          "type": "string",
          "description": "Place name to zoom to",
          "required": true
        },
        {
          "name": "zoom_level",
          "type": "integer",
          "description": "Zoom level from 1 (world) to 20 (building level)",
          "min": 1,
          "max": 20,
          "required": false,
          "default": "12"
        }
      ]
    },
    {
      "name": "toggle_layer",
      "description": "Show or hide a map layer",
      "trigger_phrases": ["show layer", "hide layer", "toggle layer"],
      "parameters": [
        {
          "name": "layer_name",
          "type": "string",
          "description": "Name of the map layer (e.g., 'satellite', 'terrain', 'labels')",
          "required": true
        },
        {
          "name": "visible",
          "type": "bool",
          "description": "Whether the layer should be visible",
          "required": false,
          "default": "true"
        }
      ]
    }
  ]
}
```

### map_commands.h

```cpp
#pragma once
#include "voice_command/icommand.h"
#include "map_application.h"  // The developer's own map app

class CreatePlacemarkCommand : public voice_command::ICommand {
 public:
  explicit CreatePlacemarkCommand(MapApplication* app) : app_(app) {}

  voice_command::CommandResult Execute(
      const voice_command::CommandContext& ctx) override {
    std::string geom_type = ctx.GetParam("geometry_type").AsString();
    int vertices = ctx.GetParam("vertex_count").AsInt();
    std::string location = ctx.GetParam("location").AsString();
    std::string style = ctx.GetParam("line_style").AsString();
    std::string color = ctx.GetParam("color").AsString();

    // Call the map application's own API.
    auto coords = app_->Geocode(location);
    auto geometry = app_->GenerateGeometry(geom_type, vertices, coords);
    auto placemark = app_->CreatePlacemark(geometry);
    placemark->SetLineStyle(style);
    placemark->SetColor(color);
    app_->AddToMap(placemark);

    std::cout << "Created " << geom_type << " with " << vertices
              << " vertices at " << location << std::endl;
    return voice_command::CommandResult::kSuccess;
  }

  std::string GetName() const override { return "create_placemark"; }

 private:
  MapApplication* app_;
};

class DeletePlacemarkCommand : public voice_command::ICommand {
 public:
  explicit DeletePlacemarkCommand(MapApplication* app) : app_(app) {}

  voice_command::CommandResult Execute(
      const voice_command::CommandContext& ctx) override {
    auto selected = app_->GetSelectedPlacemark();
    if (!selected) {
      std::cerr << "No placemark selected" << std::endl;
      return voice_command::CommandResult::kFailure;
    }
    app_->RemoveFromMap(selected);
    return voice_command::CommandResult::kSuccess;
  }

  std::string GetName() const override { return "delete_placemark"; }

 private:
  MapApplication* app_;
};

class ZoomToCommand : public voice_command::ICommand {
 public:
  explicit ZoomToCommand(MapApplication* app) : app_(app) {}

  voice_command::CommandResult Execute(
      const voice_command::CommandContext& ctx) override {
    std::string location = ctx.GetParam("location").AsString();
    int zoom = ctx.GetParam("zoom_level").AsInt();

    auto coords = app_->Geocode(location);
    app_->SetViewCenter(coords);
    app_->SetZoomLevel(zoom);

    return voice_command::CommandResult::kSuccess;
  }

  std::string GetName() const override { return "zoom_to"; }

 private:
  MapApplication* app_;
};
```

### main.cpp

```cpp
#include "voice_command/voice_assistant.h"
#include "map_commands.h"
#include "map_application.h"

#include <csignal>
#include <iostream>

static std::atomic<bool> g_running{true};

void SignalHandler(int) { g_running = false; }

int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);

  // Configure the voice assistant.
  voice_command::VoiceAssistantConfig config;
  config.commands_config_path = "commands.json";

  config.on_unrecognized = [](const std::string& transcript) {
    std::cout << "[?] Did not understand: " << transcript << std::endl;
  };

  config.on_error = [](const std::string& error) {
    std::cerr << "[!] Error: " << error << std::endl;
  };

  // Initialize the voice assistant.
  voice_command::VoiceAssistant assistant(config);
  if (!assistant.Init()) {
    std::cerr << "Failed to initialize voice assistant" << std::endl;
    return 1;
  }

  // Create the map application.
  MapApplication app;
  app.Init();

  // Register command implementations.
  auto& registry = assistant.GetRegistry();
  registry.Register(*registry.FindDescriptor("create_placemark"),
                     std::make_unique<CreatePlacemarkCommand>(&app));
  registry.Register(*registry.FindDescriptor("delete_placemark"),
                     std::make_unique<DeletePlacemarkCommand>(&app));
  registry.Register(*registry.FindDescriptor("zoom_to"),
                     std::make_unique<ZoomToCommand>(&app));

  // Start listening.
  assistant.Start();
  std::cout << "Map voice assistant running. Say a command..." << std::endl;

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  assistant.Stop();
  app.Shutdown();
  return 0;
}
```

### Voice Interaction Examples

| User says | Recognized intent | Extracted parameters |
|-----------|-------------------|---------------------|
| "create a linestring placemark with 10 vertices around New York city with dashed style and yellow color" | `create_placemark` | geometry_type=linestring, vertex_count=10, location=New York city, line_style=dashed, color=yellow |
| "add a point placemark at London" | `create_placemark` | geometry_type=point, location=London, vertex_count=4 (default), line_style=solid (default), color=red (default) |
| "generate a polygon with 6 vertices near Tokyo" | `create_placemark` | geometry_type=polygon, vertex_count=6, location=Tokyo, line_style=solid (default), color=red (default) |
| "delete placemark" | `delete_placemark` | (none) |
| "zoom to Paris" | `zoom_to` | location=Paris, zoom_level=12 (default) |
| "navigate to Berlin at zoom level 15" | `zoom_to` | location=Berlin, zoom_level=15 |

---

## 15. Future Considerations

These are out of scope for the initial implementation but should inform architectural decisions:

- **Multi-language support**: Whisper supports 100+ languages. The NLU prompt can be adapted per language. Command schemas may need localized trigger phrases.
- **Wake word detection**: A lightweight always-on detector (e.g., "Hey Map") that activates the full pipeline only when addressed. Avoids continuous whisper inference.
- **Streaming recognition**: Process audio incrementally rather than waiting for silence. Requires whisper's streaming API.
- **Confirmation dialogs**: For destructive commands ("delete all placemarks"), the library could support a confirmation flow: recognize → ask user to confirm → execute.
- **Multi-turn context**: "Create a placemark at New York" → "Make it blue" (refers to the last created placemark). Requires conversation state management.
- **Custom parameter types**: Allow developers to define custom types (e.g., coordinate pairs, date ranges) with custom validation and extraction logic.
- **Telemetry and analytics**: Track recognition accuracy, latency distributions, most-used commands for tuning.

---

## 16. Implementation Plan

Ordered phases for building the library:

### Phase 1: Foundation
1. Set up project structure (directories, CMakeLists.txt, git submodules for whisper.cpp and llama.cpp).
2. Implement core types: `ParamValue`, `CommandContext`, `ICommand`, `CommandDescriptor`, `CommandResult`.
3. Implement `CommandRegistry` with thread-safe registration and lookup.
4. Implement `CommandDispatcher` with parameter validation and default filling.
5. Write unit tests for registry and dispatcher.

### Phase 2: Audio and ASR
6. Implement `AudioEngine` wrapping SDL2 audio capture and VAD.
7. Implement `WhisperEngine` wrapping whisper.cpp for both guided and general transcription.
8. Implement `GuidedRecognitionStrategy` for simple keyword commands.
9. Integration test: microphone → whisper → guided match → dispatch → execute.

### Phase 3: NLU
10. Implement `INluEngine` interface.
11. Implement `RuleBasedNluEngine` (regex/pattern matching).
12. Implement `LlmNluEngine` wrapping llama.cpp with prompt construction and JSON response parsing.
13. Implement `NluRecognitionStrategy` connecting whisper transcription to NLU.
14. Integration test: microphone → whisper → LLM NLU → extract params → dispatch → execute.

### Phase 4: Orchestration
15. Implement JSON config loader (`config_loader.cpp`).
16. Implement `VoiceAssistant` orchestrator with thread management.
17. Build the map application example.
18. End-to-end testing with both simple and parameterized commands.

### Phase 5: Polish
19. Error handling refinement and edge cases.
20. Performance profiling and optimization (latency, memory).
21. Documentation and API reference.
