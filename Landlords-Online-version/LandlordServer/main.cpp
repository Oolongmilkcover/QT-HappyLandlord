#include "server.h"
#include <QApplication>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<Cards>("Cards&");
    Server w;
    w.show();
    return a.exec();
}
