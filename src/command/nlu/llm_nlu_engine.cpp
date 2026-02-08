#include "command/nlu/llm_nlu_engine.h"

namespace voice_command {

LlmNluEngine::LlmNluEngine(const LlmNluConfig& config)
    : config_(config) {}

LlmNluEngine::~LlmNluEngine() = default;

bool LlmNluEngine::Init() {
    // Placeholder: LLM initialization not yet implemented
    // Future: Load llama.cpp model from config_.model_path
    return false;
}

NluResult LlmNluEngine::Process(
    const std::string& /*transcript*/,
    const std::vector<const CommandDescriptor*>& /*schemas*/) {
    NluResult result;
    result.success = false;
    result.error_message = "LLM NLU engine not yet implemented. "
                           "Use RuleBasedNluEngine instead.";
    return result;
}

std::string LlmNluEngine::BuildPrompt(
    const std::string& /*transcript*/,
    const std::vector<const CommandDescriptor*>& /*schemas*/) const {
    // Placeholder: Will construct prompt as described in design.md
    return "";
}

NluResult LlmNluEngine::ParseResponse(const std::string& /*llm_output*/) const {
    // Placeholder: Will parse JSON response from LLM
    NluResult result;
    result.success = false;
    return result;
}

}  // namespace voice_command
