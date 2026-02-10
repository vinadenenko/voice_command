#include "audio_capture/qt/qt_audio_capture.h"

#include <stdexcept>

namespace voice_command {
namespace audio_capture {

namespace {
constexpr const char* kNotImplementedMessage =
    "QtAudioCapture is not yet implemented. Use SdlAudioCapture instead.";
}  // namespace

QtAudioCapture::QtAudioCapture() = default;

QtAudioCapture::~QtAudioCapture() = default;

bool QtAudioCapture::Init(const AudioCaptureConfig& /*config*/) {
    throw std::runtime_error(kNotImplementedMessage);
}

void QtAudioCapture::Shutdown() {
    throw std::runtime_error(kNotImplementedMessage);
}

bool QtAudioCapture::Start() {
    throw std::runtime_error(kNotImplementedMessage);
}

bool QtAudioCapture::Stop() {
    throw std::runtime_error(kNotImplementedMessage);
}

bool QtAudioCapture::IsRunning() const {
    throw std::runtime_error(kNotImplementedMessage);
}

void QtAudioCapture::GetAudio(int /*duration_ms*/, AudioSamples& /*samples*/) {
    throw std::runtime_error(kNotImplementedMessage);
}

bool QtAudioCapture::Clear() {
    throw std::runtime_error(kNotImplementedMessage);
}

int QtAudioCapture::GetSampleRate() const {
    throw std::runtime_error(kNotImplementedMessage);
}

int QtAudioCapture::GetBufferDurationMs() const {
    throw std::runtime_error(kNotImplementedMessage);
}

}  // namespace audio_capture
}  // namespace voice_command
