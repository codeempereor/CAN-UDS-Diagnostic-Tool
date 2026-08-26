#include <QApplication>
#include <QMainWindow>
#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CAN/UDS Tool");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CAN Tool");

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
