#include "local_whisper_engine.h"

#include <whisper.h>
#include <ggml.h>
#include <ggml-backend.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

#include <QDebug>
#include <QCoreApplication>
#include <QDir>

#include <dlfcn.h>

#define MYTAG "my-tag:"

namespace voice_command {

namespace {

/// Load GGML backends from application library path (needed for Android)
void LoadGgmlBackends() {
    // Get the application's library directory
    QString appDir = QCoreApplication::applicationDirPath();
    qDebug() << MYTAG << "App directory:" << appDir;

    // On Android, libraries are in the same directory as the app
    // Try to load backends from there
    QByteArray pathBytes = appDir.toUtf8();
    qDebug() << MYTAG << "Loading GGML backends from:" << pathBytes.constData();
    ggml_backend_load_all_from_path(pathBytes.constData());

    // Also try explicit loading of Vulkan backend
    QString vulkanPath = appDir + "/libggml-vulkan.so";
    QByteArray vulkanPathBytes = vulkanPath.toUtf8();
    qDebug() << MYTAG << "Trying ggml_backend_load:" << vulkanPathBytes.constData();

    ggml_backend_reg_t vulkan_reg = ggml_backend_load(vulkanPathBytes.constData());
    if (vulkan_reg) {
        qDebug() << MYTAG << "ggml_backend_load succeeded!";
        qDebug() << MYTAG << "  Name:" << ggml_backend_reg_name(vulkan_reg);
        qDebug() << MYTAG << "  Devices:" << ggml_backend_reg_dev_count(vulkan_reg);
    } else {
        qDebug() << MYTAG << "ggml_backend_load failed";
    }

    // Try direct dlopen to get actual error
    qDebug() << MYTAG << "Trying direct dlopen:" << vulkanPathBytes.constData();
    dlerror(); // Clear any existing error
    void* handle = dlopen(vulkanPathBytes.constData(), RTLD_NOW);
    if (handle) {
        qDebug() << MYTAG << "dlopen succeeded!";

        // Try to find the backend registration function
        typedef ggml_backend_reg_t (*ggml_backend_reg_fn)(void);
        ggml_backend_reg_fn reg_fn = (ggml_backend_reg_fn)dlsym(handle, "ggml_backend_vk_reg");
        if (reg_fn) {
            qDebug() << MYTAG << "Found ggml_backend_vk_reg symbol";
            ggml_backend_reg_t reg = reg_fn();
            if (reg) {
                qDebug() << MYTAG << "ggml_backend_vk_reg() returned valid reg";
                qDebug() << MYTAG << "  Devices:" << ggml_backend_reg_dev_count(reg);
            } else {
                qDebug() << MYTAG << "ggml_backend_vk_reg() returned NULL";
            }
        } else {
            const char* sym_err = dlerror();
            qDebug() << MYTAG << "dlsym failed:" << (sym_err ? sym_err : "unknown");
        }
    } else {
        const char* err = dlerror();
        qDebug() << MYTAG << "dlopen failed:" << (err ? err : "unknown error");
    }
}

/// Debug function to check available GGML backends
void CheckGgmlBackends() {
    // First, try to load backends (especially important on Android)
    LoadGgmlBackends();

    size_t count = ggml_backend_reg_count();
    qDebug() << MYTAG << "=== GGML Backend Check ===";
    qDebug() << MYTAG << "Total backends:" << count;

    for (size_t i = 0; i < count; i++) {
        ggml_backend_reg_t reg = ggml_backend_reg_get(i);
        const char* name = ggml_backend_reg_name(reg);
        size_t dev_count = ggml_backend_reg_dev_count(reg);
        qDebug() << MYTAG << "Backend" << i << ":" << name << "- devices:" << dev_count;

        for (size_t j = 0; j < dev_count; j++) {
            ggml_backend_dev_t dev = ggml_backend_reg_dev_get(reg, j);
            qDebug() << MYTAG << "  Device" << j << ":" << ggml_backend_dev_name(dev);
        }
    }
    qDebug() << MYTAG << "=========================";
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

}  // namespace

LocalWhisperEngine::LocalWhisperEngine() = default;

LocalWhisperEngine::~LocalWhisperEngine() {
    if (initialized_) {
        Shutdown();
    }
}

LocalWhisperEngine::LocalWhisperEngine(LocalWhisperEngine&& other) noexcept
    : ctx_(other.ctx_),
      config_(std::move(other.config_)),
      initialized_(other.initialized_) {
    other.ctx_ = nullptr;
    other.initialized_ = false;
}

LocalWhisperEngine& LocalWhisperEngine::operator=(LocalWhisperEngine&& other) noexcept {
    if (this != &other) {
        if (initialized_) {
            Shutdown();
        }
        ctx_ = other.ctx_;
        config_ = std::move(other.config_);
        initialized_ = other.initialized_;
        other.ctx_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool LocalWhisperEngine::Init(const LocalWhisperEngineConfig& config) {
    if (initialized_) {
        return false;  // Already initialized
    }

    config_ = config;

    // Debug: Check available backends before initialization
    CheckGgmlBackends();

    // Create context parameters
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = config.use_gpu;
    cparams.flash_attn = config.flash_attn;

    // Load the model
    ctx_ = whisper_init_from_file_with_params(config.model_path.c_str(), cparams);
    if (ctx_ == nullptr) {
        return false;
    }

    // Validate language if not "auto"
    if (config.language != "auto" && whisper_lang_id(config.language.c_str()) == -1) {
        whisper_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    initialized_ = true;
    return true;
}

void LocalWhisperEngine::Shutdown() {
    if (!initialized_) {
        return;
    }

    if (ctx_ != nullptr) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }

    initialized_ = false;
}

bool LocalWhisperEngine::IsInitialized() const {
    return initialized_;
}

const LocalWhisperEngineConfig& LocalWhisperEngine::GetConfig() const {
    return config_;
}

TranscriptionResult LocalWhisperEngine::Transcribe(
    const audio_capture::AudioSamples& samples) {
    TranscriptionResult result;

    if (!initialized_ || ctx_ == nullptr) {
        result.error = "Engine not initialized";
        return result;
    }

    if (samples.empty()) {
        result.error = "Empty audio samples";
        return result;
    }

    const auto t_start = std::chrono::high_resolution_clock::now();

    // Set up whisper parameters for beam search
    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);

    wparams.print_progress = false;
    wparams.print_special = config_.print_special;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = config_.translate;
    wparams.no_context = true;
    wparams.no_timestamps = true;
    wparams.single_segment = true;
    wparams.max_tokens = config_.max_tokens;
    wparams.language = config_.language.c_str();
    wparams.n_threads = config_.num_threads;
    wparams.audio_ctx = config_.audio_ctx;

    wparams.temperature = config_.temperature;
    wparams.temperature_inc = 1.0f;
    wparams.greedy.best_of = 5;
    wparams.beam_search.beam_size = config_.beam_size;

    // Run inference
    if (whisper_full(ctx_, wparams, samples.data(),
                     static_cast<int>(samples.size())) != 0) {
        result.error = "Whisper inference failed";
        return result;
    }

    // Collect results from all segments
    std::string text;
    const int n_segments = whisper_full_n_segments(ctx_);

    for (int i = 0; i < n_segments; ++i) {
        const char* segment_text = whisper_full_get_segment_text(ctx_, i);
        if (segment_text != nullptr) {
            text += segment_text;
        }

        // Collect token probabilities
        const int n_tokens = whisper_full_n_tokens(ctx_, i);
        for (int j = 0; j < n_tokens; ++j) {
            const auto token = whisper_full_get_token_data(ctx_, i, j);
            if (result.num_tokens == 0 || token.plog < result.logprob_min) {
                result.logprob_min = token.plog;
            }
            result.logprob_sum += token.plog;
            ++result.num_tokens;
        }
    }

    const auto t_end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start)
            .count();

    result.success = true;
    result.text = Trim(text);
    return result;
}

GuidedMatchResult LocalWhisperEngine::GuidedMatch(
    const audio_capture::AudioSamples& samples,
    const std::vector<std::string>& phrases) {
    GuidedMatchResult result;

    if (!initialized_ || ctx_ == nullptr) {
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

    // Tokenize all phrases
    std::vector<std::vector<whisper_token>> phrase_tokens;
    phrase_tokens.reserve(phrases.size());

    for (const auto& phrase : phrases) {
        whisper_token tokens[1024];
        std::vector<whisper_token> phrase_token_list;

        // Build token list by progressive tokenization
        for (size_t l = 0; l < phrase.size(); ++l) {
            // Add leading space for proper tokenization
            std::string prefix = " " + phrase.substr(0, l + 1);
            const int n = whisper_tokenize(ctx_, prefix.c_str(), tokens, 1024);
            if (n < 0) {
                result.error = "Failed to tokenize phrase: " + phrase;
                return result;
            }
            if (n == 1) {
                phrase_token_list.push_back(tokens[0]);
            }
        }
        phrase_tokens.push_back(std::move(phrase_token_list));
    }

    // Build the guided prompt
    std::string prompt = BuildGuidedPrompt(phrases);

    // Tokenize the prompt
    std::vector<whisper_token> prompt_tokens(1024);
    const int n_prompt_tokens =
        whisper_tokenize(ctx_, prompt.c_str(), prompt_tokens.data(), 1024);
    if (n_prompt_tokens < 0) {
        result.error = "Failed to tokenize prompt";
        return result;
    }
    prompt_tokens.resize(n_prompt_tokens);

    // Set up whisper parameters for single-token greedy decoding
    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress = false;
    wparams.print_special = config_.print_special;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = config_.translate;
    wparams.no_context = true;
    wparams.single_segment = true;
    wparams.max_tokens = 1;  // Only need one token for logits
    wparams.language = config_.language.c_str();
    wparams.n_threads = config_.num_threads;
    wparams.audio_ctx = config_.audio_ctx;

    wparams.prompt_tokens = prompt_tokens.data();
    wparams.prompt_n_tokens = static_cast<int>(prompt_tokens.size());

    // Run inference
    if (whisper_full(ctx_, wparams, samples.data(),
                     static_cast<int>(samples.size())) != 0) {
        result.error = "Whisper inference failed";
        return result;
    }

    // Get logits and convert to probabilities
    const float* logits = whisper_get_logits(ctx_);
    const int vocab_size = whisper_n_vocab(ctx_);

    std::vector<float> probs(vocab_size, 0.0f);

    // Softmax over logits
    float max_logit = -1e9f;
    for (int i = 0; i < vocab_size; ++i) {
        max_logit = std::max(max_logit, logits[i]);
    }

    float sum = 0.0f;
    for (int i = 0; i < vocab_size; ++i) {
        probs[i] = std::exp(logits[i] - max_logit);
        sum += probs[i];
    }

    for (int i = 0; i < vocab_size; ++i) {
        probs[i] /= sum;
    }

    // Score each phrase by averaging token probabilities
    result.all_scores.resize(phrases.size(), 0.0f);
    double total_score = 0.0;

    for (size_t i = 0; i < phrases.size(); ++i) {
        const auto& tokens = phrase_tokens[i];
        if (tokens.empty()) {
            continue;
        }

        float phrase_prob = 0.0f;
        for (const auto token : tokens) {
            if (token >= 0 && token < vocab_size) {
                phrase_prob += probs[token];
            }
        }
        phrase_prob /= static_cast<float>(tokens.size());
        result.all_scores[i] = phrase_prob;
        total_score += phrase_prob;
    }

    // Normalize scores
    if (total_score > 0) {
        for (auto& score : result.all_scores) {
            score /= static_cast<float>(total_score);
        }
    }

    // Find best match
    result.best_match_index = 0;
    result.best_score = result.all_scores[0];
    for (size_t i = 1; i < result.all_scores.size(); ++i) {
        if (result.all_scores[i] > result.best_score) {
            result.best_score = result.all_scores[i];
            result.best_match_index = static_cast<int>(i);
        }
    }
    result.best_match = phrases[result.best_match_index];

    const auto t_end = std::chrono::high_resolution_clock::now();
    result.processing_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start)
            .count();

    result.success = true;
    return result;
}

std::string LocalWhisperEngine::BuildGuidedPrompt(
    const std::vector<std::string>& phrases) const {
    std::ostringstream oss;
    oss << "select one from the available words: ";

    for (size_t i = 0; i < phrases.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << phrases[i];
    }
    oss << ". selected word: ";

    return oss.str();
}

std::vector<int> LocalWhisperEngine::TokenizePhrase(const std::string& phrase) const {
    if (ctx_ == nullptr) {
        return {};
    }

    whisper_token tokens[1024];
    std::string prefixed = " " + phrase;  // Add leading space
    const int n = whisper_tokenize(ctx_, prefixed.c_str(), tokens, 1024);

    if (n < 0) {
        return {};
    }

    return std::vector<int>(tokens, tokens + n);
}

}  // namespace voice_command
