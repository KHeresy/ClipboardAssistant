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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include "RegExAssistant.h"
#include "ExternalAppAssistant.h"
#include "TextInputAssistant.h"
#include "ScreenCaptureAssistant.h"
#include "ActionSetSettings.h"
#include "PipelineExecutor.h"
#include <QDialogButtonBox>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextDocument>

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
    QApplication::setQuitOnLastWindowClosed(false);
    ui->setupUi(this);
    ui->textOutput->setReadOnly(true);
    setWindowIcon(QIcon(":/ClipboardAssistant/app_icon.png"));
    m_networkManager = new QNetworkAccessManager(this);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardAssistant::onClipboardChanged);
    onClipboardChanged();
    connect(ui->btnCopyOutput, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCopyOutputClicked);
    connect(ui->btnPaste, &QPushButton::clicked, this, &ClipboardAssistant::onBtnPasteClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &ClipboardAssistant::onBtnSettingsClicked);
    connect(ui->btnAddActionSet, &QPushButton::clicked, this, &ClipboardAssistant::onBtnAddActionSetClicked);
    connect(ui->btnImportActionSet, &QPushButton::clicked, this, &ClipboardAssistant::onBtnImportActionSetClicked);
    connect(ui->btnExportAll, &QPushButton::clicked, this, &ClipboardAssistant::onBtnExportAllClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ClipboardAssistant::onBtnCancelClicked);
    connect(ui->checkAlwaysOnTop, &QCheckBox::toggled, this, &ClipboardAssistant::onCheckAlwaysOnTopToggled);
    connect(ui->spinInputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinInputFontSizeChanged);
    connect(ui->spinOutputFontSize, &QSpinBox::valueChanged, this, &ClipboardAssistant::onSpinOutputFontSizeChanged);
    ui->textClipboard->installEventFilter(this); ui->textOutput->installEventFilter(this);
    ui->textClipboard->viewport()->installEventFilter(this); ui->textOutput->viewport()->installEventFilter(this);
    
    connect(ui->listActionSets->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
        QTimer::singleShot(0, this, [this]() {
            saveSettings();
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
            connect(sc, &QShortcut::activated, [this, uid]() { onRunActionSet(nullptr, uid); });
            m_localShortcuts.append(sc); dIdx++;
        }
        if (!info.customShortcut.isEmpty()) {
            shortcuts << (info.customShortcut.toString() + (info.isCustomShortcutGlobal ? " (G)" : " (L)"));
            if (!info.isCustomShortcutGlobal) {
                QShortcut* sc = new QShortcut(info.customShortcut, this);
                connect(sc, &QShortcut::activated, [this, uid]() { onRunActionSet(nullptr, uid); });
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

    s.remove("Actions");
    s.beginWriteArray("Actions");
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        s.setArrayIndex(i);
        QString uid = ui->listActionSets->item(i)->data(Qt::UserRole).toString();
        ActionSetInfo& info = m_actionSetMap[uid];
        s.setValue("ActionSetId", info.actionSetId);
        s.setValue("Name", info.name);
        s.setValue("Shortcut", info.customShortcut.toString());
        s.setValue("IsGlobal", info.isCustomShortcutGlobal);
        s.setValue("IsAutoCopy", info.isAutoCopy);
        
        s.beginWriteArray("Steps");
        for (int j = 0; j < info.actions.size(); ++j) {
            s.setArrayIndex(j);
            s.setValue("Plugin", info.actions[j].pluginId);
            s.beginGroup("Params");
            for (auto it = info.actions[j].parameters.begin(); it != info.actions[j].parameters.end(); ++it) {
                s.setValue(it.key(), it.value());
            }
            s.endGroup();
        }
        s.endArray();
    }
    s.endArray();
}

void ClipboardAssistant::onCheckAlwaysOnTopToggled(bool c) { setWindowFlags(c ? (windowFlags() | Qt::WindowStaysOnTopHint) : (windowFlags() & ~Qt::WindowStaysOnTopHint)); show(); }
void ClipboardAssistant::onSpinInputFontSizeChanged(int s) {
    if (!m_currentImage.isNull()) {
        QTextBlock block = ui->textClipboard->document()->begin();
        while (block.isValid()) {
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment fragment = it.fragment();
                if (fragment.isValid()) {
                    QTextImageFormat fmt = fragment.charFormat().toImageFormat();
                    if (fmt.isValid()) {
                        double scale = s / 10.0;
                        fmt.setWidth(m_currentImage.width() * scale);
                        fmt.setHeight(m_currentImage.height() * scale);
                        QTextCursor helper(block);
                        helper.setPosition(fragment.position());
                        helper.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                        helper.setCharFormat(fmt);
                    }
                }
            }
            block = block.next();
        }
    }
    QFont f = ui->textClipboard->font();
    f.setPointSize(s);
    ui->textClipboard->setFont(f);
}
void ClipboardAssistant::onSpinOutputFontSizeChanged(int s) { QFont f = ui->textOutput->font(); f.setPointSize(s); ui->textOutput->setFont(f); }

bool ClipboardAssistant::eventFilter(QObject *w, QEvent *e) {
    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(e);
        if (we->modifiers() & Qt::ControlModifier) {
            int d = we->angleDelta().y() > 0 ? 1 : -1;
            if (w == ui->textClipboard || w == ui->textClipboard->viewport()) ui->spinInputFontSize->setValue(ui->spinInputFontSize->value() + d);
            else if (w == ui->textOutput || w == ui->textOutput->viewport()) ui->spinOutputFontSize->setValue(ui->spinOutputFontSize->value() + d);
            return true;
        }
    }
    return QWidget::eventFilter(w, e);
}

