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
#include <QGridLayout>
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
#include <QWheelEvent>
#include "RegExAssistant.h"

void sendCtrlKey(char key) {
    INPUT inputs[10]; ZeroMemory(inputs, sizeof(inputs)); int n = 0;
    auto rel = [&](WORD v) { inputs[n].type = INPUT_KEYBOARD; inputs[n].ki.wVk = v; inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP; };
    rel(VK_CONTROL); rel(VK_MENU); rel(VK_SHIFT); rel(VK_LWIN);
    inputs[n].type = INPUT_KEYBOARD; inputs[n++].ki.wVk = VK_CONTROL;
    inputs[n].type = INPUT_KEYBOARD; inputs[n++].ki.wVk = (WORD)key;
    inputs[n].type = INPUT_KEYBOARD; inputs[n].ki.wVk = (WORD)key; inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[n].type = INPUT_KEYBOARD; inputs[n].ki.wVk = VK_CONTROL; inputs[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(n, inputs, sizeof(INPUT));
}
void sendCtrlC() { sendCtrlKey('C'); }
void sendCtrlV() { sendCtrlKey('V'); }

ClipboardAssistant::ClipboardAssistant(QWidget *parent) : QWidget(parent), ui(new Ui::ClipboardAssistantClass) {
    ui->setupUi(this); m_networkManager = new QNetworkAccessManager(this);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardAssistant::onClipboardChanged);
    onClipboardChanged();
    connect(ui->btnCopyOutput, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCopyOutputClicked);
    connect(ui->btnPaste, &QPushButton::clicked, this, &ClipboardAssistant::onBtnPasteClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &ClipboardAssistant::onBtnSettingsClicked);
    connect(ui->btnAddFeature, &QPushButton::clicked, this, &ClipboardAssistant::onBtnAddFeatureClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCancelClicked);
    connect(ui->checkAlwaysOnTop, &QCheckBox::toggled, this, &ClipboardAssistant::onCheckAlwaysOnTopToggled);
    connect(ui->spinInputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinInputFontSizeChanged);
    connect(ui->spinOutputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinOutputFontSizeChanged);
    ui->textClipboard->installEventFilter(this); ui->textOutput->installEventFilter(this);
    
    connect(ui->listFeatures->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
        QTimer::singleShot(0, this, [this]() {
            for(int i = 0; i < ui->listFeatures->count(); ++i) {
                QListWidgetItem* item = ui->listFeatures->item(i);
                QString uid = item->data(Qt::UserRole).toString();
                if (m_featureMap.contains(uid)) {
                    m_featureMap[uid].plugin->setFeatureOrder(m_featureMap[uid].featureId, i);
                    if (!ui->listFeatures->itemWidget(item)) {
                        setupItemWidget(item, m_featureMap[uid]);
                    }
                }
            }
            updateFeatureShortcuts();
        });
    });

    loadPlugins(); reloadFeatures(); setupTrayIcon(); loadSettings();
    if (ui->splitter_horizontal->sizes().isEmpty() || ui->splitter_horizontal->sizes().at(0) == 0) {
        int w = width() / 2; ui->splitter_horizontal->setSizes({w, w});
    }
}
ClipboardAssistant::~ClipboardAssistant() { unregisterGlobalHotkey(); delete ui; }

void ClipboardAssistant::updateFeatureShortcuts() {
    unregisterGlobalHotkey();
    qDeleteAll(m_localShortcuts); m_localShortcuts.clear();
    int dIdx = 1;
    for (int i = 0; i < ui->listFeatures->count(); ++i) {
        QListWidgetItem* item = ui->listFeatures->item(i);
        QString uid = item->data(Qt::UserRole).toString();
        if (!m_featureMap.contains(uid)) continue;
        FeatureInfo& info = m_featureMap[uid];
        
        QStringList shortcuts;
        if (dIdx <= 9) {
            QString ks = QString("Ctrl+%1").arg(dIdx);
            shortcuts << ks;
            QShortcut* sc = new QShortcut(QKeySequence(ks), this);
            connect(sc, &QShortcut::activated, [this, p=info.plugin, fid=info.featureId]() {
                onRunFeature(p, fid);
            });
            m_localShortcuts.append(sc); dIdx++;
        }
        if (!info.customShortcut.isEmpty()) {
            shortcuts << (info.customShortcut.toString() + (info.isCustomShortcutGlobal ? " (G)" : " (L)"));
            if (!info.isCustomShortcutGlobal) {
                QShortcut* sc = new QShortcut(info.customShortcut, this);
                connect(sc, &QShortcut::activated, [this, p=info.plugin, fid=info.featureId]() {
                    onRunFeature(p, fid);
                });
                m_localShortcuts.append(sc);
            }
        }
        
        if (info.lblContent) {
            QString html = QString("<div align='center'>"
                                   "<b style='font-size:11pt; color:black; background:transparent;'>%1</b><br/>"
                                   "<span style='font-size:8pt; color:#666666; background:transparent;'>%2</span>"
                                   "</div>").arg(info.name.toHtmlEscaped(), shortcuts.join(" / ").toHtmlEscaped());
            info.lblContent->setText(html);
        }
    }
    registerGlobalHotkey();
}

