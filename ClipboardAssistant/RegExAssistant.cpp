#include "RegExAssistant.h"
#include <QRegularExpression>
#include <QMimeData>

RegExAssistant::RegExAssistant(QObject* parent) : QObject(parent)
{
}

QString RegExAssistant::name() const { return "RegEx Assistant"; }
QString RegExAssistant::version() const { return "0.1.0"; }

QList<ParameterDefinition> RegExAssistant::actionParameterDefinitions() const
{
    return {
        {"Pattern", "RegEx Pattern", ParameterType::String, "", {}, "Regular expression to match"},
        {"Replacement", "Replacement", ParameterType::String, "", {}, "Replacement text (optional)"}
    };
}

QList<PluginActionSet> RegExAssistant::defaultActionSets() const
{
    QList<PluginActionSet> list;
    {
        PluginActionSet f;
        f.id = "remove_extra_spaces";
        f.name = "Remove Extra Spaces";
        f.parameters["Pattern"] = "\\s+";
        f.parameters["Replacement"] = " ";
        list.append(f);
    }
    {
        PluginActionSet f;
        f.id = "extract_email";
        f.name = "Extract Email";
        f.parameters["Pattern"] = "[\\w\\.-]+@[\\w\\.-]+\\.[\\w]+";
        f.parameters["Replacement"] = "";
        list.append(f);
    }
    return list;
}

void RegExAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError("No text in clipboard"); return; }
    
    QString pattern = actionParams.value("Pattern").toString();
    QString replacement = actionParams.value("Replacement").toString();
    QString text = data->text();
    
    QRegularExpression regex(pattern);
    if (!regex.isValid()) { callback->onError("Invalid RegEx: " + regex.errorString()); return; }
    
    if (!replacement.isEmpty() || actionParams.contains("Replacement")) {
        QString result = text;
        result.replace(regex, replacement);
        callback->onTextData(result, true);
    } else {
        QStringList matches;
        QRegularExpressionMatchIterator i = regex.globalMatch(text);
        while (i.hasNext()) matches << i.next().captured(0);
        callback->onTextData(matches.isEmpty() ? "No matches found." : matches.join("\n"), true);
    }
    callback->onFinished();
}
