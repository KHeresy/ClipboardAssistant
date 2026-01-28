#include "Setting.h"
#include <QSettings>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QFormLayout>

#include <QCoreApplication>
#include <QDir>

Setting::Setting(const QList<PluginInfo>& plugins, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingClass)
{
    ui->setupUi(this);

    // Load Hotkey
    QSettings settings("Heresy", "ClipboardAssistant");
    QString hotkeyStr = settings.value("GlobalHotkey", "Ctrl+Alt+V").toString();
    ui->keySequenceEdit->setKeySequence(QKeySequence(hotkeyStr));
    ui->checkBoxAutoCopy->setChecked(settings.value("AutoCopy", false).toBool());
    ui->checkBoxCloseOnEsc->setChecked(settings.value("CloseOnEsc", false).toBool());
    ui->checkBoxStartMinimized->setChecked(settings.value("StartMinimized", false).toBool());

    // Auto-start check
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    ui->checkBoxAutoStart->setChecked(bootSettings.contains("ClipboardAssistant"));

    // Setup Plugins
    ui->listPlugins->clear();
    for (const auto& info : plugins) {
        IClipboardPlugin* plugin = info.plugin;
        m_plugins.append(plugin);
        ui->listPlugins->addItem(plugin->name());
        
        QWidget* page = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(page);
        
        // Header: Name and Version
        QLabel* title = new QLabel(QString("<b>%1</b> v%2").arg(plugin->name(), plugin->version()));
        layout->addWidget(title);

        // Source Info
        QString sourceText = info.isInternal ? tr("<i>Built-in Module</i>") : tr("<i>External Plugin: %1</i>").arg(info.filePath);
        QLabel* sourceLabel = new QLabel(sourceText);
        sourceLabel->setStyleSheet("color: gray;");
        layout->addWidget(sourceLabel);

        layout->addSpacing(10);

        // Capabilities
        auto dataTypeToString = [](IClipboardPlugin::DataTypes types) {
            QStringList parts;
            if (types.testFlag(IClipboardPlugin::Text)) parts << tr("Text");
            if (types.testFlag(IClipboardPlugin::Image)) parts << tr("Image");
            if (types.testFlag(IClipboardPlugin::Rtf)) parts << tr("RTF");
            if (types.testFlag(IClipboardPlugin::File)) parts << tr("File");
            return parts.isEmpty() ? tr("None") : parts.join(", ");
        };

        QLabel* capTitle = new QLabel(tr("<b>Module Capabilities:</b>"));
        layout->addWidget(capTitle);
        
        layout->addWidget(new QLabel(tr(" - Inputs: %1").arg(dataTypeToString(plugin->supportedInputs()))));
        layout->addWidget(new QLabel(tr(" - Outputs: %1").arg(dataTypeToString(plugin->supportedOutputs()))));
        layout->addWidget(new QLabel(tr(" - Streaming: %1").arg(plugin->supportsStreaming() ? tr("Yes") : tr("No"))));

        layout->addSpacing(10);

        // Global Parameters
        QList<ParameterDefinition> gDefs = plugin->globalParameterDefinitions();
        if (!gDefs.isEmpty()) {
            m_paramDefs[plugin] = gDefs;
            QLabel* gTitle = new QLabel(tr("<b>Module Configuration:</b>"));
            layout->addWidget(gTitle);

            QFormLayout* form = new QFormLayout();
            QMap<QString, QWidget*> widgets;
            
            QSettings ps("Heresy", "ClipboardAssistant");
            ps.beginGroup("Plugins/" + plugin->name() + "/Global");

            for (const auto& def : gDefs) {
                QWidget* widget = nullptr;
                QVariant val = ps.value(def.id, def.defaultValue);

                switch (def.type) {
                case ParameterType::String: {
                    QLineEdit* e = new QLineEdit(page); e->setText(val.toString()); widget = e; break;
                }
                case ParameterType::Password: {
                    QLineEdit* e = new QLineEdit(page); e->setEchoMode(QLineEdit::Password); e->setText(val.toString()); widget = e; break;
                }
                case ParameterType::Text: {
                    QTextEdit* e = new QTextEdit(page); e->setPlainText(val.toString()); e->setMaximumHeight(80); widget = e; break;
                }
                case ParameterType::Bool: {
                    QCheckBox* c = new QCheckBox(page); c->setChecked(val.toBool()); widget = c; break;
                }
                case ParameterType::Choice: {
                    QComboBox* c = new QComboBox(page); c->addItems(def.options); c->setCurrentText(val.toString()); widget = c; break;
                }
                case ParameterType::Number: {
                    QSpinBox* s = new QSpinBox(page); s->setRange(-999999, 999999); s->setValue(val.toInt()); widget = s; break;
                }
                }

                if (widget) {
                    widget->setToolTip(def.description);
                    form->addRow(tr("%1:").arg(def.name), widget);
                    widgets.insert(def.id, widget);
                }
            }
            ps.endGroup();
            layout->addLayout(form);
            m_paramWidgets[plugin] = widgets;
        }

        if (plugin->hasConfiguration()) {
            QPushButton* btn = new QPushButton(tr("Advanced Configuration"), page);
            connect(btn, &QPushButton::clicked, [plugin, this]() {
                plugin->showConfiguration(this);
            });
            layout->addWidget(btn);
        }
        
        layout->addStretch();
        ui->stackedWidgetPlugins->addWidget(page);
    }

    connect(ui->listPlugins, &QListWidget::currentRowChanged, this, &Setting::onPluginSelected);
    
    if (ui->listPlugins->count() > 0) {
        ui->listPlugins->setCurrentRow(0);
    }
}

