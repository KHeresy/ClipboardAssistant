#pragma once

#include "gemmiassistant_global.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../Common/IClipboardModule.h"

class GEMMIASSISTANT_EXPORT GemmiAssistant : public QObject, public IClipboardModule
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.gemini.ClipboardAssistant.IClipboardModule")
    Q_INTERFACES(IClipboardModule)

public:
    GemmiAssistant();

    QString id() const override;
    QString name() const override;
    QString version() const override;

    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<ParameterDefinition> globalParameterDefinitions() const override;
    QList<ModuleActionTemplate> actionTemplates() const override;

    DataTypes supportedInputs() const override { return Text | Image | File; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return false; }
    void abort() override;

    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IModuleCallback* callback) override;

    bool hasConfiguration() const override { return true; }
    void showConfiguration(QWidget* parent) override;

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QNetworkReply* m_currentReply = nullptr;
};
