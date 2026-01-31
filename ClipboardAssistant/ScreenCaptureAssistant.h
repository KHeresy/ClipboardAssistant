#pragma once

#include "../Common/IClipboardPlugin.h"
#include <QObject>
#include <QDialog>
#include <QPixmap>
#include <QPoint>
#include <QRect>

// 負責顯示全螢幕截圖並讓使用者選取區域的 Dialog
class SnippetOverlay : public QDialog {
    Q_OBJECT

public:
    explicit SnippetOverlay(const QPixmap& screenShot, QWidget* parent = nullptr);
    QRect selectedRect() const;
    QPixmap selectedPixmap() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPixmap m_fullScreenPixmap;
    QPoint m_startPoint;
    QRect m_selectionRect;
    bool m_isSelecting;
};

// 插件主類別
class ScreenCaptureAssistant : public QObject, public IClipboardPlugin {
    Q_OBJECT
        Q_INTERFACES(IClipboardPlugin)
public:
    ScreenCaptureAssistant(QObject* parent = nullptr);

    QString id() const override;
    QString name() const override;
    QString version() const override;
    QList<ParameterDefinition> actionParameterDefinitions() const override;
    QList<PluginActionTemplate> actionTemplates() const override;
    DataTypes supportedInputs() const override;
    DataTypes supportedOutputs() const override;
    
    void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) override;
};
