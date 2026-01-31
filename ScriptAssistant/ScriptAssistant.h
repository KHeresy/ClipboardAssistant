#pragma once

#include "scriptassistant_global.h"
#include <QObject>
#include <QJSEngine>
#include "../Common/IClipboardModule.h"

class SCRIPTASSISTANT_EXPORT ScriptAssistant : public QObject, public IClipboardModule
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.gemini.ClipboardAssistant.IClipboardModule")
    Q_INTERFACES(IClipboardModule)

public:
    ScriptAssistant();

    QString id() const override;
    QString name() const override;
    QString version() const override;

    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<ModuleActionTemplate> actionTemplates() const override;

    DataTypes supportedInputs() const override { return Text; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return false; }

    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IModuleCallback* callback) override;

private:
    QJSEngine* m_engine;
};
