#pragma once

#include "openaiassistant_global.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../Common/IClipboardPlugin.h"

class OPENAIASSISTANT_EXPORT OpenAIAssistant : public QObject, public IClipboardPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.gemini.ClipboardAssistant.IClipboardPlugin")
    Q_INTERFACES(IClipboardPlugin)

public:
    OpenAIAssistant();
    ~OpenAIAssistant() override;

    QString name() const override;
    QString version() const override;
    QList<PluginActionSet> actionSets() const override;
    
    DataTypes supportedInputs() const override { return Text | Image | File; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return true; }
    void abort() override;

    void process(const QString& actionSetId, const QMimeData* data, IPluginCallback* callback) override;
    bool hasSettings() const override;
    void showSettings(QWidget* parent) override;

    // Editable interface
    bool isEditable() const override;
    QString createActionSet(QWidget* parent) override;
    void editActionSet(const QString& actionSetId, QWidget* parent) override;
    void deleteActionSet(const QString& actionSetId) override;
    void setActionSetOrder(const QString& actionSetId, int order) override;

private:
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply = nullptr;
    void ensureDefaultActions();
};