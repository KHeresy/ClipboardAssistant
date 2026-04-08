#pragma once

#include <QDialog>
#include <QMap>
#include "ui_GemmiSettings.h"

struct GemmiAccount {
    QString id;
    QString displayName;
    QString apiKey;
    QString model;
    QString baseUrl;
    QString customSuffix;
    QString systemPrompt;
    QString authMode;
};

class GemmiSettings : public QDialog
{
    Q_OBJECT

public:
    GemmiSettings(QWidget* parent = nullptr);
    ~GemmiSettings();

    void accept() override;

private slots:
    void onAddAccount();
    void onRemoveAccount();
    void onAccountSelected();
    void onFieldChanged();
    void onTestAccount();
    void updateHelp();

private:
    void loadAccounts();
    void saveCurrentToMap();

    Ui::GemmiSettingsClass* ui;
    class QNetworkAccessManager* m_networkManager = nullptr;
    QMap<QString, GemmiAccount> m_accounts;
    QString m_currentAccountId;
    bool m_loading = false;
};
