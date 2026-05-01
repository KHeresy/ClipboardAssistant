#include "GemmiAssistant.h"
#include "GemmiSettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QBuffer>
#include <QMimeData>
#include <QImage>
#include <QInputDialog>
#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QUrlQuery>

namespace {
QString normalizeAuthMode(const QString& value)
{
    return value.trimmed().compare("api-key", Qt::CaseInsensitive) == 0 ? "api-key" : "Bearer";
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

GemmiAssistant::GemmiAssistant()
{
    m_networkManager = new QNetworkAccessManager(this);
}

QString GemmiAssistant::id() const { return "kheresy.GemmiAssistant"; }
QString GemmiAssistant::name() const { return tr("Gemmi Assistant"); }
QString GemmiAssistant::version() const { return "0.3.0"; }

void GemmiAssistant::showConfiguration(QWidget* parent)
{
    GemmiSettings(parent).exec();
}

QList<ParameterDefinition> GemmiAssistant::actionParameterDefinitions() const
{
    QStringList accounts;
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("Gemmi/Accounts");
    for (const QString& id : s.childGroups()) accounts << s.value(id + "/Name").toString();
    s.endGroup();

    if (accounts.isEmpty()) accounts << QCoreApplication::translate("GemmiAssistant", "Default (Not Configured)");

    return {
        {"Account", QCoreApplication::translate("GemmiAssistant", "Account"), ParameterType::Choice, accounts.first(), accounts, QCoreApplication::translate("GemmiAssistant", "Select which Gemmi account to use")},
        {"Prompt", QCoreApplication::translate("GemmiAssistant", "System Prompt"), ParameterType::Text, "", {}, QCoreApplication::translate("GemmiAssistant", "The prompt to send to the AI")},
        {"PromptMode", QCoreApplication::translate("GemmiAssistant", "Prompt Mode"), ParameterType::Choice, "Override", {"Override", "Append"}, QCoreApplication::translate("GemmiAssistant", "Choose whether to override or append to account default prompt")},
        {"MaxTokens", QCoreApplication::translate("GemmiAssistant", "Max Tokens"), ParameterType::Number, 0, {}, QCoreApplication::translate("GemmiAssistant", "Maximum tokens to generate (0 for model default)"), 0.0, 128000.0, 1.0, 0, true},
        {"OverrideModel", QCoreApplication::translate("GemmiAssistant", "Override Model"), ParameterType::String, "", {}, QCoreApplication::translate("GemmiAssistant", "Leave empty to use account default model"), {}, {}, {}, 4, true},
        {"Temperature", QCoreApplication::translate("GemmiAssistant", "Temperature"), ParameterType::Decimal, 1.0, {}, QCoreApplication::translate("GemmiAssistant", "What sampling temperature to use"), 0.0, 2.0, 0.1, 2, true},
        {"TopP", QCoreApplication::translate("GemmiAssistant", "Top P"), ParameterType::Decimal, 1.0, {}, QCoreApplication::translate("GemmiAssistant", "Nucleus sampling probability"), 0.0, 1.0, 0.05, 2, true},
        {"RawJsonParams", QCoreApplication::translate("GemmiAssistant", "Raw JSON Params"), ParameterType::Text, "", {}, QCoreApplication::translate("GemmiAssistant", "Optional raw JSON parameters to merge into the request body"), {}, {}, {}, 4, true}
    };
}

QList<ParameterDefinition> GemmiAssistant::globalParameterDefinitions() const
{
    return {};
}

QList<ModuleActionTemplate> GemmiAssistant::actionTemplates() const
{
    return { {"summarize", QCoreApplication::translate("GemmiAssistant", "Summarize"), {{"Prompt", "Summarize text:"}}} };
}

void GemmiAssistant::abort()
{
    if (!m_currentReply) return;
    m_currentReply->disconnect();
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void GemmiAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IModuleCallback* callback)
{
    Q_UNUSED(globalParams);
    abort();

    const QString targetAccount = actionParams.value("Account").toString();
    QString prompt = actionParams.value("Prompt").toString();
    const QString promptMode = actionParams.value("PromptMode").toString();

    QString key, model, urlStr, suffix, authMode, accountSystemPrompt;
    bool found = false;

    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("Gemmi/Accounts");
    const QStringList accountIds = s.childGroups();
    QStringList accountNames;
    QMap<QString, QString> nameToId;

    for (const QString& id : accountIds) {
        const QString n = s.value(id + "/Name").toString().trimmed();
        accountNames << n;
        nameToId[n] = id;
        if (n == targetAccount) {
            key = s.value(id + "/Key").toString().trimmed();
            model = s.value(id + "/Model", "gemini-2.0-flash").toString().trimmed();
            urlStr = s.value(id + "/Url", "https://generativelanguage.googleapis.com/v1beta").toString().trimmed();
            suffix = s.value(id + "/Suffix", "/models/{model}:generateContent").toString().trimmed();
            authMode = normalizeAuthMode(s.value(id + "/AuthMode", "api-key").toString());
            accountSystemPrompt = s.value(id + "/SystemPrompt", "You are a helpful assistant.").toString().trimmed();
            found = true;
        }
    }
    s.endGroup();

    if (!found) {
        QString selectedAccountName;
        if (accountNames.isEmpty()) {
            callback->onError(QCoreApplication::translate("GemmiAssistant", "Account not found or not configured. Please check Module Settings."));
            return;
        } else if (accountNames.size() == 1) {
            selectedAccountName = accountNames.first();
        } else {
            bool ok = false;
            selectedAccountName = QInputDialog::getItem(
                QApplication::activeWindow(),
                QCoreApplication::translate("GemmiAssistant", "Select Gemmi Account"),
                QCoreApplication::translate("GemmiAssistant", "Account '%1' not found. Please select an account:").arg(targetAccount),
                accountNames, 0, false, &ok);
            if (!ok || selectedAccountName.isEmpty()) {
                callback->onError(QCoreApplication::translate("GemmiAssistant", "No account selected."));
                return;
            }
        }

        const QString id = nameToId[selectedAccountName];
        s.beginGroup("Gemmi/Accounts/" + id);
        key = s.value("Key").toString().trimmed();
        model = s.value("Model", "gemini-2.0-flash").toString().trimmed();
        urlStr = s.value("Url", "https://generativelanguage.googleapis.com/v1beta").toString().trimmed();
        suffix = s.value("Suffix", "/models/{model}:generateContent").toString().trimmed();
        authMode = normalizeAuthMode(s.value("AuthMode", "api-key").toString());
        accountSystemPrompt = s.value("SystemPrompt", "You are a helpful assistant.").toString().trimmed();
        s.endGroup();
    }

    if (promptMode == "Append") {
        if (!accountSystemPrompt.isEmpty() && !prompt.isEmpty()) prompt = accountSystemPrompt + "\n" + prompt;
        else if (prompt.isEmpty()) prompt = accountSystemPrompt;
    } else if (prompt.isEmpty()) {
        prompt = accountSystemPrompt;
    }

    if (prompt.isEmpty()) prompt = "You are a helpful assistant.";

    const QString overrideModel = actionParams.value("OverrideModel").toString().trimmed();
    if (!overrideModel.isEmpty()) model = overrideModel;

    if (key.isEmpty()) {
        callback->onError(QCoreApplication::translate("GemmiAssistant", "API Key is empty for the selected account."));
        return;
    }
    if (model.isEmpty()) {
        callback->onError(QCoreApplication::translate("GemmiAssistant", "Model is empty for the selected account."));
        return;
    }

    const QString inputText = (data->hasText() ? data->text() : QString());
    QJsonArray parts;
    if (!inputText.isEmpty()) {
        QJsonObject textPart;
        textPart["text"] = inputText;
        parts.append(textPart);
    }

    if (data->hasImage()) {
        const QImage img = qvariant_cast<QImage>(data->imageData());
        if (!img.isNull()) {
            QByteArray ba;
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");

            QJsonObject part;
            QJsonObject inlineData;
            inlineData["mime_type"] = "image/png";
            inlineData["data"] = QString::fromLatin1(ba.toBase64());
            part["inline_data"] = inlineData;
            parts.append(part);
        }
    }

    if (parts.isEmpty()) {
        callback->onError(QCoreApplication::translate("GemmiAssistant", "No content to process"));
        return;
    }

    QJsonObject json;
    QJsonObject userContent;
    userContent["role"] = "user";
    userContent["parts"] = parts;
    QJsonArray contents;
    contents.append(userContent);
    json["contents"] = contents;

    if (!prompt.isEmpty()) {
        QJsonObject systemInstruction;
        QJsonArray systemParts;
        QJsonObject systemText;
        systemText["text"] = prompt;
        systemParts.append(systemText);
        systemInstruction["parts"] = systemParts;
        json["system_instruction"] = systemInstruction;
    }

    const int maxTokens = actionParams.value("MaxTokens").toInt();
    QJsonObject generationConfig;
    if (maxTokens > 0) generationConfig["maxOutputTokens"] = maxTokens;
    if (actionParams.contains("Temperature")) generationConfig["temperature"] = actionParams.value("Temperature").toDouble();
    if (actionParams.contains("TopP")) generationConfig["topP"] = actionParams.value("TopP").toDouble();
    if (!generationConfig.isEmpty()) json["generationConfig"] = generationConfig;

    const QString rawJson = actionParams.value("RawJsonParams").toString().trimmed();
    if (!rawJson.isEmpty()) {
        const QJsonDocument rawDoc = QJsonDocument::fromJson(rawJson.toUtf8());
        if (rawDoc.isObject()) {
            const QJsonObject rawObj = rawDoc.object();
            for (auto it = rawObj.begin(); it != rawObj.end(); ++it) json[it.key()] = it.value();
        }
    }

    const QUrl url = buildGeminiUrl(urlStr, suffix, model, key, authMode);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (authMode == "api-key") req.setRawHeader("x-goog-api-key", key.toUtf8());
    else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    m_currentReply = m_networkManager->post(req, QJsonDocument(json).toJson());
    QNetworkReply* reply = m_currentReply;

    connect(reply, &QNetworkReply::finished, [this, reply, callback]() {
        if (m_currentReply == reply) m_currentReply = nullptr;
        reply->disconnect();

        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
            callback->onError(reply->errorString() + "\n" + QString::fromUtf8(body));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QString text;
        if (doc.isObject()) {
            const QJsonArray candidates = doc.object().value("candidates").toArray();
            if (!candidates.isEmpty()) {
                const QJsonObject first = candidates.first().toObject();
                const QJsonArray responseParts = first.value("content").toObject().value("parts").toArray();
                for (const QJsonValue& v : responseParts) {
                    text += v.toObject().value("text").toString();
                }
            }
        }

        if (!text.isEmpty()) callback->onTextData(text, true);
        else callback->onTextData(QString(), true);
        callback->onFinished();
        reply->deleteLater();
    });
}
