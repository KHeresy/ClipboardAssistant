#include "TextInputAssistant.h"
#include <QMimeData>
#include <QInputDialog>
#include <QApplication>

TextInputAssistant::TextInputAssistant(QObject* parent) : QObject(parent)
{
}

QString TextInputAssistant::name() const { return "Text Input Assistant"; }
QString TextInputAssistant::version() const { return "0.1.0"; }

QList<ParameterDefinition> TextInputAssistant::actionParameterDefinitions() const
{
    return {
        {"Mode", "Input Mode", ParameterType::Choice, "Static Content", {"Static Content", "Ask at Runtime"}, "Choose how to get the text."},
        {"Content", "Text Content", ParameterType::Text, "", {}, "The text to insert (for Static Content mode)."},
        {"Position", "Placement", ParameterType::Choice, "Replace", {"Replace", "Append", "Prepend"}, "Where to put the new text relative to current content."}
    };
}

QList<PluginActionTemplate> TextInputAssistant::actionTemplates() const
{
    QList<PluginActionTemplate> list;
    list.append({"fixed_text", "Insert Fixed Text", {{"Mode", "Static Content"}, {"Position", "Append"}}});
    list.append({"ask_text", "Prompt for Input", {{"Mode", "Ask at Runtime"}, {"Position", "Replace"}}});
    return list;
}

void TextInputAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    QString mode = actionParams.value("Mode").toString();
    QString position = actionParams.value("Position").toString();
    QString inputText = actionParams.value("Content").toString();
    QString currentText = data->text();

    if (mode == "Ask at Runtime") {
        bool ok;
        // 在主執行緒彈出輸入框
        QString val = QInputDialog::getMultiLineText(nullptr, "Manual Input", "Enter text for the pipeline:", "", &ok);
        if (ok) {
            inputText = val;
        } else {
            callback->onError("User cancelled input.");
            return;
        }
    }

    QString result;
    if (position == "Append") result = currentText + inputText;
    else if (position == "Prepend") result = inputText + currentText;
    else result = inputText; // Replace

    callback->onTextData(result, true);
    callback->onFinished();
}
