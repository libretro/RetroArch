/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Andres Suarez
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

#ifndef WIMP_H
#define WIMP_H

/* this is the only define missing from config.h remove these once
 * we can build everything with a single makefile
 */

#ifndef HAVE_MENU
#define HAVE_MENU
#endif

#include "config.h"
#include "configuration.h"

#include "wimp_global.h"
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qapplication.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQuick/qquickwindow.h>

class WIMPSHARED_EXPORT Wimp : public QGuiApplication
{
    QQuickWindow *window;

    Q_OBJECT
    public:
        Wimp(int argc, char *argv[]): QGuiApplication(argc, argv) {}

        /* create the main QT window */
        int CreateMainWindow();

        /* get a pointer to RetroArch settings */
        void GetSettings(settings_t *s);

        void GetCollections(char* path);
        void GetCores(char* path);

    private:
        /* pointer to RetroArch settings */
        settings_t *settings;
        QStringList collections;
        QStringList cores;
        QQmlApplicationEngine engine;

};

#endif // WIMP_H

