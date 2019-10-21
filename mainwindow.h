#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include "socket.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:

private slots:
    void sendToDebug(const QString &msg);
    void sendToReceive(const QString &msg);

    void on_startServerBtn_clicked();
    void on_connectBtn_clicked();
    void on_disconnectBtn_clicked();
    void on_sendMsgBtn_clicked();
    void on_setFragBtn_clicked();
    void on_fileBtn_clicked();
    void on_sendFileBtn_clicked();
    void on_stopServerBtn_clicked();

    void on_checkBox_stateChanged(int arg1);

private:
    Ui::MainWindow *ui;
    Socket *m_socket;

    QString m_selectedFile;
};
#endif // MAINWINDOW_H
