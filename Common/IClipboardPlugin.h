#pragma once

#include <QtPlugin>
#include <QString>
#include <QList>
#include <QKeySequence>
#include <QMimeData>
#include <QWidget>

struct PluginFeature {
    QString id;
    QString name;
    QString description;
    QKeySequence defaultShortcut;
};

class IPluginCallback {
public:
    virtual ~IPluginCallback() = default;
    
    // Called when text data is available (can be called multiple times for streaming)
    // isFinal should be true if this is the last chunk of data OR if the operation is complete with this chunk.
    virtual void onTextData(const QString& text, bool isFinal) = 0;
    
    // Called when an error occurs
    virtual void onError(const QString& message) = 0;
    
    // Called when processing is completely finished (optional if isFinal was used, but good for cleanup)
    virtual void onFinished() = 0;
};

class IClipboardPlugin {
public:
    virtual ~IClipboardPlugin() = default;
    
    // Return the plugin name
    virtual QString name() const = 0;
    
    // Return a list of features provided by this plugin
    virtual QList<PluginFeature> features() const = 0;
    
    // Process the clipboard data using the specified feature
    // The plugin should call methods on the callback object to report results.
    // The caller guarantees the callback object remains valid until onFinished is called or the operation is cancelled (not implemented yet).
    virtual void process(const QString& featureId, const QMimeData* data, IPluginCallback* callback) = 0;
    
    // Check if the plugin has a settings dialog
    virtual bool hasSettings() const = 0;
    
    // Show the settings dialog
    virtual void showSettings(QWidget* parent) = 0;
};

#define IClipboardPlugin_iid "org.gemini.ClipboardAssistant.IClipboardPlugin"
Q_DECLARE_INTERFACE(IClipboardPlugin, IClipboardPlugin_iid)
