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
    QList<PluginFeature> features() const override;
    void process(const QString& featureId, const QMimeData* data, IPluginCallback* callback) override;
    bool hasSettings() const override;
    void showSettings(QWidget* parent) override;

private:
    QNetworkAccessManager* m_networkManager;
};