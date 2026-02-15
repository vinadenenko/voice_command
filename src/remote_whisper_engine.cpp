#include "remote_whisper_engine.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace voice_command {

namespace {

/// Convert string to lowercase
std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

/// Trim whitespace from both ends of a string
std::string Trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

/// Parse host and port from URL
bool ParseUrl(const std::string& url, std::string& host, int& port, bool& use_ssl) {
    std::string work = url;

    // Check for scheme
    use_ssl = false;
    if (work.substr(0, 8) == "https://") {
        use_ssl = true;
        work = work.substr(8);
    } else if (work.substr(0, 7) == "http://") {
        work = work.substr(7);
    }

    // Remove trailing slash and path
    size_t slash_pos = work.find('/');
    if (slash_pos != std::string::npos) {
        work = work.substr(0, slash_pos);
    }

    // Parse host:port
    size_t colon_pos = work.find(':');
    if (colon_pos != std::string::npos) {
        host = work.substr(0, colon_pos);
        try {
            port = std::stoi(work.substr(colon_pos + 1));
        } catch (...) {
            return false;
        }
    } else {
        host = work;
        port = use_ssl ? 443 : 80;
    }

    return !host.empty();
}

}  // namespace

RemoteWhisperEngine::RemoteWhisperEngine() = default;

RemoteWhisperEngine::~RemoteWhisperEngine() {
    if (initialized_) {
        Shutdown();
    }
}

RemoteWhisperEngine::RemoteWhisperEngine(RemoteWhisperEngine&& other) noexcept
    : config_(std::move(other.config_)), initialized_(other.initialized_) {
    other.initialized_ = false;
}

RemoteWhisperEngine& RemoteWhisperEngine::operator=(
    RemoteWhisperEngine&& other) noexcept {
    if (this != &other) {
        if (initialized_) {
            Shutdown();
        }
        config_ = std::move(other.config_);
        initialized_ = other.initialized_;
        other.initialized_ = false;
    }
    return *this;
}

bool RemoteWhisperEngine::Init(const RemoteAsrConfig& config) {
    if (initialized_) {
        return false;  // Already initialized
    }

    // Validate configuration
    if (config.server_url.empty()) {
        return false;
    }

    std::string host;
    int port;
    bool use_ssl;
    if (!ParseUrl(config.server_url, host, port, use_ssl)) {
        return false;
    }

    config_ = config;
    initialized_ = true;
    return true;
}

void RemoteWhisperEngine::Shutdown() {
    initialized_ = false;
}

bool RemoteWhisperEngine::IsInitialized() const {
    return initialized_;
}

const RemoteAsrConfig& RemoteWhisperEngine::GetConfig() const {
    return config_;
}

