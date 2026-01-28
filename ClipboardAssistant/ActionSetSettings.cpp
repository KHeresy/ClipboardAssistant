#include "ActionSetSettings.h"
#include "ui_ActionSetSettings.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>

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

void ActionSetSettings::setParameters(const QList<ParameterDefinition>& defs, const QVariantMap& values)
{
    m_paramDefs = defs;
    m_paramWidgets.clear();

    // Remove existing widgets if any
    QLayoutItem* item;
    while ((item = ui->verticalLayout_groupActionSet->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    QFormLayout* form = new QFormLayout();
    ui->verticalLayout_groupActionSet->addLayout(form);

    for (const auto& def : defs) {
        QWidget* widget = nullptr;
        QVariant val = values.value(def.id, def.defaultValue);

        switch (def.type) {
        case ParameterType::String: {
            QLineEdit* e = new QLineEdit(this);
            e->setText(val.toString());
            widget = e;
            break;
        }
        case ParameterType::Password: {
            QLineEdit* e = new QLineEdit(this);
            e->setEchoMode(QLineEdit::Password);
            e->setText(val.toString());
            widget = e;
            break;
        }
        case ParameterType::Text: {
            QTextEdit* e = new QTextEdit(this);
            e->setPlainText(val.toString());
            e->setMaximumHeight(100);
            widget = e;
            break;
        }
        case ParameterType::Bool: {
            QCheckBox* c = new QCheckBox(this);
            c->setChecked(val.toBool());
            widget = c;
            break;
        }
        case ParameterType::Choice: {
            QComboBox* c = new QComboBox(this);
            c->addItems(def.options);
            c->setCurrentText(val.toString());
            widget = c;
            break;
        }
        case ParameterType::Number: {
            QSpinBox* s = new QSpinBox(this);
            s->setRange(-999999, 999999);
            s->setValue(val.toInt());
            widget = s;
            break;
        }
        }

        if (widget) {
            widget->setToolTip(def.description);
            form->addRow(def.name + ":", widget);
            m_paramWidgets.insert(def.id, widget);
        }
    }
}

QVariantMap ActionSetSettings::getParameters() const
{
    QVariantMap values;
    for (const auto& def : m_paramDefs) {
        QWidget* widget = m_paramWidgets.value(def.id);
        if (!widget) continue;

        QVariant val;
        switch (def.type) {
        case ParameterType::String:
        case ParameterType::Password:
            val = qobject_cast<QLineEdit*>(widget)->text();
            break;
        case ParameterType::Text:
            val = qobject_cast<QTextEdit*>(widget)->toPlainText();
            break;
        case ParameterType::Bool:
            val = qobject_cast<QCheckBox*>(widget)->isChecked();
            break;
        case ParameterType::Choice:
            val = qobject_cast<QComboBox*>(widget)->currentText();
            break;
        case ParameterType::Number:
            val = qobject_cast<QSpinBox*>(widget)->value();
            break;
        }
        values.insert(def.id, val);
    }
    return values;
}