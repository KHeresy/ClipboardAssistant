#include "PipelineExecutor.h"
#include <QMetaObject>
#include <QApplication>
#include "ui_ClipboardAssistant.h"

PipelineExecutor::PipelineExecutor(ClipboardAssistant* parent, const ClipboardAssistant::ActionSetInfo& info, const QMimeData* initialData)
    : m_parent(parent), m_info(info), m_currentIdx(0) {
    m_currentData = new QMimeData();
    m_currentData->setText(initialData->text());
    if (initialData->hasHtml()) m_currentData->setHtml(initialData->html());
    if (initialData->hasImage()) m_currentData->setImageData(initialData->imageData());
}

PipelineExecutor::~PipelineExecutor() {
    delete m_currentData;
}

void PipelineExecutor::start() {
    executeNext();
}

void PipelineExecutor::stop() {
    m_cancelled = true;
}

void PipelineExecutor::onTextData(const QString& text, bool isFinal) {
    if (m_cancelled) return;
    
    // 如果這是一個 Action 的終結呼叫，標記當前階段已完成，防止該 Action 再次呼叫
    if (isFinal) {
        // 我們不立即設定 m_cancelled，因為下一個 Action 還要用
        // 但我們需要一個 local flag 或是確保 executeNext 會處理。
    }

    QMetaObject::invokeMethod(m_parent, [this, text, isFinal]() {
        if (m_cancelled) return;
        m_parent->handlePluginOutput(text, !m_firstChunk, isFinal);
        if (m_firstChunk) m_firstChunk = false;
        if (isFinal) m_accumulatedText += text;
    });
}

void PipelineExecutor::onError(const QString& msg) {
    if (m_cancelled) return;
    m_cancelled = true; // 發生錯誤，整個 Pipeline 停止
    
    QMetaObject::invokeMethod(m_parent, [this, msg]() {
        m_parent->handlePluginError(msg);
        if (m_parent->m_currentExecutor == this) m_parent->m_currentExecutor = nullptr;
        deleteLater();
    });
}

void PipelineExecutor::onFinished() {
    if (m_cancelled) return;
    // 使用 QueuedConnection 確保 executeNext 在目前插件的工作徹底結束後才跑
    QMetaObject::invokeMethod(this, &PipelineExecutor::executeNext, Qt::QueuedConnection);
}

void PipelineExecutor::executeNext() {
    if (m_cancelled) return;

    if (m_currentIdx >= m_info.actions.size()) {
        QMetaObject::invokeMethod(m_parent, [this]() {
            if (m_cancelled) return;
            m_parent->ui->btnCancel->setVisible(false);
            m_parent->ui->labelStatus->setText("Pipeline Done.");
            m_parent->ui->progressBar->setVisible(false);
            m_parent->m_activePlugin = nullptr;
            if (m_parent->m_currentExecutor == this) m_parent->m_currentExecutor = nullptr;
            deleteLater();
        });
        return;
    }

    const auto& action = m_info.actions[m_currentIdx++];
    IClipboardPlugin* plugin = nullptr;
    for (const auto& pi : m_parent->m_plugins) {
        if (pi.plugin->name() == action.pluginName) {
            plugin = pi.plugin;
            break;
        }
    }
    
    if (!plugin) {
        onError("Plugin not found: " + action.pluginName);
        return;
    }

    m_parent->m_activePlugin = plugin;
    m_parent->ui->labelStatus->setText(QString("[%1/%2] %3...")
        .arg(m_currentIdx)
        .arg(m_info.actions.size())
        .arg(plugin->name()));
    m_firstChunk = true;

    if (m_currentIdx > 1) {
        delete m_currentData;
        m_currentData = new QMimeData();
        m_currentData->setText(m_accumulatedText);
        m_accumulatedText.clear();
    }

    // 啟動下一個插件
    plugin->process(m_currentData, action.parameters, m_parent->m_globalSettingsMap[plugin->name()], this);
}
