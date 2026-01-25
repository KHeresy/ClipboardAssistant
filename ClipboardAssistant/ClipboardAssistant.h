#pragma once

#include <QtWidgets/QWidget>
#include "ui_ClipboardAssistant.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ClipboardAssistantClass; };
QT_END_NAMESPACE

class ClipboardAssistant : public QWidget
{
    Q_OBJECT

public:
    ClipboardAssistant(QWidget *parent = nullptr);
    ~ClipboardAssistant();

private:
    Ui::ClipboardAssistantClass *ui;
};

