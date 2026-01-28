#include "ClipboardAssistant.h"
#include <QtWidgets/QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QSettings>
#include <windows.h>

int main(int argc, char *argv[])
{
    // QApplication 必須在 QSettings 之前，因為 QSettings 會用到 Organization Name
    QApplication app(argc, argv);
    app.setOrganizationName("Heresy");
    app.setApplicationName("ClipboardAssistant");

    // 使用唯一的 Key 建立共享記憶體
    QSharedMemory shared("org.gemini.ClipboardAssistant.SingleInstanceKey");
    
    // 嘗試附加到現有的記憶體區塊，如果成功，代表程式已在執行
    if (shared.attach()) {
        // 尋找已存在的視窗並將其帶到最前景
        HWND hwnd = FindWindowW(NULL, L"ClipboardAssistant");
        if (hwnd) {
            if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0; 
    }

    // 如果附加失敗，則嘗試建立新的記憶體區塊
    if (!shared.create(1)) {
        return 0;
    }

        ClipboardAssistant window;

        window.setWindowTitle(QObject::tr("ClipboardAssistant"));

        

        QSettings settings("Heresy", "ClipboardAssistant");

        if (!settings.value("StartMinimized", false).toBool()) {

            window.show();

        }

        

        return app.exec();

    }

    