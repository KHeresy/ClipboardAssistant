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
    QStringList options;
    QString description;
};

// Represents a single configured action instance in a pipeline
struct PluginActionInstance {
    QString pluginName;
    QVariantMap parameters;
};

// Represents a template of an action provided by a plugin
struct PluginActionTemplate {
    QString id;
    QString name;
    QVariantMap defaultParameters;
};

struct PluginInfo {
    class IClipboardPlugin* plugin;
    bool isInternal;
    QString filePath;
};

class IPluginCallback {
public:
    virtual ~IPluginCallback() = default;
    virtual void onTextData(const QString& text, bool isFinal) = 0;
    virtual void onError(const QString& message) = 0;
    virtual void onFinished() = 0;
};

class IClipboardPlugin {
public:
    virtual ~IClipboardPlugin() = default;
    
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    
    // Definitions for the host to build UI
    virtual QList<ParameterDefinition> actionParameterDefinitions() const = 0;
    virtual QList<ParameterDefinition> globalParameterDefinitions() const { return {}; }

    // Templates for the "Add Step" menu
    virtual QList<PluginActionTemplate> actionTemplates() const { return {}; }

    enum DataType { None = 0, Text = 1 << 0, Image = 1 << 1, Rtf = 1 << 2, File = 1 << 3 };
    Q_DECLARE_FLAGS(DataTypes, DataType)

    virtual DataTypes supportedInputs() const = 0;
    virtual DataTypes supportedOutputs() const = 0;
    virtual bool supportsStreaming() const { return false; }

    virtual void abort() {}
    virtual void process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback) = 0;
    
    virtual bool hasConfiguration() const { return false; }
    virtual void showConfiguration(QWidget* parent) {}
};

QT_END_NAMESPACE

#define IClipboardPlugin_iid "org.gemini.ClipboardAssistant.IClipboardPlugin"
Q_DECLARE_INTERFACE(IClipboardPlugin, IClipboardPlugin_iid)
Q_DECLARE_OPERATORS_FOR_FLAGS(IClipboardPlugin::DataTypes)
