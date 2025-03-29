#pragma once

#include <QObject>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager();

public slots:
    void HandleLoadSettingsRequest();
    void HandleSaveSettingsRequest();

signals:
    void SettingsLoaded(const QString imageDir, const QString fileType);

private:
    QString GetHomeDir();
    void InitializeHomeDir();
    void SaveSettings();
    void LoadSettings();

    QString HomeDir;
    QString FileType;
};
