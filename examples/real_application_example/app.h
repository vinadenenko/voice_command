#ifndef APP_H
#define APP_H

#include <QObject>
#include <QQmlEngine>
#include <QColor>

class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit App(QObject *parent = nullptr);

    Q_INVOKABLE void test();

signals:
    void requestChangeColor(const QColor &color);
};

#endif // APP_H
