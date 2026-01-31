#include "RegExAssistant.h"
#include <QRegularExpression>
#include <QMimeData>

RegExAssistant::RegExAssistant(QObject* parent) : QObject(parent)

{

}



QString RegExAssistant::id() const { return "kheresy.RegExAssistant"; }



QString RegExAssistant::name() const { return tr("RegEx Assistant"); }
QString RegExAssistant::version() const { return "0.2.0"; }

QList<ParameterDefinition> RegExAssistant::actionParameterDefinitions() const
{
    return {
        {"Pattern", tr("RegEx Pattern"), ParameterType::String, "", {}, tr("Regular expression to match")},
        {"Replacement", tr("Replacement"), ParameterType::String, "", {}, tr("Replacement text (optional)")}
    };
}

QList<PluginActionTemplate> RegExAssistant::actionTemplates() const
{
    QList<PluginActionTemplate> list;
    list.append({"remove_extra_spaces", tr("Remove Extra Spaces"), {{"Pattern", "\\s+"}, {"Replacement", " "}}});
    list.append({"extract_email", tr("Extract Email"), {{"Pattern", "[\\w\\.-]+@[\\w\\.-]+\\.[\\w]+"}, {"Replacement", ""}}});
    return list;
}

void RegExAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError(tr("No text in clipboard")); return; }
    
    QString pattern = actionParams.value("Pattern").toString();
    QString replacement = actionParams.value("Replacement").toString();
    QString text = data->text();
    
    QRegularExpression regex(pattern);
    if (!regex.isValid()) { callback->onError(tr("Invalid RegEx: ") + regex.errorString()); return; }
    
    if (!replacement.isEmpty() || actionParams.contains("Replacement")) {
        QString result = text;
        result.replace(regex, replacement);
        callback->onTextData(result, true);
    } else {
        QStringList matches;
        QRegularExpressionMatchIterator i = regex.globalMatch(text);
        while (i.hasNext()) matches << i.next().captured(0);
        callback->onTextData(matches.isEmpty() ? tr("No matches found.") : matches.join("\n"), true);
    }
    callback->onFinished();
}
