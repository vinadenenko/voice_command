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
public:
    explicit App(QObject *parent = nullptr);

    bool isRecording() const;

public slots:
    void onButtonPressed();
    void onButtonReleased();

signals:
    void isRecordingChanged();


signals:
    void requestChangeColor(const QColor &color);
private:
    void initVoiceAssistant();
private:
    voice_command::QtVoiceAssistant *assistant_;
    bool isRecording_{false};
};

#endif // APP_H