bool ClipboardAssistant::nativeEvent(const QByteArray &et, void *m, qintptr *r) {
    MSG* msg = static_cast<MSG*>(m);
    if (msg->message == WM_HOTKEY) {
        int id = (int)msg->wParam;
        if (id == HOTKEY_ID_MAIN) {
            bool shouldAutoCopy = false;
            QSettings s("Heresy", "ClipboardAssistant");
            shouldAutoCopy = s.value("AutoCopy", false).toBool();

            auto act = [this]() {
                show(); 
                activateWindow();
            };

            if (shouldAutoCopy) {
                QTimer::singleShot(50, [this, act]() { sendCtrlC(); QTimer::singleShot(300, this, [act]() { act(); }); });
            } else act();
            return true;
        } else if (id == HOTKEY_ID_CAPTURE) {
            onCaptureHotkey();
            return true;
        } else if (m_hotkeyMap.contains(id)) {
            ActionSetInfo info = m_hotkeyMap[id];
            
            auto act = [this, info]() {
                show(); activateWindow();
                if (info.mainButton && info.mainButton->isEnabled()) onRunActionSet(nullptr, info.actionSetId);
            };

            if (info.isAutoCopy) {
                QTimer::singleShot(50, [this, act]() { sendCtrlC(); QTimer::singleShot(300, this, [act]() { act(); }); });
            } else act();
            return true;
        }
    }
    return false;
}

void ClipboardAssistant::onCaptureHotkey() {
    IClipboardPlugin* capturePlugin = nullptr;
    for(auto& pi : m_plugins) {
        if(pi.plugin->id() == "kheresy.ScreenCaptureAssistant") { 
            capturePlugin = pi.plugin; 
            break; 
        }
    }

    if (!capturePlugin) {
        QMessageBox::warning(this, tr("Error"), tr("Screen Capture Assistant plugin not found."));
        return;
    }

    // We don't need input data for capture
    QMimeData* dummyInput = new QMimeData();
    
    // Define a simple callback helper class
    class CaptureCallback : public IPluginCallback {
    public:
        void onTextData(const QString&, bool) override {}
        void onMimeData(const QMimeData* data) override {
            if (data->hasImage()) {
                QImage img = qvariant_cast<QImage>(data->imageData());
                QTimer::singleShot(0, [img]() {
                    QMimeData* clone = new QMimeData();
                    clone->setImageData(img);
                    QApplication::clipboard()->setMimeData(clone);
                });
            } else if (data->hasText()) {
                QString txt = data->text();
                QTimer::singleShot(0, [txt]() {
                    QMimeData* clone = new QMimeData();
                    clone->setText(txt);
                    QApplication::clipboard()->setMimeData(clone);
                });
            }
        }
        void onError(const QString& msg) override {
            QTimer::singleShot(0, []() { MessageBeep(MB_ICONHAND); });
        }
        void onFinished() override { 
            // Delay deletion to ensure stack is unwound and no pending events reference this
            QTimer::singleShot(0, [this]() { delete this; });
        }
    };

    capturePlugin->process(dummyInput, {}, {}, new CaptureCallback());
    delete dummyInput;
}

