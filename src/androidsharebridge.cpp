#include "androidsharebridge.h"
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>

namespace {
QMutex bridgeMutex;
AndroidShareBridge *bridge = nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_idv_coolshou_DuktoActivity_nativeShare(JNIEnv *, jclass, jstring value)
{
    const QByteArray payload = QJniObject(value).toString().toUtf8();
    QMutexLocker lock(&bridgeMutex);
    if (bridge) QMetaObject::invokeMethod(bridge, [target = bridge, payload] {
        const auto object = QJsonDocument::fromJson(payload).object();
        QStringList sources;
        for (const auto &value : object["sources"].toArray())
            if (value.isString()) sources.append(value.toString());
        const auto text = object["text"].toString();
        if (!sources.isEmpty() || !text.isEmpty()) emit target->shareReceived(sources, text);
    }, Qt::QueuedConnection);
}

extern "C" JNIEXPORT void JNICALL Java_idv_coolshou_DuktoActivity_nativeTimeout(JNIEnv *, jclass)
{
    QMutexLocker lock(&bridgeMutex);
    if (bridge) QMetaObject::invokeMethod(bridge, [target = bridge] {
        emit target->serviceUnavailable();
    }, Qt::QueuedConnection);
}

extern "C" JNIEXPORT void JNICALL Java_idv_coolshou_DuktoActivity_nativeStorageChanged(JNIEnv *, jclass)
{
    QMutexLocker lock(&bridgeMutex);
    if (bridge) QMetaObject::invokeMethod(bridge, [target = bridge] {
        emit target->storageChanged();
    }, Qt::QueuedConnection);
}

extern "C" JNIEXPORT void JNICALL Java_idv_coolshou_DuktoActivity_nativeKeyboardTop(JNIEnv *, jclass, jint screenPixels)
{
    QMutexLocker lock(&bridgeMutex);
    if (bridge) QMetaObject::invokeMethod(bridge, [target = bridge, screenPixels] {
        emit target->keyboardTopChanged(screenPixels);
    }, Qt::QueuedConnection);
}

AndroidShareBridge::~AndroidShareBridge()
{
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoActivity", "clearNativeReady", "()V");
    QMutexLocker lock(&bridgeMutex);
    if (bridge == this) bridge = nullptr;
}

void AndroidShareBridge::ready()
{
    {
        QMutexLocker lock(&bridgeMutex);
        bridge = this;
    }
    QJniObject::callStaticMethod<void>("idv/coolshou/DuktoActivity", "setNativeReady", "()V");
}
#else
AndroidShareBridge::~AndroidShareBridge() = default;
void AndroidShareBridge::ready() {}
#endif
