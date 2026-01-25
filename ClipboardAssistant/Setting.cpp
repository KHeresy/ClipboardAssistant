#include "Setting.h"
#include <QSettings>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

Setting::Setting(const QList<IClipboardPlugin*>& plugins, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingClass)
    , m_plugins(plugins)
{
    ui->setupUi(this);

    // Load Hotkey
    QSettings settings("Heresy", "ClipboardAssistant");
    QString hotkeyStr = settings.value("GlobalHotkey", "Ctrl+Alt+V").toString();
    ui->keySequenceEdit->setKeySequence(QKeySequence(hotkeyStr));

    // Setup Plugins
    ui->listPlugins->clear();
    for (IClipboardPlugin* plugin : m_plugins) {
        ui->listPlugins->addItem(plugin->name());
        
        QWidget* page = new QWidget();
        if (plugin->hasSettings()) {
            // Since the plugin interface currently only supports showing a modal dialog (showSettings),
            // we'll add a button here to launch it. 
            // Ideally, the interface should allow embedding a widget.
            QVBoxLayout* layout = new QVBoxLayout(page);
            QPushButton* btn = new QPushButton("Configure " + plugin->name(), page);
            connect(btn, &QPushButton::clicked, [plugin, this]() {
                plugin->showSettings(this);
            });
            layout->addWidget(btn);
            layout->addStretch();
        } else {
            QVBoxLayout* layout = new QVBoxLayout(page);
            layout->addWidget(new QLabel("No settings for this plugin."));
        }
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

void Setting::onAccepted()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.setValue("GlobalHotkey", ui->keySequenceEdit->keySequence().toString());
    accept();
}