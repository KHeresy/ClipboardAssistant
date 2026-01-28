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

OpenAIAssistant::OpenAIAssistant() {
    m_networkManager = new QNetworkAccessManager(this);
}
OpenAIAssistant::~OpenAIAssistant() {}
QString OpenAIAssistant::name() const { return tr("OpenAI Assistant"); }
QString OpenAIAssistant::version() const { return "0.1.0"; }

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

    if (accounts.isEmpty()) accounts << tr("Default (Not Configured)");

    return {
        {"Account", tr("Account"), ParameterType::Choice, accounts.first(), accounts, tr("Select which OpenAI account to use")},
        {"Prompt", tr("System Prompt"), ParameterType::Text, "Summarize text:", {}, tr("The prompt to send to the AI")},
        {"OverrideModel", tr("Override Model"), ParameterType::String, "", {}, tr("Leave empty to use account default model")}
    };
}

QList<ParameterDefinition> OpenAIAssistant::globalParameterDefinitions() const {
    // We use the internal account management instead of global host-managed params
    return {};
}

QList<PluginActionTemplate> OpenAIAssistant::actionTemplates() const {
    QList<PluginActionTemplate> list;
    list.append({"summarize", tr("Summarize"), {{"Prompt", "Summarize text:"}}});
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

void OpenAIAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) {
    abort();
    
    QString targetAccount = actionParams.value("Account").toString();
    QString prompt = actionParams.value("Prompt").toString();
    
    // Find account info in internal settings
    QString key, model, urlStr;
    bool isAz = false;
    bool found = false;

    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    for (const QString& id : s.childGroups()) {
        if (s.value(id + "/Name").toString() == targetAccount) {
            key = s.value(id + "/Key").toString();
            model = s.value(id + "/Model").toString();
            urlStr = s.value(id + "/Url").toString();
            isAz = s.value(id + "/IsAzure").toBool();
            found = true;
            break;
        }
    }
    s.endGroup();

    if (!found) {
        callback->onError(tr("Account not found or not configured. Please check Plugin Settings."));
        return;
    }

    // Override model if specified in action
    QString overrideModel = actionParams.value("OverrideModel").toString();
    if (!overrideModel.isEmpty()) model = overrideModel;
    
    if (key.isEmpty()) { callback->onError(tr("API Key is empty for the selected account.")); return; }
    
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
    
    if (content.isEmpty()) { callback->onError(tr("No content to process")); return; }
    
    QJsonArray msgs;
    QJsonObject sys; sys["role"]="system"; sys["content"]=prompt; msgs.append(sys);
    QJsonObject usr; usr["role"]="user"; usr["content"]=content; msgs.append(usr);
    
    QJsonObject json; json["model"]=model; json["messages"]=msgs; json["stream"]=true;
    
    QUrl url = isAz ? QUrl(urlStr) : QUrl(urlStr + "/chat/completions");
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
                        QString content = choices[0].toObject()["delta"].toObject()["content"].toString();
                        callback->onTextData(content, false);
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
