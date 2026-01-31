#pragma once

#include <QDialog>
#include <QMap>
#include "ui_OpenAISettings.h"

struct OpenAIAccount {
    QString id;
    QString displayName;
    QString apiKey;
    QString model;
    QString baseUrl;
    QString systemPrompt;
    bool isAzure;
};

class OpenAISettings : public QDialog
{
    Q_OBJECT

public:
    OpenAISettings(QWidget *parent = nullptr);
    ~OpenAISettings();

    void accept() override;

private slots:
    void onAddAccount();
    void onRemoveAccount();
    void onAccountSelected();
    void updateHelp();
    void onFieldChanged();
    void onTestAccount();

private:
    void loadAccounts();
    void saveCurrentToMap();

    Ui::OpenAISettingsClass *ui;
    class QNetworkAccessManager* m_networkManager = nullptr;
    QMap<QString, OpenAIAccount> m_accounts;
    QString m_currentAccountId;
    bool m_loading = false;
};