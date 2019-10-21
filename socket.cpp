#include "socket.h"
#include <QNetworkDatagram>
#include <QDebug>
#include <QIODevice>
#include <QtMath>
#include <QFileDialog>

#define CONNECTION_TIMEOUT_MS 72000
#define HANDSHAKE_TIMEOUT_MS 60000
#define REPEAT_LIMIT 42
#define MAX_FRAG_SIZE 1446
#define ACK_CONFIRM 20

enum class packetType {
    handshake = 1,
    shakeSyn = 2,
    ack = 4,
    init = 8,
    data = 16,
    error = 32
};

enum class ackType {
    handshake = 1,
    init = 8,
    data = 16
};

Socket::Socket(QObject *parent) : QObject(parent)
  , m_udpSocket(new QUdpSocket(this))
  , m_fragSize(MAX_FRAG_SIZE)
  , m_retryCount(0)
  , m_retrySynCount(0)
  , m_retryInitCount(0)
  , m_retryDataCount(0)
  , m_server(false)
  , m_sendCurrupt(false)
{
    connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(on_readyRead()));
    connect(m_udpSocket, SIGNAL(connected()), this, SLOT(on_connected()));
    connect(m_udpSocket, SIGNAL(disconnected()), this, SLOT(on_disconnected()));

    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setInterval(CONNECTION_TIMEOUT_MS);
    m_connectionTimer->setSingleShot(true);
    connect(m_connectionTimer, SIGNAL(timeout()), this, SLOT(on_connection_timeout()));

    m_handshakeTimer = new QTimer(this);
    m_handshakeTimer->setInterval(HANDSHAKE_TIMEOUT_MS);
    m_handshakeTimer->setSingleShot(false);
    connect(m_handshakeTimer, SIGNAL(timeout()), this, SLOT(on_handshake_timeout()));

    m_retryHandshakeTimer = new QTimer(this);
    m_retryHandshakeTimer->setInterval(1000);
    m_retryHandshakeTimer->setSingleShot(true);
    connect(m_retryHandshakeTimer, SIGNAL(timeout()), this, SLOT(on_retryHandshake_timeout()));

    m_retrySynTimer = new QTimer(this);
    m_retrySynTimer->setInterval(1000);
    m_retrySynTimer->setSingleShot(true);
    connect(m_retrySynTimer, SIGNAL(timeout()), this, SLOT(on_retrySyn_timeout()));

    m_retryInitTimer = new QTimer(this);
    m_retryInitTimer->setInterval(1000);
    m_retryInitTimer->setSingleShot(true);
    connect(m_retryInitTimer, SIGNAL(timeout()), this, SLOT(on_retryInit_timeout()));

    m_retryDataTimer = new QTimer(this);
    m_retryDataTimer->setInterval(3000);
    m_retryDataTimer->setSingleShot(true);
    connect(m_retryDataTimer, SIGNAL(timeout()), this, SLOT(on_retryData_timeout()));

    m_dataTimer = new QTimer(this);
    m_dataTimer->setInterval(1000);
    m_dataTimer->setSingleShot(true);
    connect(m_dataTimer, SIGNAL(timeout()), this, SLOT(on_data_timeout()));
}

