#include "testing/command_tester.h"

#include "command/nlu/rule_based_nlu_engine.h"

namespace voice_command::testing {

CommandTester::CommandTester() = default;

CommandTester::~CommandTester() = default;

CommandTester::CommandTester(CommandTester&&) noexcept = default;

CommandTester& CommandTester::operator=(CommandTester&&) noexcept = default;

bool CommandTester::Init(std::unique_ptr<INluEngine> nlu_engine) {
    // Create registry
    registry_ = std::make_unique<CommandRegistry>();

    // Create dispatcher
    dispatcher_ = std::make_unique<CommandDispatcher>(registry_.get());

    // Use provided NLU engine or create default RuleBasedNluEngine
    if (nlu_engine) {
        nlu_engine_ = std::move(nlu_engine);
    } else {
        nlu_engine_ = std::make_unique<RuleBasedNluEngine>();
    }

    if (!nlu_engine_->Init()) {
        return false;
    }

    initialized_ = true;
    return true;
}

CommandRegistry* CommandTester::GetRegistry() {
    return registry_.get();
}

TestResult CommandTester::ProcessText(const std::string& transcript) {
    TestResult result;
    result.raw_transcript = transcript;

    if (!initialized_) {
        result.error = "CommandTester not initialized. Call Init() first.";
        return result;
    }

    if (transcript.empty()) {
        result.error = "Empty transcript";
        return result;
    }

    // Get all command descriptors from registry
    auto descriptors = registry_->GetAllDescriptors();
    if (descriptors.empty()) {
        result.error = "No commands registered";
        return result;
    }

    // Run NLU to match intent and extract parameters
    NluResult nlu_result = nlu_engine_->Process(transcript, descriptors);

    if (!nlu_result.success) {
        result.error = nlu_result.error_message;
        return result;
    }

    if (nlu_result.confidence < min_confidence_) {
        result.error = "Confidence below threshold: " +
                       std::to_string(nlu_result.confidence) + " < " +
                       std::to_string(min_confidence_);
        return result;
    }

    // Command was recognized
    result.recognized = true;
    result.command_name = nlu_result.command_name;
    result.confidence = nlu_result.confidence;
    result.params = nlu_result.extracted_params;

    // Build command context
    CommandContext context;
    context.SetRawTranscript(transcript);
    context.SetConfidence(nlu_result.confidence);

    for (const auto& [name, value] : nlu_result.extracted_params) {
        context.SetParam(name, ParamValue(value));
    }

    // Dispatch command for execution
    result.execution_result = dispatcher_->Dispatch(nlu_result.command_name, context);

    return result;
}

std::vector<TestResult> CommandTester::ProcessBatch(
    const std::vector<std::string>& transcripts) {
    std::vector<TestResult> results;
    results.reserve(transcripts.size());

    for (const auto& transcript : transcripts) {
        results.push_back(ProcessText(transcript));
    }

    return results;
}

void CommandTester::SetMinConfidence(float threshold) {
    min_confidence_ = threshold;
}

}  // namespace voice_command::testing
