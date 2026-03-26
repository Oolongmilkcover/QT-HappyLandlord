#include "gamepanel.h"
#include "cards.h"
#include <QApplication>
#include "loading.h"
#include <QResource>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<Cards>("Cards&"); // 注册非 const 引用
    QResource::registerResource("./resource.rcc");
    Loading w;
    w.show();
    return a.exec();
}
