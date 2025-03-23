#pragma once

#include "common.h"
#include <QFileInfo>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <sstream>
#include "disk.h"

class DriveIO: public QObject
{
    Q_OBJECT

public:
    explicit DriveIO(QObject* parent = nullptr);
    ~DriveIO();

    bool SetImageFile(const QString filePath);
    bool SetDriveLetter(const char driveLetter);
    bool SetReadOnlyPartitions(const bool readOnlyPartitions);
    bool SetSkipConfirmations(const bool skip);

public slots:
    void ValidateRead();
    void ValidateWrite();
    void DoRead();
    void DoWrite();
    void DoCancel();

    void HandleReadOverwriteConfirmation(const bool confirmed);
    void HandleWriteOverwriteConfirmation(const bool confirmed);

signals:
    void StatusChanged(const Status newStatus);
    void WarnImageFileContainsNoData();
    void WarnImageFileDoesNotExist();
    void WarnImageFileLocatedOnDrive();
    void WarnImageFilePermissions();
    void WarnNoLockOnVolume();
    void WarnFailedToUnmountVolume();
    void WarnNotEnoughSpaceOnVolume(const int required, const int availableSectors,
                                    const int sectorSize,
                                    const bool dataFound);
    void WarnUnspecifiedIOError();
    void InfoGeneratedHash(const QString hashString);
    void RequestReadOverwriteConfirmation();
    void SetProgressBarRange(const int min, const int max);
    void ProgressBarStatus(const double mbpersec, const int completion);
    void OperationComplete(const bool cancelled);

private:
    void SetStatus(const Status status);

    char DriveLetter;
    QString ImageFilePath;
    Status OperationStatus;
    bool ReadOnlyPartitions;
    bool SkipConfirmations;
    HANDLE hVolume;
    HANDLE hFile;
    HANDLE hRawDisk;
};
