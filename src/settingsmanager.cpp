#include "settingsmanager.h"

#include <QDir>
#include <shlobj.h>
#include <QStandardPaths>
#include <QSettings>

SettingsManager::SettingsManager(QObject *parent)
    : QObject{parent}
    , HomeDir(GetHomeDir())
    , FileType()
{}

QString SettingsManager::GetHomeDir()
{
    if (HomeDir.isEmpty())
    {
        InitializeHomeDir();
    }

    return HomeDir;
}

void SettingsManager::InitializeHomeDir()
{
    HomeDir = QDir::homePath();
    if (HomeDir.isNull()){
        HomeDir = qgetenv("USERPROFILE");
    }

    /* Get Downloads the Windows way */
    QString downloadPath = qgetenv("DiskImagesDir");
    if (downloadPath.isEmpty()) {
        PWSTR pPath = nullptr;
        static GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39,
                                                              0xc4, 0x92, 0x5e, 0x46, 0x7b}};
        if (SHGetKnownFolderPath(downloads, 0, 0, &pPath) == S_OK) {
            downloadPath = QDir::fromNativeSeparators(QString::fromWCharArray(pPath));
            LocalFree(pPath);
            if (downloadPath.isEmpty() || !QDir(downloadPath).exists()) {
                downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            }
        }
    }

    if (downloadPath.isEmpty())
        downloadPath = QDir::currentPath();

    HomeDir = downloadPath;
}

void SettingsManager::SaveSettings()
{
    QSettings userSettings("HKEY_CURRENT_USER\\Software\\Win32DiskImager", QSettings::NativeFormat);
    userSettings.beginGroup("Settings");
    userSettings.setValue("ImageDir", HomeDir);
    userSettings.setValue("FileType", FileType);
    userSettings.endGroup();
}

void SettingsManager::LoadSettings()
{
    QSettings userSettings("HKEY_CURRENT_USER\\Software\\Win32DiskImager", QSettings::NativeFormat);
    userSettings.beginGroup("Settings");
    HomeDir = userSettings.value("ImageDir").toString();
    FileType = userSettings.value("FileType").toString();
}
