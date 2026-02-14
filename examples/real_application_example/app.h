#ifndef APP_H
#define APP_H

#include <QObject>
#include <QQmlEngine>
#include <QColor>
#include <qt_voice_assistant.h>

class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)

public:
    explicit App(QObject *parent = nullptr);

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

    voice_command::QtVoiceAssistant *assistant_;
    bool isRecording_{false};
    bool isProcessing_{false};
};

#endif // APP_H
