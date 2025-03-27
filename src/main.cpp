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

#include <QApplication>
#include <cstdlib>
#include <cxxopts.hpp>
#include <windows.h>
#include <winioctl.h>


int main(int argc, char* argv[])
{
   cxxopts::Options options("win32diskimager",
      "A Windows tool for writing images to USB sticks or SD/CF cards.");

   options.add_options()
      ("i,image", "The image file to read into/write from")
      ("v,volume", "The volume to read from/write to")
      ("d,drive", "Same as -v")
      ("s,skip-confirmation", "Skip all confirmation prompts")
      ("x,hash", "Hash algorthm to use. If -f no hash algorithm is specified, SHA256 will be used. Options are MD5, SHA1, and SHA256.")
      ("f,write-hash-to-file", "The generated has will be written to this file")
      ("w,write", "Write image file to volume")
      ("r,read", "Read image from volume to file")
      ("a,read-only-allocated-partitions", "Read only from allocated partiftions on the volume")
      ("V,verify-only", "Only verify the image")
      ("o,write-out", "Write all output from this command run to a specified file. If -q is not specified, output will still print to the shell.")
      ("q,quiet", "Hush all output from this command. If -o is specified in tandem, output will only be put to the output file.")
      ("verbose", "Verbose output (mostly for debugging)", cxxopts::value<bool>()->default_value("false"));

   auto args = options.parse(argc, argv);
   const bool headlessMode = argc > 1;

   if(!headlessMode)
   {
      QApplication app(argc, argv);
      app.setApplicationDisplayName(VER);
      app.setAttribute(Qt::AA_UseDesktopOpenGL);

      QTranslator translator;
      if(translator.load("translations/diskimager_" + QLocale::system().name()))
         app.installTranslator(&translator);

      MainWindow* mainwindow = MainWindow::getInstance();
      mainwindow->show();

      UserInterface* ui = static_cast<UserInterface*>(mainwindow);

      return app.exec();
   }
   else
   {
      return 0;
   }
}
