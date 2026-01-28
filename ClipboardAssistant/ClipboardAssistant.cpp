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
#include <QUuid>
#include "RegExAssistant.h"
#include "ExternalAppAssistant.h"
#include "ActionSetSettings.h"
#include <QDialogButtonBox>

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
    ui->setupUi(this);
    ui->textOutput->setReadOnly(true);
    QFont defaultFont("Segoe UI", 10);
    ui->textClipboard->setFont(defaultFont);
    ui->textOutput->setFont(defaultFont);
    setWindowIcon(QIcon(":/ClipboardAssistant/app_icon.png"));
    m_networkManager = new QNetworkAccessManager(this);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardAssistant::onClipboardChanged);
    onClipboardChanged();
    connect(ui->btnCopyOutput, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCopyOutputClicked);
    connect(ui->btnPaste, &QPushButton::clicked, this, &ClipboardAssistant::onBtnPasteClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &ClipboardAssistant::onBtnSettingsClicked);
    connect(ui->btnAddActionSet, &QPushButton::clicked, this, &ClipboardAssistant::onBtnAddActionSetClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCancelClicked);
    connect(ui->checkAlwaysOnTop, &QCheckBox::toggled, this, &ClipboardAssistant::onCheckAlwaysOnTopToggled);
    connect(ui->spinInputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinInputFontSizeChanged);
    connect(ui->spinOutputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinOutputFontSizeChanged);
    ui->textClipboard->installEventFilter(this); ui->textOutput->installEventFilter(this);
    
    connect(ui->listActionSets->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
        QTimer::singleShot(0, this, [this]() {
            saveSettings(); // Save new order
            updateActionSetShortcuts();
        });
    });

    loadPlugins(); reloadActionSets(); setupTrayIcon(); loadSettings();
    if (ui->splitter_horizontal->sizes().isEmpty() || ui->splitter_horizontal->sizes().at(0) == 0) {
        int w = width() / 2; ui->splitter_horizontal->setSizes({w, w});
    }
}
ClipboardAssistant::~ClipboardAssistant() { unregisterGlobalHotkey(); delete ui; }

void ClipboardAssistant::updateActionSetShortcuts() {
    unregisterGlobalHotkey();
    qDeleteAll(m_localShortcuts); m_localShortcuts.clear();
    int dIdx = 1;
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        QListWidgetItem* item = ui->listActionSets->item(i);
        QString uid = item->data(Qt::UserRole).toString();
        if (!m_actionSetMap.contains(uid)) continue;
        ActionSetInfo& info = m_actionSetMap[uid];
        
        QStringList shortcuts;
        if (dIdx <= 9) {
            QString ks = QString("Ctrl+%1").arg(dIdx);
            shortcuts << ks;
            QShortcut* sc = new QShortcut(QKeySequence(ks), this);
            connect(sc, &QShortcut::activated, [this, uid]() {
                ActionSetInfo i = m_actionSetMap[uid];
                onRunActionSet(i.plugin, i.actionSetId);
            });
            m_localShortcuts.append(sc); dIdx++;
        }
        if (!info.customShortcut.isEmpty()) {
            shortcuts << (info.customShortcut.toString() + (info.isCustomShortcutGlobal ? " (G)" : " (L)"));
            if (!info.isCustomShortcutGlobal) {
                QShortcut* sc = new QShortcut(info.customShortcut, this);
                connect(sc, &QShortcut::activated, [this, uid]() {
                    ActionSetInfo i = m_actionSetMap[uid];
                    onRunActionSet(i.plugin, i.actionSetId);
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

    // Save Action Sets
    s.remove("Actions");
    s.beginWriteArray("Actions");
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        s.setArrayIndex(i);
        QString uid = ui->listActionSets->item(i)->data(Qt::UserRole).toString();
        ActionSetInfo& info = m_actionSetMap[uid];
        s.setValue("Plugin", info.plugin->name());
        s.setValue("ActionId", info.actionSetId);
        s.setValue("Name", info.name);
        s.setValue("Shortcut", info.customShortcut.toString());
        s.setValue("IsGlobal", info.isCustomShortcutGlobal);
        s.setValue("IsAutoCopy", info.isAutoCopy);
        
        s.beginGroup("Params");
        for (auto it = info.parameters.begin(); it != info.parameters.end(); ++it) {
            s.setValue(it.key(), it.value());
        }
        s.endGroup();
    }
    s.endArray();

    // Global settings are saved in Setting::accept or whenever modified
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
                    ActionSetInfo info = m_hotkeyMap[id];
                    if (info.mainButton && info.mainButton->isEnabled()) onRunActionSet(info.plugin, info.actionSetId);
                }
            };
            
            bool shouldAutoCopy = false;
            if (id == 100) {
                QSettings s("Heresy", "ClipboardAssistant");
                shouldAutoCopy = s.value("AutoCopy", false).toBool();
            } else {
                shouldAutoCopy = m_hotkeyMap[id].isAutoCopy;
            }

            if (shouldAutoCopy) {
                QTimer::singleShot(50, [this, act]() { sendCtrlC(); QTimer::singleShot(300, this, [act]() { act(); }); });
            } else act();
            return true;
        }
    }
    return false;
}

