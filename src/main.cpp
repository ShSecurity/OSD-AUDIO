#include <QApplication>
#include "gui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("OmniStream Deck");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("OSD-AUDIO");

    MainWindow window;
    window.show();

    return app.exec();
}
