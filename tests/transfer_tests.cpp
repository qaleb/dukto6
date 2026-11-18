#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <limits>
#include <QSettings>
#include "transferqueue.h"
#include "recentlistitemmodel.h"
#include <QStorageInfo>
#include "duktoprotocol.h"
#include "transfersource.h"
#include "settings.h"

namespace {
void number(QByteArray &out,qint64 value) { out.append(reinterpret_cast<const char *>(&value),8); }
QByteArray packet(const QByteArray &name,const QByteArray &data,qint64 declared = -2) {
    QByteArray out; number(out,1); number(out,data.size()); out.append(name); out.append('\0');
    number(out,declared == -2 ? data.size() : declared); out.append(data); return out;
}
quint16 unusedPort() { QTcpServer server; server.listen(QHostAddress::LocalHost,0); return server.serverPort(); }
void writeFile(const QString &path,const QByteArray &data) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(data),data.size()); }
}
class TransfersTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("DuktoTests"); QCoreApplication::setApplicationName("Transfers");
        QVERIFY(settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,settingsDirectory.path());
        QSettings::setPath(QSettings::NativeFormat,QSettings::SystemScope,settingsDirectory.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
    }


    void receive_data() {
        QTest::addColumn<QByteArray>("name"); QTest::addColumn<QByteArray>("content");
        QTest::newRow("empty") << QByteArray("empty.txt") << QByteArray();
        QTest::newRow("unicode") << QString::fromUtf8("résumé-文件.txt").toUtf8() << QByteArray("hello\0world",11);
        QTest::newRow("large") << QByteArray("large.bin") << QByteArray(4*1024*1024,'x');
        QTest::newRow("empty-text") << QByteArray("___DUKTO___TEXT___") << QByteArray();
        QTest::newRow("text") << QByteArray("___DUKTO___TEXT___") << QString::fromUtf8("hello 世界").toUtf8();
    }
    void receive() {
        QFETCH(QByteArray,name); QFETCH(QByteArray,content);
        QTemporaryDir directory;
        DuktoProtocol receiver; receiver.setPorts(0,qint16(unusedPort()));
        // Reserve a fresh TCP port through the public configuration.
        const quint16 port=unusedPort(); receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy files(&receiver,&DuktoProtocol::receiveFileComplete), text(&receiver,&DuktoProtocol::receiveTextComplete);
        QSignalSpy failures(&receiver,&DuktoProtocol::receiveFileCancelled);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        QByteArray bytes=packet(name,content);
        // Fragment every fixed header boundary, including before any complete integer exists.
        for (int i=0;i<qMin(40,bytes.size());++i) { sender.write(bytes.mid(i,1)); sender.flush(); QTest::qWait(1); }
        sender.write(bytes.mid(qMin(40,bytes.size()))); sender.disconnectFromHost();
        QTRY_COMPARE(files.count()+text.count()+failures.count(),1);
        QCOMPARE(failures.count(),0);
        if (name == "___DUKTO___TEXT___") { QCOMPARE(text.count(),1); QCOMPARE(text[0][0].toString().toUtf8(),content); }
        else { QFile file(directory.filePath(QString::fromUtf8(name))); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),content); }
        QVERIFY(!receiver.isBusy());
    }
    void rejectsUnsafeAndMalformed_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("traversal") << packet("../outside", "bad");
        QTest::newRow("absolute") << packet("/tmp/outside", "bad");
        QTest::newRow("windows-path") << packet("C:\\outside", "bad");
        QTest::newRow("negative-length") << packet("file", "bad", -3);
        QTest::newRow("oversized-text") << packet("___DUKTO___TEXT___", "bad", 17*1024*1024);
        QTest::newRow("truncated") << packet("file", "bad").chopped(1);
        QTest::newRow("extra-data") << (packet("file", "bad")+"unexpected");
        QByteArray invalid; number(invalid,-1); number(invalid,0);
        QTest::newRow("negative-count") << invalid;
        QTest::newRow("partial-header") << QByteArray(3,'\0');
    }
    void rejectsUnsafeAndMalformed() {
        QFETCH(QByteArray,bytes); QTemporaryDir directory;
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy failure(&receiver,&DuktoProtocol::receiveFileCancelled),success(&receiver,&DuktoProtocol::receiveFileComplete);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(bytes); sender.disconnectFromHost();
        QTRY_COMPARE(failure.count(),1); QCOMPARE(success.count(),0); QVERIFY(!receiver.isBusy());
        QVERIFY(QDir(directory.path()).entryList({"*.XXXXXX"},QDir::Files).isEmpty());
    }
    void symlinkEscape() {
#ifdef Q_OS_WIN
        QSKIP("Requires Unix directory symlinks; Windows junctions need a native test.");
#endif
        QTemporaryDir directory,outside;
        if (!QFile::link(outside.path(),directory.filePath("escape"))) QSKIP("Symlinks unavailable");
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy failure(&receiver,&DuktoProtocol::receiveFileCancelled);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(packet("escape/file","bad")); sender.disconnectFromHost();
        QTRY_COMPARE(failure.count(),1); QVERIFY(!QFile::exists(outside.filePath("file")));
    }
    void sendsLegacyWireFormatAndIndependentRoots() {
        QTemporaryDir directory;
        QDir().mkpath(directory.filePath("a")); QDir().mkpath(directory.filePath("b"));
        writeFile(directory.filePath("a/same.txt"),"one"); writeFile(directory.filePath("b/same.txt"),"two");
        QTcpServer peer; QVERIFY(peer.listen(QHostAddress::LocalHost,0));
        DuktoProtocol sender; sender.initialize();
        TransferRequest request; request.host="127.0.0.1"; request.port=peer.serverPort();
        request.sources={directory.filePath("a/same.txt"),directory.filePath("b/same.txt")};
        QSignalSpy complete(&sender,&DuktoProtocol::sendFileComplete);
        sender.sendRequest(request);
        QTRY_VERIFY(peer.hasPendingConnections()); auto *socket=peer.nextPendingConnection();
        QByteArray wire;
        connect(socket,&QTcpSocket::readyRead,this,[&] { wire+=socket->readAll(); });
        wire+=socket->readAll();
        QTRY_COMPARE(complete.count(),1);
        QTRY_VERIFY(wire.endsWith("two"));
        QByteArray expected; number(expected,2); number(expected,6);
        expected+="same.txt"; expected+='\0'; number(expected,3); expected+="one";
        expected+="2-same.txt"; expected+='\0'; number(expected,3); expected+="two";
        QCOMPARE(wire,expected);
    }
    void roundTripDirectoriesAndText() {
        QTemporaryDir source,destination;
        QDir().mkpath(source.filePath("folder/empty-dir"));
        writeFile(source.filePath("folder/zero"),{});
        writeFile(source.filePath("folder/data"),"nested");
        DuktoProtocol receiver,sender;
        const auto port=unusedPort(); receiver.setPorts(0,qint16(port)); receiver.setReceivePath(destination.path()); receiver.initialize();
        sender.setPorts(0,0); sender.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy files(&receiver,&DuktoProtocol::receiveFileComplete),text(&receiver,&DuktoProtocol::receiveTextComplete);
        TransferRequest request; request.host="127.0.0.1"; request.port=port; request.sources={source.filePath("folder")};
        sender.sendRequest(request);
        QTRY_COMPARE(files.count(),1);
        QVERIFY(QFileInfo(destination.filePath("folder/empty-dir")).isDir());
        QCOMPARE(QFileInfo(destination.filePath("folder/zero")).size(),0);
        QFile content(destination.filePath("folder/data")); QVERIFY(content.open(QIODevice::ReadOnly)); QCOMPARE(content.readAll(),QByteArray("nested"));
        request.isText=true; request.text=QString::fromUtf8("Unicode 世界"); request.sources.clear();
        sender.sendRequest(request); QTRY_COMPARE(text.count(),1); QCOMPARE(text[0][0].toString(),request.text);
    }
    void tinyTransferRemainsAvailableUntilAccepted() {
        QTemporaryDir directory; const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        QSignalSpy requests(&receiver,&DuktoProtocol::incomingTransferRequested),files(&receiver,&DuktoProtocol::receiveFileComplete);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(packet("small.txt","buffered")); sender.disconnectFromHost();
        QTRY_COMPARE(requests.count(),1); QTest::qWait(100);
        QVERIFY(receiver.isBusy()); QCOMPARE(files.count(),0);
        receiver.acceptIncomingTransfer(); QTRY_COMPARE(files.count(),1);
        QFile file(directory.filePath("small.txt")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("buffered"));
    }
    void pendingReceiveCannotBeOverwritten() {
        QTemporaryDir directory; const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        QSignalSpy requests(&receiver,&DuktoProtocol::incomingTransferRequested);
        QTcpSocket first,second;
        first.connectToHost(QHostAddress::LocalHost,port); QVERIFY(first.waitForConnected()); QTRY_COMPARE(requests.count(),1);
        second.connectToHost(QHostAddress::LocalHost,port); QVERIFY(second.waitForConnected());
        QTest::qWait(50); QCOMPARE(requests.count(),1); QVERIFY(receiver.isBusy());
        receiver.rejectIncomingTransfer(); QVERIFY(!receiver.isBusy());
    }


    void missingSourcesAndCancellation() {
        std::atomic_bool cancelled{false}; QString error;
        QVERIFY(prepareSources({"/does/not/exist"},&error,cancelled).isEmpty()); QVERIFY(!error.isEmpty());
        QTemporaryDir dir; writeFile(dir.filePath("file"),"data"); error.clear(); cancelled=true;
        QVERIFY(prepareSources({dir.filePath("file")},&error,cancelled).isEmpty()); QVERIFY(!error.isEmpty());
    }
    void receiveDefaultPreservesExplicitPreference() {
#ifdef Q_OS_WIN
        QSKIP("Do not modify native user registry settings.");
#endif
        QSettings backing("dukto","Dukto"); backing.remove("R6/ConfirmBeforeReceive");
        Settings settings; QVERIFY(!settings.confirmBeforeReceive());
        settings.saveConfirmBeforeReceive(false); QVERIFY(!settings.confirmBeforeReceive());
        backing.remove("R6/ConfirmBeforeReceive");
    }

    void queueCapturesRecipientsAcrossRetry() {
        TransferQueue queue;
        QSignalSpy started(&queue,&TransferQueue::startRequested);
        TransferRequest first; first.host="192.0.2.1"; first.recipient="A";
        TransferRequest second; second.host="192.0.2.2"; second.recipient="B";
        queue.enqueue(first); queue.enqueue(second);
        second.host="192.0.2.99";
        QCOMPARE(started.count(),1);
        queue.finish(); QTRY_COMPARE(started.count(),2);
        QCOMPARE(qvariant_cast<TransferRequest>(started[1][0]).host,QString("192.0.2.2"));
        queue.finish(true); QTRY_COMPARE(started.count(),3);
        const auto retry=qvariant_cast<TransferRequest>(started[2][0]);
        QCOMPARE(retry.host,QString("192.0.2.2")); QCOMPARE(retry.attempts,2);
        queue.finish(true); QTRY_COMPARE(started.count(),4);
        queue.finish(true); QTest::qWait(10);
        QCOMPARE(started.count(),4); QVERIFY(!queue.hasWork());
    }
    void queueReservesPendingReceivesAndForegroundPause() {
        TransferQueue queue; QSignalSpy started(&queue,&TransferQueue::startRequested);
        queue.setReceiving(true); queue.enqueue(TransferRequest());
        QCOMPARE(started.count(),0);
        queue.setPaused(true); queue.setReceiving(false); QTest::qWait(10);
        QCOMPARE(started.count(),0);
        queue.setPaused(false); QTRY_COMPARE(started.count(),1);
    }
    void queueRetainsTemporaryInputUntilFinalAttempt() {
        TransferQueue queue;
        QString path;
        { TransferRequest request; request.temporary=std::make_shared<QTemporaryDir>();
          path=request.temporary->path(); queue.enqueue(request); }
        QVERIFY(QDir(path).exists()); queue.finish(true);
        QTRY_VERIFY(queue.hasActive()); QVERIFY(QDir(path).exists());
        queue.finish(); QVERIFY(!QDir(path).exists());
    }
    void clearingQueueReleasesTemporaryInput() {
        TransferQueue queue;
        QString firstPath,secondPath;
        { TransferRequest first,second;
          first.temporary=std::make_shared<QTemporaryDir>(); firstPath=first.temporary->path();
          second.temporary=std::make_shared<QTemporaryDir>(); secondPath=second.temporary->path();
          queue.enqueue(first); queue.enqueue(second); }
        queue.clear(); QVERIFY(!QDir(firstPath).exists()); QVERIFY(!QDir(secondPath).exists());
    }
    void historyIsBounded() {
        RecentListItemModel history;
        for(int i=0;i<120;++i) history.addRecent("Text",QString::number(i),"text","peer",1);
        QCOMPARE(history.rowCount(),100);
        QCOMPARE(history.item(0)->data(RecentListItemModel::Value).toString(),QString("119"));
        const QString text(9*1024*1024,'x');
        history.addRecent("Text",text,"text","peer",text.size());
        history.addRecent("Text",text,"text","peer",text.size());
        QCOMPARE(history.rowCount(),1);
    }
    void refusesInsufficientStorageBeforeWriting() {
        QTemporaryDir directory;
        const qint64 available=QStorageInfo(directory.path()).bytesAvailable();
        if(available<0 || available==std::numeric_limits<qint64>::max()) QSKIP("No storage statistics");
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy failure(&receiver,&DuktoProtocol::receiveFileCancelled);
        QByteArray header; number(header,1); number(header,available+1); header+="huge.bin"; header+='\0'; number(header,available+1);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(header); sender.flush(); QTRY_COMPARE(failure.count(),1);
        QVERIFY(!QFileInfo::exists(directory.filePath("huge.bin")));
    }
    void existingFileIsNotOverwritten() {
        QTemporaryDir directory; writeFile(directory.filePath("same.txt"),"original");
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy success(&receiver,&DuktoProtocol::receiveFileComplete);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(packet("same.txt","new")); sender.disconnectFromHost(); QTRY_COMPARE(success.count(),1);
        QFile original(directory.filePath("same.txt")); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),QByteArray("original"));
        QFile received(directory.filePath("same (2).txt")); QVERIFY(received.open(QIODevice::ReadOnly)); QCOMPARE(received.readAll(),QByteArray("new"));
    }
    void lateCancellationCannotReachNextJob() {
        DuktoProtocol sender; sender.setPorts(0,0); sender.initialize();
        sender.abortCurrentTransfer(); // No active job: must not poison a later send.
        TransferRequest cancelled; cancelled.isText=true; cancelled.text="old";
        cancelled.cancellation->store(true);
        QSignalSpy aborted(&sender,&DuktoProtocol::sendFileAborted);
        sender.sendRequest(cancelled); QCOMPARE(aborted.count(),1);
        QTcpServer peer; QVERIFY(peer.listen(QHostAddress::LocalHost,0));
        TransferRequest next; next.host="127.0.0.1"; next.port=peer.serverPort();
        next.isText=true; next.text="next";
        QSignalSpy completed(&sender,&DuktoProtocol::sendFileComplete);
        sender.sendRequest(next);
        sender.abortRequest(cancelled.id);
        cancelled.cancellation->store(true);
        QTRY_COMPARE(completed.count(),1);
        QVERIFY(!next.cancellation->load());
    }
    void atomicSinkDoesNotExposePartialFiles() {
        QTemporaryDir directory;
        const QString path=directory.filePath("photo.jpg");
        QString error;
        {
            auto output=ReceivedFile::local(path,&error); QVERIFY2(output,qPrintable(error));
            QCOMPARE(output->write("partial",7),qint64(7));
            QVERIFY(!QFileInfo::exists(path));
        }
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(QDir(directory.path()).entryList(QDir::Files|QDir::Hidden).isEmpty());
        auto output=ReceivedFile::local(path,&error); QVERIFY2(output,qPrintable(error));
        QCOMPARE(output->write("complete",8),qint64(8));
        QVERIFY(output->commit());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("complete"));
    }
    void incompleteFileIsNeverPublished() {
        QTemporaryDir directory;
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(directory.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy published(&receiver,&DuktoProtocol::receivedFilePublished),failed(&receiver,&DuktoProtocol::receiveFileCancelled);
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(packet("photo.jpg","incomplete").chopped(1)); sender.disconnectFromHost();
        QTRY_COMPARE(failed.count(),1); QCOMPARE(published.count(),0);
        QVERIFY(!QFileInfo::exists(directory.filePath("photo.jpg")));
    }
    void receiveFolderIsCapturedUntilBatchCompletes() {
        QTemporaryDir first,second;
        const auto port=unusedPort(); DuktoProtocol receiver;
        receiver.setPorts(0,qint16(port)); receiver.setReceivePath(first.path()); receiver.initialize();
        connect(&receiver,&DuktoProtocol::incomingTransferRequested,&receiver,&DuktoProtocol::acceptIncomingTransfer);
        QSignalSpy complete(&receiver,&DuktoProtocol::receiveFileComplete);
        QByteArray header; number(header,2); number(header,6);
        header+="a.txt"; header+='\0'; number(header,3); header+="one";
        QTcpSocket sender; sender.connectToHost(QHostAddress::LocalHost,port); QVERIFY(sender.waitForConnected());
        sender.write(header); sender.flush();
        QTRY_VERIFY(QFileInfo::exists(first.filePath("a.txt")));
        receiver.setReceivePath(second.path());
        QByteArray tail("b.txt"); tail+='\0'; number(tail,3); tail+="two";
        sender.write(tail); sender.disconnectFromHost(); QTRY_COMPARE(complete.count(),1);
        QVERIFY(QFileInfo::exists(first.filePath("b.txt")));
        QVERIFY(!QFileInfo::exists(second.filePath("b.txt")));
    }
};
QTEST_GUILESS_MAIN(TransfersTest)
#include "transfer_tests.moc"
