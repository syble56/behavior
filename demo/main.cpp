#include <QApplication>
#include <QCoreApplication>
#include "mainwindow.h"
#include "core/behavior_analytics.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息（用于数据路径）
    QCoreApplication::setOrganizationName("Syble");
    QCoreApplication::setApplicationName("BehaviorDemo");

    fprintf(stdout, "[Main] Initializing behavior analytics...\n"); fflush(stdout);
    // 一行初始化
    ui_shared::behavior::BehaviorAnalytics::init();

    fprintf(stdout, "[Main] Creating main window...\n"); fflush(stdout);
    MainWindow w;
    fprintf(stdout, "[Main] Showing main window...\n"); fflush(stdout);
    w.show();

    fprintf(stdout, "[Main] Entering event loop...\n"); fflush(stdout);
    int ret = app.exec();

    fprintf(stdout, "[Main] Event loop exited with code %d\n", ret); fflush(stdout);
    // 关闭
    ui_shared::behavior::BehaviorAnalytics::shutdown();
    return ret;
}
