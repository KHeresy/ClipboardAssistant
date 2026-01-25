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
#include <QPushButton>
#include <QMessageBox>
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>

OpenAIAssistant::OpenAIAssistant()
{
    m_networkManager = new QNetworkAccessManager(this);
}

OpenAIAssistant::~OpenAIAssistant()
{
}

QString OpenAIAssistant::name() const
{
    return "OpenAI Assistant";
}

QList<PluginFeature> OpenAIAssistant::features() const
{
    QList<PluginFeature> list;
    list.append({"summarize", "Summarize", "Summarize the clipboard content", QKeySequence("Ctrl+Alt+S")});
    list.append({"translate_en", "Translate to English", "Translate content to English", QKeySequence("Ctrl+Alt+E")});
    list.append({"fix_grammar", "Fix Grammar", "Fix grammar errors", QKeySequence("Ctrl+Alt+G")});
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

    if (apiKey.isEmpty()) {
        callback->onError("OpenAI API Key is not set. Please configure in settings.");
        return;
    }

    QString systemInstruction;
    if (featureId == "summarize") systemInstruction = "Summarize the following text:";
    else if (featureId == "translate_en") systemInstruction = "Translate the following text to English:";
    else if (featureId == "fix_grammar") systemInstruction = "Fix grammar in the following text:";
    else systemInstruction = "You are a helpful assistant.";

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

    QUrl url(baseUrl + "/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());

    QNetworkReply* reply = m_networkManager->post(request, postData);

    // Handle Streaming
    connect(reply, &QNetworkReply::readyRead, [reply, callback]() {
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
                                    QString content = delta["content"].toString();
                                    callback->onTextData(content, false);
                                }
                            }
                        }
                    }
                }
            }
        }
    });

    connect(reply, &QNetworkReply::finished, [reply, callback]() {
        if (reply->error() != QNetworkReply::NoError) {
            callback->onError(reply->errorString());
        } else {
            callback->onTextData("", true); 
            callback->onFinished();
        }
        reply->deleteLater();
    });
}

bool OpenAIAssistant::hasSettings() const
{
    return true;
}

void OpenAIAssistant::showSettings(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("OpenAI Assistant Settings");
    dialog.resize(400, 200);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* labelKey = new QLabel("API Key:", &dialog);
    QLineEdit* editKey = new QLineEdit(&dialog);
    editKey->setEchoMode(QLineEdit::Password);
    
    QLabel* labelModel = new QLabel("Model:", &dialog);
    QLineEdit* editModel = new QLineEdit(&dialog);
    
    QLabel* labelUrl = new QLabel("Base URL:", &dialog);
    QLineEdit* editUrl = new QLineEdit(&dialog);
    
    QSettings settings("Heresy", "ClipboardAssistant");
    editKey->setText(settings.value("OpenAI/ApiKey").toString());
    editModel->setText(settings.value("OpenAI/Model", "gpt-3.5-turbo").toString());
    editUrl->setText(settings.value("OpenAI/BaseUrl", "https://api.openai.com/v1").toString());
    
    layout->addWidget(labelKey);
    layout->addWidget(editKey);
    layout->addWidget(labelModel);
    layout->addWidget(editModel);
    layout->addWidget(labelUrl);
    layout->addWidget(editUrl);
    
    QPushButton* btnSave = new QPushButton("Save", &dialog);
    layout->addWidget(btnSave);
    
    QObject::connect(btnSave, &QPushButton::clicked, [&]() {
        settings.setValue("OpenAI/ApiKey", editKey->text());
        settings.setValue("OpenAI/Model", editModel->text());
        settings.setValue("OpenAI/BaseUrl", editUrl->text());
        dialog.accept();
    });
    
    dialog.exec();
}