void ClipboardAssistant::onClipboardChanged() {
    const QMimeData* d = QApplication::clipboard()->mimeData(); m_currentHtml.clear(); m_pendingDownloads.clear(); m_currentImage = QImage();
    if (d->hasImage()) {
        m_currentImage = qvariant_cast<QImage>(d->imageData());
        if (!m_currentImage.isNull()) {
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); m_currentImage.save(&buf, "PNG");
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
    for (auto it = m_actionSetMap.begin(); it != m_actionSetMap.end(); ++it) {
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
    m_plugins.clear(); 
    m_regexAssistant = new RegExAssistant(this); 
    m_plugins.append({m_regexAssistant, true, "Built-in"});
    
    m_externalAppAssistant = new ExternalAppAssistant(this);
    m_plugins.append({m_externalAppAssistant, true, "Built-in"});
    
    QDir dir(QCoreApplication::applicationDirPath());
    for (const QString& f : dir.entryList({"*.dll"}, QDir::Files)) {
        QPluginLoader l(dir.absoluteFilePath(f)); 
        QObject* p = l.instance();
        if (p) { 
            IClipboardPlugin* iP = qobject_cast<IClipboardPlugin*>(p); 
            if (iP) m_plugins.append({iP, false, f}); 
        }
    }

    // Load Global Settings for all plugins
    QSettings s("Heresy", "ClipboardAssistant");
    for (const auto& info : m_plugins) {
        s.beginGroup("Plugins/" + info.plugin->name() + "/Global");
        QVariantMap globalParams;
        for (const auto& def : info.plugin->globalParameterDefinitions()) {
            globalParams[def.id] = s.value(def.id, def.defaultValue);
        }
        m_globalSettingsMap[info.plugin->name()] = globalParams;
        s.endGroup();
    }
}

void ClipboardAssistant::reloadActionSets() {
    unregisterGlobalHotkey(); 
    qDeleteAll(m_localShortcuts); 
    m_localShortcuts.clear();
    m_actionSetMap.clear();
    ui->listActionSets->clear(); 
    
    QSettings s("Heresy", "ClipboardAssistant");
    int size = s.beginReadArray("Actions");
    if (size > 0) {
        for (int i = 0; i < size; ++i) {
            s.setArrayIndex(i);
            QString pluginName = s.value("Plugin").toString();
            IClipboardPlugin* target = nullptr;
            for (const auto& info : m_plugins) { if (info.plugin->name() == pluginName) { target = info.plugin; break; } }
            if (!target) continue;

            PluginActionSet f;
            f.id = s.value("ActionId").toString();
            f.name = s.value("Name").toString();
            f.customShortcut = QKeySequence(s.value("Shortcut").toString());
            f.isCustomShortcutGlobal = s.value("IsGlobal", false).toBool();
            f.isAutoCopy = s.value("IsAutoCopy", false).toBool();
            
            s.beginGroup("Params");
            for (const QString& key : s.childKeys()) {
                f.parameters[key] = s.value(key);
            }
            s.endGroup();

            addActionSetWidget(target, f, QUuid::createUuid().toString());
        }
    } else {
        // Import defaults
        for (const auto& info : m_plugins) {
            for (const auto& f : info.plugin->defaultActionSets()) {
                addActionSetWidget(info.plugin, f, QUuid::createUuid().toString());
            }
        }
    }
    s.endArray();

    updateActionSetShortcuts();
    updateButtonsState();
}

void ClipboardAssistant::addActionSetWidget(IClipboardPlugin* p, const PluginActionSet& f, const QString& internalId) {
    QListWidgetItem* item = new QListWidgetItem(ui->listActionSets);
    item->setSizeHint(QSize(0, 75));
    item->setData(Qt::UserRole, internalId);
    
    m_actionSetMap.insert(internalId, { p, f.id, nullptr, f.customShortcut, f.isCustomShortcutGlobal, f.isAutoCopy, f.name, nullptr, f.parameters });
    setupActionSetWidget(item, m_actionSetMap[internalId]);
}

void ClipboardAssistant::setupActionSetWidget(QListWidgetItem* item, ActionSetInfo& info) {
    QString uid = item->data(Qt::UserRole).toString();
    IClipboardPlugin* p = info.plugin;
    QString asid = info.actionSetId;

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
    connect(bM, &QPushButton::clicked, [this, uid]() { 
        ActionSetInfo i = m_actionSetMap[uid];
        onRunActionSet(i.plugin, i.actionSetId); 
    });
    
    info.mainButton = bM;
    info.lblContent = lbl;

    QVBoxLayout* sideLayout = new QVBoxLayout();
    sideLayout->setSpacing(1);
    
    QPushButton* bE = new QPushButton("E"); bE->setFixedSize(22,22); connect(bE, &QPushButton::clicked, [this, uid]() { onEditActionSet(m_actionSetMap[uid].plugin, uid); });
    QPushButton* bDel = new QPushButton("X"); bDel->setFixedSize(22,22); connect(bDel, &QPushButton::clicked, [this, uid]() { onDeleteActionSet(m_actionSetMap[uid].plugin, uid); });
    sideLayout->addWidget(bE); sideLayout->addWidget(bDel);
    
    rowLayout->addLayout(sideLayout);
    ui->listActionSets->setItemWidget(item, row);
}

void ClipboardAssistant::onRunActionSet(IClipboardPlugin* p, QString asid) {
    // Note: asid here is the plugin-specific ID, but we need to find the ActionSetInfo by UID if we want params.
    // Actually, onRunActionSet should probably take the UID. 
    // Let's find the UID from asid and plugin.
    ActionSetInfo* info = nullptr;
    for (auto& i : m_actionSetMap) {
        if (i.plugin == p && i.actionSetId == asid) { info = &i; break; }
    }
    if (!info) return;

    ui->textOutput->clear();
    m_activePlugin = p;
    ui->btnCancel->setVisible(true);
    ui->labelStatus->setText(QString("Processing (%1)...").arg(p->name()));
    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 0);

    QMimeData* data = new QMimeData();
    data->setText(ui->textClipboard->toPlainText());
    if (!m_currentHtml.isEmpty()) data->setHtml(m_currentHtml);
    if (!m_currentImage.isNull()) data->setImageData(m_currentImage);
    
    p->process(data, info->parameters, m_globalSettingsMap[p->name()], new PluginCallback(this));
    delete data;
}

void ClipboardAssistant::onEditActionSet(IClipboardPlugin* p, QString uid) { 
    if (!m_actionSetMap.contains(uid)) return;
    ActionSetInfo& info = m_actionSetMap[uid];

    QDialog dialog(this); dialog.setWindowTitle("Edit Action");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    ActionSetSettings* editor = new ActionSetSettings(&dialog);
    editor->setName(info.name);
    editor->setShortcut(info.customShortcut);
    editor->setIsGlobal(info.isCustomShortcutGlobal);
    editor->setIsAutoCopy(info.isAutoCopy);
    editor->setParameters(p->actionParameterDefinitions(), info.parameters);
    
    layout->addWidget(editor);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        info.name = editor->name();
        info.customShortcut = editor->shortcut();
        info.isCustomShortcutGlobal = editor->isGlobal();
        info.isAutoCopy = editor->isAutoCopy();
        info.parameters = editor->getParameters();
        saveSettings();
        updateActionSetShortcuts();
        updateButtonsState();
    }
}

