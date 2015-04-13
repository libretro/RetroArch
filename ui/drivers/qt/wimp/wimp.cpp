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
QQuickWindow *window;

int Wimp::CreateMainWindow()
{
   QQmlApplicationEngine engine;
   engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
   topLevel = engine.rootObjects().value(0);
   window = qobject_cast<QQuickWindow *>(topLevel);

   SetTitle("Hello QT");
   return this->exec();
}

void Wimp::SetTitle(char* title)
{
     window->setTitle(title);
}
