#pragma once

#include <QObject>
#include "common.h"

class UserInterface : public QObject
{
   Q_OBJECT

public slots:
   virtual void HandleArgs(const int argc, const char* argv[]) = 0;
   virtual void HandleStatusChanged(const Status newStatus) = 0;
   virtual void HandleWarnImageFileContainsNoData() = 0;
   virtual void HandleWarnImageFileDoesNotExist() = 0;
   virtual void HandleWarnImageFileLocatedOnDrive() = 0;
   virtual void HandleWarnImageFilePermissions() = 0;
   virtual void HandleWarnNoLockOnVolume() = 0;
   virtual void HandleWarnFailedToUnmountVolume() = 0;
   virtual void HandleWarnNotEnoughSpaceOnVolume(const int required, const int availableSectors,
                                                 const int sectorSize,
                                                 const bool dataFound) = 0;
   virtual void HandleWarnUnspecifiedIOError() = 0;
   virtual void HandleInfoGeneratedHash(const QString hashString) = 0;
   virtual void HandleRequestReadOverwriteConfirmation() = 0;
   virtual void HandleSetProgressBarRange(const int min, const int max) = 0;
   virtual void HandleProgressBarStatus(const double mbpersec, const int completion) = 0;
   virtual void HandleOperationComplete(const bool cancelled) = 0;
   virtual void HandleStartTimers() = 0;
   virtual void HandleSettingsLoaded(const QString imageDir, const QString fileType);
   virtual void HandleLogicalDrivesDetected(/*Need args defined!*/);

signals:
   void ReadOverwriteConfirmation(const bool confirmed);
   void WriteOverwriteConfirmation(const bool confirmed);
   void RequestReadOperation(const QString fileName);
   void RequestWriteOperation(const QString fileName);
   void RequestLoadSettings();
   void RequestSaveSettings();
   void RequestLogicalDrives();
};