Setting::~Setting()
{
    delete ui;
}

QKeySequence Setting::getHotkey() const
{
    return ui->keySequenceEdit->keySequence();
}

void Setting::setHotkey(const QKeySequence& sequence)
{
    ui->keySequenceEdit->setKeySequence(sequence);
}

void Setting::onPluginSelected(int row)
{
    if (row >= 0 && row < ui->stackedWidgetPlugins->count() - 1) {
        ui->stackedWidgetPlugins->setCurrentIndex(row + 1); 
    }
}

void Setting::accept()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.setValue("GlobalHotkey", ui->keySequenceEdit->keySequence().toString());
    settings.setValue("AutoCopy", ui->checkBoxAutoCopy->isChecked());
    settings.setValue("CloseOnEsc", ui->checkBoxCloseOnEsc->isChecked());
    settings.setValue("StartMinimized", ui->checkBoxStartMinimized->isChecked());

    // Save Auto-start
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (ui->checkBoxAutoStart->isChecked()) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        bootSettings.setValue("ClipboardAssistant", "\"" + appPath + "\"");
    } else {
        bootSettings.remove("ClipboardAssistant");
    }

    // Save Plugin Global Parameters
    for (IClipboardPlugin* plugin : m_plugins) {
        if (!m_paramWidgets.contains(plugin)) continue;
        
        settings.beginGroup("Plugins/" + plugin->name() + "/Global");
        const auto& widgets = m_paramWidgets[plugin];
        const auto& defs = m_paramDefs[plugin];

        for (const auto& def : defs) {
            QWidget* widget = widgets.value(def.id);
            if (!widget) continue;

            QVariant val;
            switch (def.type) {
            case ParameterType::String:
            case ParameterType::Password:
                val = qobject_cast<QLineEdit*>(widget)->text(); break;
            case ParameterType::Text:
                val = qobject_cast<QTextEdit*>(widget)->toPlainText(); break;
            case ParameterType::Bool:
                val = qobject_cast<QCheckBox*>(widget)->isChecked(); break;
            case ParameterType::Choice:
                val = qobject_cast<QComboBox*>(widget)->currentText(); break;
            case ParameterType::Number:
                val = qobject_cast<QSpinBox*>(widget)->value(); break;
            }
            settings.setValue(def.id, val);
        }
        settings.endGroup();
    }

    QDialog::accept();
}