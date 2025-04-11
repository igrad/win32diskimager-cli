#include "driveio.h"
#include <windows.h>
#include <winioctl.h>
#include <shlobj.h>

DriveIO::DriveIO(QObject* parent)
   : QObject(parent)
   , DriveLetter(' ')
   , ImageFilePath("")
   , OperationStatus(Status::Idle)
   , ReadOnlyPartitions(false)
   , SkipConfirmations(false)
   , VolumeHandle(INVALID_HANDLE_VALUE)
   , FileHandle(INVALID_HANDLE_VALUE)
   , RawDiskHandle(INVALID_HANDLE_VALUE)
   , SectorSize(0ul)
   , SectorData(nullptr)
   , SectorData2(nullptr)
   , HomeDir(GetHomeDir())
   , FileType("")
   , FileTypeList()
{
    GetDrives();
}

DriveIO::~DriveIO()
{}

void DriveIO::ConnectToUserInterface(const UserInterface* ui)
{
   connect(ui, &UserInterface::RequestReadOperation,
           this, &DriveIO::HandleRequestReadOperation);
   connect(ui, &UserInterface::RequestWriteOperation,
           this, &DriveIO::HandleRequestWriteOperation);
   connect(ui, &UserInterface::RequestLogicalDrives,
           this, &DriveIO::HandleRequestLogicalDrives);
   connect(ui, &UserInterface::ReadOverwriteConfirmation,
           this, &DriveIO::HandleReadOverwriteConfirmation);
   connect(ui, &UserInterface::WriteOverwriteConfirmation,
           this, &DriveIO::HandleWriteOverwriteConfirmation);
}

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

