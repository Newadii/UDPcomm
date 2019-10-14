#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QUdpSocket>

class Socket : public QObject
{
    Q_OBJECT
public:
    explicit Socket(QObject *parent = nullptr);

    void bindSocket(const QString &port);
    void closeSocket();
    void connectToHost(const QString &ip, const QString &port);
    void disconnect();

    void sendMessage(const QString &);
    void receiveMessage(const QString &);

    void setFragSize(uint);


private slots:
    void on_readyRead();
    void on_connected();
    void on_disconnected();

private:
    QUdpSocket *m_udpSocket;
    uint m_fragSize;

signals:
    void receivedMessage(const QString &);
    void debugMessage(const QString &);
};

#endif // SOCKET_H
