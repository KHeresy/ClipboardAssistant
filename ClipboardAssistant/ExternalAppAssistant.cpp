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
        {"Executable", "Executable Path", ParameterType::File, "", {}, "Full path to the .exe file"},
        {"WorkingDirectory", "Working Directory", ParameterType::Directory, "", {}, "Directory to run the app in"},
        {"Arguments", "Arguments", ParameterType::String, "{text}", {}, "Command line arguments. Use {text} for clipboard content."},
        {"CaptureOutput", "Capture Output", ParameterType::Bool, true, {}, "If checked, the app's output will be displayed as the result."}
    };
}

QList<PluginActionSet> ExternalAppAssistant::defaultActionSets() const
{
    QList<PluginActionSet> list;
    {
        PluginActionSet f;
        f.id = "notepad";
        f.name = "Open in Notepad";
        f.parameters["Executable"] = "notepad.exe";
        f.parameters["Arguments"] = "{text}";
        f.parameters["CaptureOutput"] = false;
        list.append(f);
    }
    return list;
}

void ExternalAppAssistant::process(const QMimeData* data, const QVariantMap& actionParams, const QVariantMap& globalParams, IPluginCallback* callback)
{
    if (!data->hasText()) { callback->onError("No text in clipboard"); return; }
    
    QString exe = actionParams.value("Executable").toString();
    QString workingDir = actionParams.value("WorkingDirectory").toString();
    QString argsStr = actionParams.value("Arguments").toString();
    bool captureOutput = actionParams.value("CaptureOutput", true).toBool();
    QString text = data->text();

    if (exe.isEmpty()) { callback->onError("Executable path is empty"); return; }

    // Replace placeholder
    argsStr.replace("{text}", text);

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    if (!workingDir.isEmpty()) {
        m_process->setWorkingDirectory(workingDir);
    }

    connect(m_process, &QProcess::finished, [this, callback, captureOutput](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit) {
            callback->onError("External app crashed.");
        } else {
            if (captureOutput) {
                QString output = QString::fromLocal8Bit(m_process->readAllStandardOutput());
                QString error = QString::fromLocal8Bit(m_process->readAllStandardError());
                if (!output.isEmpty()) callback->onTextData(output, false);
                if (!error.isEmpty()) callback->onTextData("\nError:\n" + error, false);
            }
            callback->onTextData("", true);
            callback->onFinished();
        }
    });

    connect(m_process, &QProcess::errorOccurred, [this, callback](QProcess::ProcessError error) {
        callback->onError("Process error: " + m_process->errorString());
    });

    m_process->startCommand(exe + " " + argsStr);
    
    if (!m_process->waitForStarted()) {
        callback->onError("Failed to start process: " + m_process->errorString());
    } else {
        if (captureOutput) {
            callback->onTextData("Started external application...\n", false);
        } else {
            callback->onTextData("External application started.", true);
            callback->onFinished();
        }
    }
}