void DriveIO::ValidateWrite()
{
   if(!ImageFilePath.isEmpty())
   {
      QFileInfo fileInfo(ImageFilePath);
      if(fileInfo.exists() && fileInfo.isFile() &&
          fileInfo.isReadable() && (fileInfo.size() > 0))
      {
         if(ImageFilePath.at(0) == DriveLetter)
         {
            emit WarnImageFileLocatedOnDrive();
            return;
         }

         emit RequestWriteOverwriteConfirmation();
      }
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

void DriveIO::DoWrite()
{
   SetStatus(Status::Writing);

   bool passfail = true;

   double mbpersec;
   unsigned long long i, lasti, availablesectors, numsectors;
   const int volumeID = DriveLetter - 'A';

   // int deviceID = ui->cboxDevice->itemData(cboxDevice->currentIndex()).toInt();
   VolumeHandle = getHandleOnVolume(volumeID, GENERIC_WRITE);
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
      return;
   }

   if(!unmountVolume(VolumeHandle))
   {
      removeLockOnVolume(VolumeHandle);
      CloseHandle(VolumeHandle);
      SetStatus(Status::Idle);
      VolumeHandle = INVALID_HANDLE_VALUE;
      return;
   }

   FileHandle = getHandleOnFile(LPCWSTR(ImageFilePath.toStdString().c_str()), GENERIC_READ);
   if(FileHandle == INVALID_HANDLE_VALUE)
   {
      removeLockOnVolume(VolumeHandle);
      CloseHandle(VolumeHandle);
      SetStatus(Status::Idle);
      VolumeHandle = INVALID_HANDLE_VALUE;
      return;
   }

   RawDiskHandle = getHandleOnDevice(deviceID, GENERIC_WRITE);
   if(RawDiskHandle == INVALID_HANDLE_VALUE)
   {
      removeLockOnVolume(VolumeHandle);
      CloseHandle(FileHandle);
      CloseHandle(VolumeHandle);
      SetStatus(Status::Idle);
      VolumeHandle = INVALID_HANDLE_VALUE;
      FileHandle = INVALID_HANDLE_VALUE;
      return;
   }

   availablesectors = getNumberOfSectors(RawDiskHandle, &SectorSize);
   if(!availablesectors)
   {
      //For external card readers you may not get device change notification when you remove the card/flash.
      //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
      removeLockOnVolume(VolumeHandle);
      CloseHandle(RawDiskHandle);
      CloseHandle(FileHandle);
      CloseHandle(VolumeHandle);
      RawDiskHandle = INVALID_HANDLE_VALUE;
      FileHandle = INVALID_HANDLE_VALUE;
      VolumeHandle = INVALID_HANDLE_VALUE;
      passfail = false;
      SetStatus(Status::Idle);
      return;

   }
   numsectors = getFileSizeInSectors(FileHandle, SectorSize);
   if (!numsectors)
   {
      //For external card readers you may not get device change notification when you remove the card/flash.
      //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
      removeLockOnVolume(VolumeHandle);
      CloseHandle(RawDiskHandle);
      CloseHandle(FileHandle);
      CloseHandle(VolumeHandle);
      RawDiskHandle = INVALID_HANDLE_VALUE;
      FileHandle = INVALID_HANDLE_VALUE;
      VolumeHandle = INVALID_HANDLE_VALUE;
      SetStatus(Status::Idle);
      return;

   }
   if (numsectors > availablesectors)
   {
      bool datafound = false;
      i = availablesectors;
      unsigned long nextchunksize = 0;
      while ( (i < numsectors) && (datafound == false) )
      {
         nextchunksize = ((numsectors - i) >= 1024ul) ? 1024ul : (numsectors - i);
         SectorData = readSectorDataFromHandle(FileHandle, i, nextchunksize, SectorSize);
         if(SectorData == nullptr)
         {
            // if there's an error verifying the truncated data, just move on to the
            //  write, as we don't care about an error in a section that we're not writing...
            i = numsectors + 1;
         } else {
            unsigned int j = 0;
            unsigned limit = nextchunksize * SectorSize;
            while ( (datafound == false) && ( j < limit ) )
            {
               if(SectorData[j++] != 0)
               {
                  datafound = true;
               }
            }
            i += nextchunksize;
         }
      }
      // delete the allocated SectorData
      delete[] SectorData;
      SectorData = nullptr;
      // build the string for the warning dialog
      std::ostringstream msg;
      msg << "More space required than is available:"
          << "\n  Required: " << numsectors << " sectors"
          << "\n  Available: " << availablesectors << " sectors"
          << "\n  Sector Size: " << SectorSize
          << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
          << "\n\nContinue Anyway?";
      emit WarnNotEnoughSpaceOnVolume(numsectors, availablesectors, SectorSize, datafound);
      if(QMessageBox::warning(this, tr("Not enough available space!"),
                               tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
      {
         // truncate the image at the device size...
         numsectors = availablesectors;
      }
      else    // Cancel
      {
         removeLockOnVolume(VolumeHandle);
         CloseHandle(RawDiskHandle);
         CloseHandle(FileHandle);
         CloseHandle(VolumeHandle);
         SetStatus(Status::Idle);
         VolumeHandle = INVALID_HANDLE_VALUE;
         FileHandle = INVALID_HANDLE_VALUE;
         RawDiskHandle = INVALID_HANDLE_VALUE;
         ui->bCancel->setEnabled(false);
         SetReadWriteButtonState();
         return;
      }
   }

   ui->progressbar->setRange(0, (numsectors == 0ul) ? 100 : (int)numsectors);
   lasti = 0ul;
   update_timer.start();
   elapsed_timer->start();
   for (i = 0ul; i < numsectors && status == STATUS_WRITING; i += 1024ul)
   {
      SectorData = readSectorDataFromHandle(FileHandle, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), SectorSize);
      if (SectorData == nullptr)
      {
         removeLockOnVolume(VolumeHandle);
         CloseHandle(RawDiskHandle);
         CloseHandle(FileHandle);
         CloseHandle(VolumeHandle);
         SetStatus(Status::Idle);
         RawDiskHandle = INVALID_HANDLE_VALUE;
         FileHandle = INVALID_HANDLE_VALUE;
         VolumeHandle = INVALID_HANDLE_VALUE;
         ui->bCancel->setEnabled(false);
         SetReadWriteButtonState();
         return;
      }
      if (!writeSectorDataToHandle(RawDiskHandle, SectorData, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), SectorSize))
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
         ui->bCancel->setEnabled(false);
         SetReadWriteButtonState();
         return;
      }
      delete[] SectorData;
      SectorData = nullptr;
      QCoreApplication::processEvents();
      if (update_timer.elapsed() >= ONE_SEC_IN_MS)
      {
         mbpersec = (((double)SectorSize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
         ui->statusbar->showMessage(QString("%1 MB/s").arg(mbpersec));
         elapsed_timer->update(i, numsectors);
         update_timer.start();
         lasti = i;
      }
      ui->progressbar->setValue(i);
      QCoreApplication::processEvents();
   }
   removeLockOnVolume(VolumeHandle);
   CloseHandle(RawDiskHandle);
   CloseHandle(FileHandle);
   CloseHandle(VolumeHandle);
   RawDiskHandle = INVALID_HANDLE_VALUE;
   FileHandle = INVALID_HANDLE_VALUE;
   VolumeHandle = INVALID_HANDLE_VALUE;
   if(OperationStatus == Status::Canceled)
   {
      passfail = false;
   }
}
else if (!fileinfo.exists() || !fileinfo.isFile())
{
   QMessageBox::critical(this, tr("File Error"), tr("The selected file does not exist."));
   passfail = false;
}
else if (!fileinfo.isReadable())
{
   QMessageBox::critical(this, tr("File Error"), tr("You do not have permision to read the selected file."));
   passfail = false;
}
else if (fileinfo.size() == 0)
{
   QMessageBox::critical(this, tr("File Error"), tr("The specified file contains no data."));
   passfail = false;
}
ui->progressbar->reset();
ui->statusbar->showMessage(tr("Done."));
ui->bCancel->setEnabled(false);
SetReadWriteButtonState();
if (passfail){
   QMessageBox::information(this, tr("Complete"), tr("Write Successful."));
}

}
else
{
   QMessageBox::critical(this, tr("File Error"), tr("Please specify an image file to use."));
}
if (status == STATUS_EXIT)
{
   ui->close();
}
status = STATUS_IDLE;
elapsed_timer->stop();
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

void DriveIO::HandleRequestLogicalDrives()
{
   // If drives haven't been found, go through and find them now
   // Then emit logical drives found signal
}

void DriveIO::HandleleFileTextUpdated(const QString text)
{
   ImageFilePath = text;
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

QString DriveIO::GetHomeDir()
{
   HomeDir = QDir::homePath();
   if(HomeDir.isNull())
   {
      HomeDir = qgetenv("USERPROFILE");
   }

   /* Get Downloads the Windows way */
   QString downloadPath = qgetenv("DiskImagesDir");
   if(downloadPath.isEmpty())
   {
      PWSTR pPath = NULL;
      static GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39,
                                                            0xc4, 0x92, 0x5e, 0x46, 0x7b}};
      if(SHGetKnownFolderPath(downloads, 0, 0, &pPath) == S_OK)
      {
         downloadPath = QDir::fromNativeSeparators(QString::fromWCharArray(pPath));
         LocalFree(pPath);
         if(downloadPath.isEmpty() || !QDir(downloadPath).exists())
         {
            downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
         }
      }
   }

   if(downloadPath.isEmpty())
   {
      downloadPath = QDir::currentPath();
   }

   HomeDir = downloadPath;

   return HomeDir;
}