void ClipboardAssistant::onDeleteActionSet(IClipboardPlugin* p, QString uid) { 
    if (QMessageBox::question(this, "Confirm", "Delete?") == QMessageBox::Yes) { 
        for (int i = 0; i < ui->listActionSets->count(); ++i) {
            if (ui->listActionSets->item(i)->data(Qt::UserRole).toString() == uid) {
                delete ui->listActionSets->takeItem(i);
                break;
            }
        }
        m_actionSetMap.remove(uid);
        saveSettings();
        updateActionSetShortcuts();
    } 
}

void ClipboardAssistant::onBtnAddActionSetClicked() {
    QStringList n; for(auto& info : m_plugins) n << info.plugin->name(); 
    bool ok; QString i = QInputDialog::getItem(this, "Select", "Plugin:", n, 0, false, &ok);
    if (!ok || i.isEmpty()) return;
    
    IClipboardPlugin* target = nullptr;
    for(auto& info : m_plugins) if (info.plugin->name() == i) { target = info.plugin; break; }
    if (!target) return;

    QDialog dialog(this); dialog.setWindowTitle("Add Action");
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    ActionSetSettings* editor = new ActionSetSettings(&dialog);
    editor->setParameters(target->actionParameterDefinitions(), {});
    layout->addWidget(editor);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
         PluginActionSet f;
         f.id = QUuid::createUuid().toString(QUuid::Id128);
         f.name = editor->name();
         f.customShortcut = editor->shortcut();
         f.isCustomShortcutGlobal = editor->isGlobal();
         f.isAutoCopy = editor->isAutoCopy();
         f.parameters = editor->getParameters();
         addActionSetWidget(target, f, QUuid::createUuid().toString());
         saveSettings();
         updateActionSetShortcuts();
         updateButtonsState();
    }
}

