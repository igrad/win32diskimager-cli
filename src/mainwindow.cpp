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
 *  Copyright (C) 2009-2017 ImageWriter developers                    *
 *                 https://sourceforge.net/projects/win32diskimager/  *
 **********************************************************************/

#ifndef WINVER
#define WINVER 0x0601
#endif

#include <QtWidgets>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDirIterator>
#include <QClipboard>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <winioctl.h>
#include <dbt.h>
#include <shlobj.h>
#include <iostream>
#include <sstream>

#include "disk.h"
#include "mainwindow.h"
#include "elapsedtimer.h"

MainWindow* MainWindow::instance = nullptr;

MainWindow::MainWindow(QWidget* parent)
{
   setParent(parent);
   ui->setupUi(TheWindow.get());
   elapsed_timer = new ElapsedTimer();
   ui->statusbar->addPermanentWidget(elapsed_timer);   // "addpermanent" puts it on the RHS of the statusbar

   emit RequestLogicalDrives();
   ui->progressbar->reset();
   clipboard = QApplication::clipboard();
   ui->statusbar->showMessage(tr("Waiting for a task."));
   // Add supported hash types.
   ui->cboxHashType->addItem("MD5",QVariant(QCryptographicHash::Md5));
   ui->cboxHashType->addItem("SHA1",QVariant(QCryptographicHash::Sha1));
   ui->cboxHashType->addItem("SHA256",QVariant(QCryptographicHash::Sha256));
   connect(this->ui->cboxHashType, SIGNAL(currentIndexChanged(int)), SLOT(HandlecboxHashType_IdxChg()));
   UpdateHashControls();
   SetReadWriteButtonState();

   emit RequestLoadSettings();
}

MainWindow::~MainWindow()
{
   emit RequestSaveSettings();

    if (elapsed_timer != nullptr)
    {
        delete elapsed_timer;
        elapsed_timer = nullptr;
    }

    if (ui->cboxHashType != nullptr)
    {
        ui->cboxHashType->clear();
    }
}

void MainWindow::SetUpUIConnections()
{
   connect(ui->bCancel, &QPushButton::clicked,
           this, &MainWindow::HandlebCancelClicked);
   connect(ui->bWrite, &QPushButton::clicked,
           this, &MainWindow::HandlebWriteClicked);
   connect(ui->bRead, &QPushButton::clicked,
           this, &MainWindow::HandlebReadClicked);
   connect(ui->bVerify, &QPushButton::clicked,
           this, &MainWindow::HandlebVerifyClicked);
   connect(ui->bHashCopy, &QPushButton::clicked,
           this, &MainWindow::HandlebHashCopyClicked);
   connect(ui->bHashGen, &QPushButton::clicked,
           this, &MainWindow::HandlebHashGenClicked);
   connect(ui->tbBrowse, &QToolButton::clicked,
           this, &MainWindow::HandletbBrowseClicked);
   connect(ui->leFile, &QLineEdit::editingFinished,
           this, &MainWindow::HandleleFileEditingFinished);
   connect(ui->cboxHashType, &QComboBox::currentIndexChanged,
           this, &MainWindow::HandlecboxHashTypeIndexChanged);
}

void MainWindow::SetReadWriteButtonState()
{
    bool fileSelected = !(ui->leFile->text().isEmpty());
    bool deviceSelected = (ui->cboxDevice->count() > 0);
    QFileInfo fi(ui->leFile->text());

    // set read and write buttons according to status of file/device
    ui->bRead->setEnabled(deviceSelected && fileSelected && (fi.exists() ? fi.isWritable() : true));
    ui->bWrite->setEnabled(deviceSelected && fileSelected && fi.isReadable());
    ui->bVerify->setEnabled(deviceSelected && fileSelected && fi.isReadable());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
   emit RequestSaveSettings();
    // if (status == STATUS_READING)
    // {
    //     if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will result in a corrupt image file.\n"
    //                                                    "Are you sure you want to exit?"),
    //                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    //     {
    //         status = STATUS_EXIT;
    //     }
    //     event->ignore();
    // }
    // else if (status == STATUS_WRITING)
    // {
    //     if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will result in a corrupt disk.\n"
    //                                                    "Are you sure you want to exit?"),
    //                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    //     {
    //         status = STATUS_EXIT;
    //     }
    //     event->ignore();
    // }
    // else if (status == STATUS_VERIFYING)
    // {
    //     if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will cancel verifying image.\n"
    //                                                    "Are you sure you want to exit?"),
    //                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    //     {
    //         status = STATUS_EXIT;
    //     }
    //     event->ignore();
    // }
}

