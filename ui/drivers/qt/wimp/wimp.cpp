/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Andres Suarez
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
#include <QQuickWindow>
#include <QDir>
#include <QDebug>
#include <QQmlContext>

#include "../../../../file_path_special.h"

QObject *topLevel;

int Wimp::CreateMainWindow()
{

   engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
   topLevel = engine.rootObjects().value(0);
   window = qobject_cast<QQuickWindow *>(topLevel);

   collections.append("Collection dir not defined");

   engine.rootContext()->setContextProperty("collections", QVariant::fromValue(collections));

   cores.append("Core dir not defined");

   engine.rootContext()->setContextProperty("cores", QVariant::fromValue(cores));

   return this->exec();

}

void Wimp::GetSettings(settings_t *s)
{
    settings = s;
}
