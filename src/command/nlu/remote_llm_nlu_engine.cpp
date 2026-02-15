#include "command/nlu/remote_llm_nlu_engine.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>

namespace voice_command {

RemoteLlmNluEngine::RemoteLlmNluEngine(const RemoteLlmNluConfig& config)
    : config_(config) {}

RemoteLlmNluEngine::~RemoteLlmNluEngine() = default;

bool RemoteLlmNluEngine::Init() {
    if (initialized_) {
        return false;  // Already initialized
    }

    // Validate required configuration
    if (config_.server_url.empty()) {
        return false;
    }

    if (config_.model.empty()) {
        return false;
    }

    initialized_ = true;
    return true;
}

NluResult RemoteLlmNluEngine::Process(
    const std::string& transcript,
    const std::vector<const CommandDescriptor*>& schemas) {
    NluResult result;

    if (!initialized_) {
        result.success = false;
        result.error_message = "Engine not initialized";
        return result;
    }

    if (transcript.empty()) {
        result.success = false;
        result.error_message = "Empty transcript";
        return result;
    }

    if (schemas.empty()) {
        result.success = false;
        result.error_message = "No command schemas provided";
        return result;
    }

    // Build messages for chat completions API
    std::string system_prompt = BuildSystemPrompt(schemas);

    nlohmann::json request_body;
    request_body["model"] = config_.model;
    request_body["messages"] = nlohmann::json::array({
        {{"role", "system"}, {"content", system_prompt}},
        {{"role", "user"}, {"content", transcript}}
    });
    request_body["temperature"] = config_.temperature;
    request_body["max_tokens"] = config_.max_tokens;

    // Parse server URL
    std::string host = config_.server_url;
    int port = 80;
    bool use_ssl = false;

    // Handle scheme
    if (host.rfind("https://", 0) == 0) {
        host = host.substr(8);
        port = 443;
        use_ssl = true;
    } else if (host.rfind("http://", 0) == 0) {
        host = host.substr(7);
    }

    // Extract port if present
    auto colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        port = std::stoi(host.substr(colon_pos + 1));
        host = host.substr(0, colon_pos);
    }

    // Create HTTP client
    std::unique_ptr<httplib::Client> client;
    if (use_ssl) {
        client = std::make_unique<httplib::Client>(
            (std::string("https://") + host + ":" + std::to_string(port)).c_str());
    } else {
        client = std::make_unique<httplib::Client>(host, port);
    }

    client->set_connection_timeout(config_.timeout_ms / 1000,
                                   (config_.timeout_ms % 1000) * 1000);
    client->set_read_timeout(config_.timeout_ms / 1000,
                             (config_.timeout_ms % 1000) * 1000);

    // Set headers
    httplib::Headers headers;
    headers.emplace("Content-Type", "application/json");
    if (!config_.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config_.api_key);
    }

    // Debug: Print request
    if (config_.enable_debug_logging) {
        std::cout << "========= LLM Request Start =========" << std::endl;
        std::cout << "[RemoteLLM] Request JSON:\n" << request_body.dump(2) << std::endl;
        std::cout << "========= LLM Request End =========" << std::endl;
    }

    // Send request
    auto response = client->Post(config_.endpoint, headers,
                                 request_body.dump(), "application/json");

    if (!response) {
        result.success = false;
        result.error_message = "HTTP request failed: " +
            httplib::to_string(response.error());
        return result;
    }

    if (response->status != 200) {
        result.success = false;
        result.error_message = "HTTP error: " + std::to_string(response->status);
        return result;
    }

    // Debug: Print raw response
    if (config_.enable_debug_logging) {
        std::cout << "========= LLM Response Start =========" << std::endl;
        std::cout << "[RemoteLLM] Response Status: " << response->status << std::endl;
        std::cout << "[RemoteLLM] Response Body:\n" << response->body << std::endl;
    }

    // Parse response
    try {
        auto response_json = nlohmann::json::parse(response->body);

        // Debug: Print token usage if available
        if (config_.enable_debug_logging && response_json.contains("usage")) {
            auto& usage = response_json["usage"];
            std::cout << "[RemoteLLM] Token Usage:" << std::endl;
            if (usage.contains("prompt_tokens")) {
                std::cout << "  Input tokens:  " << usage["prompt_tokens"] << std::endl;
            }
            if (usage.contains("completion_tokens")) {
                std::cout << "  Output tokens: " << usage["completion_tokens"] << std::endl;
            }
            if (usage.contains("total_tokens")) {
                std::cout << "  Total tokens:  " << usage["total_tokens"] << std::endl;
            }
        }

        // OpenAI format: choices[0].message.content
        if (!response_json.contains("choices") ||
            response_json["choices"].empty()) {
            result.success = false;
            result.error_message = "Invalid response: no choices";
            return result;
        }

        auto& choice = response_json["choices"][0];
        if (!choice.contains("message") ||
            !choice["message"].contains("content")) {
            result.success = false;
            result.error_message = "Invalid response: no message content";
            return result;
        }

        std::string content = choice["message"]["content"];
        
        if (config_.enable_debug_logging) {
            std::cout << "========= LLM Response End =========" << std::endl;
        }
        
        return ParseLlmContent(content);

    } catch (const nlohmann::json::exception& e) {
        result.success = false;
        result.error_message = std::string("JSON parse error: ") + e.what();
        return result;
    }
}

