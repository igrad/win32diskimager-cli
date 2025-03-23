#include "driveio.h"

DriveIO::DriveIO(QObject* parent)
    : QObject(parent)
    , DriveLetter(" ")
    , ImageFilePath("")
    , OperationStatus(Status::Idle)
    , ReadOnlyPartitions(false)
    , SkipConfirmations(false)
{

}

DriveIO::~DriveIO()
{}

bool DriveIO::SetImageFile(const QString filePath)
{
    if(Status::Idle == OperationStatus)
    {
        ImageFilePath = filePath;
    }
}

bool DriveIO::SetDriveLetter(const char driveLetter)
{
    if(Status::Idle == OperationStatus)
    {
        DriveLetter = driveLetter;
    }
}

void DriveIO::ValidateRead()
{
    QString fileName;
    QFileInfo fileInfo(ImageFilePath);

    if(fileInfo.path() == ".")
    {
        fileName = (GetHomeDir() + "/" + ImageFilePath);
    }

    // Check whether source and target device is the same...
    if(fileName.at(0) == DriveLetter)
    {
        emit WarnImageFileLocatedOnDrive();
        return;
    }

    // Confirm overwrite if the dest. file already exists
    if(fileInfo.exists() && !SkipConfirmations)
    {
        emit RequestReadOverwriteConfirmation();
    }
}

void DriveIO::DoRead()
{
    SetStatus(Status::Reading);

    double mbpersec;
    unsigned long long i, lasti, numSectors, fileSize, spaceNeeded = 0ull;
    int volumeID = DriveLetter - 'A';
    hVolume = getHandleOnVolume(volumeID, GENERIC_READ);
    if(hVolume == INVALID_HANDLE_VALUE)
    {
        SetStatus(Status::Idle);
        return;
    }

    DWORD deviceID = getDeviceID(hVolume);

    if(!getLockOnVolume(hVolume))
    {
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        hVolume = INVALID_HANDLE_VALUE;
        emit WarnNoLockOnVolume();
        return;
    }

    if (!unmountVolume(hVolume))
    {
        removeLockOnVolume(hVolume);
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        hVolume = INVALID_HANDLE_VALUE;
        emit WarnFailedToUnmountVolume();
        return;
    }

    hFile = getHandleOnFile(LPCWSTR(myFile.data()), GENERIC_WRITE);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        removeLockOnVolume(hVolume);
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        hVolume = INVALID_HANDLE_VALUE;
        emit WarnUnspecifiedIOError();
        return;
    }

    hRawDisk = getHandleOnDevice(deviceID, GENERIC_READ);
    if (hRawDisk == INVALID_HANDLE_VALUE)
    {
        removeLockOnVolume(hVolume);
        CloseHandle(hFile);
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        hVolume = INVALID_HANDLE_VALUE;
        hFile = INVALID_HANDLE_VALUE;
        emit WarnUnspecifiedIOError();
        return;
    }

    numsectors = getNumberOfSectors(hRawDisk, &sectorsize);
    if(ReadOnlyPartitions)
    {
        // Read MBR partition table
        sectorData = readSectorDataFromHandle(hRawDisk, 0, 1ul, 512ul);
        numsectors = 1ul;
        // Read partition information
        for (i=0ul; i<4ul; i++)
        {
            uint32_t partitionStartSector = *((uint32_t*) (sectorData + 0x1BE + 8 + 16*i));
            uint32_t partitionNumSectors = *((uint32_t*) (sectorData + 0x1BE + 12 + 16*i));
            // Set numsectors to end of last partition
            if (partitionStartSector + partitionNumSectors > numsectors)
            {
                numsectors = partitionStartSector + partitionNumSectors;
            }
        }
    }

    filesize = getFileSizeInSectors(hFile, sectorsize);
    if (filesize >= numsectors)
    {
        spaceneeded = 0ull;
    }
    else
    {
        spaceneeded = (unsigned long long)(numsectors - filesize) * (unsigned long long)(sectorsize);
    }

    if (!spaceAvailable(myFile.left(3).replace(QChar('/'), QChar('\\')).toLatin1().data(), spaceneeded))
    {
        emit WarnNotEnoughSpaceOnVolume();
        removeLockOnVolume(hVolume);
        CloseHandle(hRawDisk);
        CloseHandle(hFile);
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        sectorData = NULL;
        hRawDisk = INVALID_HANDLE_VALUE;
        hFile = INVALID_HANDLE_VALUE;
        hVolume = INVALID_HANDLE_VALUE;
        return;
    }

    if (numsectors == 0ul)
    {
        emit SetProgressBarRange(0, 100);
    }
    else
    {
        emit SetProgressBarRange(0, (int)numsectors);
    }

    lasti = 0ul;
    update_timer.start();
    elapsed_timer->start();
    for (i = 0ul; i < numsectors && status == STATUS_READING; i += 1024ul)
    {
        sectorData = readSectorDataFromHandle(hRawDisk, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
        if (sectorData == NULL)
        {
            SetStatus(Status::Idle);
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            emit WarnUnspecifiedIOError();
            return;
        }

        if (!writeSectorDataToHandle(hFile, sectorData, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize))
        {
            delete[] sectorData;
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            SetStatus(Status::Idle);
            sectorData = NULL;
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            emit WarnUnspecifiedIOError();
            return;
        }

        delete[] sectorData;
        sectorData = NULL;
        if (update_timer.elapsed() >= ONE_SEC_IN_MS)
        {
            mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
            statusbar->showMessage(QString("%1MB/s").arg(mbpersec));
            update_timer.start();
            elapsed_timer->update(i, numsectors);
            lasti = i;
        }
        progressbar->setValue(i);
        emit ProgressBarStatus(mbpersec, i);
        QCoreApplication::processEvents();
    }
    removeLockOnVolume(hVolume);
    CloseHandle(hRawDisk);
    CloseHandle(hFile);
    CloseHandle(hVolume);
    hRawDisk = INVALID_HANDLE_VALUE;
    hFile = INVALID_HANDLE_VALUE;
    hVolume = INVALID_HANDLE_VALUE;
    emit ProgressBarStatus(0.0, 0);
    emit OperationComplet(Status::Cancelled = OperationStatus);

    if(Status::Exit == OperationStatus)
    {
        close();
    }
    SetStatus(Status::Idle);
}

void DriveIO::HandleReadOverwriteConfirmation(const bool confirmed)
{
    if(confirmed)
    {
        DoRead();
    }
}

void DriveIO::HandleWriteOverwriteConfirmation(const bool confirmed)
{
    if(confirmed)
    {
        DoWrite();
    }
}

void DriveIO::SetStatus(const Status status)
{
    if(status != OperationStatus)
    {
        OperationStatus = status;
        emit StatusChanged(OperationStatus);
    }
}
