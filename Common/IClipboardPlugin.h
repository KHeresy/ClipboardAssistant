#pragma once

#include <QtPlugin>
#include <QString>
#include <QList>
#include <QKeySequence>
#include <QMimeData>
#include <QWidget>

struct PluginActionSet {
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
    virtual QString version() const = 0;
    
    // Return a list of ActionSets provided by this plugin
    virtual QList<PluginActionSet> actionSets() const = 0;
    
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

    // Process the clipboard data using the specified ActionSet
    virtual void process(const QString& actionSetId, const QMimeData* data, IPluginCallback* callback) = 0;
    
    // Check if the plugin has a settings dialog
    virtual bool hasSettings() const = 0;
    
    // Show the settings dialog
    virtual void showSettings(QWidget* parent) = 0;

    // -- New Methods for Dynamic Actions --

    // Check if this plugin allows the user to add/remove ActionSets
    virtual bool isEditable() const { return false; }

    // Request the plugin to create a new ActionSet (usually shows a dialog)
    // Returns the ID of the new ActionSet if successful, or empty string if cancelled.
    virtual QString createActionSet(QWidget* parent) { return QString(); }

    // Request the plugin to edit an existing ActionSet
    virtual void editActionSet(const QString& actionSetId, QWidget* parent) {}

    // Request the plugin to delete an ActionSet
    virtual void deleteActionSet(const QString& actionSetId) {}

    // Update the display order of an ActionSet
    virtual void setActionSetOrder(const QString& actionSetId, int order) {}

    // New Interface for separated settings
    // Returns a widget for editing specific settings. If actionSetId is empty, it's for creating a new action.
    virtual QWidget* getSettingsWidget(const QString& actionSetId, QWidget* parent) { return nullptr; }

    // Saves the settings. 
    // actionSetId: empty if creating new.
    // widget: the widget returned by getSettingsWidget.
    // name, shortcut, isGlobal: common settings handled by the host.
    // Returns the ID of the action set (new or existing).
    virtual QString saveSettings(const QString& actionSetId, QWidget* widget, const QString& name, const QKeySequence& shortcut, bool isGlobal) { return QString(); }
};

QT_END_NAMESPACE

#define IClipboardPlugin_iid "org.gemini.ClipboardAssistant.IClipboardPlugin"
Q_DECLARE_INTERFACE(IClipboardPlugin, IClipboardPlugin_iid)
Q_DECLARE_OPERATORS_FOR_FLAGS(IClipboardPlugin::DataTypes)