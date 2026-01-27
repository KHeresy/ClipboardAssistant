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
    QKeySequence customShortcut;
    bool isCustomShortcutGlobal = false;
};

struct PluginInfo {
    class IClipboardPlugin* plugin;
    bool isInternal;
    QString filePath;
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

    // Return the plugin version
    virtual QString version() const { return "1.0.0"; }
    
    // Return a list of features provided by this plugin
    virtual QList<PluginFeature> features() const = 0;
    
    // Capabilities
    enum DataType {
        None = 0,
        Text = 1 << 0,
        Image = 1 << 1,
        Rtf = 1 << 2,
        File = 1 << 3
    };
    Q_DECLARE_FLAGS(DataTypes, DataType)

    virtual DataTypes supportedInputs() const = 0;
    virtual DataTypes supportedOutputs() const = 0;
    virtual bool supportsStreaming() const { return false; }

    // Abort current operation
    virtual void abort() {}

    // Process the clipboard data using the specified feature
    virtual void process(const QString& featureId, const QMimeData* data, IPluginCallback* callback) = 0;
    
    // Check if the plugin has a settings dialog
    virtual bool hasSettings() const = 0;
    
    // Show the settings dialog
    virtual void showSettings(QWidget* parent) = 0;

    // -- New Methods for Dynamic Actions --

    // Check if this plugin allows the user to add/remove features
    virtual bool isEditable() const { return false; }

    // Request the plugin to create a new feature (usually shows a dialog)
    // Returns the ID of the new feature if successful, or empty string if cancelled.
    virtual QString createFeature(QWidget* parent) { return QString(); }

    // Request the plugin to edit an existing feature
    virtual void editFeature(const QString& featureId, QWidget* parent) {}

    // Request the plugin to delete a feature
    virtual void deleteFeature(const QString& featureId) {}

    // Update the display order of a feature
    virtual void setFeatureOrder(const QString& featureId, int order) {}
};

#define IClipboardPlugin_iid "org.gemini.ClipboardAssistant.IClipboardPlugin"
Q_DECLARE_INTERFACE(IClipboardPlugin, IClipboardPlugin_iid)
Q_DECLARE_OPERATORS_FOR_FLAGS(IClipboardPlugin::DataTypes)