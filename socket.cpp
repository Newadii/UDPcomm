#include "socket.h"
#include <QNetworkDatagram>

Socket::Socket(QObject *parent) : QObject(parent)
  , m_udpSocket(new QUdpSocket(this))
{


    connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(on_readyRead()));
    connect(m_udpSocket, SIGNAL(connected()), this, SLOT(on_connected()));
    connect(m_udpSocket, SIGNAL(disconnected()), this, SLOT(on_disconnected()));
}

void Socket::bindSocket(const QString &portString)
{
    bool ok = false;
    uint port = portString.toUInt(&ok);
    if (!ok) {
        emit debugMessage("Can't convert your port to a number!");
        return;
    }
    if(m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        emit debugMessage("Stopped server to start with new port.");
        m_udpSocket->close();
    }
    if(m_udpSocket->bind(QHostAddress::Any, quint16(port))) {
        emit debugMessage("Server succesfully started!");
    } else {
        emit debugMessage("Couldn't bind selected port!");
    }
}

void Socket::closeSocket()
{
    m_udpSocket->close();
}

void Socket::connectToHost(const QString &ipString, const QString &portString)
{
    bool ok;
    uint port = portString.toUInt(&ok);
    if (!ok) {
        emit debugMessage("Couldn't convert their port to a number!");
        return;
    }
    QHostAddress ip;
    if(!ip.setAddress(ipString)) {
        emit debugMessage("Couldn't convert their ip address!");
        return;
    }
    if (m_udpSocket->state() == QAbstractSocket::ConnectedState) {
        emit debugMessage("Socket is connected, disconnect first!");
        return;
    }
    m_udpSocket->connectToHost(ip, quint16(port));
}

void Socket::disconnect()
{
    m_udpSocket->disconnectFromHost();
}

void Socket::setFragSize(uint fragSize)
{
    if (fragSize > 1446) {
        emit debugMessage("Maximum fragment size is 1446, fragment size not set!");
        return;
    }
    m_fragSize = fragSize;
    emit debugMessage("Fragment size set to: " + QString::number(m_fragSize));
}

void Socket::on_connected()
{
    emit debugMessage("Socket connected.");
}

void Socket::on_disconnected()
{
    emit debugMessage("Socket disconnected.");
}

void Socket::sendMessage(const QString &msg)
{
    QByteArray byteArr(msg.toLatin1());
    m_udpSocket->write(byteArr);
}

void Socket::on_readyRead()
{
    emit debugMessage("on_readyReady");
    QNetworkDatagram datagram(m_udpSocket->receiveDatagram());

    if (m_udpSocket->state() != QAbstractSocket::ConnectedState) {
        m_udpSocket->connectToHost(datagram.senderAddress(), quint16(datagram.senderPort()));
    }

    emit receivedMessage(datagram.data());
}
