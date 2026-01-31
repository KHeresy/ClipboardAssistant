#include "ScriptAssistant.h"
#include <QMimeData>
#include <QJSValue>
#include <QCoreApplication>

ScriptAssistant::ScriptAssistant()
{
}

QString ScriptAssistant::id() const { return "kheresy.ScriptAssistant"; }

QString ScriptAssistant::name() const { return tr("Script Assistant"); }
QString ScriptAssistant::version() const { return "0.2.0"; }

QList<ParameterDefinition> ScriptAssistant::actionParameterDefinitions() const
{
    return {
        {"Script", QCoreApplication::translate("ScriptAssistant", "Script"), ParameterType::Text, 
         "function process(text) {\n    return text.toUpperCase();\n}", 
         {}, QCoreApplication::translate("ScriptAssistant", "JavaScript code. Must contain a 'process(text)' function.")}
    };
}

QList<PluginActionTemplate> ScriptAssistant::actionTemplates() const
{
    QList<PluginActionTemplate> list;
    list.append({"uppercase", QCoreApplication::translate("ScriptAssistant", "To Upper Case"), {{"Script", "function process(text) { return text.toUpperCase(); }"}}});
    list.append({"json_format", QCoreApplication::translate("ScriptAssistant", "Format JSON"), {{"Script", "function process(text) { return JSON.stringify(JSON.parse(text), null, 4); }"}}});
    return list;
}

void ScriptAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) {
        callback->onError(QCoreApplication::translate("ScriptAssistant", "No text found in clipboard."));
        return;
    }

    QString script = actionParams.value("Script").toString();
    QString input = data->text();

    // Evaluate the user script
    QJSValue result = m_engine->evaluate(script);
    
    if (result.isError()) {
        callback->onError(QCoreApplication::translate("ScriptAssistant", "Script Syntax Error: ") + result.toString());
        return;
    }

    // Call the process function
    QJSValue processFunc = m_engine->globalObject().property("process");
    
    if (!processFunc.isCallable()) {
        callback->onError(QCoreApplication::translate("ScriptAssistant", "Script must define a 'process(text)' function."));
        return;
    }

    QJSValueList args;
    args << input;
    QJSValue funcResult = processFunc.call(args);

    if (funcResult.isError()) {
        callback->onError(QCoreApplication::translate("ScriptAssistant", "Script Execution Error: ") + funcResult.toString());
    } else {
        callback->onTextData(funcResult.toString(), true);
        callback->onFinished();
    }
}
