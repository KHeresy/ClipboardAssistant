#include "ScreenCaptureAssistant.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QMimeData>
#include <QTimer>
#include <QThread>

// --- SnippetOverlay Implementation ---

SnippetOverlay::SnippetOverlay(const QPixmap& screenShot, QWidget* parent)
    : QDialog(parent), m_fullScreenPixmap(screenShot), m_isSelecting(false)
{
    // 設定為無邊框、全螢幕、置頂
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    // 設定滑鼠游標為十字
    setCursor(Qt::CrossCursor);
    // 讓 Dialog 覆蓋所有螢幕區域
    QRect totalGeo;
    for (QScreen* screen : QGuiApplication::screens()) {
        totalGeo = totalGeo.united(screen->geometry());
    }
    setGeometry(totalGeo);
}

QRect SnippetOverlay::selectedRect() const {
    return m_selectionRect.normalized();
}

QPixmap SnippetOverlay::selectedPixmap() const {
    QRect r = selectedRect();
    if (r.isEmpty()) return QPixmap();
    return m_fullScreenPixmap.copy(r);
}

void SnippetOverlay::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    
    // 1. 繪製全螢幕截圖 (背景)
    painter.drawPixmap(0, 0, m_fullScreenPixmap);

    // 2. 繪製半透明遮罩 (Dimming Layer)
    // 這裡我們用一個半透明黑色蓋住全畫面，但在選取區域則要露出原本的亮度和色彩
    // 做法：先畫全螢幕半透明黑，然後設定 CompositionMode 清除選取區域，但這樣會把背景圖也清掉。
    // 更好的做法：畫四個矩形圍繞選取區域，模擬遮罩效果。
    
    QColor dimColor(0, 0, 0, 120); // 半透明黑
    QRect r = selectedRect();

    if (r.isEmpty()) {
        painter.fillRect(rect(), dimColor);
    } else {
        // 上
        painter.fillRect(0, 0, width(), r.top(), dimColor);
        // 下
        painter.fillRect(0, r.bottom() + 1, width(), height() - r.bottom(), dimColor);
        // 左 (中間段)
        painter.fillRect(0, r.top(), r.left(), r.height(), dimColor);
        // 右 (中間段)
        painter.fillRect(r.right() + 1, r.top(), width() - r.right(), r.height(), dimColor);

        // 畫選取框紅線
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(r);

        // 顯示尺寸文字
        QString sizeText = QString("%1 x %2").arg(r.width()).arg(r.height());
        painter.setPen(Qt::white);
        painter.drawText(r.topLeft() - QPoint(0, 5), sizeText);
    }
}

void SnippetOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_selectionRect = QRect(m_startPoint, QSize());
        m_isSelecting = true;
        update();
    }
}

void SnippetOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (m_isSelecting) {
        m_selectionRect = QRect(m_startPoint, event->pos());
        update();
    }
}

void SnippetOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        // 完成選取
        accept(); 
    }
}

void SnippetOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else {
        QDialog::keyPressEvent(event);
    }
}

// --- ScreenCaptureAssistant Implementation ---

ScreenCaptureAssistant::ScreenCaptureAssistant(QObject* parent) : QObject(parent) {}

QString ScreenCaptureAssistant::name() const { return "Screen Capture"; }
QString ScreenCaptureAssistant::version() const { return "1.0.0"; }

QList<ParameterDefinition> ScreenCaptureAssistant::actionParameterDefinitions() const {
    return {
        // 不需要參數，單純觸發
    };
}

QList<PluginActionTemplate> ScreenCaptureAssistant::actionTemplates() const {
    return {
        {"capture_region", tr("Capture Region"), {}}
    };
}

IClipboardPlugin::DataTypes ScreenCaptureAssistant::supportedInputs() const {
    return IClipboardPlugin::None | IClipboardPlugin::Text | IClipboardPlugin::Image; // 其實不需要 Input，但為了流程相容可以接受任何東西然後忽略
}

IClipboardPlugin::DataTypes ScreenCaptureAssistant::supportedOutputs() const {
    return IClipboardPlugin::Image;
}

void ScreenCaptureAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) {
    // 1. 隱藏主視窗
    // 我們需要找到主視窗。通常 parent 是 ClipboardAssistant，或者透過 QApplication 找 topLevelWidgets
    QWidget* mainWindow = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->objectName() == "ClipboardAssistantClass" && w->isVisible()) {
            mainWindow = w;
            break;
        }
    }

    bool wasVisible = false;
    if (mainWindow) {
        wasVisible = mainWindow->isVisible();
        mainWindow->hide();
        // 強制處理事件循環以確保視窗真正隱藏
        QApplication::processEvents(); 
        // 稍微等待一下，避免視窗淡出特效還在
        QThread::msleep(250); 
    }

    // 2. 抓取全螢幕
    QScreen *screen = QGuiApplication::primaryScreen();
    // 抓取所有螢幕組合的大畫面
    // grabWindow(0) 在某些多螢幕配置下可能只抓主螢幕，這裡我們遍歷所有螢幕並拼接
    // 簡單起見，我們先用 grabWindow(0) 針對虛擬桌面，這通常在 Windows 上有效
    QPixmap fullScreenScreenshot = screen->grabWindow(0);

    // 3. 顯示選取介面 (Blocking Call)
    SnippetOverlay overlay(fullScreenScreenshot);
    if (overlay.exec() == QDialog::Accepted) {
        QPixmap result = overlay.selectedPixmap();
        if (!result.isNull()) {
            QMimeData* newData = new QMimeData();
            newData->setImageData(result.toImage());
            callback->onMimeData(newData);
            callback->onTextData(tr("[Image Captured]"), true);
            delete newData;
        } else {
            callback->onError(tr("No region selected."));
        }
    } else {
        callback->onError(tr("Capture cancelled."));
    }

    // 4. 恢復主視窗 (如果原本是顯示的)
    if (mainWindow && wasVisible) {
        mainWindow->show();
        // 確保視窗回到前台
        mainWindow->activateWindow();
    }
    
    callback->onFinished();
}
