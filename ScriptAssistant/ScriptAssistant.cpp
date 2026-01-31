#include "ScriptAssistant.h"
#include <QMimeData>
#include <QJSValue>
#include <QCoreApplication>

ScriptAssistant::ScriptAssistant()
{
    m_engine = new QJSEngine(this);
}

QString ScriptAssistant::id() const { return "kheresy.ScriptAssistant"; }

QString ScriptAssistant::name() const { return tr("Script Assistant"); }
QString ScriptAssistant::version() const { return "0.2.0"; }

QList<ParameterDefinition> ScriptAssistant::actionParameterDefinitions() const
{
    return {
        {"Script", QCoreApplication::translate("ScriptAssistant", "Script"), ParameterType::Text, 
         "function process(text) {\n    return text;\n}", 
         {}, QCoreApplication::translate("ScriptAssistant", "JavaScript code. Must contain a 'process(text)' function.")}
    };
}

QList<ModuleActionTemplate> ScriptAssistant::actionTemplates() const
{
    QList<ModuleActionTemplate> list;
    list.append({"upper", tr("To Upper Case"), {{"Script", "function process(text) {\n    return text.toUpperCase();\n}"}}});
    list.append({"format_json", tr("Format JSON"), {{"Script", "function process(text) {\n    try {\n        var obj = JSON.parse(text);\n        return JSON.stringify(obj, null, 4);\n    } catch (e) {\n        return \"Invalid JSON: \" + e.message;\n    }\n}"}}});
    return list;
}

void ScriptAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IModuleCallback* callback)
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
