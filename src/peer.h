#ifndef PEER_H
#define PEER_H

#include <QtNetwork/QHostInfo>

class Peer
{
public:
    Peer() { }
    inline Peer(QHostAddress a, QString n, qint16 p) { address = a; name = n; port = p; }
    QHostAddress address;
    QString name;
    qint16 port = 4644;
};

Q_DECLARE_METATYPE(Peer)

#endif // PEER_H
