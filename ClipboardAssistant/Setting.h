#pragma once

#include <QDialog>
#include <QKeySequence>
#include "ui_Setting.h"
#include "../Common/IClipboardPlugin.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingClass; };
QT_END_NAMESPACE

class Setting : public QDialog
{
    Q_OBJECT

public:
    Setting(const QList<struct PluginInfo>& plugins, QWidget *parent = nullptr);
    ~Setting();

    QKeySequence getHotkey() const;
    void setHotkey(const QKeySequence& sequence);

    bool isHotkeyEnabled() const;
    void setHotkeyEnabled(bool enabled);

    bool isCaptureHotkeyEnabled() const;
    void setCaptureHotkeyEnabled(bool enabled);

    QKeySequence getCaptureHotkey() const;
    void setCaptureHotkey(const QKeySequence& sequence);

    bool isShowAfterCaptureEnabled() const;
    void setShowAfterCaptureEnabled(bool enabled);

    void accept() override;

private slots:
    void onPluginSelected(int row);

private:
    Ui::SettingClass *ui;
    QList<IClipboardPlugin*> m_plugins;
    QMap<IClipboardPlugin*, QMap<QString, QWidget*>> m_paramWidgets;
    QMap<IClipboardPlugin*, QList<ParameterDefinition>> m_paramDefs;
};
