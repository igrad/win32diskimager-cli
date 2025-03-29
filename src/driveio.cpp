#include "driveio.h"
#include <windows.h>
#include <winioctl.h>
#include <shlobj.h>

DriveIO::DriveIO(QObject* parent)
    : QObject(parent)
    , DriveLetter(' ')
    , ImageFilePath("")
    , HomeDir(GetHomeDir())
    , OperationStatus(Status::Idle)
    , ReadOnlyPartitions(false)
    , SkipConfirmations(false)
    , VolumeHandle(INVALID_HANDLE_VALUE)
    , FileHandle(INVALID_HANDLE_VALUE)
    , RawDiskHandle(INVALID_HANDLE_VALUE)
    , SectorSize(0ul)
    , SectorData(nullptr)
    , SectorData2(nullptr)
{
    GetDrives();
    LoadSettings();
}

DriveIO::~DriveIO()
{}

bool DriveIO::SetImageFile(const QString filePath)
{
    if(Status::Idle == OperationStatus)
    {
        ImageFilePath = filePath;
        return true;
    }

    return false;
}

bool DriveIO::SetDriveLetter(const char driveLetter)
{
    if(Status::Idle == OperationStatus)
    {
        DriveLetter = driveLetter;
        return true;
    }

    return false;
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
    else
    {
        DoRead();
    }
}

