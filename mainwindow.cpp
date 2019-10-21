#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkInterface>
#include <QFileDialog>

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
    m_socket->setFragSize(ui->fragSizeEdit->text().toInt());
}

void MainWindow::on_fileBtn_clicked()
{
    QString filePath(QFileDialog::getOpenFileName(this, "Select file", "/home/"));
    QString fileName(QFileInfo(filePath).fileName());
    sendToDebug("File to send: " + fileName);
    m_selectedFile = filePath;
    ui->labelFileName->setText(fileName);
}

void MainWindow::on_sendFileBtn_clicked()
{
    m_socket->sendFile(m_selectedFile);
}

void MainWindow::on_checkBox_stateChanged(int checked)
{
    if (checked == Qt::CheckState::Checked) {
        m_socket->corruptFrag(true);
    } else {
        m_socket->corruptFrag(false);
    }
}
