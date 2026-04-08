#include "OpenAISettings.h"
#include <QSettings>
#include <QUuid>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {
QString normalizeAuthMode(const QString& value)
{
    const QString v = value.trimmed().toLower();
    return v == "api-key" ? "api-key" : "Bearer";
}

QString normalizeApiType(const QString& value)
{
    return value.trimmed().compare("Chat", Qt::CaseInsensitive) == 0 ? "Chat" : "Completions";
}
}

OpenAISettings::OpenAISettings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OpenAISettingsClass)
{
    ui->setupUi(this);

    m_networkManager = new QNetworkAccessManager(this);

    loadAccounts();

    connect(ui->btnAddAccount, &QPushButton::clicked, this, &OpenAISettings::onAddAccount);
    connect(ui->btnRemoveAccount, &QPushButton::clicked, this, &OpenAISettings::onRemoveAccount);
    connect(ui->listAccounts, &QListWidget::itemSelectionChanged, this, &OpenAISettings::onAccountSelected);

    connect(ui->editName, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editKey, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editModel, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editPrompt, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editUrl, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->editSuffix, &QLineEdit::textChanged, this, &OpenAISettings::onFieldChanged);
    connect(ui->comboApiType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenAISettings::onFieldChanged);
    connect(ui->comboAuthMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenAISettings::onFieldChanged);
    connect(ui->checkAzure, &QCheckBox::toggled, this, &OpenAISettings::onFieldChanged);
    connect(ui->btnTest, &QPushButton::clicked, this, &OpenAISettings::onTestAccount);

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
        acc.displayName = s.value("Name").toString().trimmed();
        acc.apiKey = s.value("Key").toString().trimmed();
        acc.model = s.value("Model").toString().trimmed();
        acc.systemPrompt = s.value("SystemPrompt").toString().trimmed();
        acc.baseUrl = s.value("Url").toString().trimmed();
        acc.customSuffix = s.value("Suffix").toString().trimmed();
        acc.apiType = normalizeApiType(s.value("ApiType", "Chat").toString());
        acc.authMode = normalizeAuthMode(s.value("AuthMode", "Bearer").toString());
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
    acc.displayName = QCoreApplication::translate("OpenAISettings", "New Account");
    acc.isAzure = false;
    acc.model = "gpt-3.5-turbo";
    acc.systemPrompt = "You are a helpful assistant.";
    acc.baseUrl = "https://api.openai.com/v1";
    acc.apiType = "Chat";
    acc.authMode = "Bearer";
    acc.customSuffix = "";

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
    
    m_currentAccountId.clear(); // Clear it first
    m_accounts.remove(id);
    
    delete item;
    ui->groupDetails->setEnabled(false);

    if (ui->listAccounts->count() > 0) {
        ui->listAccounts->setCurrentRow(0);
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
    ui->editPrompt->setText(acc.systemPrompt);
    ui->editUrl->setText(acc.baseUrl);
    ui->editSuffix->setText(acc.customSuffix);
    ui->comboApiType->setCurrentText(acc.apiType == "Chat" ? "Chat" : "Completions (Legacy)");
    ui->comboAuthMode->setCurrentText(acc.authMode == "api-key" ? "api-key" : "Authorization: Bearer");
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
    QString url = ui->editUrl->text().trimmed();
    bool isAzure = ui->checkAzure->isChecked();
    QString helpText;
    QString style = "font-size: 10px; ";

    if (isAzure) {
        helpText = QCoreApplication::translate("OpenAISettings", "Azure URL: ") + "https://{res}.openai.azure.com/openai/deployments/{dep}/chat/completions?api-version=2024-05-01-preview";
        if (!url.isEmpty() && !url.contains("api-version=")) {
            helpText += "<br><font color='red'>" + QCoreApplication::translate("OpenAISettings", "Warning: Azure URL usually requires '?api-version=' parameter.") + "</font>";
        }
    } else {
        helpText = QCoreApplication::translate("OpenAISettings", "OpenAI URL: ") + "https://api.openai.com/v1";
        QString suffix = ui->editSuffix->text().trimmed();
        if (url.contains("/chat/completions") || url.contains("/completions")) {
            helpText += "<br><font color='orange'>" + QCoreApplication::translate("OpenAISettings", "Warning: Base URL should usually NOT include endpoints like '/chat/completions'.") + "</font>";
        }
        if (suffix.isEmpty()) {
             QString type = ui->comboApiType->currentText();
             helpText += "<br>" + QCoreApplication::translate("OpenAISettings", "Default suffix will be: ") + (type == "Chat" ? "/chat/completions" : "/completions");
        }
    }
    
    ui->labelHelp->setText(helpText);
    ui->labelHelp->setStyleSheet(style + "color: gray;");
}

void OpenAISettings::onTestAccount()
{
    QString key = ui->editKey->text().trimmed();
    QString model = ui->editModel->text().trimmed();
    QString urlStr = ui->editUrl->text().trimmed();
    QString suffix = ui->editSuffix->text().trimmed();
    QString type = normalizeApiType(ui->comboApiType->currentText());
    QString auth = normalizeAuthMode(ui->comboAuthMode->currentText());
    bool isAz = ui->checkAzure->isChecked();

    if (key.isEmpty() || urlStr.isEmpty()) {
        QMessageBox::warning(this, QCoreApplication::translate("OpenAISettings", "Test Connection"), QCoreApplication::translate("OpenAISettings", "API Key and URL are required."));
        return;
    }

    ui->btnTest->setEnabled(false);
    ui->btnTest->setText(QCoreApplication::translate("OpenAISettings", "Testing..."));

    QUrl url;
    if (isAz) {
        url = QUrl(urlStr);
    } else {
        if (urlStr.endsWith("/")) urlStr.chop(1);
        if (suffix.isEmpty()) suffix = (type == "Chat" ? "/chat/completions" : "/completions");
        if (!suffix.startsWith("/")) suffix = "/" + suffix;
        url = QUrl(urlStr + suffix);
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (auth == "api-key") req.setRawHeader("api-key", key.toUtf8());
    else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    QJsonObject json;
    json["model"] = model;

    if (type == "Chat") {
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "Ping";
        QJsonArray msgs;
        msgs.append(msg);
        json["messages"] = msgs;
    } else {
        json["prompt"] = "Ping";
        json["max_tokens"] = 5;
    }

    QNetworkReply* reply = m_networkManager->post(req, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        ui->btnTest->setEnabled(true);
        ui->btnTest->setText(QCoreApplication::translate("OpenAISettings", "Test Connection"));

        if (reply->error() == QNetworkReply::NoError) {
            QMessageBox::information(this, QCoreApplication::translate("OpenAISettings", "Test Connection"), QCoreApplication::translate("OpenAISettings", "Connection successful!"));
        } else {
            QByteArray data = reply->readAll();
            QString errorMsg = reply->errorString();
            if (!data.isEmpty()) {
                errorMsg += "\n\n" + QString::fromUtf8(data);
            }
            QMessageBox::critical(this, QCoreApplication::translate("OpenAISettings", "Test Connection"), QCoreApplication::translate("OpenAISettings", "Connection failed: %1").arg(errorMsg));
        }
        reply->deleteLater();
    });
}

void OpenAISettings::saveCurrentToMap()
{
    if (m_currentAccountId.isEmpty() || !m_accounts.contains(m_currentAccountId)) return;

    OpenAIAccount& acc = m_accounts[m_currentAccountId];
    acc.displayName = ui->editName->text().trimmed();
    acc.apiKey = ui->editKey->text().trimmed();
    acc.model = ui->editModel->text().trimmed();
    acc.systemPrompt = ui->editPrompt->text().trimmed();
    acc.baseUrl = ui->editUrl->text().trimmed();
    acc.customSuffix = ui->editSuffix->text().trimmed();
    acc.apiType = normalizeApiType(ui->comboApiType->currentText());
    acc.authMode = normalizeAuthMode(ui->comboAuthMode->currentText());
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
        s.setValue("SystemPrompt", it.value().systemPrompt);
        s.setValue("Url", it.value().baseUrl);
        s.setValue("Suffix", it.value().customSuffix);
        s.setValue("ApiType", it.value().apiType);
        s.setValue("AuthMode", it.value().authMode);
        s.setValue("IsAzure", it.value().isAzure);
        s.endGroup();
    }
    s.endGroup();
    QDialog::accept();
}