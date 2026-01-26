#include "OpenAIAssistant.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>
#include <QUuid>

OpenAIAssistant::OpenAIAssistant()
{
    m_networkManager = new QNetworkAccessManager(this);
    ensureDefaultActions();
}

OpenAIAssistant::~OpenAIAssistant()
{
}

QString OpenAIAssistant::name() const
{
    return "OpenAI Assistant";
}

QString OpenAIAssistant::version() const
{
    return "1.1.0";
}

void OpenAIAssistant::ensureDefaultActions()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.beginGroup("OpenAI/Actions");
    if (settings.childGroups().isEmpty()) {
        settings.endGroup(); // Close before writing
        
        // Write defaults
        auto addDefault = [&](const QString& name, const QString& prompt) {
            QString id = QUuid::createUuid().toString(QUuid::Id128);
            settings.beginGroup("OpenAI/Actions/" + id);
            settings.setValue("Name", name);
            settings.setValue("Prompt", prompt);
            settings.endGroup();
        };

        addDefault("Summarize", "Summarize the following text:");
        addDefault("Translate to English", "Translate the following text to English:");
        addDefault("Fix Grammar", "Fix grammar errors in the following text:");
    } else {
        settings.endGroup();
    }
}

QList<PluginFeature> OpenAIAssistant::features() const
{
    QList<PluginFeature> list;
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.beginGroup("OpenAI/Actions");
    QStringList ids = settings.childGroups();
    
    for (const QString& id : ids) {
        settings.beginGroup(id);
        PluginFeature f;
        f.id = id;
        f.name = settings.value("Name").toString();
        f.description = settings.value("Prompt").toString();
        f.customShortcut = QKeySequence(settings.value("Shortcut").toString());
        f.isCustomShortcutGlobal = settings.value("IsGlobal", false).toBool();
        settings.endGroup();
        list.append(f);
    }
    settings.endGroup();
    return list;
}

void OpenAIAssistant::process(const QString& featureId, const QMimeData* data, IPluginCallback* callback)
{
    if (!data->hasText()) {
        callback->onError("No text in clipboard");
        return;
    }

    QString text = data->text();
    
        QSettings settings("Heresy", "ClipboardAssistant");
        QString apiKey = settings.value("OpenAI/ApiKey").toString();
        QString model = settings.value("OpenAI/Model", "gpt-3.5-turbo").toString();
        QString baseUrl = settings.value("OpenAI/BaseUrl", "https://api.openai.com/v1").toString();
        bool isAzure = settings.value("OpenAI/IsAzure", false).toBool();
    
        if (apiKey.isEmpty()) {
            callback->onError("OpenAI API Key is not set. Please configure in settings.");
            return;
        }
    
            // Load prompt from settings based on ID
    QString systemInstruction;
    QString promptKey = "OpenAI/Actions/" + featureId + "/Prompt";
    if (settings.contains(promptKey)) {
        systemInstruction = settings.value(promptKey).toString();
    } else {
        systemInstruction = "You are a helpful assistant."; 
    }

    QJsonArray messages;
            
            QJsonObject systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = systemInstruction;
            messages.append(systemMsg);
        
            QJsonObject userMsg;
            userMsg["role"] = "user";
            userMsg["content"] = text;
            messages.append(userMsg);
        
            QJsonObject json;
            json["model"] = model;
            json["messages"] = messages;
            json["stream"] = true;    
        QJsonDocument doc(json);
        QByteArray postData = doc.toJson();
    
        QUrl url;
        QNetworkRequest request;
        if (isAzure) {
            url = QUrl(baseUrl); // Azure expects full URL in settings
            request.setRawHeader("api-key", apiKey.toUtf8());
        } else {
            url = QUrl(baseUrl + "/chat/completions");
            request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
        }
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
            QNetworkReply* reply = m_networkManager->post(request, postData);
    
        
    
            // Buffer for non-streaming response
    
            QByteArray* responseBuffer = new QByteArray();
    
        
    
            connect(reply, &QNetworkReply::readyRead, [reply, callback, responseBuffer]() {
    
                while(reply->canReadLine()) {
    
                    QByteArray line = reply->readLine().trimmed();
    
                    if (line.startsWith("data: ")) {
    
                        QByteArray data = line.mid(6);
    
                        if (data == "[DONE]") continue;
    
                        
    
                        QJsonDocument doc = QJsonDocument::fromJson(data);
    
                        if (doc.isObject()) {
    
                            QJsonObject obj = doc.object();
    
                            if (obj.contains("choices")) {
    
                                QJsonArray choices = obj["choices"].toArray();
    
                                if (!choices.isEmpty()) {
    
                                    QJsonObject choice = choices[0].toObject();
    
                                    if (choice.contains("delta")) {
    
                                        QJsonObject delta = choice["delta"].toObject();
    
                                        if (delta.contains("content")) {
    
                                            callback->onTextData(delta["content"].toString(), false);
    
                                        }
    
                                    }
    
                                }
    
                            }
    
                        }
    
                    } else {
    
                        // If it doesn't look like SSE, buffer it as standard JSON
    
                        responseBuffer->append(line);
    
                    }
    
                }
    
            });
    
        
    
            connect(reply, &QNetworkReply::finished, [reply, callback, responseBuffer]() {
    
                if (reply->error() != QNetworkReply::NoError) {
    
                    QString errorMsg = reply->errorString();
    
                    if (!responseBuffer->isEmpty()) {
    
                        errorMsg += "\nResponse: " + QString::fromUtf8(*responseBuffer);
    
                    }
    
                    callback->onError(errorMsg);
    
                } else {
    
                    // If streaming didn't produce output, try parsing as a whole JSON
    
                    if (!responseBuffer->isEmpty()) {
    
                        QJsonDocument doc = QJsonDocument::fromJson(*responseBuffer);
    
                        if (doc.isObject()) {
    
                            QJsonObject obj = doc.object();
    
                            if (obj.contains("choices")) {
    
                                QJsonArray choices = obj["choices"].toArray();
    
                                if (!choices.isEmpty()) {
    
                                    QJsonObject choice = choices[0].toObject();
    
                                    if (choice.contains("message")) {
    
                                        QJsonObject msg = choice["message"].toObject();
    
                                        callback->onTextData(msg["content"].toString(), true);
    
                                    }
    
                                }
    
                            }
    
                        }
    
                    }
    
                    callback->onTextData("", true); 
    
                    callback->onFinished();
    
                }
    
                delete responseBuffer;
    
                reply->deleteLater();
    
            });
}

