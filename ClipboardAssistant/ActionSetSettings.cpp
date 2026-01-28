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
#include <QFileDialog>
#include <QDir>
#include <QPushButton>

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
        case ParameterType::File:
        case ParameterType::Directory: {
            QWidget* container = new QWidget(this);
            QHBoxLayout* h = new QHBoxLayout(container);
            h->setContentsMargins(0, 0, 0, 0);
            QLineEdit* e = new QLineEdit(container);
            e->setText(val.toString());
            e->setObjectName("PathEdit");
            QPushButton* b = new QPushButton("...", container);
            b->setFixedWidth(30);
            h->addWidget(e);
            h->addWidget(b);
            
            bool isFile = (def.type == ParameterType::File);
            connect(b, &QPushButton::clicked, [this, e, isFile]() {
                QString path;
                if (isFile) {
                    path = QFileDialog::getOpenFileName(this, "Select File", e->text());
                } else {
                    path = QFileDialog::getExistingDirectory(this, "Select Directory", e->text());
                }
                if (!path.isEmpty()) e->setText(QDir::toNativeSeparators(path));
            });
            
            widget = container;
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
        case ParameterType::File:
        case ParameterType::Directory:
            val = widget->findChild<QLineEdit*>("PathEdit")->text();
            break;
        }
        values.insert(def.id, val);
    }
    return values;
}
