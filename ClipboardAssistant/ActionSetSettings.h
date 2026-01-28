#pragma once

#include <QWidget>
#include <QKeySequence>
#include <QMap>
#include <QListWidget>
#include "../Common/IClipboardPlugin.h"

namespace Ui {
class ActionSetSettings;
}

class ActionSetSettings : public QWidget
{
    Q_OBJECT

public:
    explicit ActionSetSettings(const QList<PluginInfo>& plugins, QWidget *parent = nullptr);
    ~ActionSetSettings();

    QString name() const;
    void setName(const QString &name);

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &shortcut);

    bool isGlobal() const;
    void setIsGlobal(bool isGlobal);

    bool isAutoCopy() const;
    void setIsAutoCopy(bool isAutoCopy);

    void setActions(const QList<PluginActionInstance>& actions);
    QList<PluginActionInstance> getActions() const;

private slots:
    void onAddAction();
    void onRemoveAction();
    void onMoveUp();
    void onMoveDown();
    void onActionSelected(int row);

private:
    void updateActionList();
    void saveCurrentParams();
    void loadParamsForAction(int row);

        Ui::ActionSetSettings *ui;

        QList<PluginInfo> m_plugins;

        QList<PluginActionInstance> m_actions;

        

        class QScrollArea* m_scrollArea = nullptr;

        QMap<QString, QWidget*> m_paramWidgets;

        QList<ParameterDefinition> m_currentDefs;

        int m_lastRow = -1;

    };

    