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

#include "wimp.h"
#include "stdio.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QListView>
#include <QQuickWindow>

QObject *topLevel;

static settings_t *settings;

int Wimp::CreateMainWindow(char* windowTitle)
{
   QQmlApplicationEngine engine;
   engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
   topLevel = engine.rootObjects().value(0);
   window = qobject_cast<QQuickWindow *>(topLevel);
   SetTitle(windowTitle);

   return this->exec();
}


void Wimp::SetTitle(char* title)
{
    window->setTitle(title);
}

void Wimp::ConfigGetPtr(settings_t *g_config)
{
    settings = g_config;
    /* test print the value of max users to compare with the value in RA */
    printf("Max Users: %d\n",g_config->input.max_users);
    fflush(stdout);
}
