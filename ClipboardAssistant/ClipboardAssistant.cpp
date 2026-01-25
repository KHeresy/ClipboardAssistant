#include "ClipboardAssistant.h"

ClipboardAssistant::ClipboardAssistant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClipboardAssistantClass())
{
    ui->setupUi(this);
}

ClipboardAssistant::~ClipboardAssistant()
{
    delete ui;
}