void ClipboardAssistant::onClipboardChanged() {
    const QMimeData* d = QApplication::clipboard()->mimeData(); m_currentHtml.clear(); m_pendingDownloads.clear(); m_currentImage = QImage();
    if (d->hasImage()) {
        m_currentImage = qvariant_cast<QImage>(d->imageData());
        if (!m_currentImage.isNull()) {
            QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); m_currentImage.save(&buf, "PNG");
            ui->textClipboard->setHtml(QString("<img src='data:image/png;base64,%1' />").arg(QString::fromLatin1(ba.toBase64())));
        } else ui->textClipboard->setText(tr("[Invalid Image]"));
    } else if (d->hasHtml()) { m_currentHtml = d->html(); ui->textClipboard->setHtml(m_currentHtml); processHtmlImages(m_currentHtml); } 
    else if (d->hasText()) ui->textClipboard->setText(d->text());
    else ui->textClipboard->setText(tr("[Unknown]"));
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
        if (it.value().mainButton) {
            if (it.value().actions.isEmpty()) {
                it.value().mainButton->setEnabled(false);
                continue;
            }
            IClipboardPlugin* firstPlugin = nullptr;
            for(auto& pi : m_plugins) if(pi.plugin->id() == it.value().actions[0].pluginId) { firstPlugin = pi.plugin; break; }
            if (firstPlugin) {
                bool isEnabled = (firstPlugin->supportedInputs() & cT) != IClipboardPlugin::None;
                it.value().mainButton->setEnabled(isEnabled);
                
                QGraphicsOpacityEffect* effect = qobject_cast<QGraphicsOpacityEffect*>(it.value().mainButton->graphicsEffect());
                if (!isEnabled) {
                    if (!effect) {
                        effect = new QGraphicsOpacityEffect(it.value().mainButton);
                        it.value().mainButton->setGraphicsEffect(effect);
                    }
                    effect->setOpacity(0.33);
                } else {
                    if (effect) {
                        effect->setOpacity(1.0);
                    }
                }
            } else {
                it.value().mainButton->setEnabled(false);
                QGraphicsOpacityEffect* effect = qobject_cast<QGraphicsOpacityEffect*>(it.value().mainButton->graphicsEffect());
                if (!effect) {
                    effect = new QGraphicsOpacityEffect(it.value().mainButton);
                    it.value().mainButton->setGraphicsEffect(effect);
                }
                effect->setOpacity(0.5);
            }
        }
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
    m_textInputAssistant = new TextInputAssistant(this);
    m_plugins.append({ m_textInputAssistant, true, "" });

    m_plugins.append({ new ScreenCaptureAssistant(this), true, "" });

    // Load external plugins (DLLs)
    QDir dir(QCoreApplication::applicationDirPath());
    for (const QString& f : dir.entryList({"*.dll"}, QDir::Files)) {
        QPluginLoader l(dir.absoluteFilePath(f)); 
        QObject* p = l.instance();
        if (p) { 
            IClipboardPlugin* iP = qobject_cast<IClipboardPlugin*>(p); 
            if (iP) m_plugins.append({iP, false, f}); 
        }
    }

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
            ActionSetInfo info;
            info.actionSetId = s.value("ActionSetId").toString();
            if (info.actionSetId.isEmpty()) info.actionSetId = QUuid::createUuid().toString();
            info.name = s.value("Name").toString();
            info.customShortcut = QKeySequence(s.value("Shortcut").toString());
            info.isCustomShortcutGlobal = s.value("IsGlobal", false).toBool();
            info.isAutoCopy = s.value("IsAutoCopy", false).toBool();
            
            int stepsSize = s.beginReadArray("Steps");
            for (int j = 0; j < stepsSize; ++j) {
                s.setArrayIndex(j);
                PluginActionInstance step;
                step.pluginId = s.value("Plugin").toString();
                s.beginGroup("Params");
                for (const QString& key : s.childKeys()) {
                    step.parameters[key] = s.value(key);
                }
                s.endGroup();
                info.actions.append(step);
            }
            s.endArray();
            addActionSetWidget(info);
        }
    } else {
        for (const auto& pi : m_plugins) {
            for (const auto& tmpl : pi.plugin->actionTemplates()) {
                ActionSetInfo info;
                info.actionSetId = QUuid::createUuid().toString();
                info.name = tmpl.name;
                info.actions.append({pi.plugin->id(), tmpl.defaultParameters});
                addActionSetWidget(info);
            }
        }
    }
    s.endArray();
    updateActionSetShortcuts();
    updateButtonsState();
}