bool OpenAIAssistant::hasSettings() const
{
    return true;
}

#include <QCheckBox>

void OpenAIAssistant::showSettings(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("OpenAI Assistant Settings");
    dialog.resize(450, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QSettings s("Heresy", "ClipboardAssistant");

    QCheckBox* checkAzure = new QCheckBox("Use Azure OpenAI", &dialog);
    checkAzure->setChecked(s.value("OpenAI/IsAzure", false).toBool());
    layout->addWidget(checkAzure);
    
    auto addRow = [&](const QString& labelText, const QString& key, bool isPassword = false) {
        QLabel* label = new QLabel(labelText, &dialog);
        QLineEdit* edit = new QLineEdit(&dialog);
        if (isPassword) edit->setEchoMode(QLineEdit::Password);
        edit->setText(s.value(key).toString());
        if (key == "OpenAI/Model" && edit->text().isEmpty()) edit->setText("gpt-3.5-turbo");
        if (key == "OpenAI/BaseUrl" && edit->text().isEmpty()) edit->setText("https://api.openai.com/v1");
        
        layout->addWidget(label);
        layout->addWidget(edit);
        return edit;
    };

    QLineEdit* editKey = addRow("API Key:", "OpenAI/ApiKey", true);
    QLineEdit* editModel = addRow("Model (Deployment Name for Azure):", "OpenAI/Model");
    QLineEdit* editUrl = addRow("Base URL (Full URL for Azure):", "OpenAI/BaseUrl");
    
    QLabel* helpLabel = new QLabel(&dialog);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: gray; font-size: 10px;");
    auto updateHelp = [&](bool isAzure) {
        if (isAzure) helpLabel->setText("Azure URL: https://{res}.openai.azure.com/openai/deployments/{dep}/chat/completions?api-version=2024-02-15-preview");
        else helpLabel->setText("OpenAI URL: https://api.openai.com/v1");
    };
    updateHelp(checkAzure->isChecked());
    connect(checkAzure, &QCheckBox::toggled, updateHelp);
    layout->addWidget(helpLabel);

    QPushButton* btnSave = new QPushButton("Save", &dialog);
    layout->addWidget(btnSave);
    
    QObject::connect(btnSave, &QPushButton::clicked, [&]() {
        QSettings s("Heresy", "ClipboardAssistant");
        s.setValue("OpenAI/IsAzure", checkAzure->isChecked());
        s.setValue("OpenAI/ApiKey", editKey->text());
        s.setValue("OpenAI/Model", editModel->text());
        s.setValue("OpenAI/BaseUrl", editUrl->text());
        dialog.accept();
    });
    
    dialog.exec();
}

bool OpenAIAssistant::isEditable() const
{
    return true;
}

QString OpenAIAssistant::createFeature(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Add New Action");
    dialog.resize(400, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* lName = new QLabel("Action Name:", &dialog);
    QLineEdit* eName = new QLineEdit(&dialog);
    
    QLabel* lPrompt = new QLabel("System Prompt:", &dialog);
    QTextEdit* ePrompt = new QTextEdit(&dialog);
    
    QLabel* lShortcut = new QLabel("Custom Shortcut (Optional):", &dialog);
    QKeySequenceEdit* eShortcut = new QKeySequenceEdit(&dialog);
    
    QCheckBox* cGlobal = new QCheckBox("Register as Global Hotkey", &dialog);

    layout->addWidget(lName);
    layout->addWidget(eName);
    layout->addWidget(lPrompt);
    layout->addWidget(ePrompt);
    layout->addWidget(lShortcut);
    layout->addWidget(eShortcut);
    layout->addWidget(cGlobal);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (eName->text().trimmed().isEmpty()) return QString();
        
        QString id = QUuid::createUuid().toString(QUuid::Id128);
        QSettings settings("Heresy", "ClipboardAssistant");
        settings.beginGroup("OpenAI/Actions/" + id);
        settings.setValue("Name", eName->text());
        settings.setValue("Prompt", ePrompt->toPlainText());
        settings.setValue("Shortcut", eShortcut->keySequence().toString());
        settings.setValue("IsGlobal", cGlobal->isChecked());
        settings.endGroup();
        return id;
    }
    
    return QString();
}

void OpenAIAssistant::deleteFeature(const QString& featureId)
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.remove("OpenAI/Actions/" + featureId);
}

