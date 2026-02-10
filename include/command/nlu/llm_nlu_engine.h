#ifndef LLM_NLU_ENGINE_H
#define LLM_NLU_ENGINE_H

#include "command/nlu/inlu_engine.h"

#include <string>

namespace voice_command {

// Configuration for LLM-based NLU engine.
struct LlmNluConfig {
    std::string model_path;          // Path to GGUF model file
    int num_threads = 4;
    int max_output_tokens = 256;
    float temperature = 0.0f;        // Deterministic output
    int context_size = 2048;
    bool use_gpu = true;
};

// LLM-based NLU engine using llama.cpp.
// Uses a local language model to perform intent classification
// and parameter extraction.
//
// NOTE: This is currently a placeholder. Full implementation
// requires llama.cpp integration.
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
    // Future: llama.cpp integration
    // llama_model* model_ = nullptr;
    // llama_context* ctx_ = nullptr;
};

}  // namespace voice_command

#endif // LLM_NLU_ENGINE_H
