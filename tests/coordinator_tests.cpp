#include <QtTest>
#include <QQmlApplicationEngine>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QPointer>
#include <QQuickItem>
#include <QClipboard>
#include "guibehind.h"

class CoordinatorTest : public QObject
{
    Q_OBJECT
    QTemporaryDir mSettings;
    QString mOriginalPath;

    GuiBehind *create(QQmlApplicationEngine &engine, quint16 port = 0)
    {
        auto *backend = new GuiBehind(engine, &engine, port);
        backend->mShowBackTimer->stop();
        backend->mPeriodicHelloTimer->stop();
        delete backend->mUpdatesChecker;
        backend->mUpdatesChecker = nullptr;
        return backend;
    }

    void addPeer(GuiBehind *backend, const QString &ip, quint16 port, const QString &name)
    {
        backend->mBuddiesList.addBuddy(ip, qint16(port), name, name, "Linux", QUrl());
    }

private slots:
    void initTestCase()
    {
#ifdef Q_OS_WIN
        QSKIP("Coordinator tests use isolated native settings files; avoid the user's registry.");
#endif
        QVERIFY(mSettings.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, mSettings.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, mSettings.path());
        Settings settings;
        settings.savePath(mSettings.path());
        settings.saveConfirmBeforeReceive(false);
        mOriginalPath = QDir::currentPath();
    }

    void cleanupTestCase() { QDir::setCurrent(mOriginalPath); }