void ClipboardAssistant::addActionSetWidget(const ActionSetInfo& f) {
    QListWidgetItem* item = new QListWidgetItem(ui->listActionSets);
    item->setSizeHint(QSize(0, 75));
    item->setData(Qt::UserRole, f.actionSetId);
    m_actionSetMap.insert(f.actionSetId, f);
    setupActionSetWidget(item, m_actionSetMap[f.actionSetId]);
}

void ClipboardAssistant::setupActionSetWidget(QListWidgetItem* item, ActionSetInfo& info) {
    QString asid = info.actionSetId;
    QWidget* row = new QWidget();
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(2,2,2,2);
    QLabel* dragHandle = new QLabel("=");
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
    connect(bM, &QPushButton::clicked, [this, asid]() { onRunActionSet(nullptr, asid); });
    info.mainButton = bM;
    info.lblContent = lbl;
    QVBoxLayout* sideLayout = new QVBoxLayout();
    sideLayout->setContentsMargins(0,0,0,0);
    sideLayout->setSpacing(1);
    
    auto createBtn = [&](const QString& text, const QString& tip) {
        QPushButton* b = new QPushButton(text);
        b->setFixedSize(20, 20);
        b->setToolTip(tip);
        b->setStyleSheet("font-size: 8pt; padding: 0px;");
        return b;
    };

    QPushButton* bE = createBtn("E", tr("Edit")); 
    connect(bE, &QPushButton::clicked, [this, asid]() { onEditActionSet(nullptr, asid); });
    
    QPushButton* bExp = createBtn("↑", tr("Export")); 
    connect(bExp, &QPushButton::clicked, [this, asid]() { onExportActionSet(asid); });
    
    QPushButton* bDel = createBtn("X", tr("Delete")); 
    connect(bDel, &QPushButton::clicked, [this, asid]() { onDeleteActionSet(nullptr, asid); });
    
    sideLayout->addWidget(bE); 
    sideLayout->addWidget(bExp); 
    sideLayout->addWidget(bDel);
    rowLayout->addLayout(sideLayout);
    ui->listActionSets->setItemWidget(item, row);
}

void ClipboardAssistant::onRunActionSet(IClipboardPlugin*, QString asid) {
    if (!m_actionSetMap.contains(asid)) return;
    const ActionSetInfo& info = m_actionSetMap[asid];
    if (info.actions.isEmpty()) {
        ui->labelStatus->setText(tr("No actions in this set."));
        return;
    }
    if (m_currentExecutor) m_currentExecutor->stop();
    ui->textOutput->clear();
    ui->btnCancel->setVisible(true);
    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 0);
    const QMimeData* data = QApplication::clipboard()->mimeData();
    m_currentExecutor = new PipelineExecutor(this, info, data);
    m_currentExecutor->start();
}