std::vector<char> RemoteWhisperEngine::EncodeAsWav(
    const audio_capture::AudioSamples& samples) {
    // WAV file format for 16kHz mono float32 -> 16-bit PCM
    const int sample_rate = 16000;
    const int bits_per_sample = 16;
    const int num_channels = 1;
    const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const int block_align = num_channels * bits_per_sample / 8;

    // Convert float32 samples to int16
    std::vector<int16_t> pcm_data(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float sample = samples[i];
        // Clamp to [-1, 1]
        sample = std::max(-1.0f, std::min(1.0f, sample));
        // Convert to int16
        pcm_data[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    const uint32_t data_size = static_cast<uint32_t>(pcm_data.size() * sizeof(int16_t));
    const uint32_t file_size = 36 + data_size;

    std::vector<char> wav_data;
    wav_data.reserve(44 + data_size);

    // Helper to write bytes
    auto write_bytes = [&wav_data](const void* data, size_t size) {
        const char* bytes = static_cast<const char*>(data);
        wav_data.insert(wav_data.end(), bytes, bytes + size);
    };

    auto write_u32 = [&wav_data](uint32_t value) {
        wav_data.push_back(static_cast<char>(value & 0xFF));
        wav_data.push_back(static_cast<char>((value >> 8) & 0xFF));
        wav_data.push_back(static_cast<char>((value >> 16) & 0xFF));
        wav_data.push_back(static_cast<char>((value >> 24) & 0xFF));
    };

    auto write_u16 = [&wav_data](uint16_t value) {
        wav_data.push_back(static_cast<char>(value & 0xFF));
        wav_data.push_back(static_cast<char>((value >> 8) & 0xFF));
    };

    // RIFF header
    write_bytes("RIFF", 4);
    write_u32(file_size);
    write_bytes("WAVE", 4);

    // fmt subchunk
    write_bytes("fmt ", 4);
    write_u32(16);                          // Subchunk1Size (16 for PCM)
    write_u16(1);                           // AudioFormat (1 = PCM)
    write_u16(static_cast<uint16_t>(num_channels));
    write_u32(static_cast<uint32_t>(sample_rate));
    write_u32(static_cast<uint32_t>(byte_rate));
    write_u16(static_cast<uint16_t>(block_align));
    write_u16(static_cast<uint16_t>(bits_per_sample));

    // data subchunk
    write_bytes("data", 4);
    write_u32(data_size);
    write_bytes(pcm_data.data(), data_size);

    return wav_data;
}

TranscriptionResult RemoteWhisperEngine::Transcribe(
    const audio_capture::AudioSamples& samples) {
    TranscriptionResult result;

    if (!initialized_) {
        result.error = "Engine not initialized";
        return result;
    }

    if (samples.empty()) {
        result.error = "Empty audio samples";
        return result;
    }

    const auto t_start = std::chrono::high_resolution_clock::now();

    // Parse server URL
    std::string host;
    int port;
    bool use_ssl;
    if (!ParseUrl(config_.server_url, host, port, use_ssl)) {
        result.error = "Invalid server URL";
        return result;
    }

    // Create HTTP client
    std::unique_ptr<httplib::Client> client;
    if (use_ssl) {
        client = std::make_unique<httplib::Client>(host, port);
        // Note: For production, you might want to enable SSL verification
    } else {
        client = std::make_unique<httplib::Client>(host, port);
    }

    client->set_connection_timeout(config_.timeout_ms / 1000,
                                   (config_.timeout_ms % 1000) * 1000);
    client->set_read_timeout(config_.timeout_ms / 1000,
                             (config_.timeout_ms % 1000) * 1000);

    // Encode audio as WAV
    std::vector<char> wav_data = EncodeAsWav(samples);

    // Build multipart form data
    httplib::MultipartFormDataItems items = {
        {"file", std::string(wav_data.begin(), wav_data.end()), "audio.wav",
         "audio/wav"},
        {"response_format", "json", "", ""},
        {"language", config_.language, "", ""},
        {"temperature", std::to_string(config_.temperature), "", ""},
    };

    if (config_.translate) {
        items.push_back({"translate", "true", "", ""});
    }

    // Send request
    auto res = client->Post(config_.inference_path, items);

    const auto t_end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start)
            .count();

    if (!res) {
        result.error = "HTTP request failed: " + httplib::to_string(res.error());
        return result;
    }

    if (res->status != 200) {
        result.error =
            "Server returned error: " + std::to_string(res->status) + " " + res->body;
        return result;
    }

    // Parse JSON response
    try {
        auto json = nlohmann::json::parse(res->body);

        if (json.contains("error")) {
            result.error = json["error"].get<std::string>();
            return result;
        }

        if (json.contains("text")) {
            result.text = Trim(json["text"].get<std::string>());
            result.success = true;
        } else {
            result.error = "Response missing 'text' field";
        }
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }

    return result;
}

float RemoteWhisperEngine::CalculateSimilarity(const std::string& s1,
                                                const std::string& s2) {
    // Levenshtein distance
    std::string a = ToLower(s1);
    std::string b = ToLower(s2);

    const size_t m = a.size();
    const size_t n = b.size();

    if (m == 0) return n == 0 ? 1.0f : 0.0f;
    if (n == 0) return 0.0f;

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i - 1][j] + 1,      // deletion
                                 dp[i][j - 1] + 1,      // insertion
                                 dp[i - 1][j - 1] + cost});  // substitution
        }
    }

    size_t distance = dp[m][n];
    size_t max_len = std::max(m, n);
    return 1.0f - static_cast<float>(distance) / static_cast<float>(max_len);
}

GuidedMatchResult RemoteWhisperEngine::GuidedMatch(
    const audio_capture::AudioSamples& samples,
    const std::vector<std::string>& phrases) {
    GuidedMatchResult result;

    if (!initialized_) {
        result.error = "Engine not initialized";
        return result;
    }

    if (samples.empty()) {
        result.error = "Empty audio samples";
        return result;
    }

    if (phrases.empty()) {
        result.error = "No phrases provided";
        return result;
    }

    const auto t_start = std::chrono::high_resolution_clock::now();

    // First, transcribe the audio
    auto transcription = Transcribe(samples);

    if (!transcription.success) {
        result.error = transcription.error;
        result.processing_time_ms = transcription.processing_time_ms;
        return result;
    }

    // Fuzzy match against phrases
    std::string transcript_lower = ToLower(Trim(transcription.text));
    result.all_scores.resize(phrases.size(), 0.0f);

    float best_score = 0.0f;
    int best_index = 0;

    for (size_t i = 0; i < phrases.size(); ++i) {
        float similarity = CalculateSimilarity(transcript_lower, phrases[i]);
        result.all_scores[i] = similarity;

        if (similarity > best_score) {
            best_score = similarity;
            best_index = static_cast<int>(i);
        }
    }

    result.success = true;
    result.best_match_index = best_index;
    result.best_match = phrases[best_index];
    result.best_score = best_score;

    const auto t_end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start)
            .count();

    return result;
}

}  // namespace voice_command
