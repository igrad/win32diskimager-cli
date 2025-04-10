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

    void HandleReadOverwriteConfirmation(const bool confirmed);
    void HandleWriteOverwriteConfirmation(const bool confirmed);
    void HandleRequestReadOperation(const QString fileName);
    void HandleRequestWriteOperation(const QString fileName);
    void HandleRequestLogicalDrives();

    // UI field update handlers
    void HandleleFileTextUpdated(const QString textValue);

 private slots:
    void DoRead();
    void DoWrite();
    void DoCancel();

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
    void WarnNotEnoughSpaceOnDisk();
    void WarnUnspecifiedIOError();
    void InfoGeneratedHash(const QString hashString);
    void RequestReadOverwriteConfirmation();
    void RequestWriteOverwriteConfirmation();
    void SetProgressBarRange(const int min, const int max);
    void ProgressBarStatus(const double mbComplete, const int completion);
    void OperationComplete(const bool cancelled);
    void StartTimers();
    void DrivesDetected(const QList<QString> drives);

private:
    void SetStatus(const Status status);
    void GetDrives();
    QString GetHomeDir() const;

    char DriveLetter;
    QString ImageFilePath;
    Status OperationStatus;
    bool ReadOnlyPartitions;
    bool SkipConfirmations;
    HANDLE VolumeHandle;
    HANDLE FileHandle;
    HANDLE RawDiskHandle;
    unsigned long long SectorSize;
    char* SectorData;
    char* SectorData2; //for verify
};
