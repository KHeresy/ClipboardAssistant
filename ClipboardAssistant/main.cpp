#include "ClipboardAssistant.h"
#include <QtWidgets/QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QSettings>
#include <QTranslator>
#include <QLibraryInfo>
#include <windows.h>

int main(int argc, char *argv[])
{
    // QApplication 必須在 QSettings 之前
    QApplication app(argc, argv);
    app.setOrganizationName("Heresy");
    app.setApplicationName("ClipboardAssistant");

    QSettings settings("Heresy", "ClipboardAssistant");
    QString lang = settings.value("Language", "").toString();
    QLocale locale = lang.isEmpty() ? QLocale::system() : QLocale(lang);

    // 載入 Qt 標準元件翻譯 (如 QMessageBox 的 OK/Cancel)
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // 載入應用程式翻譯
    QTranslator appTranslator;
    if (appTranslator.load(locale, "ClipboardAssistant", "_", QApplication::applicationDirPath() + "/translations")) {
        app.installTranslator(&appTranslator);
    }

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

        if (!settings.value("StartMinimized", false).toBool()) {

            window.show();

        }

        

        return app.exec();

    }

    