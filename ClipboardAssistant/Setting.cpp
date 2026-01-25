#include "Setting.h"

Setting::Setting(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::SettingClass())
{
	ui->setupUi(this);
}

Setting::~Setting()
{
	delete ui;
}

