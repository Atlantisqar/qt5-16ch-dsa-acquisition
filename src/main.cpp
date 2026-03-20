#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Dsa16ChAcquisition");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Atom");
    const QIcon appIcon(":/icons/app_icon.png");
    app.setWindowIcon(appIcon);

    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        app.setStyleSheet(stream.readAll());
        styleFile.close();
    }

    QString startupProjectFile;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].endsWith(".acqproj", Qt::CaseInsensitive)) {
            startupProjectFile = args[i];
            break;
        }
    }

    MainWindow window(startupProjectFile);
    window.setWindowIcon(appIcon);
    window.showMaximized();
    return app.exec();
}
