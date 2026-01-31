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
#include "ui_ClipboardAssistant.h"
#include "../Common/IClipboardModule.h"

QT_END_NAMESPACE

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

    static const int HOTKEY_ID_MAIN = 100;
    static const int HOTKEY_ID_CAPTURE = 99;

protected:
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
        QList<ModuleActionInstance> actions;
        
        QPointer<QPushButton> mainButton;
        QPointer<QLabel> lblContent;
    };

    void updateActionSetShortcuts();
    void addActionSetWidget(const ActionSetInfo& info);
    void setupActionSetWidget(QListWidgetItem* item, ActionSetInfo& info);
    QMap<QString, ActionSetInfo> m_actionSetMap;
    // Plugin Name -> Global Settings
    QMap<QString, QVariantMap> m_globalSettingsMap;
    // Map WinAPI Hotkey ID to ActionSetInfo
    QMap<int, ActionSetInfo> m_hotkeyMap;
    int m_nextHotkeyId = 101; // Start after 100 (main app hotkey)
    
    // For callback access
    void handleModuleOutput(const QString& text, bool append, bool isFinal);
    void handleModuleError(const QString& msg);
    
    friend class ModuleCallback;
    friend class PipelineExecutor;
};