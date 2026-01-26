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
#include "ui_ClipboardAssistant.h"
#include "../Common/IClipboardPlugin.h"

class ClipboardAssistant : public QWidget
{
    Q_OBJECT

public:
    ClipboardAssistant(QWidget *parent = nullptr);
    ~ClipboardAssistant();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onClipboardChanged();
    void onBtnCopyOutputClicked();
    void onBtnSettingsClicked();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onImageDownloaded(QNetworkReply* reply, QString originalUrl);
    
    // New slots for dynamic features
    void onBtnAddFeatureClicked();
    void onRunFeature(IClipboardPlugin* plugin, QString featureId);
    void onEditFeature(IClipboardPlugin* plugin, QString featureId);
    void onDeleteFeature(IClipboardPlugin* plugin, QString featureId);

private:
    void loadPlugins();
    void reloadFeatures(); // Helper to refresh list
    void clearLayout(QLayout* layout);
    void addFeatureWidget(IClipboardPlugin* plugin, const PluginFeature& feature);
    void setupTrayIcon();
    void registerGlobalHotkey();
    void unregisterGlobalHotkey();
    void registerFeatureHotkey(int id, const QKeySequence& ks);
    void processHtmlImages(QString html);

    Ui::ClipboardAssistantClass *ui;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QList<IClipboardPlugin*> m_plugins;
    QList<QShortcut*> m_localShortcuts;
    QNetworkAccessManager* m_networkManager;
    QString m_currentHtml;
    QSet<QString> m_pendingDownloads;

    // Map list item unique ID to Plugin* and FeatureID
    struct FeatureInfo {
        IClipboardPlugin* plugin;
        QString featureId;
    };
    QMap<QString, FeatureInfo> m_featureMap;
    // Map WinAPI Hotkey ID to FeatureInfo
    QMap<int, FeatureInfo> m_hotkeyMap;
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
    void handlePluginOutput(const QString& text, bool append);
    void handlePluginError(const QString& msg);
};