#include <QApplication>
#include <QCoreApplication>
#include "mainwindow.h"
#include "core/BehaviorAnalytics.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息（用于数据路径）
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    // 一行初始化
    ui_shared::behavior::BehaviorAnalytics::init();

    MainWindow w;
    w.show();

    int ret = app.exec();

    // 关闭
    ui_shared::behavior::BehaviorAnalytics::shutdown();
    return ret;
}