void ClipboardAssistant::onEditActionSet(IClipboardPlugin*, QString asid) { 
    if (!m_actionSetMap.contains(asid)) return;
    ActionSetInfo& info = m_actionSetMap[asid];
    QDialog dialog(this); dialog.setWindowTitle(tr("Edit Action Set"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    ActionSetSettings* editor = new ActionSetSettings(m_plugins, &dialog);
    editor->setName(info.name);
    editor->setShortcut(info.customShortcut);
    editor->setIsGlobal(info.isCustomShortcutGlobal);
    editor->setIsAutoCopy(info.isAutoCopy);
    editor->setActions(info.actions);
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
        info.actions = editor->getActions();
        saveSettings(); updateActionSetShortcuts(); updateButtonsState();
    }
}

void ClipboardAssistant::onDeleteActionSet(IClipboardPlugin*, QString asid) { 
    if (QMessageBox::question(this, tr("Confirm"), tr("Delete pipeline?")) == QMessageBox::Yes) { 
        for (int i = 0; i < ui->listActionSets->count(); ++i) {
            if (ui->listActionSets->item(i)->data(Qt::UserRole).toString() == asid) {
                delete ui->listActionSets->takeItem(i); break;
            }
        }
        m_actionSetMap.remove(asid); saveSettings(); updateActionSetShortcuts();
    } 
}

void ClipboardAssistant::onBtnAddActionSetClicked() {
    QDialog dialog(this); dialog.setWindowTitle(tr("Add Action Set"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    ActionSetSettings* editor = new ActionSetSettings(m_plugins, &dialog);
    layout->addWidget(editor);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
         ActionSetInfo info;
         info.actionSetId = QUuid::createUuid().toString();
         info.name = editor->name();
         info.customShortcut = editor->shortcut();
         info.isCustomShortcutGlobal = editor->isGlobal();
         info.isAutoCopy = editor->isAutoCopy();
         info.actions = editor->getActions();
         addActionSetWidget(info);
         saveSettings(); updateActionSetShortcuts(); updateButtonsState();
    }
}

void ClipboardAssistant::onBtnImportActionSetClicked() {
    QString path = QFileDialog::getOpenFileName(this, tr("Import Action Set"), "", tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) { QMessageBox::critical(this, tr("Error"), tr("Invalid JSON format.")); return; }
    
    QStringList reports;
    
    auto processObj = [&](const QJsonObject& obj) {
        ActionSetInfo info;
        info.actionSetId = QUuid::createUuid().toString();
        info.name = obj["Name"].toString();
        if (info.name.isEmpty()) { reports << tr("Skipped: Missing 'Name' field."); return; }
        info.customShortcut = QKeySequence(obj["Shortcut"].toString());
        info.isCustomShortcutGlobal = obj["IsGlobal"].toBool();
        info.isAutoCopy = obj["IsAutoCopy"].toBool();
        
        QJsonArray steps = obj["Steps"].toArray();
        for (int i = 0; i < steps.size(); ++i) {
            QJsonObject sObj = steps[i].toObject();
            QString pluginId = sObj["Plugin"].toString();
            
            bool pluginExists = false;
            for(auto& pi : m_plugins) if(pi.plugin->id() == pluginId) { pluginExists = true; break; }
            
            if (!pluginExists) { reports << tr("Action '%1': Module '%2' not found. Step skipped.").arg(info.name, pluginId); continue; }
            
            PluginActionInstance step;
            step.pluginId = pluginId;
            step.parameters = sObj["Params"].toObject().toVariantMap();
            info.actions.append(step);
        }
        
        if (info.actions.isEmpty()) { reports << tr("Action '%1' has no valid steps and was not imported.").arg(info.name); }
        else { addActionSetWidget(info); }
    };

    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) processObj(arr[i].toObject());
    } else if (doc.isObject()) {
        QJsonObject root = doc.object();
        if (root.contains("Actions") && root["Actions"].isArray()) {
            QJsonArray arr = root["Actions"].toArray();
            for (int i = 0; i < arr.size(); ++i) processObj(arr[i].toObject());
        } else {
            processObj(root);
        }
    }
    
    saveSettings();
    updateActionSetShortcuts();
    
    if (!reports.isEmpty()) {
        QDialog dlg(this); dlg.setWindowTitle(tr("Import Report"));
        QVBoxLayout* l = new QVBoxLayout(&dlg);
        l->addWidget(new QLabel(tr("Issues found during import:")));
        QTextEdit* te = new QTextEdit(&dlg); te->setPlainText(reports.join("\n")); te->setReadOnly(true);
        l->addWidget(te);
        QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg); connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        l->addWidget(bb); dlg.exec();
    } else {
        QMessageBox::information(this, tr("Success"), tr("Import completed successfully."));
    }
}

void ClipboardAssistant::onBtnExportAllClicked() {
    if (m_actionSetMap.isEmpty()) return;

    QString path = QFileDialog::getSaveFileName(this, tr("Export All Action Sets"), "AllActionSets.json", tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;

    QJsonArray array;
    // Iterate through listActionSets to keep the order
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        QString asid = ui->listActionSets->item(i)->data(Qt::UserRole).toString();
        if (!m_actionSetMap.contains(asid)) continue;
        const ActionSetInfo& info = m_actionSetMap[asid];

        QJsonObject obj;
        obj["Name"] = info.name;
        obj["Shortcut"] = info.customShortcut.toString();
        obj["IsGlobal"] = info.isCustomShortcutGlobal;
        obj["IsAutoCopy"] = info.isAutoCopy;

        QJsonArray steps;
        for (const auto& step : info.actions) {
            QJsonObject sObj;
            sObj["Plugin"] = step.pluginId;
            sObj["Params"] = QJsonObject::fromVariantMap(step.parameters);
            steps.append(sObj);
        }
        obj["Steps"] = steps;
        array.append(obj);
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
        QMessageBox::information(this, tr("Export All"), tr("All Action Sets exported successfully."));
    }
}

