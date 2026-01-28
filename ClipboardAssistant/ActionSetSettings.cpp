#include "ActionSetSettings.h"
#include "ui_ActionSetSettings.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QPushButton>
#include <QMenu>
#include <QScrollArea>

ActionSetSettings::ActionSetSettings(const QList<PluginInfo>& plugins, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ActionSetSettings),
    m_plugins(plugins)
{
    ui->setupUi(this);
    
    // 事件連結
    connect(ui->btnAdd, &QPushButton::clicked, this, &ActionSetSettings::onAddAction);
    connect(ui->btnRem, &QPushButton::clicked, this, &ActionSetSettings::onRemoveAction);
    connect(ui->btnUp, &QPushButton::clicked, this, &ActionSetSettings::onMoveUp);
    connect(ui->btnDown, &QPushButton::clicked, this, &ActionSetSettings::onMoveDown);
    connect(ui->listActions, &QListWidget::currentRowChanged, this, &ActionSetSettings::onActionSelected);
    connect(ui->checkGlobal, &QCheckBox::toggled, ui->checkAutoCopy, &QCheckBox::setEnabled);
    
    connect(ui->listActions->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
        QList<PluginActionInstance> newActions;
        for(int i=0; i<ui->listActions->count(); ++i) {
            int oldIdx = ui->listActions->item(i)->data(Qt::UserRole).toInt();
            newActions.append(m_actions[oldIdx]);
        }
        m_actions = newActions;
        updateActionList();
    });
}

ActionSetSettings::~ActionSetSettings() { delete ui; }

QString ActionSetSettings::name() const { return ui->editName->text(); }
void ActionSetSettings::setName(const QString &name) { ui->editName->setText(name); }
QKeySequence ActionSetSettings::shortcut() const { return ui->editShortcut->keySequence(); }
void ActionSetSettings::setShortcut(const QKeySequence &shortcut) { ui->editShortcut->setKeySequence(shortcut); }
bool ActionSetSettings::isGlobal() const { return ui->checkGlobal->isChecked(); }
void ActionSetSettings::setIsGlobal(bool isGlobal) { ui->checkGlobal->setChecked(isGlobal); ui->checkAutoCopy->setEnabled(isGlobal); }
bool ActionSetSettings::isAutoCopy() const { return ui->checkAutoCopy->isChecked(); }
void ActionSetSettings::setIsAutoCopy(bool isAutoCopy) { ui->checkAutoCopy->setChecked(isAutoCopy); }

void ActionSetSettings::setActions(const QList<PluginActionInstance>& actions) {
    m_actions = actions;
    updateActionList();
    if (!m_actions.isEmpty()) ui->listActions->setCurrentRow(0);
}

QList<PluginActionInstance> ActionSetSettings::getActions() const {
    const_cast<ActionSetSettings*>(this)->saveCurrentParams();
    return m_actions;
}

