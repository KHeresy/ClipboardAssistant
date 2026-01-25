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
#include <QBuffer>
#include <QImageReader>
#include <QRegularExpression>
#include <QNetworkRequest>

ClipboardAssistant::ClipboardAssistant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClipboardAssistantClass)
{
    ui->setupUi(this);
    
    m_networkManager = new QNetworkAccessManager(this);

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
    
    m_currentHtml.clear();
    m_pendingDownloads.clear();

    if (data->hasImage()) {
        QImage image = qvariant_cast<QImage>(data->imageData());
        if (!image.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
            QString base64 = byteArray.toBase64();
            QString html = QString("<img src='data:image/png;base64,%1' />").arg(base64);
            ui->textClipboard->setHtml(html);
        } else {
             ui->textClipboard->setText("[Invalid Image Content]");
        }
    } else if (data->hasHtml()) {
        m_currentHtml = data->html();
        ui->textClipboard->setHtml(m_currentHtml);
        processHtmlImages(m_currentHtml);
    } else if (data->hasText()) {
        ui->textClipboard->setText(data->text());
    } else {
        ui->textClipboard->setText("[Unknown Content]");
    }
}

void ClipboardAssistant::processHtmlImages(QString html)
{
    QRegularExpression regex("<img\\s+[^>]*src=[\\\"'](http[^\\\"']+)[\\\"'][^>]*>", QRegularExpression::CaseInsensitiveOption);
    
    QRegularExpressionMatchIterator i = regex.globalMatch(html);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString urlStr = match.captured(1);
        
        if (!m_pendingDownloads.contains(urlStr)) {
            m_pendingDownloads.insert(urlStr);
            
            QNetworkRequest request((QUrl(urlStr)));
            request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
            
            QNetworkReply* reply = m_networkManager->get(request);
            connect(reply, &QNetworkReply::finished, [this, reply, urlStr]() {
                onImageDownloaded(reply, urlStr);
            });
        }
    }
}

void ClipboardAssistant::onImageDownloaded(QNetworkReply* reply, QString originalUrl)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QImage image;
        if (image.loadFromData(data)) {
            ui->textClipboard->document()->addResource(QTextDocument::ImageResource, QUrl(originalUrl), QVariant(image));
            ui->textClipboard->setHtml(m_currentHtml);
        }
    }
    
    m_pendingDownloads.remove(originalUrl);
    reply->deleteLater();
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
    
    UINT modifiers = 0;
    if (hotkeyStr.contains("Ctrl")) modifiers |= MOD_CONTROL;
    if (hotkeyStr.contains("Alt")) modifiers |= MOD_ALT;
    if (hotkeyStr.contains("Shift")) modifiers |= MOD_SHIFT;
    
    int key = 0;
    QStringList parts = hotkeyStr.split("+");
    if (!parts.isEmpty()) {
        QString keyPart = parts.last();
        if (keyPart.length() == 1) {
            key = keyPart.at(0).toUpper().unicode();
        } else {
             if (key == 0) key = 'V'; 
        }
    }
    
    RegisterHotKey((HWND)winId(), 100, modifiers, key);
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