void ClipboardAssistant::onExportActionSet(const QString& asid) {
    if (!m_actionSetMap.contains(asid)) return;
    const ActionSetInfo& info = m_actionSetMap[asid];
    
    QString path = QFileDialog::getSaveFileName(this, tr("Export Action Set"), info.name + ".json", tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;
    
    QJsonObject obj;
    obj["Name"] = info.name;
    obj["Shortcut"] = info.customShortcut.toString();
    obj["IsGlobal"] = info.isCustomShortcutGlobal;
    obj["IsAutoCopy"] = info.isAutoCopy;
    
    QJsonArray steps;
    for (const auto& step : info.actions) {
        QJsonObject sObj;
        sObj["Plugin"] = step.pluginId;
        sObj["Params"] = QJsonObject::fromVariantMap(step.parameters);
        steps.append(sObj);
    }
    obj["Steps"] = steps;
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        QMessageBox::information(this, tr("Export"), tr("Action Set exported successfully."));
    }
}

void ClipboardAssistant::onBtnCopyOutputClicked() { 
    QMimeData* data = new QMimeData();
    data->setText(ui->textOutput->toPlainText());
    data->setHtml(ui->textOutput->toHtml());
    QApplication::clipboard()->setMimeData(data);
}
void ClipboardAssistant::onBtnPasteClicked() { onBtnCopyOutputClicked(); hide(); QTimer::singleShot(500, []() { sendCtrlV(); }); }
void ClipboardAssistant::onBtnSettingsClicked() { Setting dlg(m_plugins, this); if (dlg.exec() == QDialog::Accepted) { loadPlugins(); reloadActionSets(); } }
void ClipboardAssistant::onBtnCancelClicked() { if (m_activePlugin) { m_activePlugin->abort(); ui->btnCancel->setVisible(false); ui->labelStatus->setText(tr("Cancelled.")); ui->progressBar->setVisible(false); ui->textOutput->append(tr("\n[Cancelled]")); m_activePlugin = nullptr; } }
void ClipboardAssistant::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this); 
    m_trayIcon->setIcon(QIcon(":/ClipboardAssistant/app_icon.png"));
    m_trayIcon->setToolTip(tr("Clipboard Assistant")); // Add tooltip
    m_trayMenu = new QMenu(this); 
    m_trayMenu->addAction(tr("Show"), this, &QWidget::show); 
    m_trayMenu->addAction(tr("Settings"), this, &ClipboardAssistant::onBtnSettingsClicked);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Quit"), qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(m_trayMenu); m_trayIcon->show(); connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ClipboardAssistant::onTrayIconActivated);
}
bool ClipboardAssistant::registerGlobalHotkey() {
    QSettings s("Heresy", "ClipboardAssistant"); 
    QStringList failedHotkeys;

    if (s.value("EnableGlobalHotkey", true).toBool()) {
        QKeySequence ks(s.value("GlobalHotkey", "Ctrl+Alt+V").toString());
        if (!registerActionSetHotkey(HOTKEY_ID_MAIN, ks)) {
            failedHotkeys << tr("Main Window: %1").arg(ks.toString());
        }
    }
    
    if (s.value("EnableCaptureHotkey", true).toBool()) {
        QKeySequence ks(s.value("CaptureHotkey", "Ctrl+Alt+S").toString());
        if (!registerActionSetHotkey(HOTKEY_ID_CAPTURE, ks)) {
            failedHotkeys << tr("Screen Capture: %1").arg(ks.toString());
        }
    }

    m_hotkeyMap.clear(); m_nextHotkeyId = 101;
    for (int i = 0; i < ui->listActionSets->count(); ++i) {
        QString uid = ui->listActionSets->item(i)->data(Qt::UserRole).toString();
        if (!m_actionSetMap.contains(uid)) continue;
        ActionSetInfo& info = m_actionSetMap[uid];
        if (!info.customShortcut.isEmpty() && info.isCustomShortcutGlobal) {
            int id = m_nextHotkeyId++; 
            if (!registerActionSetHotkey(id, info.customShortcut)) {
                failedHotkeys << tr("Action '%1': %2").arg(info.name, info.customShortcut.toString());
            }
            m_hotkeyMap.insert(id, info);
        }
    }

    if (!failedHotkeys.isEmpty()) {
        m_trayIcon->showMessage(tr("Hotkey Conflict"), 
            tr("Failed to register the following global hotkeys (already in use):\n%1").arg(failedHotkeys.join("\n")),
            QSystemTrayIcon::Warning, 5000);
        return false;
    }
    return true;
}