void OpenAIAssistant::editFeature(const QString& featureId, QWidget* parent)
{
    QSettings settings("Heresy", "ClipboardAssistant");
    QString group = "OpenAI/Actions/" + featureId;
    if (!settings.contains(group + "/Name")) return;

    QDialog dialog(parent);
    dialog.setWindowTitle("Edit Action");
    dialog.resize(400, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* lName = new QLabel("Action Name:", &dialog);
    QLineEdit* eName = new QLineEdit(&dialog);
    eName->setText(settings.value(group + "/Name").toString());
    
    QLabel* lPrompt = new QLabel("System Prompt:", &dialog);
    QTextEdit* ePrompt = new QTextEdit(&dialog);
    ePrompt->setPlainText(settings.value(group + "/Prompt").toString());
    
    QLabel* lShortcut = new QLabel("Custom Shortcut (Optional):", &dialog);
    QKeySequenceEdit* eShortcut = new QKeySequenceEdit(&dialog);
    eShortcut->setKeySequence(QKeySequence(settings.value(group + "/Shortcut").toString()));
    
    QCheckBox* cGlobal = new QCheckBox("Register as Global Hotkey", &dialog);
    cGlobal->setChecked(settings.value(group + "/IsGlobal", false).toBool());

    layout->addWidget(lName);
    layout->addWidget(eName);
    layout->addWidget(lPrompt);
    layout->addWidget(ePrompt);
    layout->addWidget(lShortcut);
    layout->addWidget(eShortcut);
    layout->addWidget(cGlobal);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (eName->text().trimmed().isEmpty()) return;
        
        settings.beginGroup(group);
        settings.setValue("Name", eName->text());
        settings.setValue("Prompt", ePrompt->toPlainText());
        settings.setValue("Shortcut", eShortcut->keySequence().toString());
        settings.setValue("IsGlobal", cGlobal->isChecked());
        settings.endGroup();
    }
}
