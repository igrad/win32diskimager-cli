#include "driveio.h"
#include <windows.h>
#include <winioctl.h>
#include <shlobj.h>

namespace
{
constexpr int MS_PER_SEC = 1000;
}

DriveIO::DriveIO(QObject* parent)
    : QObject(parent)
    , DriveLetter(' ')
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

    hFile = getHandleOnFile(LPCWSTR(ImageFilePath.data()), GENERIC_WRITE);
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

    numSectors = getNumberOfSectors(hRawDisk, &SectorSize);
    if(ReadOnlyPartitions)
    {
        // Read MBR partition table
        SectorData = readSectorDataFromHandle(hRawDisk, 0, 1ul, 512ul);
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

    fileSize = getFileSizeInSectors(hFile, SectorSize);
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

        removeLockOnVolume(hVolume);
        CloseHandle(hRawDisk);
        CloseHandle(hFile);
        CloseHandle(hVolume);
        SetStatus(Status::Idle);
        SectorData = nullptr;
        hRawDisk = INVALID_HANDLE_VALUE;
        hFile = INVALID_HANDLE_VALUE;
        hVolume = INVALID_HANDLE_VALUE;
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

    unsigned long long lasti = 0ul;
    emit StartTimers();
    for (unsigned long long sectorIter = 0ul;
         (sectorIter < numSectors) && (OperationStatus == Status::Reading);
         sectorIter += 1024ul)
    {
        const unsigned long long numSectorsToRead = (numSectors - sectorIter >= 1024ul) ?
                                                        1024ul :
                                                        (numSectors - sectorIter);
        SectorData = readSectorDataFromHandle(hRawDisk,
                                              sectorIter,
                                              numSectorsToRead,
                                              SectorSize);
        if (SectorData == nullptr)
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

        if (!writeSectorDataToHandle(hFile,
                                     SectorData,
                                     sectorIter,
                                     numSectorsToRead,
                                     SectorSize))
        {
            delete[] SectorData;
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            SetStatus(Status::Idle);
            SectorData = nullptr;
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            emit WarnUnspecifiedIOError();
            return;
        }

        delete[] SectorData;
        SectorData = nullptr;
        mbComplete = SectorSize * sectorIter + 1024ul;
        emit ProgressBarStatus(mbComplete, sectorIter);
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

QString DriveIO::GetHomeDir()
{
    if (HomeDir.isEmpty())
    {
        InitializeHomeDir();
    }

    return HomeDir;
}

void DriveIO::InitializeHomeDir()
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
