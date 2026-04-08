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

namespace {
QString normalizeAuthMode(const QString& value)
{
    return value.trimmed().compare("api-key", Qt::CaseInsensitive) == 0 ? "api-key" : "Bearer";
}

bool isChatApiType(const QString& value)
{
    const QString v = value.trimmed().toLower();
    if (v.contains("completion") || v.contains("legacy")) return false;
    return true;
}

QUrl buildRequestUrl(QString urlStr, QString suffix, const QString& apiType, bool isAzure)
{
    if (urlStr.endsWith('/')) urlStr.chop(1);
    if (suffix.trimmed().isEmpty()) suffix = (isChatApiType(apiType) ? "/chat/completions" : "/completions");
    if (!suffix.startsWith('/')) suffix = "/" + suffix;

    if (!isAzure) return QUrl(urlStr + suffix);

    QUrl url(urlStr);
    if (!url.isValid()) return QUrl(urlStr);

    QString path = url.path();
    const bool hasEndpoint = path.contains("/chat/completions", Qt::CaseInsensitive)
        || path.contains("/completions", Qt::CaseInsensitive);

    if (!hasEndpoint) {
        if (path.endsWith('/')) path.chop(1);
        url.setPath(path + suffix);
    }

    return url;
}
}

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
        {"OverrideModel", QCoreApplication::translate("OpenAIAssistant", "Override Model"), ParameterType::String, "", {}, QCoreApplication::translate("OpenAIAssistant", "Leave empty to use account default model")},
        {"Temperature", QCoreApplication::translate("OpenAIAssistant", "Temperature"), ParameterType::Number, 1.0, {}, QCoreApplication::translate("OpenAIAssistant", "What sampling temperature to use (0 to 2)")},
        {"TopP", QCoreApplication::translate("OpenAIAssistant", "Top P"), ParameterType::Number, 1.0, {}, QCoreApplication::translate("OpenAIAssistant", "Nucleus sampling probability (0 to 1)")},
        {"FrequencyPenalty", QCoreApplication::translate("OpenAIAssistant", "Frequency Penalty"), ParameterType::Number, 0.0, {}, QCoreApplication::translate("OpenAIAssistant", "Penalize new tokens based on their existing frequency in the text (-2.0 to 2.0)")},
        {"PresencePenalty", QCoreApplication::translate("OpenAIAssistant", "Presence Penalty"), ParameterType::Number, 0.0, {}, QCoreApplication::translate("OpenAIAssistant", "Penalize new tokens based on whether they appear in the text so far (-2.0 to 2.0)")},
        {"ReasoningEffort", QCoreApplication::translate("OpenAIAssistant", "Reasoning Effort"), ParameterType::Choice, "medium", {"low", "medium", "high"}, QCoreApplication::translate("OpenAIAssistant", "For reasoning models (o1/o3), sets how much effort to spend on thinking")},
        {"ResponseFormat", QCoreApplication::translate("OpenAIAssistant", "Response Format"), ParameterType::Choice, "Text", {"Text", "JSON Object"}, QCoreApplication::translate("OpenAIAssistant", "The format that the model must output")},
        {"RawJsonParams", QCoreApplication::translate("OpenAIAssistant", "Raw JSON Params"), ParameterType::Text, "", {}, QCoreApplication::translate("OpenAIAssistant", "Optional raw JSON parameters to merge into the request body")}
    };
}

QList<ParameterDefinition> OpenAIAssistant::globalParameterDefinitions() const {
    // We use the internal account management instead of global host-managed params
    return {};
}

