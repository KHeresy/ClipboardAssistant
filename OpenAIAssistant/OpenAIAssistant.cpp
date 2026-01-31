#include "OpenAIAssistant.h"
#include "OpenAISettings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QNetworkRequest>
#include <QBuffer>
#include <QMimeData>
#include <QImage>
#include <QSettings>
#include <QInputDialog>
#include <QApplication>
#include <QCoreApplication>

OpenAIAssistant::OpenAIAssistant()
{
    m_networkManager = new QNetworkAccessManager(this);
}

QString OpenAIAssistant::id() const { return "kheresy.OpenAIAssistant"; }

QString OpenAIAssistant::name() const { return tr("OpenAI Assistant"); }
QString OpenAIAssistant::version() const { return "0.2.0"; }

void OpenAIAssistant::showConfiguration(QWidget* parent) {
    OpenAISettings(parent).exec();
}

QList<ParameterDefinition> OpenAIAssistant::actionParameterDefinitions() const {
    QStringList accounts;
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    for (const QString& id : s.childGroups()) {
        accounts << s.value(id + "/Name").toString();
    }
    s.endGroup();

    if (accounts.isEmpty()) accounts << QCoreApplication::translate("OpenAIAssistant", "Default (Not Configured)");

    return {
        {"Account", QCoreApplication::translate("OpenAIAssistant", "Account"), ParameterType::Choice, accounts.first(), accounts, QCoreApplication::translate("OpenAIAssistant", "Select which OpenAI account to use")},
        {"Prompt", QCoreApplication::translate("OpenAIAssistant", "System Prompt"), ParameterType::Text, "", {}, QCoreApplication::translate("OpenAIAssistant", "The prompt to send to the AI")},
        {"PromptMode", QCoreApplication::translate("OpenAIAssistant", "Prompt Mode"), ParameterType::Choice, "Override", {"Override", "Append"}, QCoreApplication::translate("OpenAIAssistant", "Choose whether to override or append to account default prompt")},
        {"MaxTokens", QCoreApplication::translate("OpenAIAssistant", "Max Tokens"), ParameterType::Number, 0, {}, QCoreApplication::translate("OpenAIAssistant", "Maximum tokens to generate (0 for model default)")},
        {"OverrideModel", QCoreApplication::translate("OpenAIAssistant", "Override Model"), ParameterType::String, "", {}, QCoreApplication::translate("OpenAIAssistant", "Leave empty to use account default model")}
    };
}

QList<ParameterDefinition> OpenAIAssistant::globalParameterDefinitions() const {
    // We use the internal account management instead of global host-managed params
    return {};
}

QList<ModuleActionTemplate> OpenAIAssistant::actionTemplates() const {
    QList<ModuleActionTemplate> list;
    list.append({"summarize", QCoreApplication::translate("OpenAIAssistant", "Summarize"), {{QCoreApplication::translate("OpenAIAssistant", "Prompt"), "Summarize text:"}}});
    return list;
}

