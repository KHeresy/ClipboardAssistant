#pragma once

#include <QObject>
#include "../Common/IClipboardPlugin.h"

class RegExAssistant : public QObject, public IClipboardPlugin
{
    Q_OBJECT
    Q_INTERFACES(IClipboardPlugin)

public:
    RegExAssistant(QObject* parent = nullptr);
    
    QString name() const override;
    QString version() const override;
    QList<PluginFeature> features() const override;

    DataTypes supportedInputs() const override { return Text; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return false; }

    void process(const QString& featureId, const QMimeData* data, IPluginCallback* callback) override;
    bool hasSettings() const override;
    void showSettings(QWidget* parent) override;

    bool isEditable() const override;
    QString createFeature(QWidget* parent) override;
    void editFeature(const QString& featureId, QWidget* parent) override;
    void deleteFeature(const QString& featureId) override;
    void setFeatureOrder(const QString& featureId, int order) override;

private:
    void ensureDefaultActions();
};