void Socket::corruptFrag(bool crpt)
{
    m_sendCurrupt = crpt;
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
        emit debugMessage("Server succesfully started! \n Binded port: " + portString);
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
    qDebug() << "on_connectToHost" << m_server;
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

QByteArray Socket::intToArray(quint32 n)
{
    union {
        quint32 i;
        char ch[4];
    } u;
    QByteArray arr;
    arr.reserve(4);

    u.i = n;
    for (char ch: u.ch) {
        arr.append(ch);
    }
    return arr;
}

QByteArray Socket::intToArray(quint16 n)
{
    union {
        quint16 i;
        char ch[2];
    } u;
    QByteArray arr;
    arr.reserve(2);

    u.i = n;
    for (char ch: u.ch) {
        arr.append(ch);
    }
    return arr;
}

quint32 Socket::arrToInt(const QByteArray &arr)
{
    union {
        quint32 i;
        char ch[4];
    } u;
    for (int i = 0; i < 4; ++i) {
        u.ch[i] = arr[i];
    }
    return u.i;
}

quint16 Socket::arrToCheck(const QByteArray &arr)
{
    union {
        quint16 i;
        char ch[2];
    } u;
    for (int i = 0; i < 2; ++i) {
        u.ch[i] = arr[i];
    }
    return u.i;
}

QByteArray Socket::checksum(const QByteArray &arr)
{
    return intToArray(qChecksum(arr.data(), static_cast<uint>(arr.size())));
}

void Socket::sendFile(const QString &filePath)
{
    QFile fileToSend(filePath);
    fileToSend.open(QIODevice::ReadOnly);
    QString fileName = QFileInfo(filePath).fileName();
    fileName.truncate(42);
    qDebug() << "sending file: " << fileName << "is open: " << fileToSend.isOpen();
    emit debugMessage("Will send file: " + fileToSend.fileName());

    QByteArray fileByteArr(fileToSend.readAll());
    fileToSend.close();

    quint32 packetsToSend = static_cast<quint32>(fileByteArr.size() / m_fragSize);
    if (fileByteArr.size() % m_fragSize != 0) ++packetsToSend;
    qDebug() << "packetsToSend" << packetsToSend;

    QByteArray initData;
    initData.append(static_cast<char>(packetType::init));
    initData.append(intToArray(packetsToSend));
    initData.append(ACK_CONFIRM);
    initData.append(fileName.toLatin1());
    initData.append(checksum(initData));

    m_initToSend = initData;
    m_udpSocket->write(initData);
    m_dataToSend.append(fileByteArr);
    m_retryInitTimer->start();
}

void Socket::sendMessage(const QString &msg)
{
    QByteArray msgArr(msg.toLatin1());
    QByteArray initData;
    initData.reserve(9);

    quint32 packetsToSend = static_cast<quint32>(msgArr.size() / m_fragSize);
    if (msgArr.size() % m_fragSize != 0) ++packetsToSend;
    qDebug() << "packetsToSend" << packetsToSend;

    initData.append(static_cast<char>(packetType::init));
    initData.append(intToArray(packetsToSend));
    initData.append(ACK_CONFIRM);
    initData.append(1);
    initData.append(checksum(initData));

    m_initToSend = initData;
    m_udpSocket->write(initData);
    m_dataToSend.append(msgArr);
    m_retryInitTimer->start();
}

void Socket::prepareDataPayload()
{
    qDebug() << "sendDataPayload";
    QByteArray wholeData = m_dataToSend.takeFirst();
    qDebug() << "data size to send:" << wholeData.size();
    QVector<QByteArray> splitData;

    qint8 x = 0;
    for (qint32 i = 0; i < wholeData.size(); ++x, i += m_fragSize) {
        if (x == ACK_CONFIRM) x = 0;
        QByteArray fragment;
        fragment.append(static_cast<char>(packetType::data));
        fragment.append(x);
        fragment.append(wholeData.mid(i, m_fragSize));
        fragment.append(checksum(fragment));
        splitData.append(fragment);
    }

    m_tempSendCurrupt = m_sendCurrupt;
    m_fragsToSend = splitData;
    m_blockToSend = 0;
    qDebug() << "before on_timeout";
    on_retryData_timeout();
}

void Socket::setFragSize(int fragSize)
{
    if (fragSize > MAX_FRAG_SIZE) {
        emit debugMessage("Maximum fragment size is 1446, fragment size not set!");
        return;
    }
    m_fragSize = fragSize;
    emit debugMessage("Fragment size set to: " + QString::number(m_fragSize));
}

void Socket::on_connected()
{
    qDebug() << "on_connected" << m_server;
    if (m_server) {
        QByteArray data;
        data.append(static_cast<char>(packetType::shakeSyn));
        data.append(checksum(data));
        m_udpSocket->write(data);
        m_retrySynTimer->start();
    } else {
        emit debugMessage("Socket connected. Trying to establish connetion with server.");
        QByteArray data;
        data.append(static_cast<char>(packetType::handshake));
        data.append(checksum(data));
        m_udpSocket->write(data);
        m_retryHandshakeTimer->start();
    }
}

void Socket::on_retryHandshake_timeout()
{
    qDebug() << "on_retryHandshake_timeout" << m_server;
    if (++m_retryCount > REPEAT_LIMIT) {
        return;
    }
    QByteArray data;
    data.append(static_cast<char>(packetType::handshake));
    data.append(checksum(data));
    m_udpSocket->write(data);
    m_retryHandshakeTimer->start();
}

void Socket::on_retrySyn_timeout()
{
    if (++m_retrySynCount > REPEAT_LIMIT) {
        return;
    }
    QByteArray data;
    data.append(static_cast<char>(packetType::shakeSyn));
    data.append(checksum(data));
    m_udpSocket->write(data);
    m_retrySynTimer->start();
}

void Socket::on_retryInit_timeout()
{
    if (++m_retryInitCount > REPEAT_LIMIT) {
        return;
    }
    m_udpSocket->write(m_initToSend);
    m_retryInitTimer->start();
}

void Socket::on_retryData_timeout()
{
    qDebug() << "on_retry_timeout";
    if (++m_retryDataCount > REPEAT_LIMIT) {
        qDebug() << " data retryCount reached";
        return;
    }
    QVector<QByteArray> fragsToSend = m_fragsToSend.mid(m_blockToSend * ACK_CONFIRM, ACK_CONFIRM);
    if (fragsToSend.isEmpty()) {
        qDebug() << "frags to send empty";
        return;
    }

    for (QByteArray &data: fragsToSend) {
        if (m_tempSendCurrupt) {
            data[2] = 'x';
            m_tempSendCurrupt = false;
        }
        m_udpSocket->write(data);
    }
    m_retryDataTimer->start();
}

void Socket::on_data_timeout()
{
    m_fragsReceived -= m_ackRecieved;
    m_ackRecieved = 0;
}

void Socket::on_disconnected()
{
    qDebug() << "on_disconnected" << m_server;
    emit debugMessage("Socket disconnected.");
    m_retryHandshakeTimer->stop();
    m_retrySynTimer->stop();
    m_connectionTimer->stop();
    m_handshakeTimer->stop();
    m_retryDataTimer->stop();
    m_retryInitTimer->stop();
    m_dataTimer->stop();
    m_retryCount = 0;
    m_retrySynCount = 0;
    m_retryDataCount = 0;
    m_retryInitCount = 0;
}

void Socket::on_handshake_timeout()
{
    QByteArray data;
    data.append(static_cast<char>(packetType::handshake));
    data.append(checksum(data));
    m_udpSocket->write(data);
    m_retryHandshakeTimer->start();
}

void Socket::on_connection_timeout()
{
    emit debugMessage("Connection timed out. Disconnecting ..");
    qDebug() << "on_connection_timeout" << m_server;
    disconnect();
}

void Socket::on_got_handshake(const QNetworkDatagram &datagram)
{
    qDebug() << "on_got_handshake" << m_server;
    if (m_connectionTimer->isActive()) {
        emit debugMessage("Got Handshake, resetting connection timer.");
    } else {
        emit debugMessage("Got Handshake, starting connection timer.");
    }
    if (m_udpSocket->state() != QAbstractSocket::ConnectedState) {
            m_server = true;
            m_udpSocket->connectToHost(datagram.senderAddress(), quint16(datagram.senderPort()));
            return;
    }
    QByteArray data;
    data.append(static_cast<char>(packetType::shakeSyn));
    data.append(checksum(data));
    m_udpSocket->write(data);
    m_retrySynTimer->start();
}

void Socket::on_got_synHandshake(const QByteArray &recData)
{
    if (m_connectionTimer->isActive()) {
        emit debugMessage("Got Handshake, resetting connection timer.");
    } else {
        emit debugMessage("Got Handshake, starting connection timer.");
    }
    QByteArray chsum = checksum(recData.mid(1, 2));

    m_retryHandshakeTimer->stop();
    QByteArray data;
    data.append(static_cast<char>(packetType::ack));
    data.append(static_cast<char>(ackType::handshake));
    data.append(checksum(data));
    m_udpSocket->write(data);

    m_connectionTimer->start();
    m_handshakeTimer->start();
}

void Socket::on_got_ack(const QByteArray &recData)
{
    qDebug() << "on_got_ack" << static_cast<int>(recData[1]);

    ackType type = ackType(recData[1]);

    switch (type) {
    case ackType::data:
        qDebug() << "ack_data";
        emit debugMessage("Got ACK on DATA.");
        m_retryDataTimer->stop();
        m_retryDataCount = 0;
        ++m_blockToSend;
        on_retryData_timeout();
        break;
    case ackType::init:
        qDebug() << "ack_init";
        emit debugMessage("Got ACK on INIT.");
        m_retryInitTimer->stop();
        prepareDataPayload();
        break;
    case ackType::handshake:
        qDebug() << "ack_handshake";
        emit debugMessage("Got ACK on Handshake.");
        m_retrySynTimer->stop();
        m_connectionTimer->start();
        break;
    }
}

void Socket::on_got_init(const QByteArray &data)
{
    emit debugMessage("Got INIT");
    m_fragsToReceive = arrToInt(data.mid(1, 4));
    m_ackLimit = static_cast<quint8>(data[5]);
    qDebug() << "ack_limit: " << m_ackLimit;
    emit debugMessage("init: Send ACK every " + QString::number(m_ackLimit) + " fragments.");
    emit debugMessage("init: Total number of fragments to receive: " + QString::number(m_fragsToReceive));

    QByteArray chsum = checksum(data.mid(0, data.size() - 2));

    if (data[6] == 1) {
        qDebug() << "is not file";
        emit debugMessage("init: Receiving text message.");
        m_isFile = false;
    } else {
        m_isFile = true;
        QString fileName = data.mid(6, data.size() - 8);
        m_file = new QFile(fileName);
        m_file->open(QIODevice::WriteOnly);
        qDebug() << "is file open: " << m_file->isOpen();
        emit debugMessage("init: Receiving file: " + m_file->fileName());
    }

    m_receivedData.fill(QByteArray(), m_ackLimit);
    m_fragsReceived = 0;
    m_ackRecieved = 0;

    QByteArray ack;
    ack.append(static_cast<char>(packetType::ack));
    ack.append(static_cast<char>(ackType::init));
    ack.append(checksum(ack));
    m_udpSocket->write(ack);
}

void Socket::on_got_data(const QByteArray &data)
{
    emit debugMessage("Got data fragment.");
    quint8 fragNum = static_cast<quint8>(data[1]);
    qDebug() << "got frag #" << fragNum;

    QByteArray chsum = checksum(data.mid(0, data.size() - 2));
    if (chsum != data.mid(data.size() - 2, 2)) {
        qDebug() << "checksum doesn't match";
        return;
    }

    QByteArray payLoad = data.mid(2, data.size() - 4);

    ++m_fragsReceived;
    ++m_ackRecieved;
    emit debugMessage("data: Received fragment  #" + QString::number(m_fragsReceived) + '/' + QString::number(m_fragsToReceive) + " fragment size: " + QString::number(data.size()));

    m_receivedData[fragNum] = payLoad;

    qDebug() << "frags received: " << m_fragsReceived << " to receive: " << m_fragsToReceive << " ack limit: " << m_ackLimit;
    if (m_fragsReceived == m_fragsToReceive) {
        qDebug() << "recieved all fragments";
        emit debugMessage("Received all fragments.");
        m_ackLimit = m_ackRecieved;
    }
    if (m_ackRecieved == m_ackLimit) {

        for (const QByteArray &arr: m_receivedData) {
            if (m_isFile) {
                m_file->write(arr);
            } else {
                m_msg.append(arr);
            }
        }

        if (m_fragsReceived == m_fragsToReceive) {
            if (m_isFile) {
                delete  m_file;
            } else {
                emit receivedMessage(m_msg);
                m_msg.clear();
            }
        }

        emit debugMessage("data: Received whole data block, sending ACK.");
        qDebug() << "reached ack limit, sending ack";
        QByteArray ack;
        ack.append(static_cast<char>(packetType::ack));
        ack.append(static_cast<char>(ackType::data));
        ack.append(checksum(ack));
        m_udpSocket->write(ack);
        m_ackRecieved = 0;
        m_dataTimer->stop();
        return;
    }
    m_dataTimer->start();
}

void Socket::on_got_error(const QByteArray &data)
{
    emit debugMessage("on_got_error");
}

void Socket::on_readyRead()
{
    qDebug() << "on_readyRead";
    QNetworkDatagram datagram(m_udpSocket->receiveDatagram());

    QByteArray recData = datagram.data();
    packetType type = packetType(static_cast<char>(recData[0]));

    QByteArray chsum = checksum(recData.mid(0, recData.size() - 2));
    if (chsum != recData.mid(recData.size() - 2, 2)) {
        emit debugMessage("Got corrupted fragment, ignoring ...");
        qDebug() << "checksum doesn't match";
        return;
    }

    switch (type) {
    case packetType::handshake:
        on_got_handshake(datagram);
        break;
    case packetType::shakeSyn:
        on_got_synHandshake(recData);
        break;
    case packetType::ack:
        on_got_ack(recData);
        break;
    case packetType::init:
        on_got_init(recData);
        break;
    case packetType::data:
        on_got_data(recData);
        break;
    case packetType::error:
        on_got_error(recData);
        break;
    }
}
