#include "ActionSetSettings.h"
#include "ui_ActionSetSettings.h"

ActionSetSettings::ActionSetSettings(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ActionSetSettings)
{
    ui->setupUi(this);
    connect(ui->checkGlobal, &QCheckBox::toggled, ui->checkAutoCopy, &QCheckBox::setEnabled);
}

ActionSetSettings::~ActionSetSettings()
{
    delete ui;
}

QString ActionSetSettings::name() const
{
    return ui->editName->text();
}

void ActionSetSettings::setName(const QString &name)
{
    ui->editName->setText(name);
}

QKeySequence ActionSetSettings::shortcut() const
{
    return ui->editShortcut->keySequence();
}

void ActionSetSettings::setShortcut(const QKeySequence &shortcut)
{
    ui->editShortcut->setKeySequence(shortcut);
}

bool ActionSetSettings::isGlobal() const
{
    return ui->checkGlobal->isChecked();
}

void ActionSetSettings::setIsGlobal(bool isGlobal)
{
    ui->checkGlobal->setChecked(isGlobal);
    ui->checkAutoCopy->setEnabled(isGlobal);
}

bool ActionSetSettings::isAutoCopy() const
{
    return ui->checkAutoCopy->isChecked();
}

void ActionSetSettings::setIsAutoCopy(bool isAutoCopy)
{
    ui->checkAutoCopy->setChecked(isAutoCopy);
}

void ActionSetSettings::setContent(QWidget *content)
{
    if (content) {
        ui->verticalLayout_groupActionSet->addWidget(content);
    }
}