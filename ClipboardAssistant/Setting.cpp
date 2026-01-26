#include "Setting.h"
#include <QSettings>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

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

    // Setup Plugins
    ui->listPlugins->clear();
    for (const auto& info : plugins) {
        IClipboardPlugin* plugin = info.plugin;
        ui->listPlugins->addItem(plugin->name());
        
        QWidget* page = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(page);
        
        // Header: Name and Version
        QLabel* title = new QLabel(QString("<b>%1</b> v%2").arg(plugin->name(), plugin->version()));
        layout->addWidget(title);

        // Source Info
        QString sourceText = info.isInternal ? "<i>Built-in Module</i>" : QString("<i>External Plugin: %1</i>").arg(info.filePath);
        QLabel* sourceLabel = new QLabel(sourceText);
        sourceLabel->setStyleSheet("color: gray;");
        layout->addWidget(sourceLabel);

        layout->addSpacing(10);

        // Capabilities
        auto dataTypeToString = [](IClipboardPlugin::DataTypes types) {
            QStringList parts;
            if (types.testFlag(IClipboardPlugin::Text)) parts << "Text";
            if (types.testFlag(IClipboardPlugin::Image)) parts << "Image";
            if (types.testFlag(IClipboardPlugin::Rtf)) parts << "RTF";
            if (types.testFlag(IClipboardPlugin::File)) parts << "File";
            return parts.isEmpty() ? "None" : parts.join(", ");
        };

        QLabel* capTitle = new QLabel("<b>Module Capabilities:</b>");
        layout->addWidget(capTitle);
        
        layout->addWidget(new QLabel(QString(" - Inputs: %1").arg(dataTypeToString(plugin->supportedInputs()))));
        layout->addWidget(new QLabel(QString(" - Outputs: %1").arg(dataTypeToString(plugin->supportedOutputs()))));
        layout->addWidget(new QLabel(QString(" - Streaming: %1").arg(plugin->supportsStreaming() ? "Yes" : "No")));

        layout->addSpacing(10);

        if (plugin->hasSettings()) {
            QPushButton* btn = new QPushButton("Configure " + plugin->name(), page);
            connect(btn, &QPushButton::clicked, [plugin, this]() {
                plugin->showSettings(this);
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
    if (row >= 0 && row < ui->stackedWidgetPlugins->count() - 1) { // -1 because of initial empty page if we kept it, but we cleared list.
        // Actually, we added pages in sync with list items.
        // Let's check indices. Page 0 is "pageEmpty" from UI file.
        // We appended pages. So index 1 corresponds to item 0.
        ui->stackedWidgetPlugins->setCurrentIndex(row + 1); 
    }
}

void Setting::accept()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.setValue("GlobalHotkey", ui->keySequenceEdit->keySequence().toString());
    settings.setValue("AutoCopy", ui->checkBoxAutoCopy->isChecked());
    settings.setValue("CloseOnEsc", ui->checkBoxCloseOnEsc->isChecked());
    QDialog::accept();
}