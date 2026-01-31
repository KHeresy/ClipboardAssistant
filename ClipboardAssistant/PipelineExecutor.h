#pragma once

#include <QObject>
#include <QMimeData>
#include "../Common/IClipboardModule.h"
#include "ClipboardAssistant.h"

class PipelineExecutor : public QObject, public IModuleCallback {
    Q_OBJECT
public:
    PipelineExecutor(ClipboardAssistant* parent, const ClipboardAssistant::ActionSetInfo& info, const QMimeData* initialData);
    ~PipelineExecutor();

    void start();

    void onTextData(const QString& text, bool isFinal) override;
    void onMimeData(const QMimeData* data) override;
    void onError(const QString& msg) override;
    void onFinished() override;

    void stop();

private slots:
    void executeNext();

private:
    ClipboardAssistant* m_parent;
    ClipboardAssistant::ActionSetInfo m_info;
    int m_currentIdx;
    QMimeData* m_currentData;
    QMimeData* m_nextData = nullptr; // To store full mime data from onMimeData
    QString m_accumulatedText;
    bool m_firstChunk = true;
    bool m_cancelled = false;
};
