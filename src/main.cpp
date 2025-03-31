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
#   define WINVER 0x0601
#endif

#include "mainwindow.h"
#include "driveio.h"
#include "argsmanager.h"

#include <QApplication>
#include <cstdlib>
#include <cxxopts.hpp>
#include <windows.h>
#include <winioctl.h>

QCoreApplication* createApp(const bool headlessMode, int argc, char* argv[])
{
   if(headlessMode)
   {
      return new QCoreApplication(argc, argv);
   }
   else
   {
      return new QApplication(argc, argv);
   }
}

int main(int argc, char* argv[])
{
   ArgsManager args(argc, argv);
   const bool headlessMode = args.GetArgValue(ArgID::Headless).toBool();

   DriveIO driveIO;

   QScopedPointer<QCoreApplication> app(createApp(headlessMode, argc, argv));

   if(!headlessMode)
   {
      QScopedPointer<QApplication> theApp(qobject_cast<QApplication*>(app.data()));
      theApp.get()->setApplicationDisplayName(VER);
      theApp.get()->setAttribute(Qt::AA_UseDesktopOpenGL);

      QTranslator translator;
      if(translator.load("translations/diskimager_" + QLocale::system().name()))
         theApp.get()->installTranslator(&translator);

      MainWindow* mainwindow = MainWindow::getInstance();
      mainwindow->show();
   }
   else
   {

      return 0;
   }

   return app.get()->exec();
}
