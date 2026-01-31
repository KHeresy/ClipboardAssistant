#pragma once

#include <QDialog>
#include <QKeySequence>
#include "ui_Setting.h"
#include "../Common/IClipboardModule.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingClass; };
QT_END_NAMESPACE

class Setting : public QDialog
{
    Q_OBJECT

public:
    explicit Setting(const QList<struct ModuleInfo>& modules, QWidget *parent = nullptr);
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

private slots:
    void onModuleSelected(int row);
    void accept() override;

private:
    Ui::SettingClass *ui;
    QList<IClipboardModule*> m_modules;
    QMap<IClipboardModule*, QMap<QString, QWidget*>> m_paramWidgets;
    QMap<IClipboardModule*, QList<ParameterDefinition>> m_paramDefs;
};
