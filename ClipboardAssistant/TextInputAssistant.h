#pragma once

#include <QObject>
#include "../Common/IClipboardPlugin.h"

class TextInputAssistant : public QObject, public IClipboardPlugin
{
    Q_OBJECT
    Q_INTERFACES(IClipboardPlugin)

public:
    TextInputAssistant(QObject* parent = nullptr);
    
    QString name() const override;
    QString version() const override;

    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<PluginActionTemplate> actionTemplates() const override;

    DataTypes supportedInputs() const override { return Text; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return false; }

    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) override;
};
