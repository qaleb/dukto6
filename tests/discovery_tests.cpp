#include <QtTest>
#include <QNetworkInterface>
#include <QSettings>
#include <QTemporaryDir>

#include "duktoprotocol.h"
#include "settings.h"

class DiscoveryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir mSettingsDirectory;
    const QList<QHostAddress> mLocalAddresses {
        QHostAddress("127.0.0.1"), QHostAddress("::1"),
        QHostAddress("192.168.8.38"), QHostAddress("172.17.0.1"),
        QHostAddress("172.19.0.1"), QHostAddress("172.20.0.1"),
        QHostAddress("fe80::1234%eth0")
    };

    void deliver(DuktoProtocol &protocol, QByteArray packet, const QString &source,
                 const QList<QHostAddress> &localAddresses)
    {
        QHostAddress sender(source);
        protocol.handleMessage(packet, sender, localAddresses);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(mSettingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           mSettingsDirectory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                           mSettingsDirectory.path());
        // Settings explicitly names the application, so it uses NativeFormat.
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                           mSettingsDirectory.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope,
                           mSettingsDirectory.path());
    }

    void ignoresLocalAddresses_data()
    {
        QTest::addColumn<QString>("source");
        const QStringList sources {
            "127.0.0.1", "127.2.3.4", "::1", "::ffff:127.0.0.1", "::ffff:127.2.3.4",
            "192.168.8.38", "::ffff:192.168.8.38", "172.17.0.1", "::ffff:172.17.0.1",
            "172.19.0.1", "::ffff:172.19.0.1", "172.20.0.1", "::ffff:172.20.0.1",
            "fe80::1234%eth0"
        };
        for (const auto &source : sources)
            QTest::newRow(qPrintable(source)) << source;
    }

    void ignoresLocalAddresses()
    {
        QFETCH(QString, source);
        DuktoProtocol protocol;
        QSignalSpy added(&protocol, &DuktoProtocol::peerListAdded);
        QSignalSpy removed(&protocol, &DuktoProtocol::peerListRemoved);
        // Deliberately differs from our name: another local process or an old name.
        deliver(protocol, QByteArray("\2Old name at host (Unknown)"), source, mLocalAddresses);
        deliver(protocol, QByteArray("\3Bye Bye"), source, mLocalAddresses);
        QCOMPARE(added.count(), 0);
        QCOMPARE(removed.count(), 0);
        QVERIFY(protocol.getPeers().isEmpty());
    }

    void ignoresCurrentInterfaceAddresses()
    {
        DuktoProtocol protocol;
        const auto addresses = QNetworkInterface::allAddresses();
        for (const auto &address : addresses)
            deliver(protocol, QByteArray("\2Different local name"), address.toString(), addresses);
        QVERIFY(protocol.getPeers().isEmpty());
    }

    void acceptsRemoteDeviceWithSameName()
    {
#ifdef Q_OS_WIN
        QSKIP("Native registry settings cannot be redirected; do not touch the user's settings.");
#endif
        DuktoProtocol protocol;
        const QByteArray packet = QByteArray(1, char(2)) + protocol.getSystemSignature().toUtf8();
        deliver(protocol, packet, "192.168.8.39", mLocalAddresses);
        deliver(protocol, packet, "::ffff:192.168.8.40", mLocalAddresses);
        QCOMPARE(protocol.getPeers().size(), 2);
        QCOMPARE(protocol.getPeers().value("192.168.8.39").name,
                 protocol.getSystemSignature());
    }

    void acceptsLegacyAndExtendedHello()
    {
        DuktoProtocol protocol;
        const QString name = QString::fromUtf8("Téléphone 文件 at phone (Android)");
        deliver(protocol, QByteArray(1, char(2)) + name.toUtf8(), "192.168.8.39", mLocalAddresses);
        QCOMPARE(protocol.getPeers().value("192.168.8.39").name, name);
        QCOMPARE(protocol.getPeers().value("192.168.8.39").port, qint16(4644));

        const quint16 port = 54321;
        QByteArray packet(1, char(5));
        packet.append(reinterpret_cast<const char *>(&port), sizeof(port));
        packet.append(name.toUtf8());
        deliver(protocol, packet, "192.168.8.39", mLocalAddresses);
        QCOMPARE(protocol.getPeers().size(), 1);
        QCOMPARE(quint16(protocol.getPeers().value("192.168.8.39").port), port);
    }

    void ignoresMalformedHello_data()
    {
        QTest::addColumn<QByteArray>("packet");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("empty-broadcast-name") << QByteArray(1, char(1));
        QTest::newRow("empty-unicast-name") << QByteArray(1, char(2));
        QTest::newRow("missing-port") << QByteArray(1, char(4));
        QTest::newRow("partial-port") << QByteArray("\4\1", 2);
        QTest::newRow("extended-empty-name") << QByteArray("\5\1\1", 3);
        QTest::newRow("zero-port") << QByteArray("\5\0\0name", 7);
        QTest::newRow("oversized-name") << (QByteArray(1, char(2)) + QByteArray(4097, 'x'));
        QTest::newRow("unknown-kind") << QByteArray("\7name");
    }

    void ignoresMalformedHello()
    {
        QFETCH(QByteArray, packet);
        DuktoProtocol protocol;
        QSignalSpy added(&protocol, &DuktoProtocol::peerListAdded);
        deliver(protocol, packet, "192.168.8.39", mLocalAddresses);
        QCOMPARE(added.count(), 0);
        QVERIFY(protocol.getPeers().isEmpty());
    }

    void goodbyeOnlyRemovesKnownPeer()
    {
        DuktoProtocol protocol;
        QSignalSpy removed(&protocol, &DuktoProtocol::peerListRemoved);
        deliver(protocol, QByteArray("\3Bye Bye"), "192.168.8.39", mLocalAddresses);
        QVERIFY(protocol.getPeers().isEmpty());
        QCOMPARE(removed.count(), 0);
        deliver(protocol, QByteArray("\2Phone at phone (Android)"), "192.168.8.39", mLocalAddresses);
        connect(&protocol, &DuktoProtocol::peerListRemoved, this, [](const Peer &peer) {
            QCOMPARE(peer.address, QHostAddress("192.168.8.39"));
            QCOMPARE(peer.name, QString("Phone at phone (Android)"));
        });
        deliver(protocol, QByteArray("\3Bye Bye"), "192.168.8.39", mLocalAddresses);
        deliver(protocol, QByteArray("\3Bye Bye"), "192.168.8.39", mLocalAddresses);
        QVERIFY(protocol.getPeers().isEmpty());
        QCOMPARE(removed.count(), 1);
    }

    void picksUpAddressChanges()
    {
        DuktoProtocol protocol;
        const QHostAddress newAddress("192.168.9.12");
        auto addresses = mLocalAddresses;
        addresses.append(newAddress);
        deliver(protocol, QByteArray("\2Renamed device"), "::ffff:192.168.9.12", addresses);
        QVERIFY(protocol.getPeers().isEmpty());
    }

    void signatureUsesUpdatedBuddyName()
    {
#ifdef Q_OS_WIN
        QSKIP("Native registry settings cannot be redirected; do not touch the user's settings.");
#endif
        Settings settings;
        settings.saveBuddyName("Before rename");
        DuktoProtocol protocol;
        QVERIFY(protocol.getSystemSignature().startsWith("Before rename at "));
        settings.saveBuddyName("After rename");
        QVERIFY(protocol.getSystemSignature().startsWith("After rename at "));
    }

    void drainsEmptyAndLocalDatagrams()
    {
        DuktoProtocol protocol;
        protocol.setPorts(0, 0);
        protocol.initialize();
        QVERIFY(protocol.mSocket->localPort() != 0);
        disconnect(protocol.mSocket, &QUdpSocket::readyRead,
                   &protocol, &DuktoProtocol::newUdpData);
        QUdpSocket sender;
        QCOMPARE(sender.writeDatagram(QByteArray(), QHostAddress::LocalHost,
                                       protocol.mSocket->localPort()), qint64(0));
        const QByteArray packet("\2Different local name");
        QCOMPARE(sender.writeDatagram(packet, QHostAddress::LocalHost,
                                       protocol.mSocket->localPort()), qint64(packet.size()));
        QTRY_VERIFY(protocol.mSocket->hasPendingDatagrams());
        protocol.newUdpData();
        QVERIFY(!protocol.mSocket->hasPendingDatagrams());
        QVERIFY(protocol.getPeers().isEmpty());
    }
};

QTEST_GUILESS_MAIN(DiscoveryTest)
#include "discovery_tests.moc"
