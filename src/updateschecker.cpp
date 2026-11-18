#include "updateschecker.h"
#include "platform.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QLocale>
#include <QSysInfo>
#include <QTimer>

UpdatesChecker::UpdatesChecker(QObject *parent) : QObject(parent) {}
void UpdatesChecker::start() {
    if (mNetworkAccessManager) return;
    mNetworkAccessManager = new QNetworkAccessManager(this);
    QUrl url("http://www.msec.it/dukto/r5check.php");
    QUrlQuery query;
    // Preserve the legacy update endpoint's version identifier.
    query.addQueryItem("ver","6.0"); query.addQueryItem("locale",QLocale::system().name());
    query.addQueryItem("os",Platform::getPlatformName().toLower()); query.addQueryItem("osver",QSysInfo::productVersion());
    url.setQuery(query);
    QNetworkRequest request(url); request.setTransferTimeout(5000);
    auto *reply = mNetworkAccessManager->get(request);
    reply->setReadBufferSize(4096);
    QTimer::singleShot(5000,reply,[reply] { if (reply->isRunning()) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply] { updatedDataReady(reply); });
}
void UpdatesChecker::updatedDataReady(QNetworkReply *reply) {
    const bool available = reply->error() == QNetworkReply::NoError && !reply->readAll().isEmpty();
    reply->deleteLater();
    if (available) emit updatesAvailable();
}
