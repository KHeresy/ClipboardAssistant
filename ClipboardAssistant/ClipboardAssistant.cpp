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
#include <QHBoxLayout>
#include <QDialog>
#include <QSettings>
#include <windows.h> 
#include "Setting.h"
#include <QBuffer>
#include <QImageReader>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QInputDialog>
#include <QTimer>
#include <QMimeDatabase>
#include <QMimeType>
#include <QFileInfo>
#include "RegExAssistant.h"

void sendCtrlC() {
    INPUT inputs[8];
    ZeroMemory(inputs, sizeof(inputs));
    int n = 0;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = VK_MENU;
    inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = VK_CONTROL;
    inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n++].ki.wVk = VK_CONTROL;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n++].ki.wVk = 'C';
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = 'C';
    inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[n].type = INPUT_KEYBOARD;
    inputs[n].ki.wVk = VK_CONTROL;
    inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(n, inputs, sizeof(INPUT));
}

ClipboardAssistant::ClipboardAssistant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClipboardAssistantClass)
{
    ui->setupUi(this);
    m_networkManager = new QNetworkAccessManager(this);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardAssistant::onClipboardChanged);
    onClipboardChanged();
    connect(ui->btnCopyOutput, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCopyOutputClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &ClipboardAssistant::onBtnSettingsClicked);
    connect(ui->btnAddFeature, &QPushButton::clicked, this, &ClipboardAssistant::onBtnAddFeatureClicked);
    loadPlugins();
    reloadFeatures();
    setupTrayIcon();
    loadSettings();
    if (ui->splitter_horizontal->sizes().isEmpty() || ui->splitter_horizontal->sizes().at(0) == 0) {
        int halfWidth = width() / 2;
        ui->splitter_horizontal->setSizes({halfWidth, halfWidth});
    }
}

ClipboardAssistant::~ClipboardAssistant()
{
    unregisterGlobalHotkey();
    delete ui;
}

void ClipboardAssistant::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void ClipboardAssistant::loadSettings()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    restoreGeometry(settings.value("geometry").toByteArray());
    ui->splitter_horizontal->restoreState(settings.value("splitter_horizontal").toByteArray());
    ui->splitter->restoreState(settings.value("splitter_vertical").toByteArray());
}

void ClipboardAssistant::saveSettings()
{
    QSettings settings("Heresy", "ClipboardAssistant");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter_horizontal", ui->splitter_horizontal->saveState());
    settings.setValue("splitter_vertical", ui->splitter->saveState());
}

bool ClipboardAssistant::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == 100) {
            QSettings settings("Heresy", "ClipboardAssistant");
            if (settings.value("AutoCopy", false).toBool()) {
                QTimer::singleShot(50, [this]() {
                    sendCtrlC();
                    QTimer::singleShot(300, this, [this]() {
                        show();
                        activateWindow();
                    });
                });
            } else {
                show();
                activateWindow();
            }
            return true;
        } else if (m_hotkeyMap.contains(msg->wParam)) {
            FeatureInfo info = m_hotkeyMap[msg->wParam];
            if (info.mainButton && info.mainButton->isEnabled()) {
                onRunFeature(info.plugin, info.featureId);
            }
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
    updateButtonsState();
}

void ClipboardAssistant::updateButtonsState()
{
    const QMimeData* data = QApplication::clipboard()->mimeData();
    IClipboardPlugin::DataTypes clipboardTypes = IClipboardPlugin::None;
    if (data->hasText() || data->hasHtml()) clipboardTypes |= IClipboardPlugin::Text;
    if (data->hasImage()) clipboardTypes |= IClipboardPlugin::Image;
    if (data->hasUrls()) {
        clipboardTypes |= IClipboardPlugin::File;
        QMimeDatabase db;
        for (const QUrl& url : data->urls()) {
            if (db.mimeTypeForFile(url.toLocalFile()).name().startsWith("image/")) {
                clipboardTypes |= IClipboardPlugin::Image;
                break;
            }
        }
    }

    for (auto it = m_featureMap.begin(); it != m_featureMap.end(); ++it) {
        FeatureInfo& info = it.value();
        if (info.mainButton) {
            IClipboardPlugin::DataTypes supported = info.plugin->supportedInputs();
            bool canHandle = (supported & clipboardTypes) != IClipboardPlugin::None;
            info.mainButton->setEnabled(canHandle);
        }
    }
}

