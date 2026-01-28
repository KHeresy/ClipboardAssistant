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
#include "../Common/IClipboardPlugin.h"

QT_END_NAMESPACE

class ClipboardAssistant : public QWidget
{
    Q_OBJECT

public:
    ClipboardAssistant(QWidget *parent = nullptr);
    ~ClipboardAssistant();

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
    void onRunActionSet(IClipboardPlugin* plugin, QString actionSetId);
    void onEditActionSet(IClipboardPlugin* plugin, QString actionSetId);
    void onDeleteActionSet(IClipboardPlugin* plugin, QString actionSetId);

private:
    void loadPlugins();
    void reloadActionSets(); // Helper to refresh list
    void loadSettings();
    void saveSettings();
    void clearLayout(QLayout* layout);
    void updateButtonsState();
    void setupTrayIcon();
    void registerGlobalHotkey();
    void unregisterGlobalHotkey();
    void registerActionSetHotkey(int id, const QKeySequence& ks);
    void processHtmlImages(QString html);

    Ui::ClipboardAssistantClass *ui;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QList<PluginInfo> m_plugins;
    IClipboardPlugin* m_activePlugin = nullptr;
    class RegExAssistant* m_regexAssistant;
    QList<QShortcut*> m_localShortcuts;
    QNetworkAccessManager* m_networkManager;
    QString m_currentHtml;
    QImage m_currentImage;
    QSet<QString> m_pendingDownloads;

    // Map list item unique ID to Plugin* and ActionSetID
    struct ActionSetInfo {
        IClipboardPlugin* plugin;
        QString actionSetId;
        QPointer<QPushButton> mainButton;
        QKeySequence customShortcut;
        bool isCustomShortcutGlobal;
        bool isAutoCopy;
        QString name;
        QPointer<QLabel> lblContent;
        QVariantMap parameters;
    };

    void updateActionSetShortcuts();
    void addActionSetWidget(IClipboardPlugin* plugin, const PluginActionSet& actionSet, const QString& internalId);
    void setupActionSetWidget(QListWidgetItem* item, ActionSetInfo& info);
    QMap<QString, ActionSetInfo> m_actionSetMap;
    // Plugin Name -> Global Settings
    QMap<QString, QVariantMap> m_globalSettingsMap;
    // Map WinAPI Hotkey ID to ActionSetInfo
    QMap<int, ActionSetInfo> m_hotkeyMap;
    int m_nextHotkeyId = 101; // Start after 100 (main app hotkey)
    
    class PluginCallback : public IPluginCallback {
    public:
        PluginCallback(ClipboardAssistant* parent);
        void onTextData(const QString& text, bool isFinal) override;
        void onError(const QString& message) override;
        void onFinished() override;
    private:
        ClipboardAssistant* m_parent;
        bool m_firstChunk = true;
    };
    friend class PluginCallback;
    
    // For callback access
    void handlePluginOutput(const QString& text, bool append, bool isFinal);
    void handlePluginError(const QString& msg);
};