    void fileManagerShareShowsRecipientAndCanBeCancelled()
    {
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->setShowTermsOnStart(false);
        engine.load(QUrl::fromLocalFile(QFINDTESTDATA("../src/qml/dukto6/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().first();
        QVERIFY(QMetaObject::invokeMethod(root, "handleOverlayState", Q_ARG(QVariant, QVariant("settings"))));
        backend->stageShare({"/tmp/first selected.txt", "/tmp/second.png"}, {});
        QTRY_COMPARE(backend->overlayState(), QString());
        auto *panel = root->findChild<QQuickItem *>("pendingSharePanel");
        QTRY_VERIFY(panel && panel->isVisible());
        QCOMPARE(backend->pendingShareCount(), 1);
        QVERIFY(backend->pendingShareSummary().contains("first selected.txt"));
        QVERIFY(!backend->mTransfers.hasWork());
        backend->stageShare({"/tmp/third.txt"}, {});
        QCOMPARE(backend->pendingShareCount(), 2);
        auto *cancel = root->findChild<QObject *>("cancelSharedSelection");
        QVERIFY(cancel); QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
        QCOMPARE(backend->pendingShareCount(), 1);
        QVERIFY(backend->pendingShareSummary().contains("third.txt"));
        backend->discardPendingShare();
        QTRY_VERIFY(!panel->isVisible());
        QVERIFY(!backend->mTransfers.hasWork());
    }

    void stagedSharesAreBoundedAndDoNotReplaceActiveProgressOrTerms()
    {
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->setShowTermsOnStart(false);
        QSignalSpy choose(backend, &GuiBehind::gotoShareRecipient);
        backend->mTransfers.setPaused(true);
        backend->mTransfers.enqueue(TransferRequest());
        backend->setOverlayState("progress");
        backend->stageShare({"/tmp/waiting"}, {});
        QCOMPARE(choose.count(), 0); QCOMPARE(backend->overlayState(), QString("progress"));
        backend->mTransfers.clear();
        backend->setShowTermsOnStart(true);
        backend->stageShare({}, "Wait for terms");
        QCOMPARE(choose.count(), 0);
        backend->setShowTermsOnStart(false);
        while (backend->pendingShareCount() < 20) backend->stageShare({"/tmp/item"}, {});
        backend->stageShare({"/tmp/too-many"}, {});
        QCOMPARE(backend->pendingShareCount(), 20);
        QVERIFY(backend->messagePageText().contains("Too many"));
    }

    void settingsControlsFollowThemeAndAlignment()
    {
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->setShowTermsOnStart(false);
        engine.load(QUrl::fromLocalFile(QFINDTESTDATA("../src/qml/dukto6/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().first();
        QVERIFY(QMetaObject::invokeMethod(root, "handleOverlayState", Q_ARG(QVariant, QVariant("settings"))));
        auto *label = root->findChild<QQuickItem *>("confirmReceiveLabel");
        auto *toggle = root->findChild<QQuickItem *>("confirmReceiveSwitch");
        auto *comboText = root->findChild<QObject *>("themeModeText");
        QVERIFY(label && toggle && comboText);
        QTRY_VERIFY(label->isVisible());
        QTRY_VERIFY(qAbs(label->y() + label->height() / 2 - toggle->y() - toggle->height() / 2) < 1);
        for (const auto &mode : {QString("dark"), QString("light")}) {
            backend->setThemeMode(mode);
            QTRY_COMPARE(comboText->property("color").value<QColor>(), QColor(backend->mTheme.color4()));
        }
        auto *combo = root->findChild<QObject *>("themeModeSelector");
        auto *popup = combo->property("popup").value<QObject *>();
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        QVERIFY(popup->property("height").toReal() >= 3 * 44);
        auto *background = popup->property("background").value<QObject *>();
        QVERIFY(background);
        QCOMPARE(background->property("color").value<QColor>(), QColor(backend->mTheme.color8()));
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        backend->setThemeMode("system");
    }

    void textEditorSelectionAndKeyboardSpace()
    {
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->setShowTermsOnStart(false);
        engine.load(QUrl::fromLocalFile(QFINDTESTDATA("../src/qml/dukto6/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().first();
        backend->setTextSnippetSending(true);
        backend->setTextSnippet("alpha beta gamma");
        emit backend->gotoTextSnippet();
        auto *page = root->findChild<QQuickItem *>("textSnippetPage");
        auto *editor = root->findChild<QQuickItem *>("snippetEditor");
        auto *bar = root->findChild<QQuickItem *>("snippetActionBar");
        QVERIFY(page && editor && bar);
        QTRY_VERIFY(page->isVisible());
        QVERIFY(QMetaObject::invokeMethod(editor, "select", Q_ARG(int, 6), Q_ARG(int, 10)));
        QCOMPARE(editor->property("selectedText").toString(), QString("beta"));
        QVERIFY(QMetaObject::invokeMethod(editor, "cut"));
        QCOMPARE(editor->property("text").toString(), QString("alpha  gamma"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("beta"));
        QVERIFY(QMetaObject::invokeMethod(editor, "paste"));
        QCOMPARE(editor->property("text").toString(), QString("alpha beta gamma"));
        auto *menu = root->findChild<QObject *>("snippetEditingMenu");
        QVERIFY(menu);
        QVERIFY(QMetaObject::invokeMethod(page, "openEditingMenu"));
        QTRY_VERIFY(menu->property("visible").toBool());
        QCOMPARE(menu->property("width").toReal(), 220.0);
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        // Native Android coordinates are physical screen pixels, while the QML
        // page uses logical coordinates and may be offset from the screen origin.
        const qreal keyboardTop = page->mapToGlobal(QPointF(0, page->height() - 240)).y();
        backend->mAndroidKeyboardTopPixels = qRound(keyboardTop * QGuiApplication::primaryScreen()->devicePixelRatio());
        emit backend->androidKeyboardChanged();
        QTRY_VERIFY(bar->mapToItem(page, QPointF(0, bar->height())).y() <= page->height() - 240);
        backend->setTextSnippetSending(false);
        QVERIFY(editor->property("readOnly").toBool());
        QVERIFY(QMetaObject::invokeMethod(editor, "select", Q_ARG(int, 6), Q_ARG(int, 10)));
        QVERIFY(QMetaObject::invokeMethod(editor, "copy"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("beta"));
        QVERIFY(QMetaObject::invokeMethod(editor, "cut"));
        QCOMPARE(editor->property("text").toString(), QString("alpha beta gamma"));
    }

    void confirmationReachesVisibleInterface_data()
    {
        QTest::addColumn<bool>("text");
        QTest::addColumn<bool>("accept");
        QTest::newRow("accept-file") << false << true;
        QTest::newRow("accept-text") << true << true;
        QTest::newRow("reject-file") << false << false;
    }

    void confirmationReachesVisibleInterface()
    {
        QFETCH(bool, text);
        QFETCH(bool, accept);
        Settings settings;
        settings.saveConfirmBeforeReceive(true);
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.serverPort();
        reservation.close();
        QQmlApplicationEngine engine;
        auto *backend = create(engine, port);
        engine.load(QUrl::fromLocalFile(QFINDTESTDATA("../src/qml/dukto6/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QSignalSpy completed(backend, &GuiBehind::receiveCompleted);

        QTcpSocket sender;
        sender.connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(sender.state(), QAbstractSocket::ConnectedState);
        const QByteArray content("Confirmation regression");
        QByteArray wire;
        auto number = [&wire](qint64 value) { wire.append(reinterpret_cast<const char *>(&value), sizeof(value)); };
        number(1); number(content.size());
        wire.append(text ? "___DUKTO___TEXT___" : "confirmation-regression.txt");
        wire.append('\0'); number(content.size()); wire.append(content);
        sender.write(wire);
        sender.disconnectFromHost();

        auto *confirmation = engine.rootObjects().first()->findChild<QQuickItem *>("receiveConfirmation");
        QVERIFY(confirmation);
        // Check the actual page: changing the backend property alone does not show it.
        QTRY_VERIFY(confirmation->isVisible());
        QTRY_COMPARE(backend->overlayState(), QString("confirmreceive"));
        QCOMPARE(completed.count(), 0);
        QVERIFY(!backend->pendingIncomingSender().isEmpty());
        if (accept) {
            backend->acceptIncomingTransfer();
            QTRY_COMPARE(completed.count(), 1);
            QTRY_COMPARE(backend->overlayState(), QString());
            QTRY_VERIFY(!confirmation->isVisible());
            if (text) {
                QCOMPARE(backend->mRecentList.item(0)->data(RecentListItemModel::Value).toString(), QString::fromUtf8(content));
            } else {
                QFile file(mSettings.filePath("confirmation-regression.txt"));
                auto *recent = backend->mRecentList.item(0);
                QCOMPARE(recent->data(RecentListItemModel::Name).toString(), QString("confirmation-regression.txt"));
                QCOMPARE(recent->data(RecentListItemModel::Value).toString(), file.fileName());
                QCOMPARE(recent->data(RecentListItemModel::Size).toString(), QString::number(content.size()) + " B");
                QVERIFY(file.open(QIODevice::ReadOnly));
                QCOMPARE(file.readAll(), content);
                file.close();
                QVERIFY(file.remove());
            }
        } else {
            backend->rejectIncomingTransfer();
            QTRY_COMPARE(backend->overlayState(), QString());
            QTRY_VERIFY(!confirmation->isVisible());
            QTRY_VERIFY(!backend->mDuktoProtocol->isBusy());
            QCOMPARE(completed.count(), 0);
            QVERIFY(!QFile::exists(mSettings.filePath("confirmation-regression.txt")));
        }
        settings.saveConfirmBeforeReceive(false);
    }

    void existingSendApiCapturesQueuedRecipient()
    {
        QTcpServer first, second;
        QVERIFY(first.listen(QHostAddress::LocalHost, 0));
        QVERIFY(second.listen(QHostAddress::LocalHost, 0));
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->mTransfers.setPaused(true);
        addPeer(backend, "127.0.0.1", first.serverPort(), "A");
        addPeer(backend, "::ffff:127.0.0.1", second.serverPort(), "B");
        backend->showSendPage("127.0.0.1"); backend->startTransfer(QString("First message"));
        backend->showSendPage("::ffff:127.0.0.1"); backend->startTransfer(QString("Second message"));
        backend->showSendPage("IP"); backend->setRemoteDestinationAddress("192.0.2.99:1234");
        backend->mTransfers.setPaused(false);
        QTRY_VERIFY(first.hasPendingConnections());
        auto *socketA = first.nextPendingConnection();
        QByteArray bytesA;
        connect(socketA, &QTcpSocket::readyRead, this, [&] { bytesA += socketA->readAll(); });
        bytesA += socketA->readAll();
        QTRY_VERIFY(second.hasPendingConnections());
        auto *socketB = second.nextPendingConnection();
        QByteArray bytesB;
        connect(socketB, &QTcpSocket::readyRead, this, [&] { bytesB += socketB->readAll(); });
        bytesB += socketB->readAll();
        QTRY_VERIFY(bytesA.endsWith("First message"));
        QTRY_VERIFY(bytesB.endsWith("Second message"));
        QTRY_VERIFY(!backend->mTransfers.hasWork());
        socketA->disconnect(this); socketB->disconnect(this);
    }

    void sharedContentWaitsForExplicitRecipient()
    {
        QTcpServer peer;
        QVERIFY(peer.listen(QHostAddress::LocalHost, 0));
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        addPeer(backend, "127.0.0.1", peer.serverPort(), "A");
        backend->showSendPage("127.0.0.1");
        backend->stageShare({}, "Shared message");
        QVERIFY(backend->hasPendingShare());
        QVERIFY(!backend->mTransfers.hasWork());
        QSignalSpy completed(backend->mDuktoProtocol, &DuktoProtocol::sendFileComplete);
        backend->showSendPage("127.0.0.1");
        QVERIFY(!backend->hasPendingShare());
        QTRY_VERIFY(peer.hasPendingConnections());
        QTRY_COMPARE(completed.count(), 1);
    }

    void manualAddressKeepsSharedContentUntilSubmission()
    {
        QTcpServer peer;
        QVERIFY(peer.listen(QHostAddress::LocalHost, 0));
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        backend->stageShare({}, "Manual share");
        backend->showSendPage("IP");
        backend->setRemoteDestinationAddress("127.0.0.1:" + QString::number(peer.serverPort()));
        QVERIFY(backend->hasPendingShare());
        QVERIFY(!backend->mTransfers.hasWork());
        backend->sendPendingShare();
        QVERIFY(!backend->hasPendingShare());
        QTRY_VERIFY(peer.hasPendingConnections());
    }

    void workerShutdownCancelsActiveTransfer()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath("large.bin"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(64 * 1024 * 1024));
        file.close();
        QTcpServer peer;
        QVERIFY(peer.listen(QHostAddress::LocalHost, 0));
        QQmlApplicationEngine engine;
        auto *backend = create(engine);
        addPeer(backend, "127.0.0.1", peer.serverPort(), "A");
        backend->showSendPage("127.0.0.1");
        backend->sendSomeFiles({directory.filePath("large.bin")});
        QTRY_VERIFY(peer.hasPendingConnections());
        auto *socket = peer.nextPendingConnection();
        socket->setReadBufferSize(1);
        QElapsedTimer clock; clock.start();
        QPointer<DuktoProtocol> protocol(backend->mDuktoProtocol);
        delete backend;
        QVERIFY(clock.elapsed() < 2000);
        QVERIFY(protocol.isNull());
    }

    void themePreferenceSurvivesRestart()
    {
        Settings settings;
        settings.saveThemeMode("system");
        {
            QQmlApplicationEngine engine;
            auto *backend = create(engine);
            QCOMPARE(backend->themeMode(), QString("system"));
            backend->setThemeMode("dark");
            QCOMPARE(settings.themeMode(), QString("dark"));
        }
        {
            QQmlApplicationEngine engine;
            auto *backend = create(engine);
            QCOMPARE(backend->themeMode(), QString("dark"));
            QVERIFY(backend->mTheme.dark());
            backend->setThemeMode("light");
            QVERIFY(!backend->mTheme.dark());
            backend->setThemeMode("system");
        }
    }
};

QTEST_MAIN(CoordinatorTest)
#include "coordinator_tests.moc"
