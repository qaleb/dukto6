#pragma once
#include <QObject>
#include <QStringList>

class AndroidShareBridge : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~AndroidShareBridge() override;
    void ready();
signals:
    void shareReceived(QStringList sources, QString text);
    void serviceUnavailable();
    void storageChanged();
    void keyboardTopChanged(int screenPixels);
};
