#include "OpenAIAssistant.h"
#include "OpenAISettings.h"
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
#include <QBuffer>
#include <QMimeDatabase>
#include <QMimeType>
#include <QFile>
#include <QFileInfo>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <algorithm>

OpenAIAssistant::OpenAIAssistant() {
    m_networkManager = new QNetworkAccessManager(this);
    ensureDefaultActions();
}
OpenAIAssistant::~OpenAIAssistant() {}
QString OpenAIAssistant::name() const { return "OpenAI Assistant"; }
QString OpenAIAssistant::version() const { return "0.1.0"; }

void OpenAIAssistant::ensureDefaultActions() {
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    QString defId;
    if (s.childGroups().isEmpty()) {
        defId = QUuid::createUuid().toString(QUuid::Id128);
        s.beginGroup(defId);
        s.setValue("Name", "Default OpenAI"); s.setValue("Key", ""); s.setValue("Model", "gpt-3.5-turbo");
        s.setValue("Url", "https://api.openai.com/v1"); s.setValue("IsAzure", false);
        s.endGroup();
    } else defId = s.childGroups().first();
    s.endGroup();
    s.beginGroup("OpenAI/Actions");
    if (s.childGroups().isEmpty()) {
        auto add = [&](const QString& n, const QString& p, int o) {
            QString id = QUuid::createUuid().toString(QUuid::Id128);
            s.beginGroup(id);
            s.setValue("Name", n);
            s.setValue("Prompt", p);
            s.setValue("AccountId", defId);
            s.setValue("Order", o);
            s.endGroup();
        };
        add("Summarize", "Summarize text:", 0);
        add("Translate to English", "Translate to English:", 1);
    }
    s.endGroup();
}

QList<PluginActionSet> OpenAIAssistant::actionSets() const {
    QList<PluginActionSet> list;
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Actions");
    QStringList ids = s.childGroups();
    
    struct SortedActionSet {
        PluginActionSet f;
        int order;
    };
    QList<SortedActionSet> sortedList;

    for (const QString& id : ids) {
        s.beginGroup(id);
        PluginActionSet f;
        f.id = id;
        f.name = s.value("Name").toString();
        f.description = s.value("Prompt").toString();
        f.customShortcut = QKeySequence(s.value("Shortcut").toString());
        f.isCustomShortcutGlobal = s.value("IsGlobal", false).toBool();
        int order = s.value("Order", 999).toInt();
        sortedList.append({f, order});
        s.endGroup();
    }
    s.endGroup(); 

    // Sort by order
    std::sort(sortedList.begin(), sortedList.end(), [](const SortedActionSet& a, const SortedActionSet& b) {
        return a.order < b.order;
    });

    for(const auto& sf : sortedList) list.append(sf.f);
    return list;
}

void OpenAIAssistant::abort() { if (m_currentReply) { m_currentReply->abort(); m_currentReply = nullptr; } }

void OpenAIAssistant::process(const QString& actionSetId, const QMimeData* data, IPluginCallback* callback) {
    abort();
    QSettings s("Heresy", "ClipboardAssistant");
    QString aG = "OpenAI/Actions/" + actionSetId;
    QString prompt = s.value(aG + "/Prompt").toString();
    QString accId = s.value(aG + "/AccountId").toString();
    if (accId.isEmpty()) { s.beginGroup("OpenAI/Accounts"); if (!s.childGroups().isEmpty()) accId = s.childGroups().first(); s.endGroup(); }
    QString acG = "OpenAI/Accounts/" + accId;
    QString key = s.value(acG + "/Key").toString();
    QString model = s.value(acG + "/Model").toString();
    QString urlStr = s.value(acG + "/Url").toString();
    bool isAz = s.value(acG + "/IsAzure").toBool();
    if (key.isEmpty()) { callback->onError("API Key empty"); return; }
    QJsonArray content;
    if (data->hasText()) { QJsonObject o; o["type"]="text"; o["text"]=data->text(); content.append(o); }
    if (data->hasImage()) {
        QImage img = qvariant_cast<QImage>(data->imageData());
        if (!img.isNull()) {
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); img.save(&buf, "JPG");
            QJsonObject o; o["type"]="image_url"; QJsonObject u; u["url"]="data:image/jpeg;base64,"+QString::fromLatin1(ba.toBase64());
            o["image_url"]=u; content.append(o);
        }
    }
    QJsonArray msgs;
    QJsonObject sys; sys["role"]="system"; sys["content"]=prompt; msgs.append(sys);
    QJsonObject usr; usr["role"]="user"; usr["content"]=content; msgs.append(usr);
    QJsonObject json; json["model"]=model; json["messages"]=msgs; json["stream"]=true;
    QUrl url = isAz ? QUrl(urlStr) : QUrl(urlStr + "/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (isAz) req.setRawHeader("api-key", key.toUtf8()); else req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
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
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
            callback->onError(reply->errorString() + "\n" + QString::fromUtf8(*buf));
        } else {
            if (!buf->isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(*buf);
                QJsonArray choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty()) callback->onTextData(choices[0].toObject()["message"].toObject()["content"].toString(), true);
            }
            callback->onTextData("", true); callback->onFinished();
        }
        delete buf; reply->deleteLater();
    });
}

