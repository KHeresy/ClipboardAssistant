#include "ClipboardAssistant.h"
#include <QClipboard>
#include <QApplication>
#include <QPluginLoader>
#include <QDir>
#include <QMessageBox>
#include <QCloseEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialog>
#include <QSettings>
#include <windows.h> 
#include "Setting.h"

ClipboardAssistant::ClipboardAssistant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClipboardAssistantClass)
{
    ui->setupUi(this);

    // Clipboard Monitor
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardAssistant::onClipboardChanged);
    onClipboardChanged(); // Initial load

    // UI Signals
    connect(ui->listFeatures, &QListWidget::itemClicked, this, &ClipboardAssistant::onFeatureClicked);
    connect(ui->btnCopyOutput, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCopyOutputClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &ClipboardAssistant::onBtnSettingsClicked);

    loadPlugins();
    setupTrayIcon();
    registerGlobalHotkey();
}

ClipboardAssistant::~ClipboardAssistant()
{
    unregisterGlobalHotkey();
    delete ui;
}

void ClipboardAssistant::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

bool ClipboardAssistant::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == 100) { // Our ID
            show();
            activateWindow();
            return true;
        }
    }
    return false;
}

void ClipboardAssistant::onClipboardChanged()
{
    const QMimeData* data = QApplication::clipboard()->mimeData();
    if (data->hasText()) {
        ui->textClipboard->setText(data->text());
    } else if (data->hasImage()) {
        ui->textClipboard->setText("[Image Content]");
    } else {
        ui->textClipboard->setText("[Unknown Content]");
    }
}

void ClipboardAssistant::loadPlugins()
{
    QDir dir(QCoreApplication::applicationDirPath());
    QStringList files = dir.entryList(QStringList() << "*.dll", QDir::Files);
    
    for (const QString& fileName : files) {
        QPluginLoader loader(dir.absoluteFilePath(fileName));
        QObject* plugin = loader.instance();
        if (plugin) {
            IClipboardPlugin* iPlugin = qobject_cast<IClipboardPlugin*>(plugin);
            if (iPlugin) {
                m_plugins.append(iPlugin);
                for (const auto& feature : iPlugin->features()) {
                    QListWidgetItem* item = new QListWidgetItem(feature.name + " (" + feature.description + ")", ui->listFeatures);
                    QString uniqueId = iPlugin->name() + "::" + feature.id;
                    item->setData(Qt::UserRole, uniqueId);
                    
                    FeatureInfo info = { iPlugin, feature.id };
                    m_featureMap.insert(uniqueId, info);
                }
            }
        }
    }
}

void ClipboardAssistant::onFeatureClicked(QListWidgetItem* item)
{
    QString uniqueId = item->data(Qt::UserRole).toString();
    if (m_featureMap.contains(uniqueId)) {
        FeatureInfo info = m_featureMap[uniqueId];
        
        ui->textOutput->clear();
        ui->textOutput->setText("Processing...");
        
        PluginCallback* callback = new PluginCallback(this);
        info.plugin->process(info.featureId, QApplication::clipboard()->mimeData(), callback);
    }
}

void ClipboardAssistant::onBtnCopyOutputClicked()
{
    QApplication::clipboard()->setText(ui->textOutput->toPlainText());
}

void ClipboardAssistant::onBtnSettingsClicked()
{
    Setting dlg(m_plugins, this);
    if (dlg.exec() == QDialog::Accepted) {
        unregisterGlobalHotkey();
        registerGlobalHotkey();
    }
}

void ClipboardAssistant::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/ClipboardAssistant/app_icon.jpg"));
    
    m_trayMenu = new QMenu(this);
    QAction* showAction = m_trayMenu->addAction("Show");
    connect(showAction, &QAction::triggered, this, &QWidget::show);
    QAction* quitAction = m_trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ClipboardAssistant::onTrayIconActivated);
}

void ClipboardAssistant::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        show();
        activateWindow();
    }
}

void ClipboardAssistant::registerGlobalHotkey()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    QString hotkeyStr = settings.value("GlobalHotkey", "Ctrl+Alt+V").toString();
    QKeySequence ks(hotkeyStr);
    
    // Simple mapping for demonstration (Needs more robust mapping for production)
    // Assuming format "Ctrl+Alt+V"
    
    // Better parser needed for arbitrary keys, but sticking to basic MODs + Key char for now
    // Or we could use a library like QHotKey if external libs were allowed, but here we use WinAPI directly.
    
    // Parse QKeySequence to WinAPI
    // This is a naive implementation.
    
    UINT modifiers = 0;
    if (hotkeyStr.contains("Ctrl")) modifiers |= MOD_CONTROL;
    if (hotkeyStr.contains("Alt")) modifiers |= MOD_ALT;
    if (hotkeyStr.contains("Shift")) modifiers |= MOD_SHIFT;
    
    int key = 0;
    // Extract the last part as the key
    QStringList parts = hotkeyStr.split("+");
    if (!parts.isEmpty()) {
        QString keyPart = parts.last();
        if (keyPart.length() == 1) {
            key = keyPart.at(0).toUpper().unicode();
        } else {
             // Handle special keys F1-F12, etc if needed. 
             // Defaults to 'V' if parsing fails or fallback.
             if (key == 0) key = 'V'; 
        }
    }
    
    if (!RegisterHotKey((HWND)winId(), 100, modifiers, key)) {
         // Log or warn
    }
}

void ClipboardAssistant::unregisterGlobalHotkey()
{
    UnregisterHotKey((HWND)winId(), 100);
}

// Callback implementation
ClipboardAssistant::PluginCallback::PluginCallback(ClipboardAssistant* parent) 
    : m_parent(parent) 
{
}

void ClipboardAssistant::PluginCallback::onTextData(const QString& text, bool isFinal)
{
    // Ensure GUI updates happen on main thread
    QMetaObject::invokeMethod(m_parent, [this, text, isFinal]() {
        m_parent->handlePluginOutput(text, !m_firstChunk);
        if (m_firstChunk) m_firstChunk = false;
    });
}

void ClipboardAssistant::PluginCallback::onError(const QString& message)
{
     QMetaObject::invokeMethod(m_parent, [this, message]() {
        m_parent->handlePluginError(message);
    });
}

void ClipboardAssistant::PluginCallback::onFinished()
{
    delete this;
}

void ClipboardAssistant::handlePluginOutput(const QString& text, bool append)
{
    if (!append) {
        ui->textOutput->clear();
    }
    ui->textOutput->insertPlainText(text);
    ui->textOutput->moveCursor(QTextCursor::End);
}

void ClipboardAssistant::handlePluginError(const QString& msg)
{
    QMessageBox::critical(this, "Plugin Error", msg);
}