void ClipboardAssistant::processHtmlImages(QString html)
{
    QRegularExpression regex("<img\\s+[^>]*src=[\"'](http[^\"']+)[\"'][^>]*>", QRegularExpression::CaseInsensitiveOption);
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
    m_plugins.clear();
    m_regexAssistant = new RegExAssistant(this);
    m_plugins.append({m_regexAssistant, true, "Built-in"});
    QDir dir(QCoreApplication::applicationDirPath());
    QStringList files = dir.entryList(QStringList() << "*.dll", QDir::Files);
    for (const QString& fileName : files) {
        QPluginLoader loader(dir.absoluteFilePath(fileName));
        QObject* plugin = loader.instance();
        if (plugin) {
            IClipboardPlugin* iPlugin = qobject_cast<IClipboardPlugin*>(plugin);
            if (iPlugin) m_plugins.append({iPlugin, false, fileName});
        }
    }
}

void ClipboardAssistant::clearLayout(QLayout* layout)
{
    if (!layout) return;
    while (auto item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        else if (item->layout()) clearLayout(item->layout());
        delete item;
    }
}

void ClipboardAssistant::reloadFeatures()
{
    unregisterGlobalHotkey();
    qDeleteAll(m_localShortcuts);
    m_localShortcuts.clear();
    clearLayout(ui->layoutFeatures);
    m_featureMap.clear();
    int defaultIndex = 1;
    for (const auto& info : m_plugins) {
        IClipboardPlugin* plugin = info.plugin;
        for (const auto& feature : plugin->features()) {
            addFeatureWidget(plugin, feature, defaultIndex);
            if (defaultIndex <= 9) {
                QKeySequence ks(QString("Ctrl+%1").arg(defaultIndex));
                QShortcut* sc = new QShortcut(ks, this);
                connect(sc, &QShortcut::activated, [this, plugin, id = feature.id]() {
                    QString uniqueId = plugin->name() + "::" + id;
                    if (m_featureMap.contains(uniqueId) && m_featureMap[uniqueId].mainButton->isEnabled()) {
                        onRunFeature(plugin, id);
                    }
                });
                m_localShortcuts.append(sc);
                defaultIndex++;
            }
            if (!feature.customShortcut.isEmpty() && !feature.isCustomShortcutGlobal) {
                QShortcut* sc = new QShortcut(feature.customShortcut, this);
                connect(sc, &QShortcut::activated, [this, plugin, id = feature.id]() {
                    QString uniqueId = plugin->name() + "::" + id;
                    if (m_featureMap.contains(uniqueId) && m_featureMap[uniqueId].mainButton->isEnabled()) {
                        onRunFeature(plugin, id);
                    }
                });
                m_localShortcuts.append(sc);
            }
        }
    }
    ui->layoutFeatures->addStretch();
    registerGlobalHotkey();
    updateButtonsState();
}