void MainWindow::HandletbBrowseClicked()
{
    // Use the location of already entered file
    QString fileLocation = ui->leFile->text();
    QFileInfo fileinfo(fileLocation);

    // See if there is a user-defined file extension.
    QString fileTypeEnv = qgetenv("DiskImagerFiles");

    QStringList fileTypesList = fileTypeEnv.split(";;", Qt::SkipEmptyParts) + FileTypeList;
    int index = fileTypesList.indexOf(FileType);
    if (index != -1) {
        fileTypesList.move(index, 0);
    }

    // create a generic FileDialog
    QFileDialog dialog(TheWindow.get(), tr("Select a disk image"));
    dialog.setNameFilters(fileTypesList);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setViewMode(QFileDialog::Detail);
    if (fileinfo.exists())
    {
        dialog.selectFile(fileLocation);
    }
    else
    {
        dialog.setDirectory(HomeDir);
    }

    if (dialog.exec())
    {
        // selectedFiles returns a QStringList - we just want 1 filename,
        //	so use the zero'th element from that list as the filename
        fileLocation = (dialog.selectedFiles())[0];
        FileType = dialog.selectedNameFilter();

        if (!fileLocation.isNull())
        {
            ui->leFile->setText(fileLocation);
            QFileInfo newFileInfo(fileLocation);
            HomeDir = newFileInfo.absolutePath();
        }
        SetReadWriteButtonState();
        UpdateHashControls();
    }
}

void MainWindow::HandlebHashCopyClicked()
{
    QString hashSum(ui->hashLabel->text());
    if ( !(hashSum.isEmpty()) )
    {
        clipboard->setText(hashSum);
    }
}

// generates the hash
void MainWindow::generateHash(char *filename, int hashish)
{
    ui->hashLabel->setText(tr("Generating..."));
    QApplication::processEvents();

    QCryptographicHash filehash((QCryptographicHash::Algorithm)hashish);

    // may take a few secs - display a wait cursor
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QFile file(filename);
    file.open(QFile::ReadOnly);
    filehash.addData(&file);

    QByteArray hash = filehash.result();

    // display it in the textbox
    ui->hashLabel->setText(hash.toHex());
    ui->bHashCopy->setEnabled(true);
    // redisplay the normal cursor
    QApplication::restoreOverrideCursor();
}


// on an "editingFinished" signal (IE: return press), if the lineedit
// contains a valid file, update the controls
void MainWindow::HandleleFileEditingFinished()
{
    SetReadWriteButtonState();
    UpdateHashControls();
}

