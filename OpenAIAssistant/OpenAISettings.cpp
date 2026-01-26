#include "OpenAISettings.h"
#include <QSettings>
#include <QUuid>
#include <QMessageBox>

OpenAISettings::OpenAISettings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OpenAISettingsClass)
{
    ui->setupUi(this);

    loadAccounts();

    connect(ui->btnAddAccount, &QPushButton::clicked, this, &OpenAISettings::onAddAccount);
    connect(ui->btnRemoveAccount, &QPushButton::clicked, this, &OpenAISettings::onRemoveAccount);
    connect(ui->listAccounts, &QListWidget::itemSelectionChanged, this, &OpenAISettings::onAccountSelected);

    connect(ui->editName, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editKey, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editModel, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editUrl, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->checkAzure, &QCheckBox::toggled, this, &OpenAISettings::onFieldChanged);

    if (ui->listAccounts->count() > 0) {
        ui->listAccounts->setCurrentRow(0);
    } else {
        ui->groupDetails->setEnabled(false);
    }
}

OpenAISettings::~OpenAISettings()
{
    delete ui;
}

void OpenAISettings::loadAccounts()
{
    m_loading = true;
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    QStringList ids = s.childGroups();
    for (const QString& id : ids) {
        s.beginGroup(id);
        OpenAIAccount acc;
        acc.id = id;
        acc.displayName = s.value("Name").toString();
        acc.apiKey = s.value("Key").toString();
        acc.model = s.value("Model").toString();
        acc.baseUrl = s.value("Url").toString();
        acc.isAzure = s.value("IsAzure").toBool();
        m_accounts.insert(id, acc);
        
        QListWidgetItem* item = new QListWidgetItem(acc.displayName, ui->listAccounts);
        item->setData(Qt::UserRole, id);
        s.endGroup();
    }
    s.endGroup();
    m_loading = false;
}

void OpenAISettings::onAddAccount()
{
    QString id = QUuid::createUuid().toString(QUuid::Id128);
    OpenAIAccount acc;
    acc.id = id;
    acc.displayName = "New Account";
    acc.isAzure = false;
    acc.model = "gpt-3.5-turbo";
    acc.baseUrl = "https://api.openai.com/v1";

    m_accounts.insert(id, acc);
    QListWidgetItem* item = new QListWidgetItem(acc.displayName, ui->listAccounts);
    item->setData(Qt::UserRole, id);
    ui->listAccounts->setCurrentItem(item);
}

void OpenAISettings::onRemoveAccount()
{
    QListWidgetItem* item = ui->listAccounts->currentItem();
    if (!item) return;

    QString id = item->data(Qt::UserRole).toString();
    m_accounts.remove(id);
    delete item;

    if (ui->listAccounts->count() == 0) {
        m_currentAccountId.clear();
        ui->groupDetails->setEnabled(false);
    }
}

void OpenAISettings::onAccountSelected()
{
    saveCurrentToMap(); // Save changes of previous selection

    QListWidgetItem* item = ui->listAccounts->currentItem();
    if (!item) return;

    m_loading = true;
    m_currentAccountId = item->data(Qt::UserRole).toString();
    ui->groupDetails->setEnabled(true);

    OpenAIAccount acc = m_accounts[m_currentAccountId];
    ui->editName->setText(acc.displayName);
    ui->editKey->setText(acc.apiKey);
    ui->editModel->setText(acc.model);
    ui->editUrl->setText(acc.baseUrl);
    ui->checkAzure->setChecked(acc.isAzure);
    
    updateHelp();
    m_loading = false;
}

void OpenAISettings::onFieldChanged()
{
    if (m_loading) return;
    updateHelp();
    
    QListWidgetItem* item = ui->listAccounts->currentItem();
    if (item) {
        item->setText(ui->editName->text());
    }
}

void OpenAISettings::updateHelp()
{
    if (ui->checkAzure->isChecked()) {
        ui->labelHelp->setText("Azure URL: https://{res}.openai.azure.com/openai/deployments/{dep}/chat/completions?api-version=2024-05-01-preview");
    } else {
        ui->labelHelp->setText("OpenAI URL: https://api.openai.com/v1");
    }
}

void OpenAISettings::saveCurrentToMap()
{
    if (m_currentAccountId.isEmpty()) return;

    OpenAIAccount& acc = m_accounts[m_currentAccountId];
    acc.displayName = ui->editName->text();
    acc.apiKey = ui->editKey->text();
    acc.model = ui->editModel->text();
    acc.baseUrl = ui->editUrl->text();
    acc.isAzure = ui->checkAzure->isChecked();
}

void OpenAISettings::accept()
{
    saveCurrentToMap();

    QSettings s("Heresy", "ClipboardAssistant");
    s.remove("OpenAI/Accounts"); // Clear and rewrite
    s.beginGroup("OpenAI/Accounts");
    for (auto it = m_accounts.begin(); it != m_accounts.end(); ++it) {
        s.beginGroup(it.key());
        s.setValue("Name", it.value().displayName);
        s.setValue("Key", it.value().apiKey);
        s.setValue("Model", it.value().model);
        s.setValue("Url", it.value().baseUrl);
        s.setValue("IsAzure", it.value().isAzure);
        s.endGroup();
    }
    s.endGroup();
    QDialog::accept();
}