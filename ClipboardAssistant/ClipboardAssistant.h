#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QList>
#include <QMap>
#include <QListWidget>
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
    void onFeatureClicked(QListWidgetItem* item);
    void onBtnCopyOutputClicked();
    void onBtnSettingsClicked();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void loadPlugins();
    void setupTrayIcon();
    void registerGlobalHotkey();
    void unregisterGlobalHotkey();

    Ui::ClipboardAssistantClass *ui;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QList<IClipboardPlugin*> m_plugins;
    // Map list item unique ID to Plugin* and FeatureID
    struct FeatureInfo {
        IClipboardPlugin* plugin;
        QString featureId;
    };
    QMap<QString, FeatureInfo> m_featureMap;
    
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