void MainWindow::HandlebCancelClicked()
{
    if ( (status == STATUS_READING) || (status == STATUS_WRITING) )
    {
        if (QMessageBox::warning(this, tr("Cancel?"), tr("Canceling now will result in a corrupt destination.\n"
                                                         "Are you sure you want to cancel?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_CANCELED;
        }
    }
    else if (status == STATUS_VERIFYING)
    {
        if (QMessageBox::warning(this, tr("Cancel?"), tr("Cancel Verify.\n"
                                                         "Are you sure you want to cancel?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_CANCELED;
        }

    }
}

void MainWindow::HandlebWriteClicked()
{
    bool passfail = true;
    if (!ui->leFile->text().isEmpty())
    {
        QFileInfo fileinfo(ui->leFile->text());
        if (fileinfo.exists() && fileinfo.isFile() &&
            fileinfo.isReadable() && (fileinfo.size() > 0) )
        {
            if (ui->leFile->text().at(0) == ui->cboxDevice->currentText().at(1))
            {
                QMessageBox::critical(this, tr("Write Error"), tr("Image file cannot be located on the target device."));
                return;
            }

            // build the drive letter as a const char *
            //   (without the surrounding brackets)
            QString qs = ui->cboxDevice->currentText();
            qs.replace(QRegularExpression("[\\[\\]]"), "");
            QByteArray qba = qs.toLocal8Bit();
            const char *ltr = qba.data();
            if (QMessageBox::warning(this, tr("Confirm overwrite"), tr("Writing to a physical device can corrupt the device.\n"
                                                                       "(Target Device: %1 \"%2\")\n"
                                                                       "Are you sure you want to continue?").arg(ui->cboxDevice->currentText()).arg(getDriveLabel(ltr)),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            {
                return;
            }
            status = STATUS_WRITING;
            ui->bCancel->setEnabled(true);
            ui->bWrite->setEnabled(false);
            ui->bRead->setEnabled(false);
            ui->bVerify->setEnabled(false);
            double mbpersec;
            unsigned long long i, lasti, availablesectors, numsectors;
            int volumeID = ui->cboxDevice->currentText().at(1).toLatin1() - 'A';
            // int deviceID = ui->cboxDevice->itemData(cboxDevice->currentIndex()).toInt();
            hVolume = getHandleOnVolume(volumeID, GENERIC_WRITE);
            if (hVolume == INVALID_HANDLE_VALUE)
            {
                status = STATUS_IDLE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            DWORD deviceID = getDeviceID(hVolume);
            if (!getLockOnVolume(hVolume))
            {
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            if (!unmountVolume(hVolume))
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            hFile = getHandleOnFile(LPCWSTR(ui->leFile->text().data()), GENERIC_READ);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            hRawDisk = getHandleOnDevice(deviceID, GENERIC_WRITE);
            if (hRawDisk == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            availablesectors = getNumberOfSectors(hRawDisk, &sectorsize);
            if (!availablesectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                passfail = false;
                status = STATUS_IDLE;
                return;

            }
            numsectors = getFileSizeInSectors(hFile, sectorsize);
            if (!numsectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                status = STATUS_IDLE;
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
                    sectorData = readSectorDataFromHandle(hFile, i, nextchunksize, sectorsize);
                    if(sectorData == nullptr)
                    {
                        // if there's an error verifying the truncated data, just move on to the
                        //  write, as we don't care about an error in a section that we're not writing...
                        i = numsectors + 1;
                    } else {
                        unsigned int j = 0;
                        unsigned limit = nextchunksize * sectorsize;
                        while ( (datafound == false) && ( j < limit ) )
                        {
                            if(sectorData[j++] != 0)
                            {
                                datafound = true;
                            }
                        }
                        i += nextchunksize;
                    }
                }
                // delete the allocated sectorData
                delete[] sectorData;
                sectorData = nullptr;
                // build the string for the warning dialog
                std::ostringstream msg;
                msg << "More space required than is available:"
                    << "\n  Required: " << numsectors << " sectors"
                    << "\n  Available: " << availablesectors << " sectors"
                    << "\n  Sector Size: " << sectorsize
                    << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
                    << "\n\nContinue Anyway?";
                if(QMessageBox::warning(this, tr("Not enough available space!"),
                                         tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
                {
                    // truncate the image at the device size...
                    numsectors = availablesectors;
                }
                else    // Cancel
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hVolume = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hRawDisk = INVALID_HANDLE_VALUE;
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
                sectorData = readSectorDataFromHandle(hFile, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData == nullptr)
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    ui->bCancel->setEnabled(false);
                    SetReadWriteButtonState();
                    return;
                }
                if (!writeSectorDataToHandle(hRawDisk, sectorData, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize))
                {
                    delete[] sectorData;
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    sectorData = nullptr;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    ui->bCancel->setEnabled(false);
                    SetReadWriteButtonState();
                    return;
                }
                delete[] sectorData;
                sectorData = nullptr;
                QCoreApplication::processEvents();
                if (update_timer.elapsed() >= ONE_SEC_IN_MS)
                {
                    mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
                    ui->statusbar->showMessage(QString("%1 MB/s").arg(mbpersec));
                    elapsed_timer->update(i, numsectors);
                    update_timer.start();
                    lasti = i;
                }
                ui->progressbar->setValue(i);
                QCoreApplication::processEvents();
            }
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            if (status == STATUS_CANCELED){
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

void MainWindow::HandlebReadClicked()
{
    QString myFile;
    if (!ui->leFile->text().isEmpty())
    {
        emit RequestReadOperation(ui->leFile->text());
    }
    else
    {
        QMessageBox::critical(this, tr("File Info"), tr("Please specify a file to save data to."));
    }

    elapsed_timer->stop();
}

// Verify image with device
void MainWindow::HandlebVerifyClicked()
{
    bool passfail = true;
    if (!ui->leFile->text().isEmpty())
    {
        QFileInfo fileinfo(ui->leFile->text());
        if (fileinfo.exists() && fileinfo.isFile() &&
            fileinfo.isReadable() && (fileinfo.size() > 0) )
        {
            if (ui->leFile->text().at(0) == ui->cboxDevice->currentText().at(1))
            {
                QMessageBox::critical(this, tr("Verify Error"), tr("Image file cannot be located on the target device."));
                return;
            }
            status = STATUS_VERIFYING;
            ui->bCancel->setEnabled(true);
            ui->bWrite->setEnabled(false);
            ui->bRead->setEnabled(false);
            ui->bVerify->setEnabled(false);
            double mbpersec;
            unsigned long long i, lasti, availablesectors, numsectors, result;
            int volumeID = ui->cboxDevice->currentText().at(1).toLatin1() - 'A';
            hVolume = getHandleOnVolume(volumeID, GENERIC_READ);
            if (hVolume == INVALID_HANDLE_VALUE)
            {
                status = STATUS_IDLE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            DWORD deviceID = getDeviceID(hVolume);
            if (!getLockOnVolume(hVolume))
            {
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            if (!unmountVolume(hVolume))
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            hFile = getHandleOnFile(LPCWSTR(ui->leFile->text().data()), GENERIC_READ);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            hRawDisk = getHandleOnDevice(deviceID, GENERIC_READ);
            if (hRawDisk == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                ui->bCancel->setEnabled(false);
                SetReadWriteButtonState();
                return;
            }
            availablesectors = getNumberOfSectors(hRawDisk, &sectorsize);
            if (!availablesectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                passfail = false;
                status = STATUS_IDLE;
                return;

            }
            numsectors = getFileSizeInSectors(hFile, sectorsize);
            if (!numsectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                status = STATUS_IDLE;
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
                    sectorData = readSectorDataFromHandle(hFile, i, nextchunksize, sectorsize);
                    if(sectorData == nullptr)
                    {
                        // if there's an error verifying the truncated data, just move on to the
                        //  write, as we don't care about an error in a section that we're not writing...
                        i = numsectors + 1;
                    } else {
                        unsigned int j = 0;
                        unsigned limit = nextchunksize * sectorsize;
                        while ( (datafound == false) && ( j < limit ) )
                        {
                            if(sectorData[j++] != 0)
                            {
                                datafound = true;
                            }
                        }
                        i += nextchunksize;
                    }
                }
                // delete the allocated sectorData
                delete[] sectorData;
                sectorData = nullptr;
                // build the string for the warning dialog
                std::ostringstream msg;
                msg << "Size of image larger than device:"
                    << "\n  Image: " << numsectors << " sectors"
                    << "\n  Device: " << availablesectors << " sectors"
                    << "\n  Sector Size: " << sectorsize
                    << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
                    << "\n\nContinue Anyway?";
                if(QMessageBox::warning(this, tr("Size Mismatch!"),
                                         tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
                {
                    // truncate the image at the device size...
                    numsectors = availablesectors;
                }
                else    // Cancel
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hVolume = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    ui->bCancel->setEnabled(false);
                    SetReadWriteButtonState();
                    return;
                }
            }
            ui->progressbar->setRange(0, (numsectors == 0ul) ? 100 : (int)numsectors);
            update_timer.start();
            elapsed_timer->start();
            lasti = 0ul;
            for (i = 0ul; i < numsectors && status == STATUS_VERIFYING; i += 1024ul)
            {
                sectorData = readSectorDataFromHandle(hFile, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData == nullptr)
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    ui->bCancel->setEnabled(false);
                    SetReadWriteButtonState();
                    return;
                }
                sectorData2 = readSectorDataFromHandle(hRawDisk, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData2 == nullptr)
                {
                    QMessageBox::critical(this, tr("Verify Failure"), tr("Verification failed at sector: %1").arg(i));
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    ui->bCancel->setEnabled(false);
                    SetReadWriteButtonState();
                    return;
                }
                result = memcmp(sectorData, sectorData2, ((numsectors - i >= 1024ul) ? 1024ul:(numsectors - i)) * sectorsize);
                if (result)
                {
                    QMessageBox::critical(this, tr("Verify Failure"), tr("Verification failed at sector: %1").arg(i));
                    passfail = false;
                    break;

                }
                if (update_timer.elapsed() >= ONE_SEC_IN_MS)
                {
                    mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
                    ui->statusbar->showMessage(QString("%1MB/s").arg(mbpersec));
                    update_timer.start();
                    elapsed_timer->update(i, numsectors);
                    lasti = i;
                }
                delete[] sectorData;
                delete[] sectorData2;
                sectorData = nullptr;
                sectorData2 = nullptr;
                ui->progressbar->setValue(i);
                QCoreApplication::processEvents();
            }
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            delete[] sectorData;
            delete[] sectorData2;
            sectorData = nullptr;
            sectorData2 = nullptr;
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            if (status == STATUS_CANCELED){
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
            QMessageBox::information(this, tr("Complete"), tr("Verify Successful."));
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

// getLogicalDrives sets cBoxDevice with any logical drives found, as long
// as they indicate that they're either removable, or fixed and on USB bus
// void MainWindow::getLogicalDrives()
// {
//     // GetLogicalDrives returns 0 on failure, or a bitmask representing
//     // the drives available on the system (bit 0 = A:, bit 1 = B:, etc)
//     unsigned long driveMask = GetLogicalDrives();
//     int i = 0;
//     ULONG pID;

//     ui->cboxDevice->clear();

//     while (driveMask != 0)
//     {
//         if (driveMask & 1)
//         {
//             // the "A" in drivename will get incremented by the # of bits
//             // we've shifted
//             char drivename[] = "\\\\.\\A:\\";
//             drivename[4] += i;
//             if (checkDriveType(drivename, &pID))
//             {
//                 ui->cboxDevice->addItem(QString("[%1:\\]").arg(drivename[4]), (qulonglong)pID);
//             }
//         }
//         driveMask >>= 1;
//         ui->cboxDevice->setCurrentIndex(0);
//         ++i;
//     }
// }

// support routine for winEvent - returns the drive letter for a given mask
//   taken from http://support.microsoft.com/kb/163503
char FirstDriveFromMask (ULONG unitmask)
{
    char i;

    for (i = 0; i < 26; ++i)
    {
        if (unitmask & 0x1)
        {
            break;
        }
        unitmask = unitmask >> 1;
    }

    return (i + 'A');
}

// register to receive notifications when USB devices are inserted or removed
// adapted from http://www.known-issues.net/qt/qt-detect-event-windows.html
bool MainWindow::nativeEvent(const QByteArray &type, void *vMsg, long long *result)
{
    Q_UNUSED(type);
    MSG *msg = (MSG*)vMsg;
    if(msg->message == WM_DEVICECHANGE)
    {
        PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)msg->lParam;
        switch(msg->wParam)
        {
        case DBT_DEVICEARRIVAL:
            if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
                if(DBTF_NET)
                {
                    char ALET = FirstDriveFromMask(lpdbv->dbcv_unitmask);
                    // add device to combo box (after sanity check that
                    // it's not already there, which it shouldn't be)
                    QString qs = QString("[%1:\\]").arg(ALET);
                    if (ui->cboxDevice->findText(qs) == -1)
                    {
                        ULONG pID;
                        char longname[] = "\\\\.\\A:\\";
                        longname[4] = ALET;
                        // checkDriveType gets the physicalID
                        if (checkDriveType(longname, &pID))
                        {
                            ui->cboxDevice->addItem(qs, (qulonglong)pID);
                            SetReadWriteButtonState();
                        }
                    }
                }
            }
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
                if(DBTF_NET)
                {
                    char ALET = FirstDriveFromMask(lpdbv->dbcv_unitmask);
                    //  find the device that was removed in the combo box,
                    //  and remove it from there....
                    //  "removeItem" ignores the request if the index is
                    //  out of range, and findText returns -1 if the item isn't found.
                    ui->cboxDevice->removeItem(ui->cboxDevice->findText(QString("[%1:\\]").arg(ALET)));
                    SetReadWriteButtonState();
                }
            }
            break;
        } // skip the rest
    } // end of if msg->message
    *result = 0; //get rid of obnoxious compiler warning
    return false; // let qt handle the rest
}

void MainWindow::HandleArgs(const int argc, const char* argv[])
{
    if(argc >= 2)
    {
        QString fileLocation = argv[1];
        QFileInfo fileInfo(fileLocation);
        ui->leFile->setText(fileInfo.absoluteFilePath());
    }
}

void MainWindow::HandleStatusChanged(const Status newStatus)
{
   CurrentStatus = newStatus;

    switch(newStatus) {
    case Status::Idle:
    case Status::Canceled:
        ui->bCancel->setEnabled(false);
        ui->bWrite->setEnabled(true);
        ui->bRead->setEnabled(true);
        ui->bVerify->setEnabled(true);
        break;
    case Status::Reading:
    case Status::Writing:
    case Status::Verifying:
        ui->bCancel->setEnabled(true);
        ui->bWrite->setEnabled(false);
        ui->bRead->setEnabled(false);
        ui->bVerify->setEnabled(false);
        break;
    case Status::Exit:
        TheWindow->close();
    }
}

void MainWindow::HandleWarnImageFileContainsNoData()
{

}

void MainWindow::HandleWarnImageFileDoesNotExist()
{

}

void MainWindow::HandleWarnImageFileLocatedOnDrive()
{

}

void MainWindow::HandleWarnImageFilePermissions()
{

}

void MainWindow::HandleWarnNoLockOnVolume()
{

}

void MainWindow::HandleWarnFailedToUnmountVolume()
{

}

void MainWindow::HandleWarnNotEnoughSpaceOnVolume(const int required, const int availableSectors,
                const int sectorSize,
                const bool dataFound)
{
   // build the string for the warning dialog
   std::ostringstream msg;
   msg << "More space required than is available:"
       << "\n  Required: " << numsectors << " sectors"
       << "\n  Available: " << availablesectors << " sectors"
       << "\n  Sector Size: " << SectorSize
       << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
       << "\n\nContinue Anyway?";

   bool confirmed = QMessageBox::warning(this, tr("Not enough available space!"),
                               tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok));

   emit ConfirmNotEnoughSpaceOnVolume(confirmed);
}

void MainWindow::HandleWarnUnspecifiedIOError()
{

}

void MainWindow::HandleInfoGeneratedHash(const QString hashString)
{

}

void MainWindow::HandleRequestReadOverwriteConfirmation()
{

}

void MainWindow::HandleSetProgressBarRange(const int min, const int max)
{

}

void MainWindow::HandleProgressBarStatus(const double mbpersec, const int completion)
{

}

void MainWindow::HandleOperationComplete(const bool cancelled)
{

}

void MainWindow::HandleStartTimers()
{
    update_timer.start();
    elapsed_timer->start();
}

void MainWindow::HandleSettingsLoaded(const QString imageDir, const QString fileType)
{
    ImageDir = imageDir;
    FileType = fileType;

    if (HomeDir.isEmpty()){
        initializeHomeDir();
    }

    if (FileType.isEmpty()) {
        FileType = tr("Disk Images (*.img *.IMG)");
    }

    FileTypeList << tr("Disk Images (*.img *.IMG)") << "*.*";
}

void MainWindow::UpdateHashControls()
{
    QFileInfo fileinfo(ui->leFile->text());
    bool validFile = (fileinfo.exists() && fileinfo.isFile() &&
                      fileinfo.isReadable() && (fileinfo.size() >0));

    ui->bHashCopy->setEnabled(false);
    ui->hashLabel->clear();

    if (ui->cboxHashType->currentIndex() != 0 && !ui->leFile->text().isEmpty() && validFile)
    {
        ui->bHashGen->setEnabled(true);
    }
    else
    {
        ui->bHashGen->setEnabled(false);
    }

    // if there's a value in the md5 label make the copy button visible
    bool haveHash = !(ui->hashLabel->text().isEmpty());
    ui->bHashCopy->setEnabled(haveHash );
}

void MainWindow::HandlecboxHashTypeIndexChanged()
{
    UpdateHashControls();
}

void MainWindow::HandlebHashGenClicked()
{
    generateHash(ui->leFile->text().toLatin1().data(),ui->cboxHashType->currentData().toInt());

}