QList<ModuleActionTemplate> OpenAIAssistant::actionTemplates() const {
    QList<ModuleActionTemplate> list;
    list.append({"summarize", QCoreApplication::translate("OpenAIAssistant", "Summarize"), {{"Prompt", "Summarize text:"}}});
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
    QString key, model, urlStr, suffix, apiType, authMode, accountSystemPrompt;
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
            suffix = s.value(id + "/Suffix").toString().trimmed();
            apiType = s.value(id + "/ApiType", "Chat").toString();
            authMode = normalizeAuthMode(s.value(id + "/AuthMode", "Bearer").toString());
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
        suffix = s.value("Suffix").toString().trimmed();
        apiType = s.value("ApiType", "Chat").toString();
        authMode = normalizeAuthMode(s.value("AuthMode", "Bearer").toString());
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

    const QString inputText = (data->hasText() ? data->text() : QString());
    QJsonArray content;
    if (!inputText.isEmpty()) {
        QJsonObject o; o["type"] = "text"; o["text"] = inputText; content.append(o);
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

    const bool hasImage = data->hasImage();

    QJsonArray msgs;
    QJsonObject sys; sys["role"] = "system"; sys["content"] = prompt; msgs.append(sys);
    QJsonObject usr; usr["role"] = "user";
    if (hasImage) usr["content"] = content;
    else usr["content"] = inputText;
    msgs.append(usr);

    const bool chatApi = isChatApiType(apiType);

    QJsonObject json; json["model"]=model; json["stream"]=true;

    if (chatApi) {
        json["messages"] = msgs;
        if (actionParams.value("ResponseFormat").toString() == "JSON Object") {
            QJsonObject fmt; fmt["type"] = "json_object"; json["response_format"] = fmt;
        }
    } else {
        QString fullPrompt = prompt + "\n\n";
        if (data->hasText()) fullPrompt += data->text();
        json["prompt"] = fullPrompt;
    }
    
    int maxTokens = actionParams.value("MaxTokens").toInt();
    if (maxTokens > 0) {
        if (chatApi) json["max_tokens"] = maxTokens;
        else json["max_tokens"] = maxTokens;
    }
    
    const bool isGeminiCompat = urlStr.contains("generativelanguage.googleapis.com", Qt::CaseInsensitive);

    if (actionParams.contains("Temperature")) json["temperature"] = actionParams.value("Temperature").toDouble();
    if (actionParams.contains("TopP")) json["top_p"] = actionParams.value("TopP").toDouble();
    if (!isGeminiCompat && actionParams.contains("FrequencyPenalty")) json["frequency_penalty"] = actionParams.value("FrequencyPenalty").toDouble();
    if (!isGeminiCompat && actionParams.contains("PresencePenalty")) json["presence_penalty"] = actionParams.value("PresencePenalty").toDouble();
    
    if (actionParams.contains("ReasoningEffort") && (model.contains("o1") || model.contains("o3") || model.contains("gpt-5.4") || model.contains("o5"))) {
        json["reasoning_effort"] = actionParams.value("ReasoningEffort").toString();
    }

    QString rawJson = actionParams.value("RawJsonParams").toString().trimmed();
    if (!rawJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject rawObj = doc.object();
            for (auto it = rawObj.begin(); it != rawObj.end(); ++it) json[it.key()] = it.value();
        }
    }
    
    QUrl url = buildRequestUrl(urlStr, suffix, apiType, isAz);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (authMode == "api-key") req.setRawHeader("api-key", key.toUtf8()); 
    else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    m_currentReply = m_networkManager->post(req, QJsonDocument(json).toJson());

    QNetworkReply* reply = m_currentReply;
    QByteArray* buf = new QByteArray();

    connect(reply, &QNetworkReply::readyRead, [reply, callback, buf, apiType]() {
        const bool chatApi = isChatApiType(apiType);
        while(reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (line.startsWith("data: ")) {
                QByteArray d = line.mid(6); if (d == "[DONE]") continue;
                QJsonDocument doc = QJsonDocument::fromJson(d);
                if (doc.isObject()) {
                    QJsonArray choices = doc.object()["choices"].toArray();
                    if (!choices.isEmpty()) {
                        QJsonObject choice = choices[0].toObject();
                        QString text;
                        if (chatApi) text = choice["delta"].toObject()["content"].toString();
                        else text = choice["text"].toString();
                        
                        if (!text.isEmpty()) callback->onTextData(text, false);
                        if (choice["finish_reason"].toString() == "length") {
                            callback->onError(QCoreApplication::translate("OpenAIAssistant", "\n\n[Warning: Message truncated due to Max Tokens limit.]"));
                        }
                    }
                }
            } else buf->append(line);
        }
    });

    connect(reply, &QNetworkReply::finished, [this, reply, callback, buf, apiType]() {
        const bool chatApi = isChatApiType(apiType);
        if (m_currentReply == reply) m_currentReply = nullptr;
        reply->disconnect();
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
            callback->onError(reply->errorString() + "\n" + QString::fromUtf8(*buf));
        } else {
            if (!buf->isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(*buf);
                QJsonArray choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty()) {
                    if (chatApi) callback->onTextData(choices[0].toObject()["message"].toObject()["content"].toString(), true);
                    else callback->onTextData(choices[0].toObject()["text"].toString(), true);
                }
            }
            callback->onTextData("", true); 
            callback->onFinished();
        }
        delete buf; 
        reply->deleteLater();
    });
}