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
    Setting(const QList<IClipboardPlugin*>& plugins, QWidget *parent = nullptr);
    ~Setting();

    QKeySequence getHotkey() const;
    void setHotkey(const QKeySequence& sequence);

private slots:
    void onPluginSelected(int row);
    void onAccepted();

private:
    Ui::SettingClass *ui;
    QList<IClipboardPlugin*> m_plugins;
};