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

    void accept() override;

private slots:
    void onPluginSelected(int row);

private:
    Ui::SettingClass *ui;
    QList<IClipboardPlugin*> m_plugins;
};