void OpenAIAssistant::abort() {
    if (m_currentReply) {
        m_currentReply->disconnect();
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void OpenAIAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IModuleCallback* callback) {
    abort();

    QString targetAccount = actionParams.value("Account").toString();
    QString prompt = actionParams.value("Prompt").toString();
    QString promptMode = actionParams.value("PromptMode").toString();

    // Find account info in internal settings
    QString key, model, urlStr, accountSystemPrompt;
    bool isAz = false;
    bool found = false;

    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    QStringList accountIds = s.childGroups();
    QStringList accountNames;
    QMap<QString, QString> nameToId;

    for (const QString& id : accountIds) {
        QString name = s.value(id + "/Name").toString().trimmed();
        accountNames << name;
        nameToId[name] = id;
        if (name == targetAccount) {
            key = s.value(id + "/Key").toString().trimmed();
            model = s.value(id + "/Model").toString().trimmed();
            urlStr = s.value(id + "/Url").toString().trimmed();
            accountSystemPrompt = s.value(id + "/SystemPrompt").toString().trimmed();
            isAz = s.value(id + "/IsAzure").toBool();
            found = true;
        }
    }
    s.endGroup();

    if (!found) {
        // Account fallback logic
        QString selectedAccountName;
        if (accountNames.isEmpty()) {
            callback->onError(QCoreApplication::translate("OpenAIAssistant", "Account not found or not configured. Please check Module Settings."));
            return;
        } else if (accountNames.size() == 1) {
            selectedAccountName = accountNames.first();
        } else {
            bool ok = false;
            selectedAccountName = QInputDialog::getItem(QApplication::activeWindow(), 
                QCoreApplication::translate("OpenAIAssistant", "Select OpenAI Account"), 
                QCoreApplication::translate("OpenAIAssistant", "Account '%1' not found. Please select an account:").arg(targetAccount), 
                accountNames, 0, false, &ok);
            if (!ok || selectedAccountName.isEmpty()) {
                callback->onError(QCoreApplication::translate("OpenAIAssistant", "No account selected."));
                return;
            }
        }

        // Load the selected account
        QString id = nameToId[selectedAccountName];
        s.beginGroup("OpenAI/Accounts/" + id);
        key = s.value("Key").toString().trimmed();
        model = s.value("Model").toString().trimmed();
        urlStr = s.value("Url").toString().trimmed();
        accountSystemPrompt = s.value("SystemPrompt").toString().trimmed();
        isAz = s.value("IsAzure").toBool();
        s.endGroup();
    }

    // Handle prompt mode: Override or Append
    if (promptMode == "Append") {
        if (!accountSystemPrompt.isEmpty() && !prompt.isEmpty()) {
            prompt = accountSystemPrompt + "\n" + prompt;
        } else if (prompt.isEmpty()) {
            prompt = accountSystemPrompt;
        }
    } else {
        // Override (default)
        if (prompt.isEmpty()) prompt = accountSystemPrompt;
    }

    if (prompt.isEmpty()) prompt = "You are a helpful assistant.";

    // Override model if specified in action
    QString overrideModel = actionParams.value("OverrideModel").toString();
    if (!overrideModel.isEmpty()) model = overrideModel;

    if (key.isEmpty()) { callback->onError(QCoreApplication::translate("OpenAIAssistant", "API Key is empty for the selected account.")); return; }

    QJsonArray content;
    if (data->hasText() && !data->text().isEmpty()) { 
        QJsonObject o; o["type"]="text"; o["text"]=data->text(); content.append(o); 
    }
    if (data->hasImage()) {
        QImage img = qvariant_cast<QImage>(data->imageData());
        if (!img.isNull()) {
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); img.save(&buf, "PNG");
            QJsonObject o; o["type"]="image_url"; 
            QJsonObject u; u["url"]="data:image/png;base64,"+QString::fromLatin1(ba.toBase64());
            o["image_url"]=u; content.append(o);
        }
    }

    if (content.isEmpty()) { callback->onError(QCoreApplication::translate("OpenAIAssistant", "No content to process")); return; }

    QJsonArray msgs;
    QJsonObject sys; sys["role"]="system"; sys["content"]=prompt; msgs.append(sys);
    QJsonObject usr; usr["role"]="user"; usr["content"]=content; msgs.append(usr);

        QJsonObject json; json["model"]=model; json["messages"]=msgs; json["stream"]=true;
        
        int maxTokens = actionParams.value("MaxTokens").toInt();
        if (maxTokens > 0) {
            // Use max_completion_tokens for newer models; some models might still need max_tokens
            // But for stream=true, many providers accept either.
            json["max_completion_tokens"] = maxTokens;
        }
        
        QUrl url;    if (isAz) {
        url = QUrl(urlStr);
    } else {
        if (urlStr.endsWith("/")) urlStr.chop(1);
        url = QUrl(urlStr + "/chat/completions");
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (isAz) req.setRawHeader("api-key", key.toUtf8()); 
    else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    m_currentReply = m_networkManager->post(req, QJsonDocument(json).toJson());

    QNetworkReply* reply = m_currentReply;
    QByteArray* buf = new QByteArray();

    connect(reply, &QNetworkReply::readyRead, [reply, callback, buf]() {
        while(reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (line.startsWith("data: ")) {
                QByteArray d = line.mid(6); if (d == "[DONE]") continue;
                QJsonDocument doc = QJsonDocument::fromJson(d);
                if (doc.isObject()) {
                    QJsonArray choices = doc.object()["choices"].toArray();
                    if (!choices.isEmpty()) {
                        QJsonObject choice = choices[0].toObject();
                        QString content = choice["delta"].toObject()["content"].toString();
                        if (!content.isEmpty()) {
                            callback->onTextData(content, false);
                        }
                        
                        if (choice["finish_reason"].toString() == "length") {
                            callback->onError(QCoreApplication::translate("OpenAIAssistant", "\n\n[Warning: Message truncated due to Max Tokens limit.]"));
                        }
                    }
                }
            } else buf->append(line);
        }
    });

    connect(reply, &QNetworkReply::finished, [this, reply, callback, buf]() {
        if (m_currentReply == reply) m_currentReply = nullptr;

        // 先斷開連線防止任何重複觸發
        reply->disconnect();

        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
            callback->onError(reply->errorString() + "\n" + QString::fromUtf8(*buf));
        } else {
            if (!buf->isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(*buf);
                QJsonArray choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty()) callback->onTextData(choices[0].toObject()["message"].toObject()["content"].toString(), true);
            }
            callback->onTextData("", true); 
            callback->onFinished();
        }
        delete buf; 
        reply->deleteLater();
    });
}