std::string RemoteLlmNluEngine::BuildSystemPrompt(
    const std::vector<const CommandDescriptor*>& schemas) const {
    std::ostringstream oss;

    oss << "You are a voice command classifier. Given a transcript, identify "
           "the command and extract parameters.\n\n";
    oss << "Available commands:\n";

    for (size_t i = 0; i < schemas.size(); ++i) {
        const auto* schema = schemas[i];
        oss << (i + 1) << ". \"" << schema->name << "\"";

        if (!schema->description.empty()) {
            oss << " - " << schema->description;
        }
        oss << "\n";

        if (!schema->parameters.empty()) {
            oss << "   Parameters:\n";
            for (const auto& param : schema->parameters) {
                oss << "   - " << param.name
                    << " (" << ParamTypeToString(param.type);

                if (param.required) {
                    oss << ", required";
                } else {
                    oss << ", optional";
                    if (!param.default_value.empty()) {
                        oss << ", default=" << param.default_value;
                    }
                }
                oss << ")";

                if (!param.description.empty()) {
                    oss << ": " << param.description;
                }

                // Add constraints
                if (param.min_value.has_value() || param.max_value.has_value()) {
                    oss << " [";
                    if (param.min_value.has_value()) {
                        oss << "min=" << param.min_value.value();
                    }
                    if (param.min_value.has_value() && param.max_value.has_value()) {
                        oss << ", ";
                    }
                    if (param.max_value.has_value()) {
                        oss << "max=" << param.max_value.value();
                    }
                    oss << "]";
                }

                if (param.type == ParamType::kEnum && !param.enum_values.empty()) {
                    oss << " [values: ";
                    for (size_t j = 0; j < param.enum_values.size(); ++j) {
                        if (j > 0) oss << ", ";
                        oss << param.enum_values[j];
                    }
                    oss << "]";
                }

                oss << "\n";
            }
        }
        oss << "\n";
    }

    oss << "Respond with JSON only:\n"
           "{\"command\": \"command_name\", \"confidence\": 0.0-1.0, "
           "\"params\": {\"key\": \"value\"}}\n\n"
           "If no command matches, respond:\n"
           "{\"command\": \"\", \"confidence\": 0.0, \"params\": {}}\n";

    return oss.str();
}

std::string RemoteLlmNluEngine::ParamTypeToString(ParamType type) const {
    switch (type) {
        case ParamType::kString:
            return "string";
        case ParamType::kInteger:
            return "integer";
        case ParamType::kDouble:
            return "double";
        case ParamType::kBool:
            return "boolean";
        case ParamType::kEnum:
            return "enum";
        default:
            return "unknown";
    }
}

NluResult RemoteLlmNluEngine::ParseLlmContent(const std::string& content) const {
    NluResult result;

    // Try to extract JSON from content (LLM might include extra text)
    std::string json_str = content;

    // Find JSON object boundaries
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        json_str = content.substr(start, end - start + 1);
    }

    try {
        auto json = nlohmann::json::parse(json_str);

        // Extract command name
        if (json.contains("command") && json["command"].is_string()) {
            result.command_name = json["command"].get<std::string>();
        }

        // Extract confidence
        if (json.contains("confidence") && json["confidence"].is_number()) {
            result.confidence = json["confidence"].get<float>();
        }

        // Extract parameters
        if (json.contains("params") && json["params"].is_object()) {
            for (auto& [key, value] : json["params"].items()) {
                if (value.is_string()) {
                    result.extracted_params[key] = value.get<std::string>();
                } else if (value.is_number_integer()) {
                    result.extracted_params[key] = std::to_string(value.get<int>());
                } else if (value.is_number_float()) {
                    result.extracted_params[key] = std::to_string(value.get<double>());
                } else if (value.is_boolean()) {
                    result.extracted_params[key] = value.get<bool>() ? "true" : "false";
                }
            }
        }

        // Success if we got a command (empty command = no match, still success)
        result.success = true;

    } catch (const nlohmann::json::exception& e) {
        result.success = false;
        result.error_message = std::string("Failed to parse LLM response: ") + e.what();
    }

    return result;
}

}  // namespace voice_command
