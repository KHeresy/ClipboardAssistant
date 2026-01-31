#pragma once

#include "scriptassistant_global.h"
#include <QObject>
#include <QJSEngine>
#include "../Common/IClipboardPlugin.h"

class SCRIPTASSISTANT_EXPORT ScriptAssistant : public QObject, public IClipboardPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.gemini.ClipboardAssistant.IClipboardPlugin")
    Q_INTERFACES(IClipboardPlugin)

public:
    ScriptAssistant();

    QString id() const override;
    QString name() const override;
    QString version() const override;
    
    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<PluginActionTemplate> actionTemplates() const override;

    DataTypes supportedInputs() const override { return Text; }
    DataTypes supportedOutputs() const override { return Text; }
    
    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) override;

private:
    QJSEngine* m_engine = nullptr;
};
