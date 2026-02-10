#include "audio_capture/qt/qt_audio_capture.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace voice_command {
namespace audio_capture {

/// QIODevice subclass that receives audio data from QAudioSource
/// and forwards it to QtAudioCapture for processing
class AudioBufferDevice : public QIODevice {
    Q_OBJECT

public:
    explicit AudioBufferDevice(QtAudioCapture* capture, QObject* parent = nullptr)
        : QIODevice(parent), capture_(capture) {}

    bool open(OpenMode mode) override {
        if (mode != WriteOnly) {
            return false;
        }
        return QIODevice::open(mode);
    }

protected:
    qint64 writeData(const char* data, qint64 len) override {
        if (capture_ != nullptr) {
            capture_->OnAudioData(data, static_cast<int64_t>(len));
        }
        return len;
    }

    qint64 readData(char* /*data*/, qint64 /*maxlen*/) override {
        // This device is write-only
        return 0;
    }

private:
    QtAudioCapture* capture_;
};

QtAudioCapture::QtAudioCapture() = default;

QtAudioCapture::~QtAudioCapture() { Shutdown(); }

bool QtAudioCapture::Init(const AudioCaptureConfig& config) {
    // Configure audio format
    QAudioFormat format;
    format.setSampleRate(config.sample_rate);
    format.setChannelCount(config.channels);
    format.setSampleFormat(QAudioFormat::Int16);

    // Select audio device
    QAudioDevice selected_device;

    if (!config.device_name.empty()) {
        // Find device by name
        const auto devices = QMediaDevices::audioInputs();
        for (const auto& device : devices) {
            if (device.description().toStdString() == config.device_name) {
                selected_device = device;
                break;
            }
        }
        if (selected_device.isNull()) {
            std::fprintf(stderr,
                         "QtAudioCapture: Device '%s' not found, using default\n",
                         config.device_name.c_str());
        }
    }

    if (selected_device.isNull()) {
        selected_device = QMediaDevices::defaultAudioInput();
    }

    if (selected_device.isNull()) {
        std::fprintf(stderr, "QtAudioCapture: No audio input device available\n");
        return false;
    }

    std::fprintf(stderr, "QtAudioCapture: Using device: %s\n",
                 selected_device.description().toStdString().c_str());

    // Check if format is supported
    if (!selected_device.isFormatSupported(format)) {
        std::fprintf(stderr,
                     "QtAudioCapture: Requested format not supported, "
                     "trying nearest format\n");
        // Qt6 doesn't have nearestFormat, so we'll try to proceed anyway
        // The QAudioSource will handle format conversion if possible
    }

    // Create audio source
    audio_source_ = std::make_unique<QAudioSource>(selected_device, format);

    // Get actual format (may differ from requested)
    const QAudioFormat actual_format = audio_source_->format();
    sample_rate_ = actual_format.sampleRate();
    buffer_duration_ms_ = config.buffer_duration_ms;

    std::fprintf(stderr, "QtAudioCapture: Configured format:\n");
    std::fprintf(stderr, "QtAudioCapture:   - Sample rate: %d\n", sample_rate_);
    std::fprintf(stderr, "QtAudioCapture:   - Channels: %d\n",
                 actual_format.channelCount());
    std::fprintf(stderr, "QtAudioCapture:   - Sample format: %d\n",
                 static_cast<int>(actual_format.sampleFormat()));

    // Create buffer device
    buffer_device_ = std::make_unique<AudioBufferDevice>(this);
    buffer_device_->open(QIODevice::WriteOnly);

    // Resize circular buffer
    const size_t buffer_size =
        static_cast<size_t>(sample_rate_) * buffer_duration_ms_ / 1000;
    audio_buffer_.resize(buffer_size);
    buffer_pos_ = 0;
    buffer_len_ = 0;

    return true;
}

void QtAudioCapture::Shutdown() {
    Stop();

    if (audio_source_) {
        audio_source_.reset();
    }

    if (buffer_device_) {
        buffer_device_->close();
        buffer_device_.reset();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    audio_buffer_.clear();
    buffer_pos_ = 0;
    buffer_len_ = 0;
}

bool QtAudioCapture::Start() {
    if (!audio_source_) {
        std::fprintf(stderr, "QtAudioCapture: No audio source to start\n");
        return false;
    }

    if (running_.load()) {
        std::fprintf(stderr, "QtAudioCapture: Already running\n");
        return false;
    }

    audio_source_->start(buffer_device_.get());
    running_.store(true);

    return true;
}

bool QtAudioCapture::Stop() {
    if (!audio_source_) {
        std::fprintf(stderr, "QtAudioCapture: No audio source to stop\n");
        return false;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "QtAudioCapture: Already stopped\n");
        return false;
    }

    audio_source_->stop();
    running_.store(false);

    return true;
}

bool QtAudioCapture::IsRunning() const { return running_.load(); }

void QtAudioCapture::GetAudio(int duration_ms, AudioSamples& samples) {
    samples.clear();

    if (!audio_source_) {
        std::fprintf(stderr,
                     "QtAudioCapture: No audio source to get audio from\n");
        return;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "QtAudioCapture: Not running\n");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (duration_ms <= 0) {
        duration_ms = buffer_duration_ms_;
    }

    size_t n_samples = static_cast<size_t>(sample_rate_) * duration_ms / 1000;
    if (n_samples > buffer_len_) {
        n_samples = buffer_len_;
    }

    samples.resize(n_samples);

    // Calculate start position in circular buffer
    int start_pos = static_cast<int>(buffer_pos_) - static_cast<int>(n_samples);
    if (start_pos < 0) {
        start_pos += static_cast<int>(audio_buffer_.size());
    }

    // Copy from circular buffer
    const size_t s0 = static_cast<size_t>(start_pos);
    if (s0 + n_samples > audio_buffer_.size()) {
        // Wrap around
        const size_t first_part = audio_buffer_.size() - s0;
        std::memcpy(samples.data(), &audio_buffer_[s0],
                    first_part * sizeof(float));
        std::memcpy(&samples[first_part], audio_buffer_.data(),
                    (n_samples - first_part) * sizeof(float));
    } else {
        std::memcpy(samples.data(), &audio_buffer_[s0],
                    n_samples * sizeof(float));
    }
}

bool QtAudioCapture::Clear() {
    if (!audio_source_) {
        std::fprintf(stderr, "QtAudioCapture: No audio source to clear\n");
        return false;
    }

    if (!running_.load()) {
        std::fprintf(stderr, "QtAudioCapture: Not running\n");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    buffer_pos_ = 0;
    buffer_len_ = 0;

    return true;
}

int QtAudioCapture::GetSampleRate() const { return sample_rate_; }

int QtAudioCapture::GetBufferDurationMs() const { return buffer_duration_ms_; }

std::vector<std::string> QtAudioCapture::GetDeviceList() {
    std::vector<std::string> devices;

    const auto audio_devices = QMediaDevices::audioInputs();
    devices.reserve(static_cast<size_t>(audio_devices.size()));

    for (const auto& device : audio_devices) {
        devices.push_back(device.description().toStdString());
    }

    return devices;
}

void QtAudioCapture::OnAudioData(const char* data, int64_t len) {
    if (!running_.load()) {
        return;
    }

    // Convert Int16 samples to Float32
    const auto* int16_data = reinterpret_cast<const int16_t*>(data);
    const size_t n_samples = static_cast<size_t>(len) / sizeof(int16_t);

    size_t samples_to_copy = n_samples;

    // Handle case where incoming data is larger than buffer
    if (samples_to_copy > audio_buffer_.size()) {
        samples_to_copy = audio_buffer_.size();
        // Skip to the end of the incoming data
        int16_data += (n_samples - samples_to_copy);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Convert and copy to circular buffer
    for (size_t i = 0; i < samples_to_copy; ++i) {
        // Convert Int16 [-32768, 32767] to Float32 [-1.0, 1.0]
        const float sample =
            static_cast<float>(int16_data[i]) / 32768.0f;

        audio_buffer_[buffer_pos_] = sample;
        buffer_pos_ = (buffer_pos_ + 1) % audio_buffer_.size();
    }

    buffer_len_ = std::min(buffer_len_ + samples_to_copy, audio_buffer_.size());
}

}  // namespace audio_capture
}  // namespace voice_command

// Include MOC file for AudioBufferDevice (Q_OBJECT in cpp file)
#include "qt_audio_capture.moc"