void ClipboardAssistant::onBtnCopyOutputClicked() { 
    QMimeData* data = new QMimeData();
    data->setText(ui->textOutput->toPlainText());
    data->setHtml(ui->textOutput->toHtml());
    QApplication::clipboard()->setMimeData(data);
}
void ClipboardAssistant::onBtnPasteClicked() { onBtnCopyOutputClicked(); hide(); QTimer::singleShot(500, []() { sendCtrlV(); }); }

void ClipboardAssistant::onBtnSettingsClicked() { 
    Setting dlg(m_plugins, this); 
    if (dlg.exec() == QDialog::Accepted) {
        // Refresh global settings
        loadPlugins(); // This reloads global settings into m_globalSettingsMap
        reloadActionSets(); 
    }
}

void ClipboardAssistant::onBtnCancelClicked() { 
    if (m_activePlugin) { 
        m_activePlugin->abort(); 
        ui->btnCancel->setVisible(false); 
        ui->labelStatus->setText("Cancelled.");
        ui->progressBar->setVisible(false);
        ui->textOutput->append("\n[Cancelled]"); 
        m_activePlugin = nullptr;
    } 
}
void ClipboardAssistant::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this); m_trayIcon->setIcon(QIcon(":/ClipboardAssistant/app_icon.png"));
    m_trayMenu = new QMenu(this); 
    m_trayMenu->addAction("Show", this, &QWidget::show); 
    m_trayMenu->addAction("Settings", this, &ClipboardAssistant::onBtnSettingsClicked);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu); m_trayIcon->show(); connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ClipboardAssistant::onTrayIconActivated);
}
void ClipboardAssistant::registerGlobalHotkey() {
    QSettings s("Heresy", "ClipboardAssistant"); registerActionSetHotkey(100, QKeySequence(s.value("GlobalHotkey", "Ctrl+Alt+V").toString()));
    m_hotkeyMap.clear(); m_nextHotkeyId = 101;
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        QString uid = ui->listActionSets->item(i)->data(Qt::UserRole).toString();
        if (!m_actionSetMap.contains(uid)) continue;
        ActionSetInfo& info = m_actionSetMap[uid];
        if (!info.customShortcut.isEmpty() && info.isCustomShortcutGlobal) {
            int id = m_nextHotkeyId++; registerActionSetHotkey(id, info.customShortcut);
            m_hotkeyMap.insert(id, info);
        }
    }
}
void ClipboardAssistant::registerActionSetHotkey(int id, const QKeySequence& ks) {
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
void ClipboardAssistant::PluginCallback::onTextData(const QString& t, bool f) { QMetaObject::invokeMethod(m_parent, [this, t, f]() { m_parent->handlePluginOutput(t, !m_firstChunk, f); if (m_firstChunk) m_firstChunk = false; }); }
void ClipboardAssistant::PluginCallback::onError(const QString& m) { QMetaObject::invokeMethod(m_parent, [this, m]() { m_parent->handlePluginError(m); }); }
void ClipboardAssistant::PluginCallback::onFinished() { 
    QMetaObject::invokeMethod(m_parent, [this]() {
        m_parent->ui->btnCancel->setVisible(false); 
        m_parent->ui->labelStatus->setText("Done.");
        m_parent->ui->progressBar->setVisible(false);
        m_parent->m_activePlugin = nullptr;
    });
    delete this; 
}
void ClipboardAssistant::handlePluginOutput(const QString& t, bool a, bool f) { 
    if (!a) ui->textOutput->clear(); 
    ui->textOutput->insertPlainText(t); 
    if (f) {
        QString fullText = ui->textOutput->toPlainText();
        ui->textOutput->setMarkdown(fullText);
    }
    ui->textOutput->moveCursor(QTextCursor::End); 
}
void ClipboardAssistant::handlePluginError(const QString& m) { 
    ui->btnCancel->setVisible(false); 
    ui->labelStatus->setText("Error occurred.");
    ui->progressBar->setVisible(false);
    m_activePlugin = nullptr;
    QMessageBox::critical(this, "Plugin Error", m); 
}