bool ClipboardAssistant::registerActionSetHotkey(int id, const QKeySequence& ks) {
    if (ks.isEmpty()) return true; 
    QString ksStr = ks.toString(QKeySequence::PortableText); UINT m = 0;
    if (ksStr.contains("Ctrl")) m |= MOD_CONTROL; if (ksStr.contains("Alt")) m |= MOD_ALT; if (ksStr.contains("Shift")) m |= MOD_SHIFT; if (ksStr.contains("Meta")) m |= MOD_WIN;
    int k = 0; QStringList p = ksStr.split("+"); if (!p.isEmpty()) { QString kp = p.last();
        if (kp.length() == 1) k = kp.at(0).toUpper().unicode();
        else if (kp.startsWith("F")) { bool ok; int f = kp.mid(1).toInt(&ok); if (ok && f >= 1 && f <= 12) k = VK_F1 + (f - 1); }
        else if (kp == "Ins") k = VK_INSERT; else if (kp == "Del") k = VK_DELETE; else if (kp == "Home") k = VK_HOME; else if (kp == "End") k = VK_END;
        else if (kp == "Space") k = VK_SPACE; else if (kp == "Tab") k = VK_TAB;
    }
    if (k != 0) {
        return RegisterHotKey((HWND)winId(), id, m, k);
    }
    return false;
}
void ClipboardAssistant::unregisterGlobalHotkey() { for (int i = 100; i < m_nextHotkeyId + 20; ++i) UnregisterHotKey((HWND)winId(), i); }
bool isLikelyMarkdown(const QString& text) {
    // Simple heuristics to detect Markdown
    // Check for headers
    if (text.contains(QRegularExpression("(?m)^#{1,6}\\s"))) return true;
    // Check for bold/italic
    if (text.contains(QRegularExpression("(\\*\\*|__)(?=\\S)(.+?)(?<=\\S)\\1"))) return true;
    // Check for unordered lists
    if (text.contains(QRegularExpression("(?m)^\\s*[-*+]\\s+"))) return true;
    // Check for ordered lists
    if (text.contains(QRegularExpression("(?m)^\\s*\\d+\\.\\s+"))) return true;
    // Check for code blocks
    if (text.contains("```")) return true;
    // Check for blockquotes
    if (text.contains(QRegularExpression("(?m)^>\\s"))) return true;
    // Check for links
    if (text.contains(QRegularExpression("\\[.*?\\]\\(.*?\\)"))) return true;
    
    return false;
}

void ClipboardAssistant::handlePluginOutput(const QString& t, bool a, bool f) { 
    if (!a) ui->textOutput->clear(); 
    ui->textOutput->insertPlainText(t); 
    
    if (f) {
        QString currentText = ui->textOutput->toPlainText();
        // Only convert to Markdown if it looks like Markdown AND was generated by an AI plugin (which usually outputs Markdown)
        // OR if it strongly looks like Markdown regardless of source.
        if (isLikelyMarkdown(currentText)) {
             // To preserve single newlines which strictly speaking shouldn't be preserved in CommonMark but users expect it:
             // We can replace single newlines with double spaces + newline, but we must be careful not to break code blocks.
             // For now, let's trust the detection. If it is Markdown, we render it.
             ui->textOutput->setMarkdown(currentText);
        }
    } 
    ui->textOutput->moveCursor(QTextCursor::End); 
}
void ClipboardAssistant::handlePluginError(const QString& m) { 
    ui->btnCancel->setVisible(false); 
    ui->labelStatus->setText(m.isEmpty() ? tr("Cancelled.") : tr("Error.")); 
    ui->progressBar->setVisible(false); 
    m_activePlugin = nullptr; 
    if (!m.isEmpty()) QMessageBox::critical(this, tr("Error"), m); 
}