/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QAbstractEventDispatcher>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../ui_companion_driver.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"
#include "../../../version.h"
#include "../../../frontend/frontend.h"
#include "../../../tasks/tasks_internal.h"
#include <retro_timers.h>
#ifdef Q_OS_UNIX
#include <locale.h>
#endif

#ifndef CXX_BUILD
}
#endif

#include "../ui_qt.h"

static AppHandler *appHandler;
static ui_application_qt_t ui_application;

/* these must last for the lifetime of the QApplication */
static int app_argc = 1;
static char app_name[] = "retroarch";
static char *app_argv[] = { app_name, NULL };

/* ARGB 16x16 */
static const unsigned retroarch_qt_icon_data[] = {
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
};

AppHandler::AppHandler(QObject *parent) :
   QObject(parent)
{
}

AppHandler::~AppHandler()
{
}

void AppHandler::exit()
{
   ui_application_qt.exiting = true;

   if (qApp)
      qApp->closeAllWindows();
}

bool AppHandler::isExiting() const
{
   return ui_application_qt.exiting;
}

void AppHandler::onLastWindowClosed()
{
}

static void* ui_application_qt_initialize(void)
{
   appHandler = new AppHandler();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   /* HiDpi supported since Qt 5.6 */
   QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

   QApplication::setStyle("fusion");

   ui_application.app = new QApplication(app_argc, app_argv);
   ui_application.app->setOrganizationName("libretro");
   ui_application.app->setApplicationName("RetroArch");
   ui_application.app->setApplicationVersion(PACKAGE_VERSION);
   ui_application.app->connect(ui_application.app, SIGNAL(lastWindowClosed()), appHandler, SLOT(onLastWindowClosed()));

#ifdef Q_OS_UNIX
   setlocale(LC_NUMERIC, "C");
#endif
   {
      /* Can't declare the pixmap at the top, because: "QPixmap: Must construct a QGuiApplication before a QPixmap" */
      QImage iconImage(16, 16, QImage::Format_ARGB32);
      QPixmap iconPixmap;
      unsigned char *bits = iconImage.bits();

      memcpy(bits, retroarch_qt_icon_data, 16 * 16 * sizeof(unsigned));

      iconPixmap = QPixmap::fromImage(iconImage);

      ui_application.app->setWindowIcon(QIcon(iconPixmap));
   }

   return &ui_application;
}

static void ui_application_qt_process_events(void)
{
   QAbstractEventDispatcher *dispatcher = QApplication::eventDispatcher();
   if (dispatcher && dispatcher->hasPendingEvents())
      QApplication::processEvents();
}

static void ui_application_qt_quit(void)
{
   if (appHandler)
      appHandler->exit();
}

#ifdef HAVE_MAIN
#if defined(__cplusplus) && !defined(CXX_BUILD)
extern "C"
#endif
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif

ui_application_t ui_application_qt = {
   ui_application_qt_initialize,
   ui_application_qt_process_events,
   ui_application_qt_quit,
   false,
   "qt"
};
