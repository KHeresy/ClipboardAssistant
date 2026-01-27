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
QString RegExAssistant::version() const { return "1.0.0"; }

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

QList<PluginFeature> RegExAssistant::features() const
{
    QList<PluginFeature> list;
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.beginGroup("RegEx/Actions");
    QStringList ids = settings.childGroups();
    struct SortedFeature { PluginFeature f; int order; };
    QList<SortedFeature> sortedList;
    for (const QString& id : ids) {
        settings.beginGroup(id);
        PluginFeature f; f.id = id; f.name = settings.value("Name").toString();
        f.description = "Regex: " + settings.value("Pattern").toString();
        f.customShortcut = QKeySequence(settings.value("Shortcut").toString());
        f.isCustomShortcutGlobal = settings.value("IsGlobal", false).toBool();
        int order = settings.value("Order", 999).toInt();
        sortedList.append({f, order});
        settings.endGroup();
    }
    settings.endGroup();
    std::sort(sortedList.begin(), sortedList.end(), [](const SortedFeature& a, const SortedFeature& b) { return a.order < b.order; });
    for(const auto& sf : sortedList) list.append(sf.f);
    return list;
}

void RegExAssistant::process(const QString& featureId, const QMimeData* data, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError("No text in clipboard"); return; }
    QSettings settings("Heresy", "ClipboardAssistant");
    QString group = "RegEx/Actions/" + featureId;
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

QString RegExAssistant::createFeature(QWidget* parent)
{
    QDialog dialog(parent); dialog.setWindowTitle("Add RegEx Action");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLineEdit* eName = new QLineEdit(&dialog); QLineEdit* ePattern = new QLineEdit(&dialog);
    QLineEdit* eReplace = new QLineEdit(&dialog); QKeySequenceEdit* eShortcut = new QKeySequenceEdit(&dialog);
    QCheckBox* cGlobal = new QCheckBox("Global", &dialog);
    layout->addWidget(new QLabel("Name:")); layout->addWidget(eName);
    layout->addWidget(new QLabel("Pattern:")); layout->addWidget(ePattern);
    layout->addWidget(new QLabel("Replacement:")); layout->addWidget(eReplace);
    layout->addWidget(new QLabel("Shortcut:")); layout->addWidget(eShortcut); layout->addWidget(cGlobal);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QString id = QUuid::createUuid().toString(QUuid::Id128); QSettings s("Heresy", "ClipboardAssistant");
        s.beginGroup("RegEx/Actions/" + id); s.setValue("Name", eName->text()); s.setValue("Pattern", ePattern->text());
        s.setValue("Replacement", eReplace->text()); s.setValue("Shortcut", eShortcut->keySequence().toString());
        s.setValue("IsGlobal", cGlobal->isChecked()); s.setValue("Order", 999); s.endGroup(); return id;
    }
    return QString();
}

void RegExAssistant::editFeature(const QString& featureId, QWidget* parent)
{
    QSettings s("Heresy", "ClipboardAssistant"); QString group = "RegEx/Actions/" + featureId;
    QDialog dialog(parent); dialog.setWindowTitle("Edit RegEx Action");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLineEdit* eName = new QLineEdit(&dialog); eName->setText(s.value(group + "/Name").toString());
    QLineEdit* ePattern = new QLineEdit(&dialog); ePattern->setText(s.value(group + "/Pattern").toString());
    QLineEdit* eReplace = new QLineEdit(&dialog); eReplace->setText(s.value(group + "/Replacement").toString());
    QKeySequenceEdit* eShortcut = new QKeySequenceEdit(&dialog); eShortcut->setKeySequence(QKeySequence(s.value(group + "/Shortcut").toString()));
    QCheckBox* cGlobal = new QCheckBox("Global", &dialog); cGlobal->setChecked(s.value(group + "/IsGlobal").toBool());
    layout->addWidget(new QLabel("Name:")); layout->addWidget(eName);
    layout->addWidget(new QLabel("Pattern:")); layout->addWidget(ePattern);
    layout->addWidget(new QLabel("Replacement:")); layout->addWidget(eReplace);
    layout->addWidget(new QLabel("Shortcut:")); layout->addWidget(eShortcut); layout->addWidget(cGlobal);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        s.beginGroup(group); s.setValue("Name", eName->text()); s.setValue("Pattern", ePattern->text());
        s.setValue("Replacement", eReplace->text()); s.setValue("Shortcut", eShortcut->keySequence().toString());
        s.setValue("IsGlobal", cGlobal->isChecked()); s.endGroup();
    }
}

void RegExAssistant::deleteFeature(const QString& featureId) { QSettings s("Heresy", "ClipboardAssistant"); s.remove("RegEx/Actions/" + featureId); }
void RegExAssistant::setFeatureOrder(const QString& fid, int order) { QSettings s("Heresy", "ClipboardAssistant"); s.setValue("RegEx/Actions/" + fid + "/Order", order); }