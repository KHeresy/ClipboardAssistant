#include "ExternalAppAssistant.h"
#include <QMimeData>
#include <QDir>

ExternalAppAssistant::ExternalAppAssistant(QObject* parent) : QObject(parent), m_process(nullptr)
{
}

QString ExternalAppAssistant::name() const { return "External App Assistant"; }
QString ExternalAppAssistant::version() const { return "0.1.0"; }

QList<ParameterDefinition> ExternalAppAssistant::actionParameterDefinitions() const
{
    return {
        {"Executable", tr("Executable Path"), ParameterType::File, "", {}, tr("Full path to the .exe file")},
        {"WorkingDirectory", tr("Working Directory"), ParameterType::Directory, "", {}, tr("Directory to run the app in")},
        {"Arguments", tr("Arguments"), ParameterType::String, "{text}", {}, tr("Command line arguments. Use {text} for clipboard content.")},
        {"CaptureOutput", tr("Capture Output"), ParameterType::Bool, true, {}, tr("If checked, the app's output will be displayed as the result.")}
    };
}

QList<PluginActionTemplate> ExternalAppAssistant::actionTemplates() const
{
    QList<PluginActionTemplate> list;
    list.append({"notepad", tr("Open in Notepad"), {{"Executable", "notepad.exe"}, {"Arguments", "{text}"}, {"CaptureOutput", false}}});
    return list;
}

void ExternalAppAssistant::abort() {
    if (m_process) {
        // 重要：切斷所有可能回傳給 callback 的訊號連線
        m_process->disconnect(); 
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void ExternalAppAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError(tr("No text in clipboard")); return; }
    
    QString exe = actionParams.value("Executable").toString();
    QString workingDir = actionParams.value("WorkingDirectory").toString();
    QString argsStr = actionParams.value("Arguments").toString();
    bool captureOutput = actionParams.value("CaptureOutput", true).toBool();
    QString text = data->text();

    if (exe.isEmpty()) { callback->onError(tr("Executable path is empty")); return; }

    argsStr.replace("{text}", text);

    // 啟動前先清理舊的
    abort();

    m_process = new QProcess(this);
    if (!workingDir.isEmpty()) m_process->setWorkingDirectory(workingDir);

    connect(m_process, &QProcess::finished, [this, callback, captureOutput](int exitCode, QProcess::ExitStatus status) {
        if (!m_process) return; // 已經被 abort 清理
        
        if (status == QProcess::CrashExit) {
            callback->onError(tr("External app crashed."));
        } else {
            if (captureOutput) {
                QString output = QString::fromLocal8Bit(m_process->readAllStandardOutput());
                QString error = QString::fromLocal8Bit(m_process->readAllStandardError());
                if (!output.isEmpty()) callback->onTextData(output, false);
                if (!error.isEmpty()) callback->onTextData(tr("\nError:\n") + error, false);
            }
            callback->onTextData("", true);
            callback->onFinished();
        }
    });

    connect(m_process, &QProcess::errorOccurred, [this, callback](QProcess::ProcessError error) {
        if (!m_process) return;
        callback->onError(tr("Process error: ") + m_process->errorString());
    });

    m_process->startCommand(exe + " " + argsStr);
    
    if (!m_process->waitForStarted()) {
        callback->onError(tr("Failed to start process: ") + m_process->errorString());
    } else {
        if (captureOutput) {
            callback->onTextData(tr("Started external application...\n"), false);
        } else {
            // 重要：如果不捕捉輸出，啟動後就斷開所有訊號，避免進程結束時再次觸發 callback
            m_process->disconnect();
            callback->onTextData(tr("External application started."), true);
            callback->onFinished();
        }
    }
}