void ClipboardAssistant::addFeatureWidget(IClipboardPlugin* plugin, const PluginFeature& feature, int defaultIndex)
{
    QWidget* row = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 2, 2, 2); layout->setSpacing(2);
    QPushButton* btnMain = new QPushButton();
    btnMain->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnMain->setMinimumHeight(50);
    QVBoxLayout* btnLayout = new QVBoxLayout(btnMain);
    btnLayout->setContentsMargins(5, 5, 5, 5); btnLayout->setSpacing(0);
    QLabel* lblName = new QLabel(feature.name);
    lblName->setStyleSheet("font-weight: bold; font-size: 14px;");
    lblName->setAlignment(Qt::AlignCenter);
    QStringList shortcuts;
    if (defaultIndex <= 9) shortcuts << QString("Ctrl+%1").arg(defaultIndex);
    if (!feature.customShortcut.isEmpty()) shortcuts << (feature.customShortcut.toString() + (feature.isCustomShortcutGlobal ? " (Global)" : " (Local)"));
    QLabel* lblShortcut = new QLabel(shortcuts.join(" / "));
    lblShortcut->setStyleSheet("font-size: 9px; color: #666;");
    lblShortcut->setAlignment(Qt::AlignCenter);
    btnLayout->addWidget(lblName); btnLayout->addWidget(lblShortcut);
    connect(btnMain, &QPushButton::clicked, [this, plugin, id = feature.id]() { onRunFeature(plugin, id); });
    layout->addWidget(btnMain);
    QString uniqueId = plugin->name() + "::" + feature.id;
    m_featureMap.insert(uniqueId, { plugin, feature.id, btnMain });
    if (plugin->isEditable()) {
        QVBoxLayout* sideLayout = new QVBoxLayout();
        sideLayout->setSpacing(1);
        QPushButton* btnEdit = new QPushButton("E"); btnEdit->setFixedSize(22, 22);
        connect(btnEdit, &QPushButton::clicked, [this, plugin, id = feature.id]() { onEditFeature(plugin, id); });
        QPushButton* btnDel = new QPushButton("X"); btnDel->setFixedSize(22, 22);
        connect(btnDel, &QPushButton::clicked, [this, plugin, id = feature.id]() { onDeleteFeature(plugin, id); });
        sideLayout->addWidget(btnEdit); sideLayout->addWidget(btnDel);
        layout->addLayout(sideLayout);
    }
    ui->layoutFeatures->addWidget(row);
}

void ClipboardAssistant::onRunFeature(IClipboardPlugin* plugin, QString featureId)
{
    ui->textOutput->clear(); ui->textOutput->setText("Processing...");
    PluginCallback* callback = new PluginCallback(this);
    plugin->process(featureId, QApplication::clipboard()->mimeData(), callback);
}

void ClipboardAssistant::onEditFeature(IClipboardPlugin* plugin, QString featureId)
{
    plugin->editFeature(featureId, this); reloadFeatures();
}

void ClipboardAssistant::onDeleteFeature(IClipboardPlugin* plugin, QString featureId)
{
    if (QMessageBox::question(this, "Confirm", "Delete this action?") == QMessageBox::Yes) {
        plugin->deleteFeature(featureId); reloadFeatures();
    }
}

void ClipboardAssistant::onBtnAddFeatureClicked()
{
    QList<IClipboardPlugin*> editablePlugins;
    for (const auto& info : m_plugins) if (info.plugin->isEditable()) editablePlugins.append(info.plugin);
    if (editablePlugins.isEmpty()) return;
    IClipboardPlugin* targetPlugin = nullptr;
    if (editablePlugins.size() == 1) targetPlugin = editablePlugins.first();
    else {
        QStringList names; for(auto* p : editablePlugins) names << p->name();
        bool ok; QString item = QInputDialog::getItem(this, "Select Module", "Add action to:", names, 0, false, &ok);
        if (ok && !item.isEmpty()) {
            for(auto* p : editablePlugins) if (p->name() == item) { targetPlugin = p; break; }
        }
    }
    if (targetPlugin) {
        QString newId = targetPlugin->createFeature(this);
        if (!newId.isEmpty()) reloadFeatures();
    }
}