void DriveIO::DoRead()
{
    SetStatus(Status::Reading);

    double mbComplete;
    unsigned long long numSectors, fileSize, spaceNeeded = 0ull;
    int volumeID = DriveLetter - 'A';
    VolumeHandle = getHandleOnVolume(volumeID, GENERIC_READ);
    if(VolumeHandle == INVALID_HANDLE_VALUE)
    {
        SetStatus(Status::Idle);
        return;
    }

    DWORD deviceID = getDeviceID(VolumeHandle);

    if(!getLockOnVolume(VolumeHandle))
    {
        CloseHandle(VolumeHandle);
        SetStatus(Status::Idle);
        VolumeHandle = INVALID_HANDLE_VALUE;
        emit WarnNoLockOnVolume();
        return;
    }

    if (!unmountVolume(VolumeHandle))
    {
        removeLockOnVolume(VolumeHandle);
        CloseHandle(VolumeHandle);
        SetStatus(Status::Idle);
        VolumeHandle = INVALID_HANDLE_VALUE;
        emit WarnFailedToUnmountVolume();
        return;
    }

    FileHandle = getHandleOnFile(LPCWSTR(ImageFilePath.data()), GENERIC_WRITE);
    if (FileHandle == INVALID_HANDLE_VALUE)
    {
        removeLockOnVolume(VolumeHandle);
        CloseHandle(VolumeHandle);
        SetStatus(Status::Idle);
        VolumeHandle = INVALID_HANDLE_VALUE;
        emit WarnUnspecifiedIOError();
        return;
    }

    RawDiskHandle = getHandleOnDevice(deviceID, GENERIC_READ);
    if (RawDiskHandle == INVALID_HANDLE_VALUE)
    {
        removeLockOnVolume(VolumeHandle);
        CloseHandle(FileHandle);
        CloseHandle(VolumeHandle);
        SetStatus(Status::Idle);
        VolumeHandle = INVALID_HANDLE_VALUE;
        FileHandle = INVALID_HANDLE_VALUE;
        emit WarnUnspecifiedIOError();
        return;
    }

    numSectors = getNumberOfSectors(RawDiskHandle, &SectorSize);
    if(ReadOnlyPartitions)
    {
        // Read MBR partition table
        SectorData = readSectorDataFromHandle(RawDiskHandle, 0, 1ul, 512ul);
        numSectors = 1ul;
        // Read partition information
        for (unsigned long long i = 0ul; i < 4ul; i++)
        {
            uint32_t partitionStartSector = *((uint32_t*) (SectorData + 0x1BE + 8 + 16*i));
            uint32_t partitionNumSectors = *((uint32_t*) (SectorData + 0x1BE + 12 + 16*i));
            // Set numSectors to end of last partition
            if (partitionStartSector + partitionNumSectors > numSectors)
            {
                numSectors = partitionStartSector + partitionNumSectors;
            }
        }
    }

    fileSize = getFileSizeInSectors(FileHandle, SectorSize);
    if (fileSize >= numSectors)
    {
        spaceNeeded = 0ull;
    }
    else
    {
        spaceNeeded = (unsigned long long)(numSectors - fileSize) * (unsigned long long)(SectorSize);
    }

    if (!spaceAvailable(ImageFilePath.left(3).replace(QChar('/'), QChar('\\')).toLatin1().data(), spaceNeeded))
    {
        emit WarnNotEnoughSpaceOnDisk();

        removeLockOnVolume(VolumeHandle);
        CloseHandle(RawDiskHandle);
        CloseHandle(FileHandle);
        CloseHandle(VolumeHandle);
        SetStatus(Status::Idle);
        SectorData = nullptr;
        RawDiskHandle = INVALID_HANDLE_VALUE;
        FileHandle = INVALID_HANDLE_VALUE;
        VolumeHandle = INVALID_HANDLE_VALUE;
        return;
    }

    if (numSectors == 0ul)
    {
        emit SetProgressBarRange(0, 100);
    }
    else
    {
        emit SetProgressBarRange(0, (int)numSectors);
    }

    emit StartTimers();
    for (unsigned long long sectorIter = 0ul;
         (sectorIter < numSectors) && (OperationStatus == Status::Reading);
         sectorIter += 1024ul)
    {
        const unsigned long long numSectorsToRead = (numSectors - sectorIter >= 1024ul) ?
                                                        1024ul :
                                                        (numSectors - sectorIter);
        SectorData = readSectorDataFromHandle(RawDiskHandle,
                                              sectorIter,
                                              numSectorsToRead,
                                              SectorSize);
        if (SectorData == nullptr)
        {
            SetStatus(Status::Idle);
            removeLockOnVolume(VolumeHandle);
            CloseHandle(RawDiskHandle);
            CloseHandle(FileHandle);
            CloseHandle(VolumeHandle);
            RawDiskHandle = INVALID_HANDLE_VALUE;
            FileHandle = INVALID_HANDLE_VALUE;
            VolumeHandle = INVALID_HANDLE_VALUE;
            emit WarnUnspecifiedIOError();
            return;
        }

        if (!writeSectorDataToHandle(FileHandle,
                                     SectorData,
                                     sectorIter,
                                     numSectorsToRead,
                                     SectorSize))
        {
            delete[] SectorData;
            removeLockOnVolume(VolumeHandle);
            CloseHandle(RawDiskHandle);
            CloseHandle(FileHandle);
            CloseHandle(VolumeHandle);
            SetStatus(Status::Idle);
            SectorData = nullptr;
            RawDiskHandle = INVALID_HANDLE_VALUE;
            FileHandle = INVALID_HANDLE_VALUE;
            VolumeHandle = INVALID_HANDLE_VALUE;
            emit WarnUnspecifiedIOError();
            return;
        }

        delete[] SectorData;
        SectorData = nullptr;
        mbComplete = SectorSize * sectorIter + 1024ul;
        emit ProgressBarStatus(mbComplete, sectorIter);
        QCoreApplication::processEvents();
    }
    removeLockOnVolume(VolumeHandle);
    CloseHandle(RawDiskHandle);
    CloseHandle(FileHandle);
    CloseHandle(VolumeHandle);
    RawDiskHandle = INVALID_HANDLE_VALUE;
    FileHandle = INVALID_HANDLE_VALUE;
    VolumeHandle = INVALID_HANDLE_VALUE;
    emit ProgressBarStatus(0.0, 0);
    emit OperationComplete(Status::Canceled == OperationStatus);

    // Completed successfully
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

void DriveIO::HandleRequestReadOperation(const QString fileName)
{
    SetImageFile(fileName);
    ValidateRead();
}

void DriveIO::HandleRequestWriteOperation(const QString fileName)
{
    SetImageFile(fileName);
    ValidateWrite();
}

void DriveIO::SetStatus(const Status status)
{
    if(status != OperationStatus)
    {
        OperationStatus = status;
        emit StatusChanged(OperationStatus);
    }
}

void DriveIO::GetDrives()
{
    // GetLogicalDrives returns 0 on failure, or a bitmask representing
    // the drives available on the system (bit 0 = A:, bit 1 = B:, etc)
    unsigned long driveMask = GetLogicalDrives();
    int iter = 0;
    ULONG pID;

    QList<QString> driveNames;

    while (driveMask != 0)
    {
        if (driveMask & 1)
        {
            // the "A" in drivename will get incremented by the # of bits
            // we've shifted
            char drivename[] = "\\\\.\\A:\\";
            drivename[4] += iter;
            if (checkDriveType(drivename, &pID))
            {
                driveNames.insert((qulonglong)pID, (QString("[%1:\\]").arg(drivename[4])));
            }
        }

        driveMask >>= 1;
        ++iter;
    }

    emit DrivesDetected(driveNames);
}
