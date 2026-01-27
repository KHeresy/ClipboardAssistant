#pragma once

#include <QWidget>
#include <QKeySequence>

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

    // Adds a custom widget to the layout, below the general settings
    void setContent(QWidget *content);

private:
    Ui::ActionSetSettings *ui;
};