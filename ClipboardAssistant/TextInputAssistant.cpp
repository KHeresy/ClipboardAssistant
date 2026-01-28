#include "TextInputAssistant.h"
#include <QMimeData>
#include <QInputDialog>
#include <QApplication>
#include <QWindow>
#include <QKeyEvent>
#include <QPlainTextEdit>

class TextInputEventFilter : public QObject {
public:
    TextInputEventFilter(QDialog* dlg) : QObject(dlg), m_dlg(dlg) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return && (keyEvent->modifiers() & Qt::ControlModifier)) {
                m_dlg->accept();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Escape) {
                m_dlg->reject();
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
private:
    QDialog* m_dlg;
};

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
        dlg.setLabelText("Enter text for the pipeline:<br/>(Ctrl+Enter to Finish, Esc to Cancel)");
        dlg.setOption(QInputDialog::UsePlainTextEditForTextInput, true);
        
        // 確保它是個 Dialog 並在最上層
        dlg.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);

        // 安裝事件過濾器以處理 Ctrl+Enter 和 Esc
        TextInputEventFilter* filter = new TextInputEventFilter(&dlg);
        dlg.installEventFilter(filter);
        // 也需要安裝在輸入框上，因為 QInputDialog 的內容通常是它的子元件
        if (QPlainTextEdit* edit = dlg.findChild<QPlainTextEdit*>()) {
            edit->installEventFilter(filter);
        }

        if (dlg.exec() == QDialog::Accepted) {
            inputText = dlg.textValue();
            ok = true;
        }

        if (!ok) {
            // 中斷流程且不彈出錯誤
            callback->onError("");
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

