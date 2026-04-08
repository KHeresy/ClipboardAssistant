#include "GemmiSettings.h"
#include <QSettings>
#include <QUuid>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

namespace {
QString normalizeAuthMode(const QString& value)
{
    const QString v = value.trimmed().toLower();
    return v == "api-key" ? "api-key" : "Bearer";
}

QUrl buildGeminiUrl(QString baseUrl, QString suffix, const QString& model, const QString& key, const QString& authMode)
{
    if (baseUrl.endsWith('/')) baseUrl.chop(1);
    if (suffix.isEmpty()) suffix = "/models/{model}:generateContent";
    if (!suffix.startsWith('/')) suffix = "/" + suffix;
    suffix.replace("{model}", model);

    QUrl url(baseUrl + suffix);
    if (authMode == "api-key") {
        QUrlQuery query(url);
        if (!query.hasQueryItem("key")) query.addQueryItem("key", key);
        url.setQuery(query);
    }
    return url;
}
}

GemmiSettings::GemmiSettings(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::GemmiSettingsClass)
{
    ui->setupUi(this);
    m_networkManager = new QNetworkAccessManager(this);

    loadAccounts();

    connect(ui->btnAddAccount, &QPushButton::clicked, this, &GemmiSettings::onAddAccount);
    connect(ui->btnRemoveAccount, &QPushButton::clicked, this, &GemmiSettings::onRemoveAccount);
    connect(ui->listAccounts, &QListWidget::itemSelectionChanged, this, &GemmiSettings::onAccountSelected);

    connect(ui->editName, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->editKey, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->editModel, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->editPrompt, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->editUrl, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->editSuffix, &QLineEdit::textChanged, this, &GemmiSettings::onFieldChanged);
    connect(ui->comboAuthMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GemmiSettings::onFieldChanged);
    connect(ui->btnTest, &QPushButton::clicked, this, &GemmiSettings::onTestAccount);

    if (ui->listAccounts->count() > 0) ui->listAccounts->setCurrentRow(0);
    else ui->groupDetails->setEnabled(false);
}

GemmiSettings::~GemmiSettings()
{
    delete ui;
}

void GemmiSettings::loadAccounts()
{
    m_loading = true;
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("Gemmi/Accounts");
    const QStringList ids = s.childGroups();
    for (const QString& id : ids) {
        s.beginGroup(id);
        GemmiAccount acc;
        acc.id = id;
        acc.displayName = s.value("Name").toString().trimmed();
        acc.apiKey = s.value("Key").toString().trimmed();
        acc.model = s.value("Model", "gemini-2.0-flash").toString().trimmed();
        acc.systemPrompt = s.value("SystemPrompt", "You are a helpful assistant.").toString().trimmed();
        acc.baseUrl = s.value("Url", "https://generativelanguage.googleapis.com/v1beta").toString().trimmed();
        acc.customSuffix = s.value("Suffix", "/models/{model}:generateContent").toString().trimmed();
        acc.authMode = normalizeAuthMode(s.value("AuthMode", "Bearer").toString());
        m_accounts.insert(id, acc);

        QListWidgetItem* item = new QListWidgetItem(acc.displayName, ui->listAccounts);
        item->setData(Qt::UserRole, id);
        s.endGroup();
    }
    s.endGroup();
    m_loading = false;
}

void GemmiSettings::onAddAccount()
{
    QString id = QUuid::createUuid().toString(QUuid::Id128);
    GemmiAccount acc;
    acc.id = id;
    acc.displayName = QCoreApplication::translate("GemmiSettings", "New Account");
    acc.model = "gemini-2.0-flash";
    acc.systemPrompt = "You are a helpful assistant.";
    acc.baseUrl = "https://generativelanguage.googleapis.com/v1beta";
    acc.customSuffix = "/models/{model}:generateContent";
    acc.authMode = "api-key";

    m_accounts.insert(id, acc);
    QListWidgetItem* item = new QListWidgetItem(acc.displayName, ui->listAccounts);
    item->setData(Qt::UserRole, id);
    ui->listAccounts->setCurrentItem(item);
}

void GemmiSettings::onRemoveAccount()
{
    QListWidgetItem* item = ui->listAccounts->currentItem();
    if (!item) return;

    m_currentAccountId.clear();
    m_accounts.remove(item->data(Qt::UserRole).toString());
    delete item;
    ui->groupDetails->setEnabled(false);

    if (ui->listAccounts->count() > 0) ui->listAccounts->setCurrentRow(0);
}

void GemmiSettings::onAccountSelected()
{
    saveCurrentToMap();

    QListWidgetItem* item = ui->listAccounts->currentItem();
    if (!item) return;

    m_loading = true;
    m_currentAccountId = item->data(Qt::UserRole).toString();
    ui->groupDetails->setEnabled(true);

    const GemmiAccount& acc = m_accounts[m_currentAccountId];
    ui->editName->setText(acc.displayName);
    ui->editKey->setText(acc.apiKey);
    ui->editModel->setText(acc.model);
    ui->editPrompt->setText(acc.systemPrompt);
    ui->editUrl->setText(acc.baseUrl);
    ui->editSuffix->setText(acc.customSuffix);
    ui->comboAuthMode->setCurrentText(acc.authMode == "api-key" ? "api-key" : "Authorization: Bearer");

    updateHelp();
    m_loading = false;
}

