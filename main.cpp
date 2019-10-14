#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w, z;
    w.show();
    z.show();
    return a.exec();
}
