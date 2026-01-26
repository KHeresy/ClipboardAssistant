#pragma once

#include "openaiassistant_global.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../Common/IClipboardPlugin.h"

class OPENAIASSISTANT_EXPORT OpenAIAssistant : public QObject, public IClipboardPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.gemini.ClipboardAssistant.IClipboardPlugin")
    Q_INTERFACES(IClipboardPlugin)

public:
    OpenAIAssistant();
    ~OpenAIAssistant() override;

    QString name() const override;
    QString version() const override;
    QList<PluginFeature> features() const override;
    
    DataTypes supportedInputs() const override { return Text | Image | File; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return true; }

    void process(const QString& featureId, const QMimeData* data, IPluginCallback* callback) override;
    bool hasSettings() const override;
    void showSettings(QWidget* parent) override;

    // Editable interface
    bool isEditable() const override;
    QString createFeature(QWidget* parent) override;
    void editFeature(const QString& featureId, QWidget* parent) override;
    void deleteFeature(const QString& featureId) override;

private:
    QNetworkAccessManager* m_networkManager;
    void ensureDefaultActions();
};
