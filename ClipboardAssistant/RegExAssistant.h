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
    QList<PluginActionSet> actionSets() const override;

    DataTypes supportedInputs() const override { return Text; }
    DataTypes supportedOutputs() const override { return Text; }
    bool supportsStreaming() const override { return false; }

        void process(const QString& actionSetId, const QMimeData* data, IPluginCallback* callback) override;

    

        bool hasSettings() const override;

        void showSettings(QWidget* parent) override;

        bool isEditable() const override;

        

        // Deprecated

        QString createActionSet(QWidget* parent) override;

        void editActionSet(const QString& actionSetId, QWidget* parent) override;

    

        QWidget* getSettingsWidget(const QString& actionSetId, QWidget* parent) override;

        QString saveSettings(const QString& actionSetId, QWidget* widget, const QString& name, const QKeySequence& shortcut, bool isGlobal) override;

    

        void deleteActionSet(const QString& actionSetId) override;

        void setActionSetOrder(const QString& actionSetId, int order) override;

        

    private:

        void ensureDefaultActions();

    };

    