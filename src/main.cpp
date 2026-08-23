#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    // The native Windows style (windowsvista/windows11) only partially
    // respects QSS "border-radius" on QPushButton - corners paint as a
    // squarish rounded-rect instead of a true circle no matter how large
    // the radius is set. Fusion is a QSS-driven style that renders
    // border-radius exactly as specified, which is what the transport
    // buttons need to actually look round. Must be set before any widget
    // is constructed (this string overload is safe to call before
    // QApplication itself exists).
    QApplication::setStyle("Fusion");

    QApplication app(argc, argv);
    QApplication::setApplicationName("FreeMusicPlayer");
    QApplication::setOrganizationName("FreeMusicPlayer");
    QApplication::setApplicationVersion("1.0.0");

    // Window/taskbar icon while running (from the qrc resource). The .exe
    // icon shown by Explorer/Alt-Tab before the app is even running comes
    // from a separate mechanism - resources/app.rc, compiled in via CMake.
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