bool OpenAIAssistant::hasSettings() const { return true; }
void OpenAIAssistant::showSettings(QWidget* parent) { OpenAISettings(parent).exec(); }
bool OpenAIAssistant::isEditable() const { return true; }

QComboBox* createAccCombo(QWidget* p, QString selId = "") {
    QComboBox* c = new QComboBox(p);
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Accounts");
    for (const QString& id : s.childGroups()) {
        c->addItem(s.value(id + "/Name").toString(), id);
        if (id == selId) c->setCurrentIndex(c->count() - 1);
    }
    s.endGroup();
    return c;
}

QWidget* OpenAIAssistant::getSettingsWidget(const QString& actionSetId, QWidget* parent) {
    QWidget* content = new QWidget(parent);
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0,0,0,0);
    
    QComboBox* cA = nullptr;
    QTextEdit* eP = new QTextEdit(content);
    eP->setObjectName("Prompt");

    if (!actionSetId.isEmpty()) {
        QSettings s("Heresy", "ClipboardAssistant");
        QString g = "OpenAI/Actions/" + actionSetId;
        cA = createAccCombo(content, s.value(g + "/AccountId").toString());
        eP->setPlainText(s.value(g + "/Prompt").toString());
    } else {
        cA = createAccCombo(content);
    }
    cA->setObjectName("Account");
    
    contentLayout->addWidget(new QLabel("Account:")); contentLayout->addWidget(cA);
    contentLayout->addWidget(new QLabel("Prompt:")); contentLayout->addWidget(eP);

    return content;
}

QString OpenAIAssistant::saveSettings(const QString& actionSetId, QWidget* widget, const QString& name, const QKeySequence& shortcut, bool isGlobal) {
    QString id = actionSetId;
    if (id.isEmpty()) id = QUuid::createUuid().toString(QUuid::Id128);

    QComboBox* cA = widget->findChild<QComboBox*>("Account");
    QTextEdit* eP = widget->findChild<QTextEdit*>("Prompt");

    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("OpenAI/Actions/" + id);
    s.setValue("Name", name);
    s.setValue("Prompt", eP ? eP->toPlainText() : "");
    s.setValue("AccountId", cA ? cA->currentData().toString() : "");
    s.setValue("Shortcut", shortcut.toString());
    s.setValue("IsGlobal", isGlobal);
    if (actionSetId.isEmpty()) s.setValue("Order", 999);
    s.endGroup();

    return id;
}

QString OpenAIAssistant::createActionSet(QWidget* p) { return QString(); }
void OpenAIAssistant::editActionSet(const QString& asid, QWidget* p) {}

void OpenAIAssistant::deleteActionSet(const QString& asid) { QSettings s("Heresy", "ClipboardAssistant"); s.remove("OpenAI/Actions/" + asid); }

void OpenAIAssistant::setActionSetOrder(const QString& asid, int order) {
    QSettings s("Heresy", "ClipboardAssistant");
    s.setValue("OpenAI/Actions/" + asid + "/Order", order);
}
