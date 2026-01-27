#include "RegExAssistant.h"
#include <QSettings>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QUuid>
#include <QKeySequenceEdit>
#include <QCheckBox>
#include <algorithm>

RegExAssistant::RegExAssistant(QObject* parent) : QObject(parent)
{
    ensureDefaultActions();
}

QString RegExAssistant::name() const { return "RegEx Assistant"; }
QString RegExAssistant::version() const { return "0.1.0"; }

void RegExAssistant::ensureDefaultActions()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.beginGroup("RegEx/Actions");
    if (settings.childGroups().isEmpty()) {
        settings.endGroup();
        auto addDefault = [&](const QString& name, const QString& pattern, const QString& replace, int o) {
            QString id = QUuid::createUuid().toString(QUuid::Id128);
            settings.beginGroup("RegEx/Actions/" + id);
            settings.setValue("Name", name); settings.setValue("Pattern", pattern);
            settings.setValue("Replacement", replace); settings.setValue("Order", o);
            settings.endGroup();
        };
        addDefault("Remove Extra Spaces", "\\s+", " ", 0);
        addDefault("Extract Email", "[\\w\\.-]+@[\\w\\.-]+\\.[\\w]+", "", 1);
    } else settings.endGroup();
}

QList<PluginActionSet> RegExAssistant::actionSets() const
{
    QList<PluginActionSet> list;
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.beginGroup("RegEx/Actions");
    QStringList ids = settings.childGroups();
    struct SortedActionSet { PluginActionSet f; int order; };
    QList<SortedActionSet> sortedList;
    for (const QString& id : ids) {
        settings.beginGroup(id);
        PluginActionSet f; f.id = id; f.name = settings.value("Name").toString();
        f.description = "Regex: " + settings.value("Pattern").toString();
        f.customShortcut = QKeySequence(settings.value("Shortcut").toString());
        f.isCustomShortcutGlobal = settings.value("IsGlobal", false).toBool();
        f.isAutoCopy = settings.value("IsAutoCopy", false).toBool();
        int order = settings.value("Order", 999).toInt();
        sortedList.append({f, order});
        settings.endGroup();
    }
    settings.endGroup();
    std::sort(sortedList.begin(), sortedList.end(), [](const SortedActionSet& a, const SortedActionSet& b) { return a.order < b.order; });
    for(const auto& sf : sortedList) list.append(sf.f);
    return list;
}

void RegExAssistant::process(const QString& actionSetId, const QMimeData* data, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError("No text in clipboard"); return; }
    QSettings settings("Heresy", "ClipboardAssistant");
    QString group = "RegEx/Actions/" + actionSetId;
    QString pattern = settings.value(group + "/Pattern").toString();
    QString replacement = settings.value(group + "/Replacement").toString();
    QString text = data->text();
    QRegularExpression regex(pattern);
    if (!regex.isValid()) { callback->onError("Invalid RegEx: " + regex.errorString()); return; }
    if (!replacement.isEmpty() || (settings.contains(group + "/Replacement") && !settings.value(group + "/Replacement").toString().isNull())) {
        QString result = text; result.replace(regex, replacement); callback->onTextData(result, true);
    } else {
        QStringList matches; QRegularExpressionMatchIterator i = regex.globalMatch(text);
        while (i.hasNext()) matches << i.next().captured(0);
        callback->onTextData(matches.isEmpty() ? "No matches found." : matches.join("\n"), true);
    }
    callback->onFinished();
}

bool RegExAssistant::hasSettings() const { return false; }
void RegExAssistant::showSettings(QWidget*) {}
bool RegExAssistant::isEditable() const { return true; }

QWidget* RegExAssistant::getSettingsWidget(const QString& actionSetId, QWidget* parent) {
    QWidget* content = new QWidget(parent);
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0,0,0,0);

    QLineEdit* ePattern = new QLineEdit(content);
    QLineEdit* eReplace = new QLineEdit(content);
    
    // Store pointers for access in saveSettings? No, we can't easily. 
    // We should name them or use object names.
    ePattern->setObjectName("Pattern");
    eReplace->setObjectName("Replacement");

    if (!actionSetId.isEmpty()) {
        QSettings s("Heresy", "ClipboardAssistant"); 
        QString group = "RegEx/Actions/" + actionSetId;
        ePattern->setText(s.value(group + "/Pattern").toString());
        eReplace->setText(s.value(group + "/Replacement").toString());
    }
    
    contentLayout->addWidget(new QLabel("Pattern:")); contentLayout->addWidget(ePattern);
    contentLayout->addWidget(new QLabel("Replacement:")); contentLayout->addWidget(eReplace);
    
    return content;
}

QString RegExAssistant::saveSettings(const QString& actionSetId, QWidget* widget, const QString& name, const QKeySequence& shortcut, bool isGlobal, bool isAutoCopy) {
    QString id = actionSetId;
    if (id.isEmpty()) id = QUuid::createUuid().toString(QUuid::Id128);
    
    QLineEdit* ePattern = widget->findChild<QLineEdit*>("Pattern");
    QLineEdit* eReplace = widget->findChild<QLineEdit*>("Replacement");
    
    QSettings s("Heresy", "ClipboardAssistant");
    s.beginGroup("RegEx/Actions/" + id);
    s.setValue("Name", name);
    s.setValue("Pattern", ePattern ? ePattern->text() : "");
    s.setValue("Replacement", eReplace ? eReplace->text() : "");
    s.setValue("Shortcut", shortcut.toString());
    s.setValue("IsGlobal", isGlobal);
    s.setValue("IsAutoCopy", isAutoCopy);
    if (actionSetId.isEmpty()) s.setValue("Order", 999);
    s.endGroup();
    
    return id;
}

QString RegExAssistant::createActionSet(QWidget* parent) { return QString(); }
void RegExAssistant::editActionSet(const QString& actionSetId, QWidget* parent) {}

void RegExAssistant::deleteActionSet(const QString& actionSetId) { QSettings s("Heresy", "ClipboardAssistant"); s.remove("RegEx/Actions/" + actionSetId); }
void RegExAssistant::setActionSetOrder(const QString& fid, int order) { QSettings s("Heresy", "ClipboardAssistant"); s.setValue("RegEx/Actions/" + fid + "/Order", order); }