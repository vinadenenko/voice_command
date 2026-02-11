#ifndef VOICE_COMMAND_TESTING_COMMAND_TESTER_H_
#define VOICE_COMMAND_TESTING_COMMAND_TESTER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "command/command_result.h"
#include "command/context/commandcontext.h"
#include "command/descriptor/commanddescriptor.h"
#include "command/dispatcher/commanddispatcher.h"
#include "command/nlu/inlu_engine.h"
#include "command/registry/commandregistry.h"

namespace voice_command::testing {

/**
 * @brief Result of processing a test transcript through the command pipeline.
 *
 * Contains all information about command recognition and execution,
 * useful for assertions in unit tests.
 */
struct TestResult {
    bool recognized = false;           ///< Whether a command was recognized
    std::string command_name;          ///< Name of the matched command (empty if not recognized)
    float confidence = 0.0f;           ///< NLU confidence score (0.0 - 1.0)
    std::unordered_map<std::string, std::string> params;  ///< Extracted parameters
    CommandResult execution_result = CommandResult::kNotHandled;  ///< Result of command execution
    std::string raw_transcript;        ///< The input transcript
    std::string error;                 ///< Error message if recognition or execution failed
};

/**
 * @brief Utility class for testing voice commands without audio input.
 *
 * CommandTester allows testing the entire command recognition and execution
 * pipeline by providing text strings directly, bypassing the Whisper speech
 * recognition. This is useful for:
 *
 * - Unit testing custom commands
 * - Verifying parameter extraction logic
 * - Testing edge cases without speaking
 * - Regression testing command schemas
 *
 * ## Usage Example
 *
 * ```cpp
 * #include <testing/command_tester.h>
 * #include "my_commands.h"
 *
 * void TestMyCommands() {
 *     // Create and initialize tester
 *     voice_command::testing::CommandTester tester;
 *     tester.Init();
 *
 *     // Register commands (same API as VoiceAssistant)
 *     auto* registry = tester.GetRegistry();
 *
 *     CommandDescriptor desc;
 *     desc.name = "zoom_to";
 *     desc.trigger_phrases = {"zoom to", "zoom level"};
 *
 *     ParamDescriptor level_param;
 *     level_param.name = "level";
 *     level_param.type = ParamType::kInteger;
 *     level_param.required = true;
 *     desc.parameters.push_back(level_param);
 *
 *     registry->Register(desc, std::make_unique<ZoomToCommand>());
 *
 *     // Test with text input (simulates Whisper transcription)
 *     auto result = tester.ProcessText("zoom to 15");
 *
 *     // Verify results
 *     assert(result.recognized == true);
 *     assert(result.command_name == "zoom_to");
 *     assert(result.params["level"] == "15");
 *     assert(result.execution_result == CommandResult::kSuccess);
 * }
 * ```
 *
 * ## Batch Testing
 *
 * ```cpp
 * auto results = tester.ProcessBatch({
 *     "zoom to 5",
 *     "zoom to 10",
 *     "invalid command",
 *     "zoom to 20"
 * });
 *
 * for (const auto& r : results) {
 *     std::cout << r.raw_transcript << " -> "
 *               << (r.recognized ? r.command_name : "NOT RECOGNIZED")
 *               << std::endl;
 * }
 * ```
 *
 * ## With Google Test
 *
 * ```cpp
 * TEST(MyCommands, ZoomToExtractsLevel) {
 *     CommandTester tester;
 *     tester.Init();
 *     RegisterMyCommands(tester.GetRegistry());
 *
 *     auto result = tester.ProcessText("zoom to level 15");
 *
 *     EXPECT_TRUE(result.recognized);
 *     EXPECT_EQ(result.command_name, "zoom_to");
 *     EXPECT_EQ(result.params["level"], "15");
 *     EXPECT_EQ(result.execution_result, CommandResult::kSuccess);
 * }
 * ```
 */
class CommandTester {
public:
    CommandTester();
    ~CommandTester();

    // Non-copyable, movable
    CommandTester(const CommandTester&) = delete;
    CommandTester& operator=(const CommandTester&) = delete;
    CommandTester(CommandTester&&) noexcept;
    CommandTester& operator=(CommandTester&&) noexcept;

    /**
     * @brief Initialize the tester with an NLU engine.
     *
     * @param nlu_engine NLU engine to use for intent recognition and parameter
     *                   extraction. If nullptr, creates a RuleBasedNluEngine.
     * @return true if initialization succeeded, false otherwise.
     */
    bool Init(std::unique_ptr<INluEngine> nlu_engine = nullptr);

    /**
     * @brief Get the command registry for registering commands.
     *
     * Use this to register your commands before calling ProcessText().
     * Same API as VoiceAssistant::GetRegistry().
     *
     * @return Pointer to the command registry.
     */
    CommandRegistry* GetRegistry();

    /**
     * @brief Process a text transcript through the command pipeline.
     *
     * Simulates what would happen if Whisper transcribed audio to this text:
     * 1. NLU matches transcript to a command and extracts parameters
     * 2. CommandDispatcher validates parameters and calls ICommand::Execute()
     * 3. Results are collected and returned
     *
     * @param transcript The text to process (as if Whisper output it).
     * @return TestResult containing recognition and execution details.
     */
    TestResult ProcessText(const std::string& transcript);

    /**
     * @brief Process multiple transcripts and return all results.
     *
     * Convenience method for batch testing.
     *
     * @param transcripts Vector of text strings to process.
     * @return Vector of TestResult, one per input transcript.
     */
    std::vector<TestResult> ProcessBatch(const std::vector<std::string>& transcripts);

    /**
     * @brief Set minimum confidence threshold for command recognition.
     *
     * Commands with confidence below this threshold will not be recognized.
     * Default is 0.5.
     *
     * @param threshold Confidence threshold (0.0 - 1.0).
     */
    void SetMinConfidence(float threshold);

private:
    std::unique_ptr<CommandRegistry> registry_;
    std::unique_ptr<INluEngine> nlu_engine_;
    std::unique_ptr<CommandDispatcher> dispatcher_;
    float min_confidence_ = 0.5f;
    bool initialized_ = false;
};

}  // namespace voice_command::testing

#endif  // VOICE_COMMAND_TESTING_COMMAND_TESTER_H_