void ClipboardAssistant::onBtnCopyOutputClicked() { QApplication::clipboard()->setText(ui->textOutput->toPlainText()); }
void ClipboardAssistant::onBtnSettingsClicked() { Setting dlg(m_plugins, this); if (dlg.exec() == QDialog::Accepted) reloadFeatures(); }
void ClipboardAssistant::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this); m_trayIcon->setIcon(QIcon(":/ClipboardAssistant/app_icon.jpg"));
    m_trayMenu = new QMenu(this);
    QAction* showAction = m_trayMenu->addAction("Show"); connect(showAction, &QAction::triggered, this, &QWidget::show);
    QAction* quitAction = m_trayMenu->addAction("Quit"); connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu); m_trayIcon->show();
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ClipboardAssistant::onTrayIconActivated);
}
void ClipboardAssistant::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::DoubleClick) { show(); activateWindow(); } }
void ClipboardAssistant::registerGlobalHotkey() {
    QSettings settings("Heresy", "ClipboardAssistant");
    QString hotkeyStr = settings.value("GlobalHotkey", "Ctrl+Alt+V").toString();
    registerFeatureHotkey(100, QKeySequence(hotkeyStr));
    m_hotkeyMap.clear(); m_nextHotkeyId = 101;
    for (const auto& info : m_plugins) {
        IClipboardPlugin* plugin = info.plugin;
        for (const auto& feature : plugin->features()) {
            if (!feature.customShortcut.isEmpty() && feature.isCustomShortcutGlobal) {
                int id = m_nextHotkeyId++;
                registerFeatureHotkey(id, feature.customShortcut);
                m_hotkeyMap.insert(id, { plugin, feature.id });
            }
        }
    }
}
void ClipboardAssistant::registerFeatureHotkey(int id, const QKeySequence& ks) {
    if (ks.isEmpty()) return;
    QString ksStr = ks.toString(); UINT modifiers = 0;
    if (ksStr.contains("Ctrl")) modifiers |= MOD_CONTROL;
    if (ksStr.contains("Alt")) modifiers |= MOD_ALT;
    if (ksStr.contains("Shift")) modifiers |= MOD_SHIFT;
    if (ksStr.contains("Meta")) modifiers |= MOD_WIN;
    int key = 0; QStringList parts = ksStr.split("+");
    if (!parts.isEmpty()) {
        QString keyPart = parts.last();
        if (keyPart.length() == 1) key = keyPart.at(0).toUpper().unicode();
        else if (keyPart.startsWith("F") && keyPart.length() > 1) {
            bool ok; int fNum = keyPart.mid(1).toInt(&ok); if (ok && fNum >= 1 && fNum <= 12) key = VK_F1 + (fNum - 1);
        } else if (keyPart == "Ins") key = VK_INSERT; else if (keyPart == "Del") key = VK_DELETE;
        else if (keyPart == "Home") key = VK_HOME; else if (keyPart == "End") key = VK_END;
    }
    if (key != 0) RegisterHotKey((HWND)winId(), id, modifiers, key);
}
void ClipboardAssistant::unregisterGlobalHotkey() { for (int i = 100; i < m_nextHotkeyId + 20; ++i) UnregisterHotKey((HWND)winId(), i); }
ClipboardAssistant::PluginCallback::PluginCallback(ClipboardAssistant* parent) : m_parent(parent) {}
void ClipboardAssistant::PluginCallback::onTextData(const QString& text, bool isFinal) {
    QMetaObject::invokeMethod(m_parent, [this, text, isFinal]() { m_parent->handlePluginOutput(text, !m_firstChunk); if (m_firstChunk) m_firstChunk = false; });
}
void ClipboardAssistant::PluginCallback::onError(const QString& message) { QMetaObject::invokeMethod(m_parent, [this, message]() { m_parent->handlePluginError(message); }); }
void ClipboardAssistant::PluginCallback::onFinished() { delete this; }
void ClipboardAssistant::handlePluginOutput(const QString& text, bool append) { if (!append) ui->textOutput->clear(); ui->textOutput->insertPlainText(text); ui->textOutput->moveCursor(QTextCursor::End); }
void ClipboardAssistant::handlePluginError(const QString& msg) { QMessageBox::critical(this, "Plugin Error", msg); }