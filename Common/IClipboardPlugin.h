#pragma once

#include <QtPlugin>
#include <QString>
#include <QList>
#include <QKeySequence>
#include <QMimeData>
#include <QWidget>
#include <QVariant>
#include <QMap>

enum class ParameterType {
    String,
    Text,
    Choice,
    Bool,
    Password,
    Number,
    File,
    Directory
};

struct ParameterDefinition {
    QString id;
    QString name;
    ParameterType type;
    QVariant defaultValue;
    QStringList options; // For Choice type
    QString description;
};

struct PluginActionSet {
    QString id;
    QString name;
    QString description;
    QKeySequence defaultShortcut;
    QKeySequence customShortcut;
    bool isCustomShortcutGlobal = false;
    bool isAutoCopy = false;
    QVariantMap parameters;
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
    virtual QString version() const = 0;
    
    // Parameters for an action
    virtual QList<ParameterDefinition> actionParameterDefinitions() const = 0;

    // Global parameters for the plugin (shared across actions)
    virtual QList<ParameterDefinition> globalParameterDefinitions() const { return {}; }

    // Return a list of default ActionSets provided by this plugin
    virtual QList<PluginActionSet> defaultActionSets() const { return {}; }

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

    // Process the clipboard data using the specified parameters
    virtual void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) = 0;
    
    // Check if the plugin has its own configuration dialog (optional, preferred to use globalParameterDefinitions)
    virtual bool hasConfiguration() const { return false; }
    
    // Show the configuration dialog
    virtual void showConfiguration(QWidget* parent) {}
};

QT_END_NAMESPACE

#define IClipboardPlugin_iid "org.gemini.ClipboardAssistant.IClipboardPlugin"
Q_DECLARE_INTERFACE(IClipboardPlugin, IClipboardPlugin_iid)
Q_DECLARE_OPERATORS_FOR_FLAGS(IClipboardPlugin::DataTypes)