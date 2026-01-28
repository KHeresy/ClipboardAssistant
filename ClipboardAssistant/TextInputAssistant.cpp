#include "TextInputAssistant.h"
#include <QMimeData>
#include <QInputDialog>
#include <QApplication>
#include <QWindow>

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
        bool ok = false;
        
        // 尋找主視窗作為 Parent
        QWidget* parentWidget = QApplication::activeWindow();
        if (!parentWidget) {
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (w->isVisible() && w->objectName() == "ClipboardAssistantClass") {
                    parentWidget = w;
                    break;
                }
            }
        }

        QInputDialog dlg(parentWidget);
        dlg.setWindowTitle("Manual Input");
        dlg.setLabelText("Enter text for the pipeline:");
        dlg.setOption(QInputDialog::UsePlainTextEditForTextInput, true);
        
        // 修正：只保留 WindowStaysOnTopHint，移除衝突的 BottomHint
        // 同時確保它是個 Dialog 並在最上層
        dlg.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);

        if (dlg.exec() == QDialog::Accepted) {
            inputText = dlg.textValue();
            ok = true;
        }

        if (!ok) {
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
