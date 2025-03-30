#pragma once

#include <QObject>
#include "userinterface.h"

class HeadlessRunner: public UserInterface
{
    Q_OBJECT

public:
    explicit HeadlessRunner(QObject* parent = nullptr);
    ~HeadlessRunner();

public slots:
    void HandleArgs(int argc, char* argv[]) = override;
    void HandleStatusChanged(const Status newStatus) override;
    void HandleWarnImageFileContainsNoData() override;
    void HandleWarnImageFileDoesNotExist() override;
    void HandleWarnImageFileLocatedOnDrive() override;
    void HandleWarnImageFilePermissions() override;
    void HandleWarnNoLockOnVolume() override;
    void HandleWarnFailedToUnmountVolume() override;
    void HandleWarnNotEnoughSpaceOnVolume(const int required, const int availableSectors,
                                    const int sectorSize,
                                    const bool dataFound) override;
    void HandleWarnUnspecifiedIOError() override;
    void HandleInfoGeneratedHash(const QString hashString) override;
    void HandleRequestReadOverwriteConfirmation() override;
    void HandleSetProgressBarRange(const int min, const int max) override;
    void HandleProgressBarStatus(const double mbpersec, const int completion) override;
    void HandleOperationComplete(const bool cancelled) override;
    void HandleStartTimers() override;
    void HandleSettingsLoaded(const QString imageDir, const QString fileType);

};