void ClipboardAssistant::closeEvent(QCloseEvent *e) { saveSettings(); if (m_trayIcon->isVisible()) { hide(); e->ignore(); } }
void ClipboardAssistant::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        QSettings s("Heresy", "ClipboardAssistant");
        if (s.value("CloseOnEsc", false).toBool()) { hide(); return; }
    }
    QWidget::keyPressEvent(e);
}
void ClipboardAssistant::onTrayIconActivated(QSystemTrayIcon::ActivationReason r) { if (r == QSystemTrayIcon::DoubleClick) { show(); activateWindow(); } }

void ClipboardAssistant::loadSettings() {
    QSettings s("Heresy", "ClipboardAssistant"); restoreGeometry(s.value("geometry").toByteArray());
    ui->splitter_horizontal->restoreState(s.value("splitter_horizontal").toByteArray()); ui->splitter->restoreState(s.value("splitter_vertical").toByteArray());
    bool ontap = s.value("AlwaysOnTop", false).toBool(); ui->checkAlwaysOnTop->setChecked(ontap); onCheckAlwaysOnTopToggled(ontap);
    int inS = s.value("InputFontSize", 10).toInt(); ui->spinInputFontSize->setValue(inS); onSpinInputFontSizeChanged(inS);
    int outS = s.value("OutputFontSize", 10).toInt(); ui->spinOutputFontSize->setValue(outS); onSpinOutputFontSizeChanged(outS);
}
void ClipboardAssistant::saveSettings() {
    QSettings s("Heresy", "ClipboardAssistant"); s.setValue("geometry", saveGeometry());
    s.setValue("splitter_horizontal", ui->splitter_horizontal->saveState()); s.setValue("splitter_vertical", ui->splitter->saveState());
    s.setValue("AlwaysOnTop", ui->checkAlwaysOnTop->isChecked()); s.setValue("InputFontSize", ui->spinInputFontSize->value()); s.setValue("OutputFontSize", ui->spinOutputFontSize->value());
}
void ClipboardAssistant::onCheckAlwaysOnTopToggled(bool c) { setWindowFlags(c ? (windowFlags() | Qt::WindowStaysOnTopHint) : (windowFlags() & ~Qt::WindowStaysOnTopHint)); show(); }
void ClipboardAssistant::onSpinInputFontSizeChanged(int s) { QFont f = ui->textClipboard->font(); f.setPointSize(s); ui->textClipboard->setFont(f); }
void ClipboardAssistant::onSpinOutputFontSizeChanged(int s) { QFont f = ui->textOutput->font(); f.setPointSize(s); ui->textOutput->setFont(f); }
bool ClipboardAssistant::eventFilter(QObject *w, QEvent *e) {
    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(e);
        if (we->modifiers() & Qt::ControlModifier) {
            int d = we->angleDelta().y() > 0 ? 1 : -1;
            if (w == ui->textClipboard) ui->spinInputFontSize->setValue(ui->spinInputFontSize->value() + d);
            else if (w == ui->textOutput) ui->spinOutputFontSize->setValue(ui->spinOutputFontSize->value() + d);
            return true;
        }
    }
    return QWidget::eventFilter(w, e);
}
bool ClipboardAssistant::nativeEvent(const QByteArray &et, void *m, qintptr *r) {
    MSG* msg = static_cast<MSG*>(m);
    if (msg->message == WM_HOTKEY) {
        int id = (int)msg->wParam;
        if (id == 100 || m_hotkeyMap.contains(id)) {
            auto act = [this, id]() {
                show(); activateWindow();
                if (m_hotkeyMap.contains(id)) {
                    FeatureInfo info = m_hotkeyMap[id];
                    if (info.mainButton && info.mainButton->isEnabled()) onRunFeature(info.plugin, info.featureId);
                }
            };
            QSettings s("Heresy", "ClipboardAssistant");
            if (s.value("AutoCopy", false).toBool()) {
                QTimer::singleShot(50, [this, act]() { sendCtrlC(); QTimer::singleShot(300, this, [act]() { act(); }); });
            } else act();
            return true;
        }
    }
    return false;
}
void ClipboardAssistant::onClipboardChanged() {
    const QMimeData* d = QApplication::clipboard()->mimeData(); m_currentHtml.clear(); m_pendingDownloads.clear();
    if (d->hasImage()) {
        QImage img = qvariant_cast<QImage>(d->imageData());
        if (!img.isNull()) {
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); img.save(&buf, "PNG");
            ui->textClipboard->setHtml(QString("<img src='data:image/png;base64,%1' />").arg(QString::fromLatin1(ba.toBase64())));
        } else ui->textClipboard->setText("[Invalid Image]");
    } else if (d->hasHtml()) { m_currentHtml = d->html(); ui->textClipboard->setHtml(m_currentHtml); processHtmlImages(m_currentHtml); }
    else if (d->hasText()) ui->textClipboard->setText(d->text());
    else ui->textClipboard->setText("[Unknown]");
    updateButtonsState();
}
void ClipboardAssistant::updateButtonsState() {
    const QMimeData* d = QApplication::clipboard()->mimeData();
    IClipboardPlugin::DataTypes cT = IClipboardPlugin::None;
    if (d->hasText() || d->hasHtml()) cT |= IClipboardPlugin::Text;
    if (d->hasImage()) cT |= IClipboardPlugin::Image;
    if (d->hasUrls()) {
        cT |= IClipboardPlugin::File; QMimeDatabase db;
        for (const QUrl& u : d->urls()) if (db.mimeTypeForFile(u.toLocalFile()).name().startsWith("image/")) { cT |= IClipboardPlugin::Image; break; }
    }
    for (auto it = m_featureMap.begin(); it != m_featureMap.end(); ++it) {
        if (it.value().mainButton) it.value().mainButton->setEnabled((it.value().plugin->supportedInputs() & cT) != IClipboardPlugin::None);
    }
}
void ClipboardAssistant::processHtmlImages(QString h) {
    QRegularExpression r("<img\\s+[^>]*src=[\"'](http[^\"']+)[\"'][^>]*>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator i = r.globalMatch(h);
    while (i.hasNext()) {
        QString u = i.next().captured(1);
        if (!m_pendingDownloads.contains(u)) {
            m_pendingDownloads.insert(u); QNetworkRequest req((QUrl(u)));
            req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
            QNetworkReply* rep = m_networkManager->get(req);
            connect(rep, &QNetworkReply::finished, [this, rep, u]() { onImageDownloaded(rep, u); });
        }
    }
}
void ClipboardAssistant::onImageDownloaded(QNetworkReply* rep, QString u) {
    if (rep->error() == QNetworkReply::NoError) {
        QImage img; if (img.loadFromData(rep->readAll())) {
            ui->textClipboard->document()->addResource(QTextDocument::ImageResource, QUrl(u), QVariant(img));
            ui->textClipboard->setHtml(m_currentHtml);
        }
    }
    m_pendingDownloads.remove(u); rep->deleteLater();
}
void ClipboardAssistant::loadPlugins() {
    m_plugins.clear(); m_regexAssistant = new RegExAssistant(this); m_plugins.append({m_regexAssistant, true, "Built-in"});
    QDir dir(QCoreApplication::applicationDirPath());
    for (const QString& f : dir.entryList({"*.dll"}, QDir::Files)) {
        QPluginLoader l(dir.absoluteFilePath(f)); QObject* p = l.instance();
        if (p) { IClipboardPlugin* iP = qobject_cast<IClipboardPlugin*>(p); if (iP) m_plugins.append({iP, false, f}); }
    }
}
void ClipboardAssistant::reloadFeatures() {
    unregisterGlobalHotkey(); qDeleteAll(m_localShortcuts); m_localShortcuts.clear();
    m_featureMap.clear();
    ui->listFeatures->clear(); 
    for (const auto& info : m_plugins) {
        for (const auto& f : info.plugin->features()) {
            addFeatureWidget(info.plugin, f, 0);
        }
    }
    updateFeatureShortcuts();
    updateButtonsState();
}
void ClipboardAssistant::addFeatureWidget(IClipboardPlugin* p, const PluginFeature& f, int) {
    QListWidgetItem* item = new QListWidgetItem(ui->listFeatures);
    item->setSizeHint(QSize(0, 75));
    QString uid = p->name() + "::" + f.id;
    item->setData(Qt::UserRole, uid);
    
    m_featureMap.insert(uid, { p, f.id, nullptr, f.customShortcut, f.isCustomShortcutGlobal, f.name, nullptr });
    setupItemWidget(item, m_featureMap[uid]);
}

void ClipboardAssistant::setupItemWidget(QListWidgetItem* item, FeatureInfo& info) {
    QString uid = item->data(Qt::UserRole).toString();
    IClipboardPlugin* p = info.plugin;
    QString fid = info.featureId;

    QWidget* row = new QWidget();
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(2,2,2,2);
    
    QLabel* dragHandle = new QLabel("≡");
    dragHandle->setFixedWidth(20);
    dragHandle->setAlignment(Qt::AlignCenter);
    dragHandle->setStyleSheet("background-color: #E0E0E0; color: #555555; border-radius: 2px; font-weight: bold;");
    dragHandle->setCursor(Qt::SizeAllCursor);
    dragHandle->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(dragHandle);

    QPushButton* bM = new QPushButton();
    bM->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout* btnLayout = new QVBoxLayout(bM);
    btnLayout->setContentsMargins(0,0,0,0);
    QLabel* lbl = new QLabel();
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setTextFormat(Qt::RichText);
    lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(lbl);
    
    rowLayout->addWidget(bM);
    connect(bM, &QPushButton::clicked, [this, p, fid]() { onRunFeature(p, fid); });
    
    info.mainButton = bM;
    info.lblContent = lbl;

    QVBoxLayout* sideLayout = new QVBoxLayout();
    sideLayout->setSpacing(1);
    
    if (p->isEditable()) {
        QPushButton* bE = new QPushButton("E"); bE->setFixedSize(22,22); connect(bE, &QPushButton::clicked, [this, p, fid]() { onEditFeature(p, fid); });
        QPushButton* bDel = new QPushButton("X"); bDel->setFixedSize(22,22); connect(bDel, &QPushButton::clicked, [this, p, fid]() { onDeleteFeature(p, fid); });
        sideLayout->addWidget(bE); sideLayout->addWidget(bDel);
    }
    rowLayout->addLayout(sideLayout);
    ui->listFeatures->setItemWidget(item, row);
}
void ClipboardAssistant::onRunFeature(IClipboardPlugin* p, QString fid) {
    ui->textOutput->clear(); ui->textOutput->setText("Processing..."); m_activePlugin = p;
    if (p->supportsStreaming()) ui->btnCancel->setVisible(true);
    p->process(fid, QApplication::clipboard()->mimeData(), new PluginCallback(this));
}
void ClipboardAssistant::onEditFeature(IClipboardPlugin* p, QString fid) { p->editFeature(fid, this); reloadFeatures(); }
void ClipboardAssistant::onDeleteFeature(IClipboardPlugin* p, QString fid) { if (QMessageBox::question(this, "Confirm", "Delete?") == QMessageBox::Yes) { p->deleteFeature(fid); reloadFeatures(); } }
void ClipboardAssistant::onBtnAddFeatureClicked() {
    QList<IClipboardPlugin*> eP; for (const auto& info : m_plugins) if (info.plugin->isEditable()) eP.append(info.plugin);
    if (eP.isEmpty()) return;
    IClipboardPlugin* target = (eP.size() == 1) ? eP.first() : nullptr;
    if (!target) { QStringList n; for(auto* p : eP) n << p->name(); bool ok; QString i = QInputDialog::getItem(this, "Select", "Add to:", n, 0, false, &ok);
        if (ok && !i.isEmpty()) for(auto* p : eP) if (p->name() == i) { target = p; break; }
    }
    if (target && !target->createFeature(this).isEmpty()) reloadFeatures();
}
void ClipboardAssistant::onBtnCopyOutputClicked() { QApplication::clipboard()->setText(ui->textOutput->toPlainText()); }
void ClipboardAssistant::onBtnPasteClicked() { onBtnCopyOutputClicked(); hide(); QTimer::singleShot(500, []() { sendCtrlV(); }); }
void ClipboardAssistant::onBtnSettingsClicked() { Setting dlg(m_plugins, this); if (dlg.exec() == QDialog::Accepted) reloadFeatures(); }
void ClipboardAssistant::onBtnCancelClicked() { if (m_activePlugin) { m_activePlugin->abort(); ui->btnCancel->setVisible(false); ui->textOutput->append("\n[Cancelled]"); } }
void ClipboardAssistant::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this); m_trayIcon->setIcon(QIcon(":/ClipboardAssistant/app_icon.jpg"));
    m_trayMenu = new QMenu(this); m_trayMenu->addAction("Show", this, &QWidget::show); m_trayMenu->addAction("Quit", qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu); m_trayIcon->show(); connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ClipboardAssistant::onTrayIconActivated);
}
void ClipboardAssistant::registerGlobalHotkey() {
    QSettings s("Heresy", "ClipboardAssistant"); registerFeatureHotkey(100, QKeySequence(s.value("GlobalHotkey", "Ctrl+Alt+V").toString()));
    m_hotkeyMap.clear(); m_nextHotkeyId = 101;
    for (int i = 0; i < ui->listFeatures->count(); ++i) {
        QString uid = ui->listFeatures->item(i)->data(Qt::UserRole).toString();
        if (!m_featureMap.contains(uid)) continue;
        FeatureInfo& info = m_featureMap[uid];
        if (!info.customShortcut.isEmpty() && info.isCustomShortcutGlobal) {
            int id = m_nextHotkeyId++; registerFeatureHotkey(id, info.customShortcut);
            m_hotkeyMap.insert(id, { info.plugin, info.featureId, info.mainButton, info.customShortcut, info.isCustomShortcutGlobal, info.name, info.lblContent });
        }
    }
}
void ClipboardAssistant::registerFeatureHotkey(int id, const QKeySequence& ks) {
    if (ks.isEmpty()) return; QString ksStr = ks.toString(QKeySequence::PortableText); UINT m = 0;
    if (ksStr.contains("Ctrl")) m |= MOD_CONTROL; if (ksStr.contains("Alt")) m |= MOD_ALT; if (ksStr.contains("Shift")) m |= MOD_SHIFT; if (ksStr.contains("Meta")) m |= MOD_WIN;
    int k = 0; QStringList p = ksStr.split("+"); if (!p.isEmpty()) { QString kp = p.last();
        if (kp.length() == 1) k = kp.at(0).toUpper().unicode();
        else if (kp.startsWith("F")) { bool ok; int f = kp.mid(1).toInt(&ok); if (ok && f >= 1 && f <= 12) k = VK_F1 + (f - 1); }
        else if (kp == "Ins") k = VK_INSERT; else if (kp == "Del") k = VK_DELETE; else if (kp == "Home") k = VK_HOME; else if (kp == "End") k = VK_END;
        else if (kp == "Space") k = VK_SPACE; else if (kp == "Tab") k = VK_TAB;
    }
    if (k != 0) RegisterHotKey((HWND)winId(), id, m, k);
}
void ClipboardAssistant::unregisterGlobalHotkey() { for (int i = 100; i < m_nextHotkeyId + 20; ++i) UnregisterHotKey((HWND)winId(), i); }
ClipboardAssistant::PluginCallback::PluginCallback(ClipboardAssistant* p) : m_parent(p) {}
void ClipboardAssistant::PluginCallback::onTextData(const QString& t, bool f) { QMetaObject::invokeMethod(m_parent, [this, t, f]() { m_parent->handlePluginOutput(t, !m_firstChunk); if (m_firstChunk) m_firstChunk = false; }); }
void ClipboardAssistant::PluginCallback::onError(const QString& m) { QMetaObject::invokeMethod(m_parent, [this, m]() { m_parent->handlePluginError(m); }); }
void ClipboardAssistant::PluginCallback::onFinished() { m_parent->ui->btnCancel->setVisible(false); delete this; }
void ClipboardAssistant::handlePluginOutput(const QString& t, bool a) { if (!a) ui->textOutput->clear(); ui->textOutput->insertPlainText(t); ui->textOutput->moveCursor(QTextCursor::End); }
void ClipboardAssistant::handlePluginError(const QString& m) { ui->btnCancel->setVisible(false); QMessageBox::critical(this, "Plugin Error", m); }