void GemmiSettings::onFieldChanged()
{
    if (m_loading) return;
    updateHelp();
    if (auto* item = ui->listAccounts->currentItem()) item->setText(ui->editName->text());
}

void GemmiSettings::updateHelp()
{
    QString helpText = QCoreApplication::translate("GemmiSettings", "Gemini Native URL: ") + "https://generativelanguage.googleapis.com/v1beta";
    if (ui->editSuffix->text().trimmed().isEmpty()) {
        helpText += "<br>" + QCoreApplication::translate("GemmiSettings", "Default suffix will be: /models/{model}:generateContent");
    }
    ui->labelHelp->setText(helpText);
    ui->labelHelp->setStyleSheet("font-size: 10px; color: gray;");
}

void GemmiSettings::onTestAccount()
{
    const QString key = ui->editKey->text().trimmed();
    const QString model = ui->editModel->text().trimmed();
    const QString urlStr = ui->editUrl->text().trimmed();
    QString suffix = ui->editSuffix->text().trimmed();
    const QString auth = normalizeAuthMode(ui->comboAuthMode->currentText());

    if (key.isEmpty() || urlStr.isEmpty() || model.isEmpty()) {
        QMessageBox::warning(this, QCoreApplication::translate("GemmiSettings", "Test Connection"), QCoreApplication::translate("GemmiSettings", "API Key, Model and URL are required."));
        return;
    }

    ui->btnTest->setEnabled(false);
    ui->btnTest->setText(QCoreApplication::translate("GemmiSettings", "Testing..."));

    const QUrl url = buildGeminiUrl(urlStr, suffix, model, key, auth);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (auth == "api-key") req.setRawHeader("x-goog-api-key", key.toUtf8());
    else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    QJsonObject json;
    QJsonObject userContent;
    userContent["role"] = "user";
    QJsonArray parts;
    QJsonObject part;
    part["text"] = "Ping";
    parts.append(part);
    userContent["parts"] = parts;

    QJsonArray contents;
    contents.append(userContent);
    json["contents"] = contents;

    QJsonObject generationConfig;
    generationConfig["maxOutputTokens"] = 8;
    json["generationConfig"] = generationConfig;

    QNetworkReply* reply = m_networkManager->post(req, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        ui->btnTest->setEnabled(true);
        ui->btnTest->setText(QCoreApplication::translate("GemmiSettings", "Test Connection"));

        if (reply->error() == QNetworkReply::NoError) {
            QMessageBox::information(this, QCoreApplication::translate("GemmiSettings", "Test Connection"), QCoreApplication::translate("GemmiSettings", "Connection successful!"));
        } else {
            QString errorMsg = reply->errorString();
            const QByteArray data = reply->readAll();
            if (!data.isEmpty()) errorMsg += "\n\n" + QString::fromUtf8(data);
            QMessageBox::critical(this, QCoreApplication::translate("GemmiSettings", "Test Connection"), QCoreApplication::translate("GemmiSettings", "Connection failed: %1").arg(errorMsg));
        }
        reply->deleteLater();
    });
}

void GemmiSettings::saveCurrentToMap()
{
    if (m_currentAccountId.isEmpty() || !m_accounts.contains(m_currentAccountId)) return;

    GemmiAccount& acc = m_accounts[m_currentAccountId];
    acc.displayName = ui->editName->text().trimmed();
    acc.apiKey = ui->editKey->text().trimmed();
    acc.model = ui->editModel->text().trimmed();
    acc.systemPrompt = ui->editPrompt->text().trimmed();
    acc.baseUrl = ui->editUrl->text().trimmed();
    acc.customSuffix = ui->editSuffix->text().trimmed();
    acc.authMode = normalizeAuthMode(ui->comboAuthMode->currentText());
}

void GemmiSettings::accept()
{
    saveCurrentToMap();

    QSettings s("Heresy", "ClipboardAssistant");
    s.remove("Gemmi/Accounts");
    s.beginGroup("Gemmi/Accounts");
    for (auto it = m_accounts.begin(); it != m_accounts.end(); ++it) {
        s.beginGroup(it.key());
        s.setValue("Name", it.value().displayName);
        s.setValue("Key", it.value().apiKey);
        s.setValue("Model", it.value().model);
        s.setValue("SystemPrompt", it.value().systemPrompt);
        s.setValue("Url", it.value().baseUrl);
        s.setValue("Suffix", it.value().customSuffix);
        s.setValue("AuthMode", it.value().authMode);
        s.endGroup();
    }
    s.endGroup();

    QDialog::accept();
}
