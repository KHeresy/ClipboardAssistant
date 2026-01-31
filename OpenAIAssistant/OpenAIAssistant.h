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
    
    QString id() const override;
    QString name() const override;
    QString version() const override;
    
    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<ParameterDefinition> globalParameterDefinitions() const override;
    QList<PluginActionTemplate> actionTemplates() const override;

    DataTypes supportedInputs() const override { return Text | Image | File; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return true; }
    void abort() override;

    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) override;

    bool hasConfiguration() const override { return true; }
    void showConfiguration(QWidget* parent) override;

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QNetworkReply* m_currentReply = nullptr;
};