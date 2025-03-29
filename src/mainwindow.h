/**********************************************************************
 *  This program is free software; you can redistribute it and/or     *
 *  modify it under the terms of the GNU General Public License       *
 *  as published by the Free Software Foundation; either version 2    *
 *  of the License, or (at your option) any later version.            *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     *
 *  GNU General Public License for more details.                      *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, see http://gnu.org/licenses/     *
 *  ---                                                               *
 *  Copyright (C) 2009, Justin Davis <tuxdavis@gmail.com>             *
 *  Copyright (C) 2009-2014 ImageWriter developers                    *
 *                 https://sourceforge.net/projects/win32diskimager/  *
 **********************************************************************/

#pragma once

#ifndef WINVER
#define WINVER 0x0601
#endif

#include "userinterface.h"
#include <QtWidgets/QMainWindow>
#include <QClipboard>
#include <windows.h>
#include "ui_mainwindow.h"
#include "elapsedtimer.h"

class MainWindow : public QMainWindow, public Ui::MainWindow, public UserInterface
{
    Q_OBJECT

public:
    static MainWindow* getInstance() {
        // !NOT thread safe  - first call from main only
        if (!instance)
            instance = new MainWindow();
        return instance;
    }
    static MainWindow* getInstanceIfAvailable() {
        // getInstance crashes if invoked during init to get MessageBox-parent
        // thus, we simply return instance (NULL, if not yet created) to avoid app crash.
        return instance;
    }

    ~MainWindow();
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &type, void *vMsg, long long *result) override;

public slots:
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
    void HandleSettingsLoaded(const QString imageDir, const QString fileType) override;

protected slots:
    void on_tbBrowse_clicked();
    void on_bCancel_clicked();
    void on_bWrite_clicked();
    void on_bRead_clicked();
    void on_bVerify_clicked();
    void on_leFile_editingFinished();
    void on_bHashCopy_clicked();

private slots:
    void on_cboxHashType_IdxChg();
    void on_bHashGen_clicked();

protected:
    MainWindow(QWidget* = NULL);

private:
    static MainWindow* instance;
    // find attached devices
    void setReadWriteButtonState();

    QElapsedTimer update_timer;
    ElapsedTimer *elapsed_timer = NULL;
    QClipboard *clipboard;
    void generateHash(char *filename, int hashish);
    QStringList myFileTypeList;
};
