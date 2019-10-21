#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QFile>

class Socket : public QObject
{
    Q_OBJECT
public:
    explicit Socket(QObject *parent = nullptr);

    void bindSocket(const QString &port);
    void closeSocket();
    void connectToHost(const QString &ip, const QString &port);
    void disconnect();
    void corruptFrag(bool);

    void sendMessage(const QString &);
    void sendFile(const QString &);
    void receiveMessage(const QString &);

    void setFragSize(int);
    static QByteArray checksum(const QByteArray &);
    static QByteArray intToArray(quint32);
    static QByteArray intToArray(quint16);
    static quint32 arrToInt(const QByteArray &);
    static quint16 arrToCheck(const QByteArray &);

private slots:
    void on_readyRead();
    void on_connected();
    void on_disconnected();

    void on_connection_timeout();
    void on_handshake_timeout();
    void on_retryHandshake_timeout();
    void on_retrySyn_timeout();
    void on_retryInit_timeout();
    void on_retryData_timeout();
    void on_data_timeout();

protected:
    void on_got_handshake(const QNetworkDatagram &datagram);
    void on_got_synHandshake(const QByteArray &data);
    void on_got_ack(const QByteArray &data);
    void on_got_init(const QByteArray &data);
    void on_got_data(const QByteArray &data);
    void on_got_error(const QByteArray &data);
    void prepareDataPayload();

private:
    QUdpSocket *m_udpSocket;
    int m_fragSize;

    QNetworkDatagram m_recDatagram();

    QTimer *m_connectionTimer;
    QTimer *m_handshakeTimer;
    QTimer *m_retryHandshakeTimer;
    QTimer *m_retrySynTimer;
    QTimer *m_retryInitTimer;
    QTimer *m_retryDataTimer;
    QTimer *m_dataTimer;

    QVector<QByteArray> m_dataToSend;
    QVector<QByteArray> m_fragsToSend;
    QByteArray m_initToSend;
    QVector<QByteArray> m_receivedData;

    quint8 m_retryCount;
    quint8 m_retrySynCount;
    quint8 m_retryInitCount;
    quint8 m_retryDataCount;
    quint8 m_blockToSend;
    bool m_server;

    quint32 m_fragsToReceive;
    quint32 m_fragsReceived;
    quint8 m_ackLimit;
    quint8 m_ackRecieved;
    bool m_isFile;
    QFile *m_file;
    QString m_msg;

    bool m_sendCurrupt;
    bool m_tempSendCurrupt;

signals:
    void receivedMessage(const QString &);
    void debugMessage(const QString &);
};

#endif // SOCKET_H
