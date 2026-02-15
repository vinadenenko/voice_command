#ifndef APP_H
#define APP_H

#include <QObject>
#include <QQmlEngine>
#include <QColor>
#include <memory>
#include <qt_voice_assistant.h>
#include <local_whisper_engine.h>
#include "remote_whisper_engine.h"

class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    bool isRecording() const;
    bool isProcessing() const;

public slots:
    void onButtonPressed();
    void onButtonReleased();

signals:
    void isRecordingChanged();
    void isProcessingChanged();
    void requestChangeColor(const QColor &color);

private:
    void initVoiceAssistant();
    void setProcessing(bool processing);

    std::unique_ptr<voice_command::LocalWhisperEngine> localWhisperEngine_;
    std::unique_ptr<voice_command::RemoteWhisperEngine> remoteWhisperEngine_;
    voice_command::QtVoiceAssistant *assistant_;
    bool isRecording_{false};
    bool isProcessing_{false};
};

#endif // APP_H
