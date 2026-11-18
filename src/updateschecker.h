#pragma once
#include <QObject>
class QNetworkAccessManager;
class QNetworkReply;

// Qt networking is asynchronous; an idle update-check thread is unnecessary.
class UpdatesChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdatesChecker(QObject *parent = nullptr);
public slots:
    void start();
    void updatedDataReady(QNetworkReply *reply);
signals:
    void updatesAvailable();
private:
    QNetworkAccessManager *mNetworkAccessManager = nullptr;
};
