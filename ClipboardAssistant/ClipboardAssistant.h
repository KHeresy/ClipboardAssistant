#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QList>
#include <QMap>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QShortcut>
#include <QPointer>
#include <QDialog>
#include <QProgressDialog>
#include <QShowEvent>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include "ui_ClipboardAssistant.h"
#include "../Common/IClipboardModule.h"

QT_END_NAMESPACE

struct SelectionState;

class SnippetOverlay : public QDialog {
    Q_OBJECT
public:
    explicit SnippetOverlay(const QPixmap& fullCanvas, const QRect& screenGeo, const QRect& totalGeo, SelectionState* state, QWidget* parent = nullptr);
    QPixmap selectedPixmap() const;
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    QRect selectedRect() const;
    const QPixmap& m_fullCanvas;
    QRect m_screenGeo; // My geometry in global logical coordinates
    QRect m_totalGeo;  // Total bounding box of all screens
    SelectionState* m_state;
};

class ModuleCallback : public QObject, public IModuleCallback {
    Q_OBJECT
public:
    ModuleCallback(class ClipboardAssistant* parent);
    void onTextData(const QString& text, bool isFinal) override;
    void onError(const QString& message) override;
    void onFinished() override;
private:
    class ClipboardAssistant* m_parent;
    bool m_firstChunk = true;
};

class ClipboardAssistant : public QWidget
{
    Q_OBJECT

public:
    ClipboardAssistant(QWidget *parent = nullptr);
    ~ClipboardAssistant();

    static void sendCtrlKey(char key);
    static void sendCtrlC();
    static void sendCtrlV();

    static const int HOTKEY_ID_MAIN = 100;
    static const int HOTKEY_ID_CAPTURE = 99;

    enum CompletionAction {
        CA_ShowResult = 0,
        CA_CopyToClipboard = 1,
        CA_Paste = 2
    };

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onClipboardChanged();
    void onBtnCopyOutputClicked();
    void onBtnPasteClicked();
    void onBtnSettingsClicked();
    void onBtnCancelClicked();
    void onCheckAlwaysOnTopToggled(bool checked);
    void onSpinInputFontSizeChanged(int size);
    void onSpinOutputFontSizeChanged(int size);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onImageDownloaded(QNetworkReply* reply, QString originalUrl);
    void showProgressDialog();
    
    // New slots for dynamic ActionSets
    void onBtnAddActionSetClicked();
    void onBtnImportActionSetClicked();
    void onBtnExportAllClicked();
    void onExportActionSet(const QString& asid);
    void onRunActionSet(IClipboardModule* module, QString actionSetId);
    void onEditActionSet(IClipboardModule* module, QString actionSetId);
    void onDeleteActionSet(IClipboardModule* module, QString actionSetId);

private:
    void loadModules();
    void reloadActionSets(); // Helper to refresh list
    void loadSettings();
    void saveSettings();
    void clearLayout(QLayout* layout);
    void updateButtonsState();
    void setupTrayIcon();
    bool registerGlobalHotkey();
    void unregisterGlobalHotkey();
    bool registerActionSetHotkey(int id, const QKeySequence& ks);
    void processHtmlImages(QString html);
    void onCaptureHotkey(); // Helper for screen capture

    Ui::ClipboardAssistantClass *ui;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QList<ModuleInfo> m_modules;
    IClipboardModule* m_activeModule = nullptr;
    class PipelineExecutor* m_currentExecutor = nullptr;
    class RegExAssistant* m_regexAssistant;
    class ExternalAppAssistant* m_externalAppAssistant;
    class TextInputAssistant* m_textInputAssistant;
    QList<QShortcut*> m_localShortcuts;
    QNetworkAccessManager* m_networkManager;
    QString m_currentHtml;
    QImage m_currentImage;
    QSet<QString> m_pendingDownloads;

    // Map list item unique ID to ActionSet configuration
    struct ActionSetInfo {
        QString actionSetId; // Internal UID
        QString name;
        QKeySequence customShortcut;
        bool isCustomShortcutGlobal;
        bool isAutoCopy;
        bool isCaptureScreen = false;
        bool isCaptureCopy = false;
        int completionAction = 0; // 0: Show, 1: Copy, 2: Paste
        bool autoClose = false;
        bool startHidden = false;
        QList<ModuleActionInstance> actions;
        
        QPointer<QPushButton> mainButton;
        QPointer<QLabel> lblContent;
    };

    void updateActionSetShortcuts();
    void addActionSetWidget(const ActionSetInfo& info);
    void setupActionSetWidget(QListWidgetItem* item, ActionSetInfo& info);
    QMimeData* captureScreenRegion(bool restoreWindow);
    QMap<QString, ActionSetInfo> m_actionSetMap;
    // Plugin Name -> Global Settings
    QMap<QString, QVariantMap> m_globalSettingsMap;
    // Map WinAPI Hotkey ID to ActionSetInfo
    QMap<int, ActionSetInfo> m_hotkeyMap;
    int m_nextHotkeyId = 101; // Start after 100 (main app hotkey)
    
    QTimer* m_progressTimer;
    QPointer<QProgressDialog> m_progressDlg;

    // For callback access
    void handleModuleOutput(const QString& text, bool append, bool isFinal);
    void handleModuleError(const QString& msg);
    void closeProgressReporter();
    
    friend class ModuleCallback;
    friend class PipelineExecutor;
};