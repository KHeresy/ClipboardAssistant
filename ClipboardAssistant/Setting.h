#pragma once

#include <QWidget>
#include "ui_Setting.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingClass; };
QT_END_NAMESPACE

class Setting : public QWidget
{
	Q_OBJECT

public:
	Setting(QWidget *parent = nullptr);
	~Setting();

private:
	Ui::SettingClass *ui;
};

