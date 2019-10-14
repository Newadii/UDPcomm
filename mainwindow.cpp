#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkInterface>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_socket(new Socket(this))
{
    ui->setupUi(this);

    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
            ui->ipEditMy->setText(address.toString());
            ui->ipEditTheir->setText(address.toString());
            break;
        }
    }

    connect(m_socket, SIGNAL(debugMessage(QString)), this, SLOT(sendToDebug(const QString &)));
    connect(m_socket, SIGNAL(receivedMessage(QString)), this, SLOT(sendToReceive(const QString &)));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_socket;
}

void MainWindow::sendToDebug(const QString &msg)
{
    ui->debugText->appendPlainText(msg);
}

void MainWindow::sendToReceive(const QString &msg)
{
    ui->messageText->appendPlainText(msg);
}

void MainWindow::on_startServerBtn_clicked()
{
    QString port = ui->portEditMy->text();

    m_socket->bindSocket(port);
}

void MainWindow::on_stopServerBtn_clicked()
{
    m_socket->closeSocket();
}

void MainWindow::on_connectBtn_clicked()
{
    QString port = ui->portEditTheir->text();
    QString ip = ui->ipEditTheir->text();

    m_socket->connectToHost(ip, port);
}

void MainWindow::on_disconnectBtn_clicked()
{
    m_socket->disconnect();
}

void MainWindow::on_sendMsgBtn_clicked()
{
    m_socket->sendMessage(ui->inputMsg->toPlainText());
}

void MainWindow::on_setFragBtn_clicked()
{
    m_socket->setFragSize(ui->fragSizeEdit->text().toUInt());
}

void MainWindow::on_fileBtn_clicked()
{

}

void MainWindow::on_sendFileBtn_clicked()
{

}
