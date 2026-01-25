#include "ClipboardAssistant.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ClipboardAssistant window;
    window.show();
    return app.exec();
}