void ActionSetSettings::updateActionList() {
    ui->listActions->blockSignals(true);
    int current = ui->listActions->currentRow();
    ui->listActions->clear();
    for (int i = 0; i < m_actions.size(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(QString("%1. %2").arg(i + 1).arg(m_actions[i].pluginName), ui->listActions);
        item->setData(Qt::UserRole, i);
    }
    ui->listActions->setCurrentRow(current);
    ui->listActions->blockSignals(false);
}

void ActionSetSettings::onAddAction() {
    QMenu menu(this);
    for (const auto& info : m_plugins) {
        QMenu* sub = menu.addMenu(info.plugin->name());
        QAction* actNew = sub->addAction("New " + info.plugin->name() + " Action");
        connect(actNew, &QAction::triggered, [this, info]() {
            saveCurrentParams();
            m_actions.append({info.plugin->name(), {}});
            updateActionList();
            ui->listActions->setCurrentRow(m_actions.size() - 1);
        });
        auto templates = info.plugin->actionTemplates();
        if (!templates.isEmpty()) {
            sub->addSeparator();
            for (const auto& tmpl : templates) {
                QAction* act = sub->addAction(tmpl.name);
                connect(act, &QAction::triggered, [this, info, tmpl]() {
                    saveCurrentParams();
                    m_actions.append({info.plugin->name(), tmpl.defaultParameters});
                    updateActionList();
                    ui->listActions->setCurrentRow(m_actions.size() - 1);
                });
            }
        }
    }
    menu.exec(QCursor::pos());
}

void ActionSetSettings::onRemoveAction() {
    int row = ui->listActions->currentRow();
    if (row >= 0) {
        m_actions.removeAt(row);
        m_lastRow = -1;
        updateActionList();
        loadParamsForAction(-1);
    }
}

void ActionSetSettings::onMoveUp() {
    int row = ui->listActions->currentRow();
    if (row > 0) {
        saveCurrentParams();
        m_actions.swapItemsAt(row, row - 1);
        updateActionList();
        ui->listActions->setCurrentRow(row - 1);
    }
}

void ActionSetSettings::onMoveDown() {
    int row = ui->listActions->currentRow();
    if (row >= 0 && row < m_actions.size() - 1) {
        saveCurrentParams();
        m_actions.swapItemsAt(row, row + 1);
        updateActionList();
        ui->listActions->setCurrentRow(row + 1);
    }
}

void ActionSetSettings::onActionSelected(int row) {
    if (row == m_lastRow) return;
    saveCurrentParams();
    loadParamsForAction(row);
}

void ActionSetSettings::saveCurrentParams() {
    if (m_lastRow < 0 || m_lastRow >= m_actions.size()) return;
    QVariantMap values;
    for (const auto& def : m_currentDefs) {
        QWidget* widget = m_paramWidgets.value(def.id);
        if (!widget) continue;
        QVariant val;
        switch (def.type) {
            case ParameterType::String: case ParameterType::Password: val = qobject_cast<QLineEdit*>(widget)->text(); break;
            case ParameterType::Text: val = qobject_cast<QTextEdit*>(widget)->toPlainText(); break;
            case ParameterType::Bool: val = qobject_cast<QCheckBox*>(widget)->isChecked(); break;
            case ParameterType::Choice: val = qobject_cast<QComboBox*>(widget)->currentText(); break;
            case ParameterType::Number: val = qobject_cast<QSpinBox*>(widget)->value(); break;
            case ParameterType::File: case ParameterType::Directory: 
                if (QLineEdit* le = widget->findChild<QLineEdit*>("PathEdit")) val = le->text(); 
                break;
        }
        values.insert(def.id, val);
    }
    m_actions[m_lastRow].parameters = values;
}

void ActionSetSettings::loadParamsForAction(int row) {
    m_lastRow = row;
    m_paramWidgets.clear();
    m_currentDefs.clear();
    
    // 清除 ScrollArea 的內容
    QWidget* oldWidget = ui->scrollArea->takeWidget();
    if (oldWidget) oldWidget->deleteLater();

    if (row < 0 || row >= m_actions.size()) return;
    
    PluginActionInstance& action = m_actions[row];
    IClipboardPlugin* plugin = nullptr;
    for (const auto& info : m_plugins) { if (info.plugin->name() == action.pluginName) { plugin = info.plugin; break; } }
    if (!plugin) return;

    QWidget* container = new QWidget();
    QVBoxLayout* mainVBox = new QVBoxLayout(container);
    QFormLayout* form = new QFormLayout();
    
    m_currentDefs = plugin->actionParameterDefinitions();
    for (const auto& def : m_currentDefs) {
        QWidget* widget = nullptr;
        QVariant val = action.parameters.value(def.id, def.defaultValue);
        switch (def.type) {
            case ParameterType::String: { QLineEdit* e = new QLineEdit(container); e->setText(val.toString()); widget = e; break; }
            case ParameterType::Password: { QLineEdit* e = new QLineEdit(container); e->setEchoMode(QLineEdit::Password); e->setText(val.toString()); widget = e; break; }
            case ParameterType::Text: { QTextEdit* e = new QTextEdit(container); e->setPlainText(val.toString()); e->setMaximumHeight(80); widget = e; break; }
            case ParameterType::Bool: { QCheckBox* c = new QCheckBox(container); c->setChecked(val.toBool()); widget = c; break; }
            case ParameterType::Choice: { QComboBox* c = new QComboBox(container); c->addItems(def.options); c->setCurrentText(val.toString()); widget = c; break; }
            case ParameterType::Number: { QSpinBox* s = new QSpinBox(container); s->setRange(-999,999999); s->setValue(val.toInt()); widget = s; break; }
            case ParameterType::File: case ParameterType::Directory: {
                QWidget* c = new QWidget(container); QHBoxLayout* h = new QHBoxLayout(c); h->setContentsMargins(0,0,0,0);
                QLineEdit* e = new QLineEdit(c); e->setText(val.toString()); e->setObjectName("PathEdit");
                QPushButton* b = new QPushButton("...", c); b->setFixedWidth(30); h->addWidget(e); h->addWidget(b);
                connect(b, &QPushButton::clicked, [this, e, def]() {
                    QString p = (def.type == ParameterType::File) ? QFileDialog::getOpenFileName(this, "Select File", e->text()) : QFileDialog::getExistingDirectory(this, "Select Directory", e->text());
                    if (!p.isEmpty()) e->setText(QDir::toNativeSeparators(p));
                });
                widget = c; break;
            }
        }
        if (widget) { 
            widget->setToolTip(def.description);
            form->addRow(def.name + ":", widget); 
            m_paramWidgets.insert(def.id, widget); 
        }
    }
    mainVBox->addLayout(form);
    mainVBox->addStretch();
    ui->scrollArea->setWidget(container);
}