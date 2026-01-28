#pragma once

#include <QWidget>
#include <QKeySequence>
#include <QMap>
#include "../Common/IClipboardPlugin.h"

namespace Ui {
class ActionSetSettings;
}

class ActionSetSettings : public QWidget
{
    Q_OBJECT

public:
    explicit ActionSetSettings(QWidget *parent = nullptr);
    ~ActionSetSettings();

    QString name() const;
    void setName(const QString &name);

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &shortcut);

    bool isGlobal() const;
    void setIsGlobal(bool isGlobal);

    bool isAutoCopy() const;
    void setIsAutoCopy(bool isAutoCopy);

        // Adds a custom widget to the layout, below the general settings

        void setContent(QWidget *content);

    

        // Initialize parameter widgets based on definitions

        void setParameters(const QList<ParameterDefinition>& defs, const QVariantMap& values);

        // Get current parameter values

        QVariantMap getParameters() const;

    

    private:

        Ui::ActionSetSettings *ui;

        QMap<QString, QWidget*> m_paramWidgets;

        QList<ParameterDefinition> m_paramDefs;

    };

    