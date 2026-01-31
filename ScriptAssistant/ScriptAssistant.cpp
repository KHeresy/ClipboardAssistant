#include "ScriptAssistant.h"
#include <QMimeData>
#include <QJSValue>

ScriptAssistant::ScriptAssistant() {
    m_engine = new QJSEngine(this);
}

ScriptAssistant::~ScriptAssistant() {}

QString ScriptAssistant::name() const { return "Script Assistant"; }
QString ScriptAssistant::version() const { return "0.1.0"; }

QList<ParameterDefinition> ScriptAssistant::actionParameterDefinitions() const
{
    return {
        {"Script", tr("Script"), ParameterType::Text, 
         "function process(text) {\n    return text.toUpperCase();\n}", 
         {}, tr("JavaScript code. Must contain a 'process(text)' function.")}
    };
}

QList<PluginActionTemplate> ScriptAssistant::actionTemplates() const
{
    QList<PluginActionTemplate> list;
    list.append({"uppercase", tr("To Upper Case"), {{"Script", "function process(text) { return text.toUpperCase(); }"}}});
    list.append({"json_format", tr("Format JSON"), {{"Script", "function process(text) { return JSON.stringify(JSON.parse(text), null, 4); }"}}});
    return list;
}

void ScriptAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) {
        callback->onError(tr("No text found in clipboard."));
        return;
    }

    QString script = actionParams.value("Script").toString();
    QString input = data->text();

    // Evaluate the user script
    QJSValue result = m_engine->evaluate(script);
    
    if (result.isError()) {
        callback->onError(tr("Script Syntax Error: ") + result.toString());
        return;
    }

    // Call the process function
    QJSValue processFunc = m_engine->globalObject().property("process");
    
    if (!processFunc.isCallable()) {
        callback->onError(tr("Script must define a 'process(text)' function."));
        return;
    }

    QJSValueList args;
    args << input;
    QJSValue funcResult = processFunc.call(args);

    if (funcResult.isError()) {
        callback->onError(tr("Script Execution Error: ") + funcResult.toString());
    } else {
        callback->onTextData(funcResult.toString(), true);
        callback->onFinished();
    }
}
